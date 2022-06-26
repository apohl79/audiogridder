/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _PROCESSORCLIENT_HPP_
#define _PROCESSORCLIENT_HPP_

#include <JuceHeader.h>

#include "Utils.hpp"
#include "Message.hpp"
#include "ParameterValue.hpp"
#include "ChannelMapper.hpp"
#include "ChannelSet.hpp"

namespace e47 {

class ProcessorClient : public Thread, public LogTag {
  public:
    ProcessorClient(const String& id, HandshakeRequest cfg)
        : Thread("ProcessorClient"),
          LogTag("processorclient"),
          m_port(getWorkerPort()),
          m_id(id),
          m_cfg(cfg),
          m_activeChannels(cfg.activeChannels, cfg.channelsIn > 0),
          m_channelMapper(this) {
        m_activeChannels.setNumChannels(cfg.channelsIn + cfg.channelsSC, cfg.channelsOut);
        m_channelMapper.createPluginMapping(m_activeChannels);
    }
    ~ProcessorClient() override;

    bool init();
    void shutdown();
    bool isOk();
    const String& getError() const { return m_error; }

    void run() override;

    std::function<void(int channel, int paramIdx, float val)> onParamValueChange;
    std::function<void(int channel, int paramIdx, bool gestureIsStarting)> onParamGestureChange;
    std::function<void(Message<Key>&)> onKeysFromSandbox;
    std::function<void(bool ok, const String& err)> onStatusChange;

    bool load(const String& settings, const String& layout, uint64 monoChannels, String& err);
    void unload();

    bool isLoaded() const { return m_loaded; }

    const String getName();
    bool hasEditor();
    void showEditor(int channel, int x, int y);
    void hideEditor();
    bool supportsDoublePrecisionProcessing();
    bool isSuspended();
    double getTailLengthSeconds();
    void getStateInformation(String&);
    void setStateInformation(const String&);
    void setPlayHead(AudioPlayHead*);
    const json& getParameters();
    int getNumPrograms();
    const String getProgramName(int);
    void setCurrentProgram(int);
    void suspendProcessing(bool);
    void suspendProcessingRemoteOnly(bool);
    int getTotalNumOutputChannels();
    int getLatencySamples();
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages);
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages);
    juce::Rectangle<int> getScreenBounds();
    void setParameterValue(int channel, int paramIdx, float value);
    float getParameterValue(int channel, int paramIdx);
    std::vector<Srv::ParameterValue> getAllParameterValues();
    void setMonoChannels(uint64 channels);
    int getChannelInstances() const { return m_lastChannelInstances; }

  private:
    int m_port;
    String m_id;
    HandshakeRequest m_cfg;
    ChildProcess m_process;
    std::unique_ptr<StreamingSocket> m_sockCmdIn, m_sockCmdOut, m_sockAudio;
    std::mutex m_cmdMtx, m_audioMtx;
    std::shared_ptr<Meter> m_bytesOutMeter, m_bytesInMeter;
    String m_error;

    bool m_loaded = false;
    String m_name;
    StringArray m_presets;
    json m_parameters;
    int m_latency = 0;
    bool m_hasEditor = false;
    bool m_scDisabled = false;
    bool m_supportsDoublePrecision = true;
    double m_tailSeconds = 0.0;
    int m_numOutputChannels = 0;
    AudioPlayHead* m_playhead = nullptr;
    std::atomic_bool m_suspended{false};
    String m_lastSettings;
    String m_lastLayout;
    uint64 m_lastMonoChannels;
    juce::Rectangle<int> m_lastScreenBounds;
    int m_lastChannelInstances = 0;

    ChannelSet m_activeChannels;
    ChannelMapper m_channelMapper;

    static std::unordered_set<int> m_workerPorts;
    static std::mutex m_workerPortsMtx;

    static int getWorkerPort();
    static void removeWorkerPort(int port);

    bool startSandbox();
    bool connectSandbox();

    void setAndLogError(const String& e) {
        m_error = e;
        logln(e);
    }

    template <typename T>
    void processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages);

    void handleMessage(std::shared_ptr<Message<Key>> msg);
    void handleMessage(std::shared_ptr<Message<ParameterValue>> msg);
    void handleMessage(std::shared_ptr<Message<ParameterGesture>> msg);
    void handleMessage(std::shared_ptr<Message<ScreenBounds>> msg);
};

}  // namespace e47

#endif  // _PROCESSORCLIENT_HPP_
