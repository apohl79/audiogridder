/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginProcessor.hpp"
#include "PluginEditor.hpp"
#include "json.hpp"

#include <signal.h>

using json = nlohmann::json;

using namespace e47;

AudioGridderAudioProcessor::AudioGridderAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsSynth
                         .withInput("Input", AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
      m_client(this) {

#ifdef JUCE_MAC
    signal(SIGPIPE, SIG_IGN);
#endif

    updateLatency(0);

    File cfg(PLUGIN_CONFIG_FILE);
    try {
        if (cfg.exists()) {
            FileInputStream fis(cfg);
            json j = json::parse(fis.readEntireStreamAsString().toStdString());
            if (j.find("Servers") != j.end()) {
                for (auto& srv : j["Servers"]) {
                    m_servers.push_back(srv.get<std::string>());
                }
            }
            if (j.find("Last") != j.end()) {
                m_activeServer = j["Last"].get<int>();
            }
            if (j.find("NumberOfBuffers") != j.end()) {
                m_client.NUM_OF_BUFFERS = j["NumberOfBuffers"].get<int>();
            }
            if (j.find("NumberOfAutomationSlots") != j.end()) {
                m_numberOfAutomationSlots = j["NumberOfAutomationSlots"].get<int>();
            }
        }
    } catch (json::parse_error& e) {
        logln_clnt(&m_client, "parsing config failed: " << e.what());
    }

    m_unusedParam.name = "(unassigned)";
    m_unusedDummyPlugin.name = "(unused)";
    m_unusedDummyPlugin.bypassed = false;
    m_unusedDummyPlugin.ok = true;
    m_unusedDummyPlugin.params.add(m_unusedParam);

    for (int i = 0; i < m_numberOfAutomationSlots; i++) {
        addParameter(new Parameter(*this, i));
    }

    // load plugins on reconnect
    m_client.setOnConnectCallback([this] {
        int idx = 0;
        for (auto& p : m_loadedPlugins) {
            p.ok = m_client.addPlugin(p.id, p.presets, p.params, p.settings);
            logln_clnt(&m_client, "loading " << p.name << " (" << p.id << ")... " << (p.ok ? "ok" : "failed"));
            if (p.ok) {
                logln_clnt(&m_client, "updating latency samples to " << m_client.getLatencySamples());
                updateLatency(m_client.getLatencySamples());
                if (p.bypassed) {
                    m_client.bypassPlugin(idx);
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
    m_client.setOnCloseCallback([this] {
        MessageManager::callAsync([this] {
            auto* editor = getActiveEditor();
            if (editor != nullptr) {
                dynamic_cast<AudioGridderAudioProcessorEditor*>(editor)->setConnected(false);
            }
        });
    });
    if (m_activeServer > -1 && as<size_t>(m_activeServer) < m_servers.size()) {
        m_client.setServer(m_servers[as<size_t>(m_activeServer)]);
    }
}

AudioGridderAudioProcessor::~AudioGridderAudioProcessor() {
    m_client.signalThreadShouldExit();
    m_client.close();
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

double AudioGridderAudioProcessor::getTailLengthSeconds() const { return 0.0; }

bool AudioGridderAudioProcessor::supportsDoublePrecisionProcessing() const { return true; }

int AudioGridderAudioProcessor::getNumPrograms() { return 1; }

int AudioGridderAudioProcessor::getCurrentProgram() { return 0; }

void AudioGridderAudioProcessor::setCurrentProgram(int /* index */) {}

const String AudioGridderAudioProcessor::getProgramName(int /* index */) { return {}; }

void AudioGridderAudioProcessor::changeProgramName(int /* index */, const String& /* newName */) {}

void AudioGridderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    logln_clnt(&m_client, "prepareToPlay: sampleRate = " << sampleRate << ", samplesPerBlock=" << samplesPerBlock);
    m_client.init(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, samplesPerBlock,
                  isUsingDoublePrecision());
}

void AudioGridderAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

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

    if (!m_client.isReadyLockFree()) {
        for (auto i = 0; i < buffer.getNumChannels(); ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
    } else {
        if ((buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) || midiMessages.getNumEvents() > 0) {
            m_client.send(buffer, midiMessages, posInfo);
            m_client.read(buffer, midiMessages);
            if (m_client.getLatencySamples() != getLatencySamples()) {
                logln_clnt(&m_client, "updating latency samples to " << m_client.getLatencySamples());
                updateLatency(m_client.getLatencySamples());
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
            logln_clnt(&m_client, "processBlockBypassed: error: m_bypassBufferF has less channels than buffer");
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
            logln_clnt(&m_client, "processBlockBypassed: error: m_bypassBufferD has less channels than buffer");
        }
    }
}

void AudioGridderAudioProcessor::updateLatency(int samples) {
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
    json j;
    auto jservers = json::array();
    for (auto& srv : m_servers) {
        jservers.push_back(srv.toStdString());
    }
    j["version"] = 2;
    j["servers"] = jservers;
    j["activeServer"] = m_activeServer;
    auto jplugs = json::array();
    for (size_t i = 0; i < m_loadedPlugins.size(); i++) {
        auto& plug = m_loadedPlugins[i];
        if (m_client.isReadyLockFree()) {
            auto settings = m_client.getPluginSettings(static_cast<int>(i));
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

void AudioGridderAudioProcessor::saveConfig(int numOfBuffers) {
    auto jservers = json::array();
    for (auto& srv : m_servers) {
        jservers.push_back(srv.toStdString());
    }
    if (numOfBuffers < 0) {
        numOfBuffers = m_client.NUM_OF_BUFFERS;
    }
    json jcfg;
    jcfg["_comment_"] = "PLEASE DO NOT CHANGE THIS FILE WHILE YOUR DAW IS RUNNING AND HAS AUDIOGRIDDER PLUGINS LOADED";
    jcfg["Servers"] = jservers;
    jcfg["Last"] = m_activeServer;
    jcfg["NumberOfBuffers"] = numOfBuffers;
    jcfg["NumberOfAutomationSlots"] = m_numberOfAutomationSlots;
    File cfg(PLUGIN_CONFIG_FILE);
    cfg.deleteFile();
    FileOutputStream fos(cfg);
    fos.writeText(jcfg.dump(4), false, false, "\n");
}

void AudioGridderAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
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
                m_servers.push_back(srv.get<std::string>());
            }
        }
        if (j.find("activeServer") != j.end()) {
            m_activeServer = j["activeServer"].get<int>();
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
        if (m_activeServer > -1 && as<size_t>(m_activeServer) < m_servers.size()) {
            m_client.setServer(m_servers[as<size_t>(m_activeServer)]);
            m_client.reconnect();
        }
    } catch (json::parse_error& e) {
        logln_clnt(&m_client, "parsing state info failed: " << e.what());
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
    for (auto& plugin : m_client.getPlugins()) {
        ret.insert(plugin.getType());
    }
    return ret;
}

bool AudioGridderAudioProcessor::loadPlugin(const String& id, const String& name) {
    StringArray presets;
    Array<e47::Client::Parameter> params;
    logln_clnt(&m_client, "loading " << name << " (" << id << ")... ");
    suspendProcessing(true);
    bool success = m_client.addPlugin(id, presets, params);
    suspendProcessing(false);
    logln_clnt(&m_client, "..." << (success ? "ok" : "error"));
    if (success) {
        logln_clnt(&m_client, "updating latency samples to " << m_client.getLatencySamples());
        updateLatency(m_client.getLatencySamples());
        m_loadedPlugins.push_back({id, name, "", presets, params, false, true});
    }
    return success;
}

void AudioGridderAudioProcessor::unloadPlugin(int idx) {
    suspendProcessing(true);
    m_client.delPlugin(idx);
    suspendProcessing(false);
    logln_clnt(&m_client, "updating latency samples to " << m_client.getLatencySamples());
    updateLatency(m_client.getLatencySamples());
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
    m_client.editPlugin(idx);
    m_activePlugin = idx;
}

void AudioGridderAudioProcessor::hidePlugin(bool updateServer) {
    if (updateServer) {
        m_client.hidePlugin();
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
        m_client.bypassPlugin(idx);
        m_loadedPlugins[as<size_t>(idx)].bypassed = true;
    }
}

void AudioGridderAudioProcessor::unbypassPlugin(int idx) {
    if (idx > -1 && as<size_t>(idx) < m_loadedPlugins.size()) {
        m_client.unbypassPlugin(idx);
        m_loadedPlugins[as<size_t>(idx)].bypassed = false;
    }
}

void AudioGridderAudioProcessor::exchangePlugins(int idxA, int idxB) {
    if (idxA > -1 && as<size_t>(idxA) < m_loadedPlugins.size() && idxB > -1 &&
        as<size_t>(idxB) < m_loadedPlugins.size()) {
        suspendProcessing(true);
        m_client.exchangePlugins(idxA, idxB);
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
    }
}

bool AudioGridderAudioProcessor::enableParamAutomation(int idx, int paramIdx, int slot) {
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
    return false;
}

void AudioGridderAudioProcessor::disableParamAutomation(int idx, int paramIdx) {
    auto& param = m_loadedPlugins[as<size_t>(idx)].params.getReference(paramIdx);
    auto* pparam = dynamic_cast<Parameter*>(getParameters()[param.automationSlot]);
    pparam->reset();
    updateHostDisplay();
    param.automationSlot = -1;
}

void AudioGridderAudioProcessor::delServer(int idx) {
    int i = 0;
    for (auto it = m_servers.begin(); it < m_servers.end(); it++) {
        if (i++ == idx) {
            m_servers.erase(it);
            return;
        }
    }
}

void AudioGridderAudioProcessor::setActiveServer(int idx) {
    if (idx > -1 && as<size_t>(idx) < m_servers.size()) {
        m_activeServer = idx;
        m_client.setServer(m_servers[as<size_t>(idx)]);
    }
}

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
