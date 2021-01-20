/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "AudioWorker.hpp"
#include <memory>
#include "Message.hpp"
#include "Defaults.hpp"
#include "App.hpp"
#include "Metrics.hpp"

namespace e47 {

std::atomic_uint32_t AudioWorker::count{0};
std::atomic_uint32_t AudioWorker::runCount{0};
std::unordered_map<String, AudioWorker::RecentsListType> AudioWorker::m_recents;
std::mutex AudioWorker::m_recentsMtx;

AudioWorker::AudioWorker(LogTag* tag) : Thread("AudioWorker"), LogTagDelegate(tag) {
    initAsyncFunctors();
    count++;
}

AudioWorker::~AudioWorker() {
    traceScope();
    stopAsyncFunctors();
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
    waitForThreadAndLog(getLogTagSource(), this);
    count--;
}

void AudioWorker::init(std::unique_ptr<StreamingSocket> s, int channelsIn, int channelsOut, double rate,
                       int samplesPerBlock, bool doublePrecission) {
    traceScope();
    m_socket = std::move(s);
    m_rate = rate;
    m_samplesPerBlock = samplesPerBlock;
    m_doublePrecission = doublePrecission;
    m_channelsIn = channelsIn;
    m_channelsOut = channelsOut;
    m_chain = std::make_shared<ProcessorChain>(ProcessorChain::createBussesProperties(channelsIn == 0));
    m_chain->setLogTagSource(getLogTagSource());
    if (m_doublePrecission && m_chain->supportsDoublePrecisionProcessing()) {
        m_chain->setProcessingPrecision(AudioProcessor::doublePrecision);
    }
    m_chain->updateChannels(channelsIn, channelsOut);
}

void AudioWorker::run() {
    traceScope();
    runCount++;
    logln("audio processor started");

    AudioBuffer<float> bufferF;
    AudioBuffer<double> bufferD;
    MidiBuffer midi;
    AudioMessage msg(getLogTagSource());
    AudioPlayHead::CurrentPositionInfo posInfo;
    auto duration = TimeStatistic::getDuration("audio");
    auto bytesIn = Metrics::getStatistic<Meter>("NetBytesIn");
    auto bytesOut = Metrics::getStatistic<Meter>("NetBytesOut");

    ProcessorChain::PlayHead playHead(&posInfo);
    m_chain->prepareToPlay(m_rate, m_samplesPerBlock);
    bool hasToSetPlayHead = true;

    MessageHelper::Error e;
    while (!currentThreadShouldExit() && nullptr != m_socket && m_socket->isConnected()) {
        // Read audio chunk
        if (m_socket->waitUntilReady(true, 1000)) {
            if (msg.readFromClient(m_socket.get(), bufferF, bufferD, midi, posInfo, m_chain->getExtraChannels(), &e,
                                   *bytesIn)) {
                duration.reset();
                if (hasToSetPlayHead) {  // do not set the playhead before it's initialized
                    m_chain->setPlayHead(&playHead);
                    hasToSetPlayHead = false;
                }
                int bufferChannels = msg.isDouble() ? bufferD.getNumChannels() : bufferF.getNumChannels();
                if (m_channelsOut > bufferChannels) {
                    logln("error processing audio message: buffer has not enough channels: out channels is "
                          << m_channelsOut << ", but buffer has " << bufferChannels);
                    m_chain->releaseResources();
                    m_socket->close();
                    break;
                }
                bool sendOk;
                if (msg.isDouble()) {
                    if (m_chain->supportsDoublePrecisionProcessing()) {
                        m_chain->processBlock(bufferD, midi);
                    } else {
                        bufferF.makeCopyOf(bufferD);
                        m_chain->processBlock(bufferF, midi);
                        bufferD.makeCopyOf(bufferF);
                    }
                    sendOk = msg.sendToClient(m_socket.get(), bufferD, midi, m_chain->getLatencySamples(),
                                              m_channelsOut, &e, *bytesOut);
                } else {
                    m_chain->processBlock(bufferF, midi);
                    sendOk = msg.sendToClient(m_socket.get(), bufferF, midi, m_chain->getLatencySamples(),
                                              m_channelsOut, &e, *bytesOut);
                }
                if (!sendOk) {
                    logln("error: failed to send audio data to client: " << e.toString());
                    m_socket->close();
                }
                duration.update();
            } else {
                logln("error: failed to read audio message: " << e.toString());
                m_socket->close();
            }
        }
    }

    m_chain->setPlayHead(nullptr);

    duration.clear();
    clear();
    signalThreadShouldExit();
    logln("audio processor terminated");
    runCount--;
}

void AudioWorker::shutdown() {
    traceScope();
    signalThreadShouldExit();
}

void AudioWorker::clear() {
    traceScope();
    if (nullptr != m_chain) {
        m_chain->clear();
    }
}

bool AudioWorker::addPlugin(const String& id, String& err) {
    traceScope();
    return m_chain->addPluginProcessor(id, err);
}

void AudioWorker::delPlugin(int idx) {
    traceScope();
    logln("deleting plugin " << idx);
    m_chain->delProcessor(idx);
}

void AudioWorker::exchangePlugins(int idxA, int idxB) {
    traceScope();
    logln("exchanging plugins idxA=" << idxA << " idxB=" << idxB);
    m_chain->exchangeProcessors(idxA, idxB);
}

String AudioWorker::getRecentsList(String host) const {
    traceScope();
    std::lock_guard<std::mutex> lock(m_recentsMtx);
    if (m_recents.find(host) == m_recents.end()) {
        return "";
    }
    auto& recents = m_recents[host];
    String list;
    for (auto& r : recents) {
        list += AGProcessor::createString(r) + "\n";
    }
    return list;
}

void AudioWorker::addToRecentsList(const String& id, const String& host) {
    traceScope();
    auto plug = AGProcessor::findPluginDescritpion(id);
    if (plug != nullptr) {
        std::lock_guard<std::mutex> lock(m_recentsMtx);
        auto& recents = m_recents[host];
        recents.removeAllInstancesOf(*plug);
        recents.insert(0, *plug);
        int toRemove = recents.size() - Defaults::DEFAULT_NUM_RECENTS;
        if (toRemove > 0) {
            recents.removeLast(toRemove);
        }
    }
}

}  // namespace e47
