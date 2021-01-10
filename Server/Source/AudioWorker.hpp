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

namespace e47 {

struct audio_chunk_hdr_t {
    int channels;
    int samples;
    bool isDouble;
};

class ProcessorChain;

class AudioWorker : public Thread, public LogTagDelegate {
  public:
    static std::atomic_uint32_t count;
    static std::atomic_uint32_t runCount;

    AudioWorker(LogTag* tag);
    virtual ~AudioWorker() override;

    void init(std::unique_ptr<StreamingSocket> s, int channelsIn, int channelsOut, double rate, int samplesPerBlock,
              bool doublePrecission);

    void run() override;
    void shutdown();
    void clear();

    int getChannelsIn() const { return m_channelsIn; }
    int getChannelsOut() const { return m_channelsOut; }

    bool addPlugin(const String& id, String& err);
    void delPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::shared_ptr<AGProcessor> getProcessor(int idx) const { return m_chain->getProcessor(idx); }
    int getSize() const { return static_cast<int>(m_chain->getSize()); }
    int getLatencySamples() const { return m_chain->getLatencySamples(); }
    void update() { m_chain->update(); }

    float getParameterValue(int idx, int paramIdx) { return m_chain->getParameterValue(idx, paramIdx); }

    struct ComparablePluginDescription : PluginDescription {
        ComparablePluginDescription(const PluginDescription& other) : PluginDescription(other) {}
        bool operator==(const ComparablePluginDescription& other) const { return isDuplicateOf(other); }
    };

    using RecentsListType = Array<ComparablePluginDescription>;
    String getRecentsList(String host) const;
    void addToRecentsList(const String& id, const String& host);

  private:
    std::unique_ptr<StreamingSocket> m_socket;
    int m_channelsIn;
    int m_channelsOut;
    double m_rate;
    int m_samplesPerBlock;
    bool m_doublePrecission;
    std::shared_ptr<ProcessorChain> m_chain;
    static std::unordered_map<String, RecentsListType> m_recents;
    static std::mutex m_recentsMtx;

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* AudioWorker_hpp */
