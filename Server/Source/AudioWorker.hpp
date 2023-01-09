/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef AudioWorker_hpp
#define AudioWorker_hpp

#include <JuceHeader.h>
#include <thread>
#include <unordered_map>

#include "ProcessorChain.hpp"
#include "Message.hpp"
#include "Utils.hpp"
#include "ChannelMapper.hpp"

namespace e47 {

struct audio_chunk_hdr_t {
    int channels;
    int samples;
    bool isDouble;
};

class ProcessorChain;

class AudioWorker : public Thread, public LogTagDelegate {
  public:
    AudioWorker(LogTag* tag);
    virtual ~AudioWorker() override;

    void init(std::unique_ptr<StreamingSocket> s, HandshakeRequest cfg);

    void run() override;
    void shutdown();
    void clear();

    bool isOk() {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (nullptr == m_socket) {
            m_error = "socket is nullptr";
            m_wasOk = false;
        } else if (!m_socket->isConnected()) {
            m_error = "socket is not connected";
            m_wasOk = false;
        } else {
            m_wasOk = true;
        }
        return m_wasOk;
    }

    bool isOkNoLock() const { return m_wasOk; }

    int getChannelsIn() const { return m_channelsIn; }
    int getChannelsOut() const { return m_channelsOut; }
    int getChannelsSC() const { return m_channelsSC; }

    bool addPlugin(const String& id, const String& settings, const String& layout, uint64 monoChannels, String& err);
    void delPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::shared_ptr<Processor> getProcessor(int idx) const { return m_chain->getProcessor(idx); }
    int getSize() const { return static_cast<int>(m_chain->getSize()); }
    int getLatencySamples() const { return m_chain->getLatencySamples(); }
    void update() { m_chain->update(); }
    bool isSidechainDisabled() const { return m_chain->isSidechainDisabled(); }

    float getParameterValue(int idx, int channel, int paramIdx) {
        return m_chain->getParameterValue(idx, channel, paramIdx);
    }

    struct ComparablePluginDescription : PluginDescription {
        ComparablePluginDescription(const PluginDescription& other) : PluginDescription(other) {}
        bool operator==(const ComparablePluginDescription& other) const { return isDuplicateOf(other); }
    };

    using RecentsListType = Array<ComparablePluginDescription>;
    String getRecentsList(String host) const;
    void addToRecentsList(const String& id, const String& host);

  private:
    std::mutex m_mtx;
    std::atomic_bool m_wasOk{true};
    std::unique_ptr<StreamingSocket> m_socket;
    String m_error;
    int m_channelsIn;
    int m_channelsOut;
    int m_channelsSC;
    ChannelSet m_activeChannels;
    ChannelMapper m_channelMapper;
    double m_sampleRate;
    int m_samplesPerBlock;
    bool m_doublePrecission;
    std::shared_ptr<ProcessorChain> m_chain;
    static std::unordered_map<String, RecentsListType> m_recents;
    static std::mutex m_recentsMtx;

    AudioBuffer<float> m_procBufferF;
    AudioBuffer<double> m_procBufferD;

    bool waitForData();

    template <typename T>
    AudioBuffer<T>* getProcBuffer() {
        return &m_procBufferF;
    }

    template <typename T>
    void processBlock(AudioBuffer<T>& buffer, MidiBuffer& midi);

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* AudioWorker_hpp */
