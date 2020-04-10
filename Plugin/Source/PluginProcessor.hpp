/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "Client.hpp"

#include <set>

class AudioGridderAudioProcessor : public AudioProcessor {
  public:
    AudioGridderAudioProcessor();
    ~AudioGridderAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(AudioBuffer<float>&, MidiBuffer&) override;
    void processBlock(AudioBuffer<double>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;
    StringArray getAlternateDisplayNames() const override { return {"AuGrid", "AuGr", "AG"}; }

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    bool supportsDoublePrecisionProcessing() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& newName) override;

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    e47::Client& getClient() { return m_client; }
    std::vector<ServerPlugin> getPlugins(const String& type) const;
    const std::vector<ServerPlugin>& getPlugins() const { return m_client.getPlugins(); }
    std::set<String> getPluginTypes() const;

    struct LoadedPlugin {
        String id;
        String name;
        String settings;
        StringArray presets;
        bool bypassed;
        bool ok;
    };

    auto& getLoadedPlugins() const { return m_loadedPlugins; }
    const LoadedPlugin& getLoadedPlugin(int idx) const { return m_loadedPlugins[idx]; }
    bool loadPlugin(const String& id, const String& name);
    void unloadPlugin(int idx);
    void editPlugin(int idx);
    void hidePlugin(bool updateServer = true);
    int getActivePlugin() const { return m_activePlugin; }
    bool isBypassed(int idx);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);

    auto& getServers() const { return m_servers; }
    void addServer(const String& s) { m_servers.push_back(s); }
    void delServer(int idx);
    int getActiveServer() const { return m_activeServer; }
    void setActiveServer(int i);

  private:
    e47::Client m_client;
    std::vector<LoadedPlugin> m_loadedPlugins;
    int m_activePlugin = -1;
    std::vector<String> m_servers;
    int m_activeServer = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGridderAudioProcessor)
};
