/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginProcessor.hpp"

#include <signal.h>

#include "PluginEditor.hpp"
#include "json.hpp"

using json = nlohmann::json;

AudioGridderAudioProcessor::AudioGridderAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", AudioChannelSet::stereo(), true)
#endif
                         ),
#endif
      m_client(this) {
    signal(SIGPIPE, SIG_IGN);

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
                e47::Client::NUM_OF_BUFFERS = j["NumberOfBuffers"].get<int>();
            }
        }
    } catch (json::parse_error& e) {
        std::cerr << "parsing config failed: " << e.what() << std::endl;
    }

    // load plugins on reconnect
    m_client.setOnConnectCallback([this] {
        int idx = 0;
        for (auto& p : m_loadedPlugins) {
            p.ok = m_client.addPlugin(p.id, p.presets, p.settings);
            std::cout << "loading " << p.name << " (" << p.id << ")... " << (p.ok ? "ok" : "failed") << std::endl;
            if (p.ok) {
                std::cout << "updating latency samples to " << m_client.getLatencySamples() << std::endl;
                setLatencySamples(m_client.getLatencySamples());
                if (p.bypassed) {
                    m_client.bypassPlugin(idx);
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
    if (m_activeServer > -1 && m_activeServer < m_servers.size()) {
        m_client.setServer(m_servers[m_activeServer]);
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

void AudioGridderAudioProcessor::setCurrentProgram(int index) {}

const String AudioGridderAudioProcessor::getProgramName(int index) { return {}; }

void AudioGridderAudioProcessor::changeProgramName(int index, const String& newName) {}

void AudioGridderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    m_client.init(getTotalNumInputChannels(), sampleRate, samplesPerBlock, isUsingDoublePrecision());
}

void AudioGridderAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioGridderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo()) {
        return false;
    }

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
#endif

    return true;
#endif
}
#endif

void AudioGridderAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto* playHead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo posInfo;
    playHead->getCurrentPosition(posInfo);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    if (!m_client.isReadyLockFree()) {
        for (auto i = 0; i < totalNumInputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
    } else {
        if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
            m_client.send(buffer, midiMessages, posInfo);
            m_client.read(buffer, midiMessages);
            if (m_client.getLatencySamples() != getLatencySamples()) {
                std::cout << "updating latency samples to " << m_client.getLatencySamples() << std::endl;
                setLatencySamples(m_client.getLatencySamples());
            }
        }
    }
}

void AudioGridderAudioProcessor::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto* playHead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo posInfo;
    playHead->getCurrentPosition(posInfo);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    if (!m_client.isReadyLockFree()) {
        for (auto i = 0; i < totalNumInputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
    } else {
        if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0) {
            m_client.send(buffer, midiMessages, posInfo);
            m_client.read(buffer, midiMessages);
            if (m_client.getLatencySamples() != getLatencySamples()) {
                std::cout << "updating latency samples to " << m_client.getLatencySamples() << std::endl;
                setLatencySamples(m_client.getLatencySamples());
            }
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
    for (int i = 0; i < m_loadedPlugins.size(); i++) {
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
        jplugs.push_back(
            {plug.id.toStdString(), plug.name.toStdString(), plug.settings.toStdString(), jpresets, plug.bypassed});
    }
    j["loadedPlugins"] = jplugs;

    auto dump = j.dump();
    destData.append(dump.data(), dump.length());

    json jcfg;
    jcfg["Servers"] = jservers;
    jcfg["Last"] = m_activeServer;
    jcfg["NumberOfBuffers"] = e47::Client::NUM_OF_BUFFERS;
    File cfg(PLUGIN_CONFIG_FILE);
    cfg.deleteFile();
    FileOutputStream fos(cfg);
    fos.writeText(jcfg.dump(4), false, false, "\n");
}

void AudioGridderAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::string dump(static_cast<const char*>(data), sizeInBytes);
    json j = json::parse(dump);
    int version = 0;
    if (j.find("version") != j.end()) {
        version = j["version"].get<int>();
    }
    m_servers.clear();
    for (auto& srv : j["servers"]) {
        m_servers.push_back(srv.get<std::string>());
    }
    m_activeServer = j["activeServer"].get<int>();
    for (auto& plug : j["loadedPlugins"]) {
        if (version < 1) {
            StringArray dummy;
            m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                       plug[2].get<std::string>(), dummy, false, false});
        } else if (version == 1) {
            StringArray dummy;
            m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                       plug[2].get<std::string>(), dummy, plug[3].get<bool>(), false});
        } else {
            StringArray presets;
            for (auto& p : plug[3]) {
                presets.add(p.get<std::string>());
            }
            m_loadedPlugins.push_back({plug[0].get<std::string>(), plug[1].get<std::string>(),
                                       plug[2].get<std::string>(), presets, plug[4].get<bool>(), false});
        }
    }
    m_client.setServer(m_servers[m_activeServer]);
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
    suspendProcessing(true);
    bool success = m_client.addPlugin(id, presets);
    suspendProcessing(false);
    std::cout << "loading " << name << " (" << id << ")... " << (success ? "xx" : "error") << std::endl;
    if (success) {
        std::cout << "updating latency samples to " << m_client.getLatencySamples() << std::endl;
        setLatencySamples(m_client.getLatencySamples());
        m_loadedPlugins.push_back({id, name, "", presets, false, true});
    }
    return success;
}

void AudioGridderAudioProcessor::unloadPlugin(int idx) {
    suspendProcessing(true);
    m_client.delPlugin(idx);
    suspendProcessing(false);
    std::cout << "updating latency samples to " << m_client.getLatencySamples() << std::endl;
    setLatencySamples(m_client.getLatencySamples());
    if (idx == m_activePlugin) {
        hidePlugin();
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
    if (idx > -1 && idx < m_loadedPlugins.size()) {
        return m_loadedPlugins[idx].bypassed;
    }
    return false;
}

void AudioGridderAudioProcessor::bypassPlugin(int idx) {
    if (idx > -1 && idx < m_loadedPlugins.size()) {
        m_client.bypassPlugin(idx);
        m_loadedPlugins[idx].bypassed = true;
    }
}

void AudioGridderAudioProcessor::unbypassPlugin(int idx) {
    if (idx > -1 && idx < m_loadedPlugins.size()) {
        m_client.unbypassPlugin(idx);
        m_loadedPlugins[idx].bypassed = false;
    }
}

void AudioGridderAudioProcessor::exchangePlugins(int idxA, int idxB) {
    if (idxA > -1 && idxA < m_loadedPlugins.size() && idxB > -1 && idxB < m_loadedPlugins.size()) {
        suspendProcessing(true);
        m_client.exchangePlugins(idxA, idxB);
        suspendProcessing(false);
        std::swap(m_loadedPlugins[idxA], m_loadedPlugins[idxB]);
        if (idxA == m_activePlugin) {
            m_activePlugin = idxB;
        } else if (idxB == m_activePlugin) {
            m_activePlugin = idxA;
        }
    }
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

void AudioGridderAudioProcessor::setActiveServer(int i) {
    if (i > -1 && i < m_servers.size()) {
        m_activeServer = i;
        m_client.setServer(m_servers[i]);
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AudioGridderAudioProcessor(); }
