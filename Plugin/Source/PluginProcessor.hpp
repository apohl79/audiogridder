/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include <JuceHeader.h>
#include <set>

#include "Client.hpp"
#include "Utils.hpp"
#include "json.hpp"
#include "ChannelSet.hpp"
#include "ChannelMapper.hpp"
#include "AudioRingBuffer.hpp"

using json = nlohmann::json;

namespace e47 {

class WrapperTypeReaderAudioProcessor : public AudioProcessor {
  public:
    const String getName() const override { return {}; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(AudioBuffer<float>&, MidiBuffer&) override {}
    void processBlock(AudioBuffer<double>&, MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

class PluginProcessor : public AudioProcessor, public AudioProcessorParameter::Listener, public LogTagDelegate {
  public:
    PluginProcessor(WrapperType wt);
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    // Array<std::pair<short, short>> getAUChannelInfo() const override;

    bool canAddBus(bool /*isInput*/) const override { return true; }
    bool canRemoveBus(bool /*isInput*/) const override { return true; }
    void numChannelsChanged() override;

    template <typename T>
    void processBlockInternal(AudioBuffer<T>& buf, MidiBuffer& midi);

    void processBlock(AudioBuffer<float>& buf, MidiBuffer& midi) override { processBlockInternal(buf, midi); }
    void processBlock(AudioBuffer<double>& buf, MidiBuffer& midi) override { processBlockInternal(buf, midi); }

    void processBlockBypassed(AudioBuffer<float>& buf, MidiBuffer& midi) override;
    void processBlockBypassed(AudioBuffer<double>& buf, MidiBuffer& midi) override;

    void updateLatency(int samples);

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;
    StringArray getAlternateDisplayNames() const override { return {"AGrid", "AG"}; }

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

    json getState(bool withServers);
    bool setState(const json& j);

    const String& getMode() const { return m_mode; }

    void updateTrackProperties(const TrackProperties& properties) override {
        traceScope();
        std::lock_guard<std::mutex> lock(m_trackPropertiesMtx);
        m_trackProperties = properties;
    }

    TrackProperties getTrackProperties() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_trackPropertiesMtx);
        return m_trackProperties;
    }

    void loadConfig();
    void loadConfig(const json& j, bool isUpdate = false);
    void saveConfig(int numOfBuffers = -1);

    Client& getClient() { return *m_client; }
    std::vector<ServerPlugin> getPlugins(const String& type) const;
    const std::vector<ServerPlugin>& getPlugins() const { return m_client->getPlugins(); }
    std::set<String> getPluginTypes() const;

    struct LoadedPlugin {
        String id;
        String name;
        String settings;
        StringArray presets;
        Array<Client::Parameter> params;
        bool bypassed = false;
        bool hasEditor = true;
        bool ok = false;
    };

    // Called by the client object to trigger resyncing the remote plugin settings
    void sync();

    enum SyncRemoteMode { SYNC_ALWAYS, SYNC_WITH_EDITOR, SYNC_DISABLED };
    SyncRemoteMode getSyncRemoteMode() const { return m_syncRemote; }
    void setSyncRemoteMode(SyncRemoteMode m) { m_syncRemote = m; }

    ChannelSet& getActiveChannels() { return m_activeChannels; }

    void updateChannelMapping() {
        m_channelMapper.createPluginMapping(m_activeChannels);
        m_channelMapper.print();
    }

    int getNumOfLoadedPlugins() {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        return (int)m_loadedPlugins.size();
    }
    LoadedPlugin& getLoadedPlugin(int idx) {
        std::lock_guard<std::mutex> lock(m_loadedPluginsSyncMtx);
        return idx > -1 && idx < (int)m_loadedPlugins.size() ? m_loadedPlugins[(size_t)idx] : m_unusedDummyPlugin;
    }

    bool loadPlugin(const ServerPlugin& plugin, String& err);
    void unloadPlugin(int idx);
    String getLoadedPluginsString() const;
    void editPlugin(int idx, int x, int y);
    void hidePlugin(bool updateServer = true);
    int getActivePlugin() const { return m_activePlugin; }
    int getLastActivePlugin() const { return m_lastActivePlugin; }
    bool isEditAlways() const { return m_editAlways; }
    void setEditAlways(bool b) { m_editAlways = b; }
    bool isBypassed(int idx);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    bool enableParamAutomation(int idx, int paramIdx, int slot = -1);
    void disableParamAutomation(int idx, int paramIdx);
    void getAllParameterValues(int idx);
    void updateParameterValue(int idx, int paramIdx, float val, bool updateServer = true);
    void updateParameterGestureTracking(int idx, int paramIdx, bool starting);
    void increaseSCArea();
    void decreaseSCArea();
    void toggleFullscreenSCArea();

    void storeSettingsA();
    void storeSettingsB();
    void restoreSettingsA();
    void restoreSettingsB();
    void resetSettingsAB();

    bool getMenuShowCategory() const { return m_menuShowCategory; }
    void setMenuShowCategory(bool b) { m_menuShowCategory = b; }
    bool getMenuShowCompany() const { return m_menuShowCompany; }
    void setMenuShowCompany(bool b) { m_menuShowCompany = b; }
    bool getGenericEditor() const { return m_genericEditor; }
    void setGenericEditor(bool b) { m_genericEditor = b; }
    bool getConfirmDelete() const { return m_confirmDelete; }
    void setConfirmDelete(bool b) { m_confirmDelete = b; }
    bool getShowSidechainDisabledInfo() const { return m_showSidechainDisabledInfo; }
    void setShowSidechainDisabledInfo(bool b) { m_showSidechainDisabledInfo = b; }
    bool getNoSrvPluginListFilter() const { return m_noSrvPluginListFilter; }
    void setNoSrvPluginListFilter(bool b) { m_noSrvPluginListFilter = b; }
    float getScaleFactor() const { return m_scale; }
    void setScaleFactor(float f) { m_scale = f; }
    bool getCrashReporting() const { return m_crashReporting; }
    void setCrashReporting(bool b) { m_crashReporting = b; }
    bool supportsCrashReporting() const { return wrapperType != wrapperType_AAX; }
    Array<ServerPlugin> getRecents();
    void updateRecents(const ServerPlugin& plugin);

    auto& getServers() const { return m_servers; }
    void addServer(const String& s) { m_servers.add(s); }
    void delServer(const String& s);
    String getActiveServerHost() const { return m_client->getServer().getHostAndID(); }
    String getActiveServerName() const;
    void setActiveServer(const ServerInfo& s);
    Array<ServerInfo> getServersMDNS();
    void setCPULoad(float load);

    int getLatencyMillis() const {
        return (int)lround(m_client->NUM_OF_BUFFERS * getBlockSize() * 1000 / getSampleRate());
    }

    void showMonitor() { m_tray->showMonitor(); }

    String getPresetDir() const { return m_presetsDir; }
    void setPresetDir(const String& d) { m_presetsDir = d; }
    bool hasDefaultPreset() const { return m_defaultPreset.isNotEmpty() && File(m_defaultPreset).existsAsFile(); }
    void storePreset(const File& file);
    bool loadPreset(const File& file);
    void storePresetDefault();
    void resetPresetDefault();

    bool getTransferWhenPlayingOnly() const { return m_transferWhenPlayingOnly; }
    void setTransferWhenPlayingOnly(bool b) { m_transferWhenPlayingOnly = b; }

    bool getDisableTray() const { return m_disableTray; }
    void setDisableTray(bool b);
    bool getDisableRecents() const { return m_disableRecents; }
    void setDisableRecents(bool b) { m_disableRecents = b; }
    bool getKeepEditorOpen() const { return m_keepEditorOpen; }
    void setKeepEditorOpen(bool b) { m_keepEditorOpen = b; }
    bool getBypassWhenNotConnected() const { return m_bypassWhenNotConnected; }
    void setBypassWhenNotConnected(bool b) { m_bypassWhenNotConnected = b; }
    bool getBufferSizeByPlugin() const { return m_bufferSizeByPlugin; }
    void setBufferSizeByPlugin(bool b) { m_bufferSizeByPlugin = b; }

    int getNumBuffers() const { return m_client->NUM_OF_BUFFERS; }
    void setNumBuffers(int n);

    // AudioProcessorParameter::Listener
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}

    // It looks like most hosts do not support dynamic parameter creation or changes to existing parameters. Logic
    // at least allows for the name to be updated. So we create slots at the start.
    class Parameter : public AudioProcessorParameter, public LogTagDelegate {
      public:
        Parameter(PluginProcessor& proc, int slot) : m_proc(proc), m_slotId(slot) {
            setLogTagSource(m_proc.getLogTagSource());
            initAsyncFunctors();
        }
        ~Parameter() override {
            traceScope();
            stopAsyncFunctors();
        }
        float getValue() const override;
        void setValue(float newValue) override;
        float getValueForText(const String& /* text */) const override { return 0; }
        float getDefaultValue() const override { return getParam().defaultValue; }
        String getName(int maximumStringLength) const override;
        String getLabel() const override { return getParam().label; }
        int getNumSteps() const override { return getParam().numSteps; }
        bool isDiscrete() const override { return getParam().isDiscrete; }
        bool isBoolean() const override { return getParam().isBoolean; }
        bool isOrientationInverted() const override { return getParam().isOrientInv; }
        bool isMetaParameter() const override { return getParam().isMeta; }

      private:
        friend PluginProcessor;
        PluginProcessor& m_proc;
        int m_idx = -1;
        int m_paramIdx = 0;
        int m_slotId = 0;

        const LoadedPlugin& getPlugin() const { return m_proc.getLoadedPlugin(m_idx); }
        const Client::Parameter& getParam() const { return getPlugin().params.getReference(m_paramIdx); }

        void reset() {
            m_idx = -1;
            m_paramIdx = 0;
        }

        ENABLE_ASYNC_FUNCTORS();
    };

    class TrayConnection : public InterprocessConnection, public Thread, public LogTagDelegate {
      public:
        std::atomic_bool connected{false};

        TrayConnection(PluginProcessor* p)
            : InterprocessConnection(false), Thread("TrayConnection"), LogTagDelegate(p), m_processor(p) {}

        ~TrayConnection() override { stopThread(-1); }

        void run() override;

        void connectionMade() override { connected = true; }
        void connectionLost() override { connected = false; }
        void messageReceived(const MemoryBlock& message) override;
        void sendStatus();
        void sendStop();
        void showMonitor();
        void sendMessage(const PluginTrayMessage& msg);

        Array<ServerPlugin> getRecents() {
            std::lock_guard<std::mutex> lock(m_recentsMtx);
            return m_recents;
        }

      private:
        PluginProcessor* m_processor;
        Array<ServerPlugin> m_recents;
        std::mutex m_recentsMtx;
        std::mutex m_sendMtx;
    };

  private:
    Uuid m_instId;
    String m_mode;
    std::unique_ptr<Client> m_client;
    std::unique_ptr<TrayConnection> m_tray;
    std::atomic_bool m_prepared{false};
    std::vector<LoadedPlugin> m_loadedPlugins;
    mutable std::mutex m_loadedPluginsSyncMtx;
    int m_activePlugin = -1;
    int m_lastActivePlugin = -1;
    bool m_editAlways = true;
    StringArray m_servers;
    String m_activeServerFromCfg;
    int m_activeServerLegacyFromCfg;
    String m_presetsDir;
    String m_defaultPreset;

    int m_numberOfAutomationSlots = 16;
    LoadedPlugin m_unusedDummyPlugin;
    Client::Parameter m_unusedParam;

    AudioRingBuffer<float> m_bypassBufferF;
    AudioRingBuffer<double> m_bypassBufferD;
    std::mutex m_bypassBufferMtx;

    String m_settingsA, m_settingsB;

    bool m_menuShowCategory = true;
    bool m_menuShowCompany = true;
    bool m_genericEditor = false;
    bool m_confirmDelete = true;
    bool m_showSidechainDisabledInfo = true;
    bool m_noSrvPluginListFilter = false;
    float m_scale = 1.0;
    bool m_crashReporting = true;

    bool m_transferWhenPlayingOnly = false;
    bool m_disableTray = false;
    bool m_disableRecents = false;
    bool m_keepEditorOpen = false;
    std::atomic_bool m_bypassWhenNotConnected{false};
    bool m_bufferSizeByPlugin = false;

    TrackProperties m_trackProperties;
    std::mutex m_trackPropertiesMtx;

    SyncRemoteMode m_syncRemote = SYNC_WITH_EDITOR;

    ChannelSet m_activeChannels;
    ChannelMapper m_channelMapper;

    static BusesProperties createBusesProperties(WrapperType wt) {
        int chIn = Defaults::PLUGIN_CHANNELS_IN;
        int chOut = Defaults::PLUGIN_CHANNELS_OUT;
        int chSC = Defaults::PLUGIN_CHANNELS_SC;
        bool useMonoMainBus = false;
        bool useMonoOutputBuses = true;
        bool useMultipleOutputBusses = false;

#if JucePlugin_IsSynth
        chIn = 0;
        useMultipleOutputBusses = true;
        if (wt == WrapperType::wrapperType_AudioUnit) {
            useMonoMainBus = true;
        } else if (wt == WrapperType::wrapperType_AAX) {
            useMonoOutputBuses = false;
        }
#else
        ignoreUnused(wt);
#endif

        auto bp = BusesProperties();

        if (chIn == 1) {
            bp = bp.withInput("Input", AudioChannelSet::mono(), true);
        } else if (chIn == 2) {
            bp = bp.withInput("Input", AudioChannelSet::stereo(), true);
        } else if (chIn > 0) {
            bp = bp.withInput("Input", AudioChannelSet::discreteChannels(chIn), true);
        }

        if (chOut == 1) {
            bp = bp.withOutput("Output", AudioChannelSet::mono(), true);
        } else if (chOut == 2) {
            bp = bp.withOutput("Output", AudioChannelSet::stereo(), true);
        } else if (chOut > 0) {
            if (useMultipleOutputBusses) {
                if (useMonoMainBus) {
                    bp = bp.withOutput("Main", AudioChannelSet::mono(), true);
                } else {
                    bp = bp.withOutput("Main", AudioChannelSet::stereo(), true);
                }
                if (useMonoOutputBuses) {
                    for (int i = useMonoMainBus ? 1 : 2; i < chOut; i++) {
                        bp = bp.withOutput("Ch " + String(i + 1), AudioChannelSet::mono(), true);
                    }
                } else {
                    for (int i = useMonoMainBus ? 1 : 2; i < chOut; i += 2) {
                        bp = bp.withOutput("Ch " + String(i / 2 + 1), AudioChannelSet::mono(), true);
                    }
                }
            } else {
                bp = bp.withOutput("Output", AudioChannelSet::discreteChannels(chOut));
            }
        }

        if (chSC == 1) {
            bp = bp.withInput("Sidechain", AudioChannelSet::mono(), true);
        } else if (chSC == 2) {
            bp = bp.withInput("Sidechain", AudioChannelSet::stereo(), true);
        } else if (chSC > 0) {
            bp = bp.withInput("Sidechain", AudioChannelSet::discreteChannels(chSC), true);
        }

        return bp;
    }

    void printBusesLayout(const AudioProcessor::BusesLayout& l) const {
        logln("input buses: " << l.inputBuses.size());
        for (int i = 0; i < l.inputBuses.size(); i++) {
            logln("  [" << i << "] " << l.inputBuses[i].size() << " channel(s)");
            for (auto ct : l.inputBuses[i].getChannelTypes()) {
                logln("    <- " << AudioChannelSet::getAbbreviatedChannelTypeName(ct));
            }
        }
        logln("output buses: " << l.outputBuses.size());
        for (int i = 0; i < l.outputBuses.size(); i++) {
            logln("  [" << i << "] " << l.outputBuses[i].size() << " channel(s)");
            for (auto ct : l.outputBuses[i].getChannelTypes()) {
                logln("    -> " << AudioChannelSet::getAbbreviatedChannelTypeName(ct));
            }
        }
    }

    ENABLE_ASYNC_FUNCTORS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

}  // namespace e47
