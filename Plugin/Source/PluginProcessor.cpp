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

#ifdef JUCE_WINDOWS
#include "MiniDump.hpp"
#endif

#ifdef JUCE_MAC
#include <signal.h>
#endif

using namespace e47;

AudioGridderAudioProcessor::AudioGridderAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsSynth
                         .withInput("Input", AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", AudioChannelSet::stereo(), true)) {

#ifdef JUCE_MAC
    signal(SIGPIPE, SIG_IGN);
#endif

    String mode;
#if JucePlugin_IsSynth
    mode = "Instrument";
#else
    mode = "FX";
#endif

    String appName = "AudioGridder" + mode;
    String logName = "AudioGridderPlugin_";

#ifdef JUCE_WINDOWS
    auto dumpPath = FileLogger::getSystemLogFileFolder().getFullPathName();
    MiniDump::initialize(dumpPath.toWideCharPointer(), appName.toWideCharPointer(), logName.toWideCharPointer(),
                         AUDIOGRIDDER_VERSIONW, true);
#endif

    AGLogger::initialize(appName, logName);
    TimeStatistics::initialize();
    ServiceReceiver::initialize(m_instId.hash(), [this] {
        MessageManager::callAsync([this] {
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(m_client->isReadyLockFree());
            }
        });
    });

    m_client = std::make_unique<e47::Client>(this);
    setLogTagSource(m_client.get());
    logln(mode << " plugin loaded (version: " << AUDIOGRIDDER_VERSION << ")");

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
    m_client->setOnConnectCallback([this] {
        logln("connected");
        int idx = 0;
        for (auto& p : m_loadedPlugins) {
            logln("loading " << p.name << " (" << p.id << ") [on connect]... ");
            p.ok = m_client->addPlugin(p.id, p.presets, p.params, p.settings);
            logln("..." << (p.ok ? "ok" : "failed"));
            if (p.ok) {
                updateLatency(m_client->getLatencySamples());
                if (p.bypassed) {
                    m_client->bypassPlugin(idx);
                }
                for (auto& param : p.params) {
                    if (param.automationSlot > -1) {
                        if (param.automationSlot < m_numberOfAutomationSlots) {
                            enableParamAutomation(idx, param.idx, param.automationSlot);
                        } else {
                            param.automationSlot = -1;
                        }
                    }
                }
            }
            idx++;
        }
        MessageManager::callAsync([this] {
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(true);
            }
        });
    });
    // handle connection close
    m_client->setOnCloseCallback([this] {
        logln("disconnected");
        MessageManager::callAsync([this] {
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(false);
            }
        });
    });
    if (m_activeServerFromCfg.isNotEmpty()) {
        m_client->setServer(m_activeServerFromCfg);
    } else if (m_activeServerLegacyFromCfg > -1 && m_activeServerLegacyFromCfg < m_servers.size()) {
        m_client->setServer(m_servers[m_activeServerLegacyFromCfg]);
    }

    m_client->startThread();
}

AudioGridderAudioProcessor::~AudioGridderAudioProcessor() {
    m_client->signalThreadShouldExit();
    m_client->close();
    waitForThreadAndLog(m_client.get(), m_client.get());
    logln("plugin unloaded");
    TimeStatistics::cleanup();
    ServiceReceiver::cleanup(m_instId.hash());
    AGLogger::cleanup();
}

void AudioGridderAudioProcessor::loadConfig() {
    File cfg(PLUGIN_CONFIG_FILE);
    try {
        if (cfg.exists()) {
            FileInputStream fis(cfg);
            json j = json::parse(fis.readEntireStreamAsString().toStdString());
            loadConfig(j);
        }
    } catch (json::parse_error& e) {
        logln("parsing config failed: " << e.what());
    }
}

void AudioGridderAudioProcessor::loadConfig(const json& j, bool isUpdate) {
    if (j.find("Servers") != j.end() && !isUpdate) {
        for (auto& srv : j["Servers"]) {
            m_servers.add(srv.get<std::string>());
        }
    }
    if (j.find("LastServer") != j.end() && !isUpdate) {
        m_activeServerFromCfg = j["LastServer"].get<std::string>();
    }
    if (j.find("Last") != j.end() && !isUpdate) {
        m_activeServerLegacyFromCfg = j["Last"].get<int>();
    }
    if (j.find("NumberOfBuffers") != j.end() && !isUpdate) {
        m_client->NUM_OF_BUFFERS = j["NumberOfBuffers"].get<int>();
    }
    if (j.find("LoadPluginTimeout") != j.end() && !isUpdate) {
        m_client->LOAD_PLUGIN_TIMEOUT = j["LoadPluginTimeout"].get<int>();
    }
    if (j.find("NumberOfAutomationSlots") != j.end()) {
        m_numberOfAutomationSlots = j["NumberOfAutomationSlots"].get<int>();
    }
    if (j.find("MenuShowCategory") != j.end()) {
        m_menuShowCategory = j["MenuShowCategory"].get<bool>();
    }
    if (j.find("MenuShowCompany") != j.end()) {
        m_menuShowCompany = j["MenuShowCompany"].get<bool>();
    }
    if (j.find("GenericEditor") != j.end()) {
        m_genericEditor = j["GenericEditor"].get<bool>();
    }
}

void AudioGridderAudioProcessor::saveConfig(int numOfBuffers) {
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
    jcfg["MenuShowCategory"] = m_menuShowCategory;
    jcfg["MenuShowCompany"] = m_menuShowCompany;
    jcfg["GenericEditor"] = m_genericEditor;
    File cfg(PLUGIN_CONFIG_FILE);
    cfg.deleteFile();
    FileOutputStream fos(cfg);
    fos.writeText(jcfg.dump(4), false, false, "\n");
}

const String AudioGridderAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioGridderAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioGridderAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

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
    logln("prepareToPlay: sampleRate = " << sampleRate << ", samplesPerBlock=" << samplesPerBlock);
    m_client->init(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, samplesPerBlock,
                   isUsingDoublePrecision());
}

void AudioGridderAudioProcessor::releaseResources() { logln("releaseResources"); }

bool AudioGridderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) {
        return false;
    }
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
#endif
    return true;
}

template <typename T>
void AudioGridderAudioProcessor::processBlockReal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto* phead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo posInfo;
    phead->getCurrentPosition(posInfo);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    if ((buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) || midiMessages.getNumEvents() > 0) {
        if (m_client->audioLock()) {
            m_client->send(buffer, midiMessages, posInfo);
            m_client->read(buffer, midiMessages);
            m_client->audioUnlock();
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
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    for (auto c = 0; c < totalNumOutputChannels; ++c) {
        std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
        if (c < totalNumOutputChannels) {
            auto& buf = m_bypassBufferF.getReference(c);
            for (auto s = 0; s < buffer.getNumSamples(); ++s) {
                buf.add(buffer.getSample(c, s));
                buffer.setSample(c, s, buf.getFirst());
                buf.remove(0);
            }
        } else {
            logln("processBlockBypassed: error: m_bypassBufferF has less channels than buffer");
        }
    }
}

void AudioGridderAudioProcessor::processBlockBypassed(AudioBuffer<double>& buffer, MidiBuffer& /* midiMessages */) {
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    for (auto c = 0; c < totalNumOutputChannels; ++c) {
        std::lock_guard<std::mutex> lock(m_bypassBufferMtx);
        if (c < totalNumOutputChannels) {
            auto& buf = m_bypassBufferD.getReference(c);
            for (auto s = 0; s < buffer.getNumSamples(); ++s) {
                buf.add(buffer.getSample(c, s));
                buffer.setSample(c, s, buf.getFirst());
                buf.remove(0);
            }
        } else {
            logln("processBlockBypassed: error: m_bypassBufferD has less channels than buffer");
        }
    }
}

void AudioGridderAudioProcessor::updateLatency(int samples) {
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
    logln("getStateInformation");

    json j;
    auto jservers = json::array();
    for (auto& srv : m_servers) {
        jservers.push_back(srv.toStdString());
    }
    j["version"] = 2;
    j["servers"] = jservers;
    j["activeServerStr"] = m_client->getServerHostAndID().toStdString();
    auto jplugs = json::array();
    for (size_t i = 0; i < m_loadedPlugins.size(); i++) {
        auto& plug = m_loadedPlugins[i];
        if (m_client->isReadyLockFree()) {
            auto settings = m_client->getPluginSettings(static_cast<int>(i));
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
    j["loadedPlugins"] = jplugs;

    auto dump = j.dump();
    destData.append(dump.data(), dump.length());

    saveConfig();
}

void AudioGridderAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    logln("setStateInformation");

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

std::vector<ServerPlugin> AudioGridderAudioProcessor::getPlugins(const String& type) const {
    std::vector<ServerPlugin> ret;
    for (auto& plugin : getPlugins()) {
        if (!plugin.getType().compare(type)) {
            ret.push_back(plugin);
        }
    }
    return ret;
}

std::set<String> AudioGridderAudioProcessor::getPluginTypes() const {
    std::set<String> ret;
    for (auto& plugin : m_client->getPlugins()) {
        ret.insert(plugin.getType());
    }
    return ret;
}

bool AudioGridderAudioProcessor::loadPlugin(const String& id, const String& name) {
    StringArray presets;
    Array<e47::Client::Parameter> params;
    logln("loading " << name << " (" << id << ")... ");
    suspendProcessing(true);
    bool success = m_client->addPlugin(id, presets, params);
    suspendProcessing(false);
    logln("..." << (success ? "ok" : "error"));
    if (success) {
        updateLatency(m_client->getLatencySamples());
        m_loadedPlugins.push_back({id, name, "", presets, params, false, true});
    }
    return success;
}

void AudioGridderAudioProcessor::unloadPlugin(int idx) {
    suspendProcessing(true);
    m_client->delPlugin(idx);
    suspendProcessing(false);
    updateLatency(m_client->getLatencySamples());
    if (idx == m_activePlugin) {
        hidePlugin();
    } else if (idx < m_activePlugin) {
        m_activePlugin--;
    }
    int i = 0;
    for (auto it = m_loadedPlugins.begin(); it < m_loadedPlugins.end(); it++) {
        if (i++ == idx) {
            m_loadedPlugins.erase(it);
            return;
        }
    }
}

void AudioGridderAudioProcessor::editPlugin(int idx) {
    logln("edit plugin " << idx);
    if (!m_genericEditor) {
        m_client->editPlugin(idx);
    }
    m_activePlugin = idx;
}

void AudioGridderAudioProcessor::hidePlugin(bool updateServer) {
    logln("hiding plugin: active plugin " << m_activePlugin << ", "
                                          << (updateServer ? "updating server" : "not updating server"));
    if (updateServer) {
        m_client->hidePlugin();
    }
    m_activePlugin = -1;
}

bool AudioGridderAudioProcessor::isBypassed(int idx) {
    if (idx > -1 && as<size_t>(idx) < m_loadedPlugins.size()) {
        return m_loadedPlugins[as<size_t>(idx)].bypassed;
    }
    return false;
}

void AudioGridderAudioProcessor::bypassPlugin(int idx) {
    if (idx > -1 && as<size_t>(idx) < m_loadedPlugins.size()) {
        logln("bypassing plugin " << idx);
        m_client->bypassPlugin(idx);
        m_loadedPlugins[as<size_t>(idx)].bypassed = true;
    } else {
        logln("failed to bypass plugin " << idx << ": out of range");
    }
}

void AudioGridderAudioProcessor::unbypassPlugin(int idx) {
    if (idx > -1 && as<size_t>(idx) < m_loadedPlugins.size()) {
        logln("unbypassing plugin " << idx);
        m_client->unbypassPlugin(idx);
        m_loadedPlugins[as<size_t>(idx)].bypassed = false;
    } else {
        logln("failed to unbypass plugin " << idx << ": out of range");
    }
}

void AudioGridderAudioProcessor::exchangePlugins(int idxA, int idxB) {
    if (idxA > -1 && as<size_t>(idxA) < m_loadedPlugins.size() && idxB > -1 &&
        as<size_t>(idxB) < m_loadedPlugins.size()) {
        logln("exchanging plugins " << idxA << " and " << idxB);
        suspendProcessing(true);
        m_client->exchangePlugins(idxA, idxB);
        suspendProcessing(false);
        std::swap(m_loadedPlugins[as<size_t>(idxA)], m_loadedPlugins[as<size_t>(idxB)]);
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
    logln("enabling automation for plugin " << idx << ", parameter " << paramIdx << ", slot " << slot);
    auto& param = m_loadedPlugins[as<size_t>(idx)].params.getReference(paramIdx);
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
        updateHostDisplay();
        return true;
    }
    logln("failed to enable automation: no slot available, "
          << "you can increase the value for NumberOfAutomationSlots in the config");
    return false;
}

void AudioGridderAudioProcessor::disableParamAutomation(int idx, int paramIdx) {
    logln("disabling automation for plugin " << idx << ", parameter " << paramIdx);
    auto& param = m_loadedPlugins[as<size_t>(idx)].params.getReference(paramIdx);
    auto* pparam = dynamic_cast<Parameter*>(getParameters()[param.automationSlot]);
    pparam->reset();
    updateHostDisplay();
    param.automationSlot = -1;
}

void AudioGridderAudioProcessor::getAllParameterValues(int idx) {
    logln("reading all parameter values for plugin " << idx);
    auto& params = m_loadedPlugins[as<size_t>(idx)].params;
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
    if (m_servers.contains(s)) {
        logln("deleting server " << s);
        m_servers.removeString(s);
    } else {
        logln("can't delete server " << s << ": not found");
    }
}

void AudioGridderAudioProcessor::increaseSCArea() {
    logln("increasing screen capturing area by +" << SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(SCAREA_STEPS);
}

void AudioGridderAudioProcessor::decreaseSCArea() {
    logln("decreasing screen capturing area by -" << SCAREA_STEPS << "px");
    m_client->updateScreenCaptureArea(-SCAREA_STEPS);
}

void AudioGridderAudioProcessor::storeSettingsA() {
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsA = settings.toBase64Encoding();
    }
}

void AudioGridderAudioProcessor::storeSettingsB() {
    if (m_activePlugin < 0) {
        return;
    }
    auto settings = m_client->getPluginSettings(m_activePlugin);
    if (settings.getSize() > 0) {
        m_settingsB = settings.toBase64Encoding();
    }
}

void AudioGridderAudioProcessor::restoreSettingsA() {
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsA);
}

void AudioGridderAudioProcessor::restoreSettingsB() {
    if (m_activePlugin < 0) {
        return;
    }
    m_client->setPluginSettings(m_activePlugin, m_settingsB);
}

void AudioGridderAudioProcessor::resetSettingsAB() {
    m_settingsA = "";
    m_settingsB = "";
}

void AudioGridderAudioProcessor::setActiveServer(const ServerString& s) { m_client->setServer(s); }

String AudioGridderAudioProcessor::getActiveServerName() const {
    String ret = ServiceReceiver::hostToName(m_client->getServerHost());
    int id = m_client->getServerID();
    if (id > 0) {
        ret << ":" << id;
    }
    return ret;
}

Array<ServerString> AudioGridderAudioProcessor::getServersMDNS() { return ServiceReceiver::getServers(); }

float AudioGridderAudioProcessor::Parameter::getValue() const {
    if (m_idx > -1 && m_paramIdx > -1) {
        return m_processor.getClient().getParameterValue(m_idx, m_paramIdx);
    }
    return 0;
}

void AudioGridderAudioProcessor::Parameter::setValue(float newValue) {
    if (m_idx > -1 && m_paramIdx > -1) {
        MessageManager::callAsync(
            [this, newValue] { m_processor.getClient().setParameterValue(m_idx, m_paramIdx, newValue); });
    }
}

String AudioGridderAudioProcessor::Parameter::getName(int maximumStringLength) const {
    String name;
    name << m_slotId << ":" << getPlugin().name << ":" << getParam().name;
    if (name.length() <= maximumStringLength) {
        return name;
    } else {
        return name.dropLastCharacters(name.length() - maximumStringLength);
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AudioGridderAudioProcessor(); }
