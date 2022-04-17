/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "ServiceReceiver.hpp"
#include "Version.hpp"
#include "Signals.hpp"
#include "Sentry.hpp"
#include "AudioStreamer.hpp"
#include "WindowPositions.hpp"

#if !defined(JUCE_WINDOWS)
#include <signal.h>
#endif

namespace e47 {

PluginProcessor::PluginProcessor(AudioProcessor::WrapperType wt)
    : AudioProcessor(createBusesProperties(wt)), m_channelMapper(this) {
    initAsyncFunctors();

    Defaults::initPluginTheme();

#if JucePlugin_IsSynth
    m_mode = "Instrument";
#elif JucePlugin_IsMidiEffect
    m_mode = "Midi";
#else
    m_mode = "FX";
#endif

    String appName = m_mode;
    String logName = "AudioGridderPlugin_";

    Logger::initialize(appName, logName, Defaults::getConfigFileName(Defaults::ConfigPlugin));

    m_client = std::make_unique<Client>(this);
    setLogTagSource(m_client.get());
    logln(m_mode << " plugin loaded, " << getWrapperTypeDescription(wrapperType)
                 << " (version: " << AUDIOGRIDDER_VERSION << ", build date: " << AUDIOGRIDDER_BUILD_DATE << ")");

    Tracer::initialize(appName, logName);
    Signals::initialize();
    Metrics::initialize();
    WindowPositions::initialize();

    traceScope();

    ServiceReceiver::initialize(m_instId.hash(), [this] {
        traceScope();
        runOnMsgThreadAsync([this] {
            traceScope();
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<PluginEditor*>(editor)->setConnected(m_client->isReadyLockFree());
            }
        });
    });

    updateLatency(0);

    loadConfig();

    if (supportsCrashReporting() && m_crashReporting) {
        Sentry::initialize();
    }

    m_unusedParam.name = "(unassigned)";
    m_unusedDummyPlugin.name = "(unused)";
    m_unusedDummyPlugin.bypassed = false;
    m_unusedDummyPlugin.ok = true;
    m_unusedDummyPlugin.params.add(m_unusedParam);

    for (int i = 0; i < m_numberOfAutomationSlots; i++) {
        auto pparam = new Parameter(*this, i);
        pparam->addListener(this);
        addParameter(pparam);
    }

#if JucePlugin_IsSynth
    // activate main outs per default
    for (int c = 0; c < getMainBusNumOutputChannels(); c++) {
        m_activeChannels.setOutputActive(c);
    }
#elif !JucePlugin_IsMidiEffect
    // activate all input/output channels per default
    m_activeChannels.setRangeActive();
#endif

    m_channelMapper.setLogTagSource(this);

    // load plugins on reconnect
    m_client->setOnConnectCallback(safeLambda([this] {
        traceScope();
        logln("connected");
        bool updLatency = false;
        std::vector<std::tuple<int, int, int>> automationParams;
        int idx = 0;
        {
            std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
            for (auto& p : m_loadedPlugins) {
                logln("loading " << p.name << " (" << p.id << ") [on connect]... ");
                String err;
                bool scDisabled;
                p.ok = m_client->addPlugin(p.id, p.presets, p.params, p.hasEditor, scDisabled, p.settings, err);
                if (p.ok) {
                    logln("...ok");
                } else {
                    logln("...failed: " << err);
                }
                if (p.ok) {
                    updLatency = true;
                    if (p.bypassed) {
                        m_client->bypassPlugin(idx);
                    }
                    for (auto& param : p.params) {
                        if (param.automationSlot > -1) {
                            if (param.automationSlot < m_numberOfAutomationSlots) {
                                automationParams.push_back({idx, param.idx, param.automationSlot});
                            } else {
                                param.automationSlot = -1;
                            }
                        }
                    }
                }
                idx++;
            }
        }
        m_client->setLoadedPluginsString(getLoadedPluginsString());

        for (auto& ap : automationParams) {
            enableParamAutomation(std::get<0>(ap), std::get<1>(ap), std::get<2>(ap));
        }

        if (updLatency) {
            updateLatency(m_client->getLatencySamples());
        }

        runOnMsgThreadAsync([this] {
            traceScope();
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<PluginEditor*>(editor)->setConnected(true);
            }
        });
    }));

    // handle connection close
    m_client->setOnCloseCallback(safeLambda([this] {
        traceScope();
        logln("disconnected");
        runOnMsgThreadAsync([this] {
            traceScope();
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<PluginEditor*>(editor)->setConnected(false);
            }
        });
    }));

    if (m_activeServerFromCfg.isNotEmpty()) {
        m_client->setServer(m_activeServerFromCfg);
    } else if (m_activeServerLegacyFromCfg > -1 && m_activeServerLegacyFromCfg < m_servers.size()) {
        m_client->setServer(m_servers[m_activeServerLegacyFromCfg]);
    }

#if !JucePlugin_IsSynth && !JucePlugin_IsMidiEffect
    if (m_defaultPreset.isNotEmpty()) {
        File preset(m_defaultPreset);
        if (preset.existsAsFile()) {
            loadPreset(preset);
        }
    }
#endif

    if (!m_disableTray) {
        m_tray = std::make_unique<TrayConnection>(this);
    }
}

PluginProcessor::~PluginProcessor() {
    traceScope();
    stopAsyncFunctors();
    logln("plugin shutdown: terminating client");
    m_client->signalThreadShouldExit();
    m_client->close();
    waitForThreadAndLog(m_client.get(), m_client.get());
    m_tray.reset();
    logln("plugin shutdown: cleaning up");
    WindowPositions::cleanup();
    Metrics::cleanup();
    ServiceReceiver::cleanup(m_instId.hash());
    logln("plugin unloaded");
    Tracer::cleanup();
    Logger::cleanup();
    Sentry::cleanup();
}

void PluginProcessor::loadConfig() {
    traceScope();
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
    if (cfg.size() > 0) {
        loadConfig(cfg);
    }
}

void PluginProcessor::loadConfig(const json& j, bool isUpdate) {
    traceScope();

    Tracer::setEnabled(jsonGetValue(j, "Tracer", Tracer::isEnabled()));
    Logger::setEnabled(jsonGetValue(j, "Logger", Logger::isEnabled()));

    m_scale = jsonGetValue(j, "ZoomFactor", m_scale);

    if (!isUpdate) {
        if (jsonHasValue(j, "Servers")) {
            for (auto& srv : j["Servers"]) {
                m_servers.add(srv.get<std::string>());
            }
        }
        m_activeServerFromCfg = jsonGetValue(j, "LastServer", m_activeServerFromCfg);
        m_activeServerLegacyFromCfg = jsonGetValue(j, "Last", m_activeServerLegacyFromCfg);
        m_client->NUM_OF_BUFFERS = jsonGetValue(j, "NumberOfBuffers", m_client->NUM_OF_BUFFERS.load());
        m_client->LOAD_PLUGIN_TIMEOUT = jsonGetValue(j, "LoadPluginTimeoutMS", m_client->LOAD_PLUGIN_TIMEOUT.load());

        if (m_scale != Desktop::getInstance().getGlobalScaleFactor()) {
            Desktop::getInstance().setGlobalScaleFactor(m_scale);
        }
    }

    m_numberOfAutomationSlots = jsonGetValue(j, "NumberOfAutomationSlots", m_numberOfAutomationSlots);
    m_menuShowCategory = jsonGetValue(j, "MenuShowCategory", m_menuShowCategory);
    m_menuShowCompany = jsonGetValue(j, "MenuShowCompany", m_menuShowCompany);
    m_genericEditor = jsonGetValue(j, "GenericEditor", m_genericEditor);
    m_confirmDelete = jsonGetValue(j, "ConfirmDelete", m_confirmDelete);
    m_transferWhenPlayingOnly = jsonGetValue(j, "TransferWhenPlayingOnly", m_transferWhenPlayingOnly);
    m_syncRemote = jsonGetValue(j, "SyncRemoteMode", m_syncRemote);
    m_presetsDir = jsonGetValue(j, "PresetsDir", Defaults::PRESETS_DIR);
    m_defaultPreset = jsonGetValue(j, "DefaultPreset", m_defaultPreset);
    m_editAlways = jsonGetValue(j, "EditAlways", m_editAlways);
    auto noSrvPluginListFilter = jsonGetValue(j, "NoSrvPluginListFilter", m_noSrvPluginListFilter);
    if (noSrvPluginListFilter != m_noSrvPluginListFilter) {
        m_noSrvPluginListFilter = noSrvPluginListFilter;
        m_client->reconnect();
    }
    m_crashReporting = jsonGetValue(j, "CrashReporting", m_crashReporting);
    m_showSidechainDisabledInfo = jsonGetValue(j, "ShowSidechainDisabledInfo", m_showSidechainDisabledInfo);
    m_disableTray = jsonGetValue(j, "DisableTray", m_disableTray);
    m_disableRecents = jsonGetValue(j, "DisableRecents", m_disableRecents);
    m_keepEditorOpen = jsonGetValue(j, "KeepEditorOpen", m_keepEditorOpen);
    m_bypassWhenNotConnected = jsonGetValue(j, "BypassWhenNotConnected", m_bypassWhenNotConnected.load());
}

void PluginProcessor::saveConfig(int numOfBuffers) {
    traceScope();

    auto jservers = json::array();
    for (auto& srv : m_servers) {
        jservers.push_back(srv.toStdString());
    }

    if (numOfBuffers < 0) {
        numOfBuffers = m_client->NUM_OF_BUFFERS;
    }

    json jcfg;
    jcfg["_comment_"] = "PLEASE DO NOT CHANGE THIS FILE WHILE YOUR DAW IS RUNNING AND HAS AUDIOGRIDDER PLUGINS LOADED";
    jcfg["Servers"] = jservers;
    jcfg["LastServer"] = m_client->getServer().serialize().toStdString();
    jcfg["NumberOfBuffers"] = numOfBuffers;
    jcfg["NumberOfAutomationSlots"] = m_numberOfAutomationSlots;
    jcfg["LoadPluginTimeoutMS"] = m_client->LOAD_PLUGIN_TIMEOUT.load();
    jcfg["MenuShowCategory"] = m_menuShowCategory;
    jcfg["MenuShowCompany"] = m_menuShowCompany;
    jcfg["GenericEditor"] = m_genericEditor;
    jcfg["ConfirmDelete"] = m_confirmDelete;
    jcfg["TransferWhenPlayingOnly"] = m_transferWhenPlayingOnly;
    jcfg["Tracer"] = Tracer::isEnabled();
    jcfg["Logger"] = Logger::isEnabled();
    jcfg["SyncRemoteMode"] = m_syncRemote;
    jcfg["NoSrvPluginListFilter"] = m_noSrvPluginListFilter;
    jcfg["ZoomFactor"] = m_scale;
    jcfg["PresetsDir"] = m_presetsDir.toStdString();
    jcfg["DefaultPreset"] = m_defaultPreset.toStdString();
    jcfg["EditAlways"] = m_editAlways;
    jcfg["CrashReporting"] = m_crashReporting;
    jcfg["ShowSidechainDisabledInfo"] = m_showSidechainDisabledInfo;
    jcfg["DisableTray"] = m_disableTray;
    jcfg["DisableRecents"] = m_disableRecents;
    jcfg["KeepEditorOpen"] = m_keepEditorOpen;
    jcfg["BypassWhenNotConnected"] = m_bypassWhenNotConnected.load();

    configWriteFile(Defaults::getConfigFileName(Defaults::ConfigPlugin), jcfg);
}

void PluginProcessor::storePreset(const File& file) {
    logln("storing preset " << file.getFullPathName());
    auto j = getState(false);
    configWriteFile(file.getFullPathName(), j);
}

bool PluginProcessor::loadPreset(const File& file) {
    String err;
    auto j = configParseFile(file.getFullPathName(), &err);
    if (err.isEmpty() && !setState(j)) {
        String mode = jsonGetValue(j, "Mode", String());
        if (mode != m_mode) {
            err << "Can't load " << mode << " presets into " << m_mode << " plugins!";
        } else {
            err = "Error in the preset file. Check the plugin log for more info.";
        }
    }
    if (err.isNotEmpty()) {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error",
                                         "Failed to load preset " + file.getFullPathName() + "!\n\nError: " + err,
                                         "OK");
        return false;
    }
    return true;
}

void PluginProcessor::storePresetDefault() {
    File d(m_presetsDir);
    if (!d.exists()) {
        d.createDirectory();
    }
    File preset = d.getNonexistentChildFile("Default", "").withFileExtension(".preset");
    storePreset(preset);
    m_defaultPreset = preset.getFullPathName();
    saveConfig();
}

void PluginProcessor::resetPresetDefault() {
    File preset(m_defaultPreset);
    if (preset.existsAsFile()) {
        preset.deleteFile();
    }
    m_defaultPreset = "";
    saveConfig();
}

const String PluginProcessor::getName() const {
    auto pluginStr = getLoadedPluginsString();
    if (pluginStr.isNotEmpty()) {
        return "AG: " + pluginStr;
    } else {
        return JucePlugin_Name;
    }
}

bool PluginProcessor::acceptsMidi() const { return true; }

bool PluginProcessor::producesMidi() const { return true; }

bool PluginProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const { return 0.0; }

bool PluginProcessor::supportsDoublePrecisionProcessing() const { return true; }

int PluginProcessor::getNumPrograms() { return 1; }

int PluginProcessor::getCurrentProgram() { return 0; }

void PluginProcessor::setCurrentProgram(int /* index */) {}

const String PluginProcessor::getProgramName(int /* index */) { return {}; }

void PluginProcessor::changeProgramName(int /* index */, const String& /* newName */) {}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    traceScope();
    logln("prepareToPlay: sampleRate = " << sampleRate << ", samplesPerBlock=" << samplesPerBlock);

    if (!m_client->isThreadRunning()) {
        m_client->startThread();
    }

    if (!m_disableTray && m_tray != nullptr && !m_tray->isThreadRunning()) {
        m_tray->startThread();
    }

    printBusesLayout(getBusesLayout());

    int channelsIn = 0;
    int channelsSC = 0;
    int channelsOut = 0;

    for (int i = 0; i < getBusCount(true); i++) {
        auto* bus = getBus(true, i);
        if (bus->isEnabled()) {
            if (bus->getName() != "Sidechain") {
                channelsIn += bus->getNumberOfChannels();
            } else {
                channelsSC += bus->getNumberOfChannels();
            }
        }
    }

    for (int i = 0; i < getBusCount(false); i++) {
        auto* bus = getBus(false, i);
        if (bus->isEnabled()) {
            channelsOut += bus->getNumberOfChannels();
        }
    }

    logln("uncapped channel config: " << channelsIn << ":" << channelsOut << "+" << channelsSC);

    channelsIn = jmin(channelsIn, 16);
    channelsSC = jmin(channelsSC, 16);
    channelsOut = jmin(channelsOut, 64);
    m_activeChannels.setNumChannels(channelsIn + channelsSC, channelsOut);
    updateChannelMapping();

    m_client->init(channelsIn, channelsOut, channelsSC, sampleRate, samplesPerBlock, isUsingDoublePrecision());

    m_prepared = true;
}

void PluginProcessor::releaseResources() {
    traceScope();
    logln("releaseResources");
    m_prepared = false;
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if !JucePlugin_IsSynth && !JucePlugin_IsMidiEffect
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet() ||
        layouts.getMainInputChannelSet().isDisabled()) {
        return false;
    }
#elif JucePlugin_IsSynth
    for (auto& outbus : layouts.outputBuses) {
        for (auto ct : outbus.getChannelTypes()) {
            // make sure JuceAU::busIgnoresLayout returns false (see juce_AU_Wrapper.mm)
            if (ct > 255) {
                return false;
            }
        }
    }
#endif
    int numOfInputs = 0, numOfOutputs = 0;
    for (auto& bus : layouts.inputBuses) {
        numOfInputs += bus.size();
    }
    for (auto& bus : layouts.outputBuses) {
        numOfOutputs += bus.size();
    }
    return numOfInputs <= Defaults::PLUGIN_CHANNELS_MAX && numOfOutputs <= Defaults::PLUGIN_CHANNELS_MAX;
}

//Array<std::pair<short, short>> PluginProcessor::getAUChannelInfo() const {
//#if JucePlugin_IsSynth && JUCE_MAC
//    Array<std::pair<short, short>> info;
//    info.add(std::make_pair((short)0, -(short)Defaults::PLUGIN_CHANNELS_OUT));
//    return info;
//#else
//    return {};
//#endif
//}

void PluginProcessor::numChannelsChanged() {
    traceScope();
#if JucePlugin_IsSynth
    // activate main outs per default
    m_activeChannels.setOutputRangeActive(false);
    for (int c = 0; c < getMainBusNumOutputChannels(); c++) {
        m_activeChannels.setOutputActive(c);
    }
#elif !JucePlugin_IsMidiEffect
    // activate all input/output channels per default
    m_activeChannels.setRangeActive();
#endif
}

template <typename T>
void PluginProcessor::processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();

    if (m_bypassWhenNotConnected && !m_client->isReadyLockFree()) {
        processBlockBypassed(buffer, midiMessages);
        return;
    }

    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (totalNumInputChannels > buffer.getNumChannels()) {
        logln("error in processBlock: buffer has less channels than main input channels");
        totalNumInputChannels = buffer.getNumChannels();
    }
    if (totalNumOutputChannels > buffer.getNumChannels()) {
        logln("error in processBlock: buffer has less channels than main output channels");
        totalNumOutputChannels = buffer.getNumChannels();
    }

    auto* phead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo posInfo;
    phead->getCurrentPosition(posInfo);

    // buffer to be send
    int sendBufChannels = m_activeChannels.getNumActiveChannelsCombined();
    AudioBuffer<T>* sendBuffer = sendBufChannels != buffer.getNumChannels()
                                     ? new AudioBuffer<T>(sendBufChannels, buffer.getNumSamples())
                                     : &buffer;

#if JucePlugin_IsSynth || JucePlugin_IsMidiEffect
    buffer.clear();
#else
    // clear inactive outputs if we need no mapping, as the mapper takes care otherwise
    if (sendBuffer == &buffer) {
        for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
            if (!m_activeChannels.isOutputActive(ch)) {
                buffer.clear(ch, 0, buffer.getNumSamples());
            }
        }
    }
#endif

    if (!m_transferWhenPlayingOnly || posInfo.isPlaying || posInfo.isRecording) {
        if ((buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) || midiMessages.getNumEvents() > 0) {
            auto streamer = m_client->getStreamer<T>();
            if (nullptr != streamer) {
                m_channelMapper.map(&buffer, sendBuffer);
                streamer->send(*sendBuffer, midiMessages, posInfo);
                streamer->read(*sendBuffer, midiMessages);
                m_channelMapper.mapReverse(sendBuffer, &buffer);

                if (m_client->getLatencySamples() != getLatencySamples()) {
                    runOnMsgThreadAsync([this] { updateLatency(m_client->getLatencySamples()); });
                }
            } else {
                buffer.clear();
            }
        }
    }
}

void PluginProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& /* midiMessages */) {
    traceScope();

    if (getLatencySamples() == 0) {
        return;
    }

    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (totalNumInputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main input channels");
        totalNumInputChannels = buffer.getNumChannels();
    }
    if (totalNumOutputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main output channels");
        totalNumOutputChannels = buffer.getNumChannels();
    }

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    std::lock_guard<std::mutex> lock(m_bypassBufferMtx);

    if (m_bypassBufferF.getNumChannels() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed");
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    m_bypassBufferF.write(buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    m_bypassBufferF.read(buffer.getArrayOfWritePointers(), buffer.getNumSamples());
}

void PluginProcessor::processBlockBypassed(AudioBuffer<double>& buffer, MidiBuffer& /* midiMessages */) {
    traceScope();

    if (getLatencySamples() == 0) {
        return;
    }

    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (totalNumInputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main input channels");
        totalNumInputChannels = buffer.getNumChannels();
    }
    if (totalNumOutputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main output channels");
        totalNumOutputChannels = buffer.getNumChannels();
    }

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    std::lock_guard<std::mutex> lock(m_bypassBufferMtx);

    if (m_bypassBufferD.getNumChannels() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed");
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    m_bypassBufferD.write(buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    m_bypassBufferD.read(buffer.getArrayOfWritePointers(), buffer.getNumSamples());
}

void PluginProcessor::updateLatency(int samples) {
    traceScope();
    if (!m_prepared) {
        return;
    }

    logln("updating latency samples to " << samples);
    setLatencySamples(samples);
    int channels = getTotalNumOutputChannels();

    std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
    m_bypassBufferF.resize(channels, samples * 2);
    m_bypassBufferF.clear();
    m_bypassBufferF.setReadOffset(samples);
    m_bypassBufferD.resize(channels, samples * 2);
    m_bypassBufferD.clear();
    m_bypassBufferD.setReadOffset(samples);
}

bool PluginProcessor::hasEditor() const { return true; }

AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor(*this); }

void PluginProcessor::getStateInformation(MemoryBlock& destData) {
    traceScope();
    auto j = getState(true);
    auto dump = j.dump();
    destData.append(dump.data(), dump.length());
    saveConfig();
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    traceScope();

    std::string dump(static_cast<const char*>(data), (size_t)sizeInBytes);
    try {
        json j = json::parse(dump);
        setState(j);
    } catch (json::parse_error& e) {
        logln("parsing state info failed: " << e.what());
    }
}

json PluginProcessor::getState(bool withServers) {
    traceScope();
    json j;
    j["version"] = 2;
    j["Mode"] = m_mode.toStdString();

    if (withServers) {
        auto jservers = json::array();
        for (auto& srv : m_servers) {
            jservers.push_back(srv.toStdString());
        }
        j["servers"] = jservers;
        j["activeServerStr"] = m_client->getServer().serialize().toStdString();
    }

    j["ActiveChannels"] = m_activeChannels.toInt();

    auto jplugs = json::array();
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        for (int i = 0; i < (int)m_loadedPlugins.size(); i++) {
            auto& plug = m_loadedPlugins[(size_t)i];
            if (plug.ok && m_client->isReadyLockFree()) {
                auto settings = m_client->getPluginSettings(i);
                if (settings.getSize() > 0) {
                    plug.settings = settings.toBase64Encoding();
                }
            }
            auto jpresets = json::array();
            for (auto& p : plug.presets) {
                jpresets.push_back(p.toStdString());
            }
            auto jparams = json::array();
            for (auto& p : plug.params) {
                jparams.push_back(p.toJson());
            }
            jplugs.push_back({plug.id.toStdString(), plug.name.toStdString(), plug.settings.toStdString(), jpresets,
                              jparams, plug.bypassed});
        }
    }
    j["loadedPlugins"] = jplugs;

    return j;
}

bool PluginProcessor::setState(const json& j) {
    traceScope();

    int version = jsonGetValue(j, "version", 0);

    if (jsonHasValue(j, "Mode")) {
        String mode = jsonGetValue(j, "Mode", String());
        if (m_mode != mode) {
            logln("error: mode mismatch, not setting state: cannot load  mode " << mode << " into " << m_mode
                                                                                << " plugin");
            return false;
        }
    }

    if (j.find("servers") != j.end()) {
        m_servers.clear();
        for (auto& srv : j["servers"]) {
            m_servers.add(srv.get<std::string>());
        }
    }

    String activeServerStr = jsonGetValue(j, "activeServerStr", String());
    int activeServer = jsonGetValue(j, "activeServer", -1);

    if (jsonHasValue(j, "ActiveChannels")) {
        m_activeChannels = jsonGetValue(j, "ActiveChannels", (uint64)3);
        m_channelMapper.createMapping(m_activeChannels);
    }

    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        m_loadedPlugins.clear();
        m_activePlugin = -1;
        if (j.find("loadedPlugins") != j.end()) {
            for (auto& plug : j["loadedPlugins"]) {
                if (version < 1) {
                    StringArray dummy;
                    Array<Client::Parameter> dummy2;
                    m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                               plug[2].get<std::string>(), dummy, dummy2, false, false});
                } else if (version == 1) {
                    StringArray dummy;
                    Array<Client::Parameter> dummy2;
                    m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                               plug[2].get<std::string>(), dummy, dummy2, plug[3].get<bool>(), false});
                } else {
                    StringArray presets;
                    for (auto& p : plug[3]) {
                        presets.add(p.get<std::string>());
                    }
                    Array<e47::Client::Parameter> params;
                    for (auto& p : plug[4]) {
                        params.add(e47::Client::Parameter::fromJson(p));
                    }
                    m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                               plug[2].get<std::string>(), presets, params, plug[5].get<bool>(),
                                               false});
                }
            }
        }
    }

    if (activeServerStr.isNotEmpty()) {
        m_client->setServer(activeServerStr);
        m_client->reconnect();
    } else if (activeServer > -1 && activeServer < m_servers.size()) {
        m_client->setServer(m_servers[activeServer]);
        m_client->reconnect();
    }

    return true;
}

void PluginProcessor::sync() {
    traceScope();
    traceln("sync mode is " << m_syncRemote);
    if ((m_syncRemote == SYNC_ALWAYS) || (m_syncRemote == SYNC_WITH_EDITOR && nullptr != getActiveEditor())) {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        for (int i = 0; i < (int)m_loadedPlugins.size(); i++) {
            auto& plug = m_loadedPlugins[(size_t)i];
            if (plug.ok && m_client->isReadyLockFree()) {
                auto settings = m_client->getPluginSettings(static_cast<int>(i));
                if (settings.getSize() > 0) {
                    plug.settings = settings.toBase64Encoding();
                }
            }
        }
    }
}

std::vector<ServerPlugin> PluginProcessor::getPlugins(const String& type) const {
    traceScope();
    std::vector<ServerPlugin> ret;
    for (auto& plugin : getPlugins()) {
        if (!plugin.getType().compare(type)) {
            ret.push_back(plugin);
        }
    }
    return ret;
}

std::set<String> PluginProcessor::getPluginTypes() const {
    traceScope();
    std::set<String> ret;
    for (auto& plugin : m_client->getPlugins()) {
        ret.insert(plugin.getType());
    }
    return ret;
}

bool PluginProcessor::loadPlugin(const ServerPlugin& plugin, String& err) {
    traceScope();
    StringArray presets;
    Array<e47::Client::Parameter> params;
    bool hasEditor, scDisabled;
    logln("loading " << plugin.getName() << " (" << plugin.getId() << ")...");
    suspendProcessing(true);
    bool success = m_client->addPlugin(plugin.getId(), presets, params, hasEditor, scDisabled, "", err);
    suspendProcessing(false);
    if (success) {
        logln("...ok");
    } else {
        logln("...error: " << err);
    }
    if (success) {
        updateLatency(m_client->getLatencySamples());
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        m_loadedPlugins.push_back({plugin.getId(), plugin.getName(), "", presets, params, false, hasEditor, true});
        updateRecents(plugin);
        if (scDisabled && m_showSidechainDisabledInfo) {
            struct cb : ModalComponentManager::Callback {
                PluginProcessor* p;
                cb(PluginProcessor* p_) : p(p_) {}
                void modalStateFinished(int returnValue) override {
                    if (returnValue == 0) {
                        p->m_showSidechainDisabledInfo = false;
                        p->saveConfig();
                    }
                }
            };
            AlertWindow::showOkCancelBox(AlertWindow::InfoIcon, "Sidechain Disabled",
                                         "The server had to disable the sidechain input of the chain to make >" +
                                             plugin.getName() +
                                             "< load.\n\nPress CANCEL to permanently hide this message.",
                                         "OK", "Cancel", nullptr, new cb(this));
        }
    }
    m_client->setLoadedPluginsString(getLoadedPluginsString());
    return success;
}

void PluginProcessor::unloadPlugin(int idx) {
    traceScope();

    for (auto& p : getLoadedPlugin(idx).params) {
        if (p.automationSlot > -1) {
            disableParamAutomation(idx, p.idx);
        }
    }

    if (getLoadedPlugin(idx).ok) {
        suspendProcessing(true);
        m_client->delPlugin(idx);
        suspendProcessing(false);
        updateLatency(m_client->getLatencySamples());
    }

    if (idx == m_activePlugin) {
        m_activePlugin = -1;
    } else if (idx < m_activePlugin) {
        m_activePlugin--;
    }

    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        int i = 0;
        for (auto it = m_loadedPlugins.begin(); it < m_loadedPlugins.end(); it++) {
            if (i++ == idx) {
                m_loadedPlugins.erase(it);
                break;
            }
        }
    }

    m_client->setLoadedPluginsString(getLoadedPluginsString());
}

String PluginProcessor::getLoadedPluginsString() const {
    traceScope();
    String ret;
    bool first = true;
    std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
    for (auto& p : m_loadedPlugins) {
        if (first) {
            first = false;
        } else {
            ret << " > ";
        }
        ret << p.name;
    }
    return ret;
}

void PluginProcessor::editPlugin(int idx, int x, int y) {
    traceScope();
    logln("edit plugin " << idx << " x=" << x << " y=" << y);
    if (!m_genericEditor && getLoadedPlugin(idx).ok) {
        m_client->editPlugin(idx, x, y);
    }
    m_activePlugin = idx;
}

void PluginProcessor::hidePlugin(bool updateServer) {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    logln("hiding plugin: active plugin " << m_activePlugin << ", "
                                          << (updateServer ? "updating server" : "not updating server"));
    if (updateServer) {
        m_client->hidePlugin();
    }
    m_lastActivePlugin = m_activePlugin;
    m_activePlugin = -1;
}

bool PluginProcessor::isBypassed(int idx) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
    if (idx > -1 && idx < (int)m_loadedPlugins.size()) {
        return m_loadedPlugins[(size_t)idx].bypassed;
    }
    return false;
}

void PluginProcessor::bypassPlugin(int idx) {
    traceScope();
    bool updateServer = false;
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        if (idx > -1 && idx < (int)m_loadedPlugins.size()) {
            logln("bypassing plugin " << idx);
            m_loadedPlugins[(size_t)idx].bypassed = true;
            updateServer = true;
        } else {
            logln("failed to bypass plugin " << idx << ": out of range");
        }
    }
    if (updateServer) {
        m_client->bypassPlugin(idx);
    }
}

void PluginProcessor::unbypassPlugin(int idx) {
    traceScope();
    bool updateServer = false;
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        if (idx > -1 && idx < (int)m_loadedPlugins.size()) {
            logln("unbypassing plugin " << idx);
            m_loadedPlugins[(size_t)idx].bypassed = false;
            updateServer = true;
        } else {
            logln("failed to unbypass plugin " << idx << ": out of range");
        }
    }
    if (updateServer) {
        m_client->unbypassPlugin(idx);
    }
}

void PluginProcessor::exchangePlugins(int idxA, int idxB) {
    traceScope();
    bool idxOk = false;
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        idxOk = idxA > -1 && idxA < (int)m_loadedPlugins.size() && idxB > -1 && idxB < (int)m_loadedPlugins.size();
    }
    if (idxOk) {
        logln("exchanging plugins " << idxA << " and " << idxB);
        suspendProcessing(true);
        m_client->exchangePlugins(idxA, idxB);
        suspendProcessing(false);
        {
            std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
            std::swap(m_loadedPlugins[(size_t)idxA], m_loadedPlugins[(size_t)idxB]);
        }
        if (idxA == m_activePlugin) {
            m_activePlugin = idxB;
        } else if (idxB == m_activePlugin) {
            m_activePlugin = idxA;
        }
        for (auto* p : getParameters()) {
            auto* param = dynamic_cast<Parameter*>(p);
            if (param->m_idx == idxA) {
                param->m_idx = idxB;
            } else if (param->m_idx == idxB) {
                param->m_idx = idxA;
            }
        }
    } else {
        logln("failed to exchange plugins " << idxA << " and " << idxB << ": out of range");
    }
}

bool PluginProcessor::enableParamAutomation(int idx, int paramIdx, int slot) {
    traceScope();
    logln("enabling automation for plugin " << idx << ", parameter " << paramIdx << ", slot " << slot);
    bool updateHost = false;
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        auto& param = m_loadedPlugins[(size_t)idx].params.getReference(paramIdx);
        Parameter* pparam = nullptr;
        if (slot == -1) {
            for (slot = 0; slot < m_numberOfAutomationSlots; slot++) {
                pparam = dynamic_cast<Parameter*>(getParameters()[slot]);
                if (pparam->m_idx == -1) {
                    logln("  using slot " << slot);
                    break;
                }
            }
        } else {
            pparam = dynamic_cast<Parameter*>(getParameters()[slot]);
        }
        if (slot < m_numberOfAutomationSlots) {
            pparam->m_idx = idx;
            pparam->m_paramIdx = paramIdx;
            param.automationSlot = slot;
            updateHost = true;
        }
    }
    if (updateHost) {
        updateHostDisplay();
        return true;
    }
    logln("failed to enable automation: no slot available, "
          << "you can increase the value for NumberOfAutomationSlots in the config");
    return false;
}

void PluginProcessor::disableParamAutomation(int idx, int paramIdx) {
    traceScope();
    logln("disabling automation for plugin " << idx << ", parameter " << paramIdx);
    {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        auto& param = m_loadedPlugins[(size_t)idx].params.getReference(paramIdx);
        auto* pparam = dynamic_cast<Parameter*>(getParameters()[param.automationSlot]);
        pparam->reset();
        param.automationSlot = -1;
    }
    updateHostDisplay();
}

void PluginProcessor::getAllParameterValues(int idx) {
    traceScope();
    logln("reading all parameter values for plugin " << idx);
    std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
    auto& params = m_loadedPlugins[(size_t)idx].params;
    for (auto& res : m_client->getAllParameterValues(idx, params.size())) {
        if (res.idx > -1 && res.idx < params.size()) {
            auto& param = params.getReference(res.idx);
            if (param.idx == res.idx) {
                param.currentValue = (float)res.value;
            } else {
                logln("error: index mismatch in getAllParameterValues");
            }
        }
    }
}

void PluginProcessor::updateParameterValue(int idx, int paramIdx, float val, bool updateServer) {
    traceScope();
    runOnMsgThreadAsync([this, idx, paramIdx, val, updateServer] {
        traceScope();

        int slot = -1;

        {
            std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
            if (idx < 0 || idx >= (int)m_loadedPlugins.size()) {
                logln("idx out of range");
                return;
            }
            if (paramIdx < 0 || paramIdx >= m_loadedPlugins[(size_t)idx].params.size()) {
                logln("paramIdx out of range");
                return;
            }
            auto& param = m_loadedPlugins[(size_t)idx].params.getReference(paramIdx);

            param.currentValue = val;
            slot = param.automationSlot;
        }

        logln("parameter update (slot=" << slot << ", index=" << idx << ", param index=" << paramIdx
                                        << ") new value is " << val << " [" << (updateServer && slot < 0 ? "" : "NOT ")
                                        << "updating server]");

        if (slot > -1) {
            auto* pparam = dynamic_cast<Parameter*>(getParameters()[slot]);
            if (nullptr != pparam) {
                // this will trigger the server update as well, need to call this on the message thread or automation
                // recording does not work for VST3
                pparam->setValueNotifyingHost(val);
                return;
            }
        } else {
            logln("parameter update ignored: unassigned parameter");
        }

        if (updateServer) {
            m_client->setParameterValue(idx, paramIdx, val);
        }
    });
}

void PluginProcessor::updateParameterGestureTracking(int idx, int paramIdx, bool starting) {
    traceScope();
    runOnMsgThreadAsync([this, idx, paramIdx, starting] {
        traceScope();

        int slot = -1;

        {
            std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
            if (idx < 0 || idx >= (int)m_loadedPlugins.size()) {
                logln("idx out of range");
                return;
            }
            if (paramIdx < 0 || paramIdx >= m_loadedPlugins[(size_t)idx].params.size()) {
                logln("paramIdx out of range");
                return;
            }
            auto& param = m_loadedPlugins[(size_t)idx].params.getReference(paramIdx);
            slot = param.automationSlot;
        }

        if (slot > -1) {
            auto* pparam = dynamic_cast<Parameter*>(getParameters()[slot]);
            if (nullptr != pparam) {
                logln("parameter (slot=" << pparam->m_slotId << ", index=" << pparam->m_idx << ", param index="
                                         << pparam->m_paramIdx << ") " << (starting ? "begin" : "end") << " gesture");
                // need to call this on the message thread or automation recording does not work for VST3
                if (starting) {
                    pparam->beginChangeGesture();
                } else {
                    pparam->endChangeGesture();
                }
            }
        }
    });
}

void PluginProcessor::parameterValueChanged(int parameterIndex, float newValue) {
    traceScope();
    // update generic editor
    if (auto* e = dynamic_cast<PluginEditor*>(getActiveEditor())) {
        auto* pparam = dynamic_cast<Parameter*>(getParameters()[parameterIndex]);
        if (nullptr != pparam && m_activePlugin == pparam->m_idx) {
            auto& param = getLoadedPlugin(pparam->m_idx).params.getReference(pparam->m_paramIdx);
            param.currentValue = newValue;
            e->updateParamValue(pparam->m_paramIdx);
        }
    }
}

void PluginProcessor::delServer(const String& s) {
    traceScope();
    if (m_servers.contains(s)) {
        logln("deleting server " << s);
        m_servers.removeString(s);
    } else {
        logln("can't delete server " << s << ": not found");
    }
}

void PluginProcessor::increaseSCArea() {
    traceScope();
    logln("increasing screen capturing area by +" << Defaults::SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(Defaults::SCAREA_STEPS);
}

void PluginProcessor::decreaseSCArea() {
    traceScope();
    logln("decreasing screen capturing area by -" << Defaults::SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(-Defaults::SCAREA_STEPS);
}

void PluginProcessor::toggleFullscreenSCArea() {
    traceScope();
    logln("toggle fullscreen for screen capturing area");
    m_client->updateScreenCaptureArea(Defaults::SCAREA_FULLSCREEN);
}

void PluginProcessor::storeSettingsA() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsA = settings.toBase64Encoding();
    }
}

void PluginProcessor::storeSettingsB() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsB = settings.toBase64Encoding();
    }
}

void PluginProcessor::restoreSettingsA() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsA);
}

void PluginProcessor::restoreSettingsB() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsB);
}

void PluginProcessor::resetSettingsAB() {
    traceScope();
    m_settingsA = "";
    m_settingsB = "";
}

Array<ServerPlugin> PluginProcessor::getRecents() {
    if (!m_disableRecents) {
        if (m_tray != nullptr && m_tray->connected) {
            return m_tray->getRecents();
        } else {
            return m_client->getRecents();
        }
    }
    return {};
}

void PluginProcessor::updateRecents(const ServerPlugin& plugin) {
    if (!m_disableRecents && m_tray != nullptr && m_tray->connected) {
        m_tray->sendMessage(
            PluginTrayMessage(PluginTrayMessage::UPDATE_RECENTS, {{"plugin", plugin.toString().toStdString()}}));
    }
}

void PluginProcessor::setActiveServer(const ServerInfo& s) {
    traceScope();
    m_client->setServer(s);
}

String PluginProcessor::getActiveServerName() const {
    traceScope();
    auto srvInfo = m_client->getServer();
    String ret = ServiceReceiver::hostToName(srvInfo.getHost());
    if (srvInfo.getID() > 0) {
        ret << ":" << srvInfo.getID();
    }
    return ret;
}

Array<ServerInfo> PluginProcessor::getServersMDNS() {
    traceScope();
    return ServiceReceiver::getServers();
}

void PluginProcessor::setCPULoad(float load) {
    traceScope();
    runOnMsgThreadAsync([this, load] {
        traceScope();
        auto* editor = getActiveEditor();
        if (editor != nullptr) {
            dynamic_cast<PluginEditor*>(editor)->setCPULoad(load);
        }
    });
}

float PluginProcessor::Parameter::getValue() const {
    traceScope();
    if (m_idx > -1 && m_paramIdx > -1) {
        return m_proc.getClient().getParameterValue(m_idx, m_paramIdx);
    }
    return 0;
}

void PluginProcessor::Parameter::setValue(float newValue) {
    traceScope();
    if (m_idx > -1 && m_idx < m_proc.getNumOfLoadedPlugins() && m_paramIdx > -1) {
        runOnMsgThreadAsync([this, newValue] {
            traceScope();
            m_proc.getClient().setParameterValue(m_idx, m_paramIdx, newValue);
        });
    }
}

String PluginProcessor::Parameter::getName(int maximumStringLength) const {
    traceScope();
    String name;
    name << m_slotId << ":" << getPlugin().name << ":" << getParam().name;
    if (name.length() <= maximumStringLength) {
        return name;
    } else {
        return name.dropLastCharacters(name.length() - maximumStringLength);
    }
}

void PluginProcessor::setDisableTray(bool b) {
    m_disableTray = b;
    if (m_disableTray) {
        m_tray.reset();
    } else if (nullptr == m_tray) {
        m_tray = std::make_unique<TrayConnection>(this);
        if (m_prepared) {
            m_tray->startThread();
        }
    }
}

void PluginProcessor::TrayConnection::messageReceived(const MemoryBlock& message) {
    PluginTrayMessage msg;
    msg.deserialize(message);
    if (msg.type == PluginTrayMessage::CHANGE_SERVER) {
        m_processor->getClient().setServer(ServerInfo(msg.data["serverInfo"].get<std::string>()));
    } else if (msg.type == PluginTrayMessage::GET_RECENTS) {
        logln("updating recents from tray");
        Array<ServerPlugin> recents;
        for (auto& plugin : msg.data["recents"]) {
            recents.add(ServerPlugin::fromString(plugin.get<std::string>()));
        }
        std::lock_guard<std::mutex> lock(m_recentsMtx);
        m_recents.swapWith(recents);
    }
}

void PluginProcessor::TrayConnection::sendStatus() {
    auto& client = m_processor->getClient();
    auto track = m_processor->getTrackProperties();
    String statId = "audio.";
    statId << m_processor->getId();
    auto ts = Metrics::getStatistic<TimeStatistic>(statId);

    json j;
    j["ok"] = client.isReadyLockFree();
    j["name"] = track.name.toStdString();
    j["channelsIn"] = m_processor->getMainBusNumInputChannels();
    j["channelsOut"] = m_processor->getTotalNumOutputChannels();
    j["channelsSC"] = m_processor->getBusCount(true) > 0 ? m_processor->getChannelCountOfBus(true, 1) : 0;
#ifdef JucePlugin_IsSynth
    j["instrument"] = true;
#else
    j["instrument"] = false;
#endif
    j["colour"] = track.colour.getARGB();
    j["loadedPlugins"] = client.getLoadedPluginsString().toStdString();
    j["perf95th"] = ts->get1minHistogram().nintyFifth;
    j["blocks"] = client.NUM_OF_BUFFERS.load();
    j["serverNameId"] = m_processor->getActiveServerName().toStdString();
    j["serverHost"] = client.getServer().getHost().toStdString();

    sendMessage(PluginTrayMessage(PluginTrayMessage::STATUS, j));
}

void PluginProcessor::TrayConnection::sendStop() { sendMessage(PluginTrayMessage(PluginTrayMessage::STOP, {})); }

void PluginProcessor::TrayConnection::showMonitor() {
    sendMessage(PluginTrayMessage(PluginTrayMessage::SHOW_MONITOR, {}));
}

void PluginProcessor::TrayConnection::sendMessage(const PluginTrayMessage& msg) {
    MemoryBlock block;
    msg.serialize(block);
    std::lock_guard<std::mutex> lock(m_sendMtx);
    InterprocessConnection::sendMessage(block);
}

void PluginProcessor::TrayConnection::run() {
    while (!threadShouldExit()) {
        if (!connected) {
            bool success;
            if (Defaults::unixDomainSocketsSupported()) {
                success = connectToSocket(Defaults::getSocketPath(Defaults::PLUGIN_TRAY_SOCK), 500);
            } else {
                success = connectToSocket("localhost", Defaults::PLUGIN_TRAY_PORT, 500);
            }
            if (!success) {
                String path = File::getSpecialLocation(File::globalApplicationsDirectory).getFullPathName();
#ifdef JUCE_MAC
#ifdef DEBUG
                path << "/Debug";
#endif
                path << "/AudioGridderPluginTray.app/Contents/MacOS/AudioGridderPluginTray";
#elif JUCE_WINDOWS
                path << "/AudioGridderPluginTray/AudioGridderPluginTray.exe";
#elif JUCE_LINUX
                path << "/local/bin/AudioGridderPluginTray";
#endif
                if (File(path).existsAsFile()) {
                    logln("tray connection failed, trying to run tray app: " << path);
                    ChildProcess proc;
                    if (!proc.start(path, 0)) {
                        logln("failed to start tray app");
                    }
                } else {
                    logln("no tray app available");
                    static std::once_flag once;
                    std::call_once(once, [] {
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon, "Error",
                            "AudioGridder tray application not found! Please uninstall the "
                            "AudioGridder plugin and reinstall it!",
                            "OK");
                    });
                    return;
                }
                sleepExitAware(2500);
            }
        } else {
            sendStatus();
        }
        sleepExitAware(750);
    }
    sendStop();
    disconnect();
}

}  // namespace e47

AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    e47::WrapperTypeReaderAudioProcessor wr;
    return new e47::PluginProcessor(wr.wrapperType);
}
