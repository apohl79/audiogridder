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
#include "CoreDump.hpp"
#include "AudioStreamer.hpp"
#include "PluginMonitor.hpp"
#include "WindowPositions.hpp"

#if !defined(JUCE_WINDOWS)
#include <signal.h>
#endif

namespace e47 {

AudioGridderAudioProcessor::AudioGridderAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsSynth && !JucePlugin_IsMidiEffect
                         .withInput("Input", AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", AudioChannelSet::stereo(), true)) {
    initAsyncFunctors();

    String mode;
#if JucePlugin_IsSynth
    mode = "Instrument";
#elif JucePlugin_IsMidiEffect
    mode = "Midi";
#else
    mode = "FX";
#endif

    String appName = mode;
    String logName = "AudioGridderPlugin_";

    AGLogger::initialize(appName, logName, Defaults::getConfigFileName(Defaults::ConfigPlugin));
    Tracer::initialize(appName, logName);
    Signals::initialize();
    CoreDump::initialize(appName, logName, true);
    Metrics::initialize();
    WindowPositions::initialize();
    PluginMonitor::initialize();

    m_client = std::make_unique<Client>(this);
    setLogTagSource(m_client.get());
    traceScope();
    logln(mode << " plugin loaded (version: " << AUDIOGRIDDER_VERSION << ", build date: " << AUDIOGRIDDER_BUILD_DATE
               << ")");

    ServiceReceiver::initialize(m_instId.hash(), [this] {
        traceScope();
        runOnMsgThreadAsync([this] {
            traceScope();
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(m_client->isReadyLockFree());
            }
        });
    });

    updateLatency(0);

    loadConfig();

    m_unusedParam.name = "(unassigned)";
    m_unusedDummyPlugin.name = "(unused)";
    m_unusedDummyPlugin.bypassed = false;
    m_unusedDummyPlugin.ok = true;
    m_unusedDummyPlugin.params.add(m_unusedParam);

    for (int i = 0; i < m_numberOfAutomationSlots; i++) {
        addParameter(new Parameter(*this, i));
    }

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
                p.ok = m_client->addPlugin(p.id, p.presets, p.params, p.settings, err);
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
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(true);
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
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(false);
            }
        });
    }));

    if (m_activeServerFromCfg.isNotEmpty()) {
        m_client->setServer(m_activeServerFromCfg);
    } else if (m_activeServerLegacyFromCfg > -1 && m_activeServerLegacyFromCfg < m_servers.size()) {
        m_client->setServer(m_servers[m_activeServerLegacyFromCfg]);
    }

    m_client->startThread();

    PluginMonitor::add(this);
}

AudioGridderAudioProcessor::~AudioGridderAudioProcessor() {
    traceScope();
    stopAsyncFunctors();
    logln("plugin shutdown: terminating client");
    PluginMonitor::remove(this);
    m_client->signalThreadShouldExit();
    m_client->close();
    waitForThreadAndLog(m_client.get(), m_client.get());
    logln("plugin shutdown: cleaning up");
    PluginMonitor::cleanup();
    WindowPositions::cleanup();
    Metrics::cleanup();
    ServiceReceiver::cleanup(m_instId.hash());
    logln("plugin unloaded");
    Tracer::cleanup();
    AGLogger::cleanup();
}

void AudioGridderAudioProcessor::loadConfig() {
    traceScope();
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
    if (cfg.size() > 0) {
        loadConfig(cfg);
    }
}

void AudioGridderAudioProcessor::loadConfig(const json& j, bool isUpdate) {
    traceScope();

    Tracer::setEnabled(jsonGetValue(j, "Tracer", Tracer::isEnabled()));
    AGLogger::setEnabled(jsonGetValue(j, "Logger", AGLogger::isEnabled()));
    PluginMonitor::setAutoShow(jsonGetValue(j, "PluginMonAutoShow", PluginMonitor::getAutoShow()));
    PluginMonitor::setShowChannelColor(jsonGetValue(j, "PluginMonChanColor", PluginMonitor::getShowChannelColor()));
    PluginMonitor::setShowChannelName(jsonGetValue(j, "PluginMonChanName", PluginMonitor::getShowChannelName()));

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
    m_syncRemote = jsonGetValue(j, "SyncRemoteMode", m_syncRemote);
    auto noSrvPluginListFilter = jsonGetValue(j, "NoSrvPluginListFilter", m_noSrvPluginListFilter);
    if (noSrvPluginListFilter != m_noSrvPluginListFilter) {
        m_noSrvPluginListFilter = noSrvPluginListFilter;
        m_client->reconnect();
    }
}

void AudioGridderAudioProcessor::saveConfig(int numOfBuffers) {
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
    jcfg["LastServer"] = m_client->getServerHostAndID().toStdString();
    jcfg["NumberOfBuffers"] = numOfBuffers;
    jcfg["NumberOfAutomationSlots"] = m_numberOfAutomationSlots;
    jcfg["LoadPluginTimeoutMS"] = m_client->LOAD_PLUGIN_TIMEOUT.load();
    jcfg["MenuShowCategory"] = m_menuShowCategory;
    jcfg["MenuShowCompany"] = m_menuShowCompany;
    jcfg["GenericEditor"] = m_genericEditor;
    jcfg["ConfirmDelete"] = m_confirmDelete;
    jcfg["Tracer"] = Tracer::isEnabled();
    jcfg["Logger"] = AGLogger::isEnabled();
    jcfg["PluginMonAutoShow"] = PluginMonitor::getAutoShow();
    jcfg["PluginMonChanColor"] = PluginMonitor::getShowChannelColor();
    jcfg["PluginMonChanName"] = PluginMonitor::getShowChannelName();
    jcfg["SyncRemoteMode"] = m_syncRemote;
    jcfg["NoSrvPluginListFilter"] = m_noSrvPluginListFilter;
    jcfg["ZoomFactor"] = m_scale;

    configWriteFile(Defaults::getConfigFileName(Defaults::ConfigPlugin), jcfg);
}

const String AudioGridderAudioProcessor::getName() const {
    auto pluginStr = getLoadedPluginsString();
    if (pluginStr.isNotEmpty()) {
        return "AG: " + pluginStr;
    } else {
        return JucePlugin_Name;
    }
}

bool AudioGridderAudioProcessor::acceptsMidi() const { return true; }

bool AudioGridderAudioProcessor::producesMidi() const { return true; }

bool AudioGridderAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioGridderAudioProcessor::getTailLengthSeconds() const { return 0.0; }

bool AudioGridderAudioProcessor::supportsDoublePrecisionProcessing() const { return true; }

int AudioGridderAudioProcessor::getNumPrograms() { return 1; }

int AudioGridderAudioProcessor::getCurrentProgram() { return 0; }

void AudioGridderAudioProcessor::setCurrentProgram(int /* index */) {}

const String AudioGridderAudioProcessor::getProgramName(int /* index */) { return {}; }

void AudioGridderAudioProcessor::changeProgramName(int /* index */, const String& /* newName */) {}

void AudioGridderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    traceScope();
    logln("prepareToPlay: sampleRate = " << sampleRate << ", samplesPerBlock=" << samplesPerBlock);
    m_client->init(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, samplesPerBlock,
                   isUsingDoublePrecision());
    m_prepared = true;
}

void AudioGridderAudioProcessor::releaseResources() {
    m_prepared = false;
    logln("releaseResources");
}

bool AudioGridderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) {
        return false;
    }
#if !JucePlugin_IsSynth && !JucePlugin_IsMidiEffect
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
#endif
    return true;
}

template <typename T>
void AudioGridderAudioProcessor::processBlockReal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();
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

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    if ((buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) || midiMessages.getNumEvents() > 0) {
        auto streamer = m_client->getStreamer<T>();
        if (nullptr != streamer) {
            streamer->send(buffer, midiMessages, posInfo);
            streamer->read(buffer, midiMessages);
            if (m_client->getLatencySamples() != getLatencySamples()) {
                updateLatency(m_client->getLatencySamples());
            }
        } else {
            for (auto i = 0; i < buffer.getNumChannels(); ++i) {
                buffer.clear(i, 0, buffer.getNumSamples());
            }
        }
    }
}

void AudioGridderAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& /* midiMessages */) {
    traceScope();
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

    if (m_bypassBufferF.size() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed");
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    for (auto c = 0; c < totalNumOutputChannels; ++c) {
        std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
        auto& buf = m_bypassBufferF.getReference(c);
        for (auto s = 0; s < buffer.getNumSamples(); ++s) {
            buf.add(buffer.getSample(c, s));
            buffer.setSample(c, s, buf.getFirst());
            buf.remove(0);
        }
    }
}

void AudioGridderAudioProcessor::processBlockBypassed(AudioBuffer<double>& buffer, MidiBuffer& /* midiMessages */) {
    traceScope();
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

    if (m_bypassBufferD.size() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed");
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    for (auto c = 0; c < totalNumOutputChannels; ++c) {
        std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
        auto& buf = m_bypassBufferD.getReference(c);
        for (auto s = 0; s < buffer.getNumSamples(); ++s) {
            buf.add(buffer.getSample(c, s));
            buffer.setSample(c, s, buf.getFirst());
            buf.remove(0);
        }
    }
}

void AudioGridderAudioProcessor::updateLatency(int samples) {
    traceScope();
    if (!m_prepared) {
        return;
    }
    logln("updating latency samples to " << samples);
    setLatencySamples(samples);
    int channels = getTotalNumOutputChannels();
    std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
    while (m_bypassBufferF.size() < channels) {
        Array<float> buf;
        for (int i = 0; i < samples; i++) {
            buf.add(0);
        }
        m_bypassBufferF.add(std::move(buf));
    }
    while (m_bypassBufferD.size() < channels) {
        Array<double> buf;
        for (int i = 0; i < samples; i++) {
            buf.add(0);
        }
        m_bypassBufferD.add(std::move(buf));
    }
    for (int c = 0; c < channels; c++) {
        auto& bufF = m_bypassBufferF.getReference(c);
        while (bufF.size() > samples) {
            bufF.remove(0);
        }
        while (bufF.size() < samples) {
            bufF.add(0);
        }
        auto& bufD = m_bypassBufferD.getReference(c);
        while (bufD.size() > samples) {
            bufD.remove(0);
        }
        while (bufD.size() < samples) {
            bufD.add(0);
        }
    }
}

bool AudioGridderAudioProcessor::hasEditor() const { return true; }

AudioProcessorEditor* AudioGridderAudioProcessor::createEditor() { return new AudioGridderAudioProcessorEditor(*this); }

void AudioGridderAudioProcessor::getStateInformation(MemoryBlock& destData) {
    traceScope();

    json j;
    auto jservers = json::array();
    for (auto& srv : m_servers) {
        jservers.push_back(srv.toStdString());
    }
    j["version"] = 2;
    j["servers"] = jservers;
    j["activeServerStr"] = m_client->getServerHostAndID().toStdString();
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

    auto dump = j.dump();
    destData.append(dump.data(), dump.length());

    saveConfig();
}

void AudioGridderAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    traceScope();

    std::string dump(static_cast<const char*>(data), as<size_t>(sizeInBytes));
    try {
        json j = json::parse(dump);
        int version = 0;
        if (j.find("version") != j.end()) {
            version = j["version"].get<int>();
        }
        m_servers.clear();
        if (j.find("servers") != j.end()) {
            for (auto& srv : j["servers"]) {
                m_servers.add(srv.get<std::string>());
            }
        }
        String activeServerStr;
        int activeServer = -1;
        if (j.find("activeServerStr") != j.end()) {
            activeServerStr = j["activeServerStr"].get<std::string>();
        } else if (j.find("activeServer") != j.end()) {
            activeServer = j["activeServer"].get<int>();
        }
        {
            std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
            m_loadedPlugins.clear();
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
                                                   plug[2].get<std::string>(), dummy, dummy2, plug[3].get<bool>(),
                                                   false});
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
    } catch (json::parse_error& e) {
        logln("parsing state info failed: " << e.what());
    }
}

void AudioGridderAudioProcessor::sync() {
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

std::vector<ServerPlugin> AudioGridderAudioProcessor::getPlugins(const String& type) const {
    traceScope();
    std::vector<ServerPlugin> ret;
    for (auto& plugin : getPlugins()) {
        if (!plugin.getType().compare(type)) {
            ret.push_back(plugin);
        }
    }
    return ret;
}

std::set<String> AudioGridderAudioProcessor::getPluginTypes() const {
    traceScope();
    std::set<String> ret;
    for (auto& plugin : m_client->getPlugins()) {
        ret.insert(plugin.getType());
    }
    return ret;
}

bool AudioGridderAudioProcessor::loadPlugin(const String& id, const String& name, String& err) {
    traceScope();
    StringArray presets;
    Array<e47::Client::Parameter> params;
    logln("loading " << name << " (" << id << ")...");
    suspendProcessing(true);
    bool success = m_client->addPlugin(id, presets, params, "", err);
    suspendProcessing(false);
    if (success) {
        logln("...ok");
    } else {
        logln("...error: " << err);
    }
    if (success) {
        updateLatency(m_client->getLatencySamples());
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        m_loadedPlugins.push_back({id, name, "", presets, params, false, true});
    }
    m_client->setLoadedPluginsString(getLoadedPluginsString());
    return success;
}

void AudioGridderAudioProcessor::unloadPlugin(int idx) {
    traceScope();
    if (getLoadedPlugin(idx).ok) {
        suspendProcessing(true);
        m_client->delPlugin(idx);
        suspendProcessing(false);
        updateLatency(m_client->getLatencySamples());
    }

    if (idx == m_activePlugin) {
        hidePlugin();
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

String AudioGridderAudioProcessor::getLoadedPluginsString() const {
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

void AudioGridderAudioProcessor::editPlugin(int idx) {
    traceScope();
    logln("edit plugin " << idx);
    if (!m_genericEditor && getLoadedPlugin(idx).ok) {
        m_client->editPlugin(idx);
    }
    m_activePlugin = idx;
}

void AudioGridderAudioProcessor::hidePlugin(bool updateServer) {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    logln("hiding plugin: active plugin " << m_activePlugin << ", "
                                          << (updateServer ? "updating server" : "not updating server"));
    if (updateServer) {
        m_client->hidePlugin();
    }
    m_activePlugin = -1;
}

bool AudioGridderAudioProcessor::isBypassed(int idx) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
    if (idx > -1 && idx < (int)m_loadedPlugins.size()) {
        return m_loadedPlugins[(size_t)idx].bypassed;
    }
    return false;
}

void AudioGridderAudioProcessor::bypassPlugin(int idx) {
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

void AudioGridderAudioProcessor::unbypassPlugin(int idx) {
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

void AudioGridderAudioProcessor::exchangePlugins(int idxA, int idxB) {
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

bool AudioGridderAudioProcessor::enableParamAutomation(int idx, int paramIdx, int slot) {
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

void AudioGridderAudioProcessor::disableParamAutomation(int idx, int paramIdx) {
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

void AudioGridderAudioProcessor::getAllParameterValues(int idx) {
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

void AudioGridderAudioProcessor::delServer(const String& s) {
    traceScope();
    if (m_servers.contains(s)) {
        logln("deleting server " << s);
        m_servers.removeString(s);
    } else {
        logln("can't delete server " << s << ": not found");
    }
}

void AudioGridderAudioProcessor::increaseSCArea() {
    traceScope();
    logln("increasing screen capturing area by +" << Defaults::SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(Defaults::SCAREA_STEPS);
}

void AudioGridderAudioProcessor::decreaseSCArea() {
    traceScope();
    logln("decreasing screen capturing area by -" << Defaults::SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(-Defaults::SCAREA_STEPS);
}

void AudioGridderAudioProcessor::toggleFullscreenSCArea() {
    traceScope();
    logln("toggle fullscreen for screen capturing area");
    m_client->updateScreenCaptureArea(Defaults::SCAREA_FULLSCREEN);
}

void AudioGridderAudioProcessor::storeSettingsA() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsA = settings.toBase64Encoding();
    }
}

void AudioGridderAudioProcessor::storeSettingsB() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsB = settings.toBase64Encoding();
    }
}

void AudioGridderAudioProcessor::restoreSettingsA() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsA);
}

void AudioGridderAudioProcessor::restoreSettingsB() {
    traceScope();
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsB);
}

void AudioGridderAudioProcessor::resetSettingsAB() {
    traceScope();
    m_settingsA = "";
    m_settingsB = "";
}

void AudioGridderAudioProcessor::setActiveServer(const ServerInfo& s) {
    traceScope();
    m_client->setServer(s);
}

String AudioGridderAudioProcessor::getActiveServerName() const {
    traceScope();
    String ret = ServiceReceiver::hostToName(m_client->getServerHost());
    int id = m_client->getServerID();
    if (id > 0) {
        ret << ":" << id;
    }
    return ret;
}

Array<ServerInfo> AudioGridderAudioProcessor::getServersMDNS() {
    traceScope();
    return ServiceReceiver::getServers();
}

void AudioGridderAudioProcessor::setCPULoad(float load) {
    traceScope();
    runOnMsgThreadAsync([this, load] {
        traceScope();
        auto* editor = getActiveEditor();
        if (editor != nullptr) {
            dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setCPULoad(load);
        }
    });
}

float AudioGridderAudioProcessor::Parameter::getValue() const {
    traceScope();
    if (m_idx > -1 && m_paramIdx > -1) {
        return m_processor.getClient().getParameterValue(m_idx, m_paramIdx);
    }
    return 0;
}

void AudioGridderAudioProcessor::Parameter::setValue(float newValue) {
    traceScope();
    if (m_idx > -1 && m_paramIdx > -1) {
        runOnMsgThreadAsync([this, newValue] {
            traceScope();
            m_processor.getClient().setParameterValue(m_idx, m_paramIdx, newValue);
        });
    }
}

String AudioGridderAudioProcessor::Parameter::getName(int maximumStringLength) const {
    traceScope();
    String name;
    name << m_slotId << ":" << getPlugin().name << ":" << getParam().name;
    if (name.length() <= maximumStringLength) {
        return name;
    } else {
        return name.dropLastCharacters(name.length() - maximumStringLength);
    }
}

}  // namespace e47

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new e47::AudioGridderAudioProcessor(); }
