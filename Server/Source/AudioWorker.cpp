/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "AudioWorker.hpp"

#include "Message.hpp"
#include "Utils.hpp"

namespace e47 {

HashMap<String, AudioWorker::RecentsListType> AudioWorker::m_recents;
std::mutex AudioWorker::m_recentsMtx;

AudioWorker::~AudioWorker() {
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
    m_recents.clear();
    stopThread(-1);
}

void AudioWorker::init(std::unique_ptr<StreamingSocket> s, int channels, double rate, int samplesPerBlock,
                       bool doublePrecission, std::function<void()> fn) {
    m_socket = std::move(s);
    m_rate = rate;
    m_samplesPerBlock = samplesPerBlock;
    m_doublePrecission = doublePrecission;
    m_onTerminate = fn;
    if (m_doublePrecission && m_chain->supportsDoublePrecisionProcessing()) {
        m_chain->setProcessingPrecision(AudioProcessor::doublePrecision);
    }
    m_channels = channels;
    m_chain->updateChannels(channels);
}

void AudioWorker::run() {
    AudioBuffer<float> bufferF;
    AudioBuffer<double> bufferD;
    MidiBuffer midi;
    AudioMessage msg;
    AudioPlayHead::CurrentPositionInfo posInfo;

    ProcessorChain::PlayHead playHead(&posInfo);
    m_chain->prepareToPlay(m_rate, m_samplesPerBlock);
    m_chain->setPlayHead(&playHead);

    while (!currentThreadShouldExit() && nullptr != m_socket && m_socket->isConnected()) {
        // Read audio chunk
        if (m_socket->waitUntilReady(true, 1000)) {
            if (msg.readFromClient(m_socket.get(), bufferF, bufferD, midi, posInfo)) {
                if (msg.isDouble() && bufferD.getNumChannels() > 0 && bufferD.getNumSamples() > 0) {
                    if (m_channels > bufferD.getNumChannels()) {
                        dbgln("updating bus layout at processing time due to channel mismatch");
                        m_chain->releaseResources();
                        if (!m_chain->updateChannels(bufferD.getNumChannels())) {
                            logln("failed setting bus layout");
                            m_socket->close();
                            break;
                        }
                        m_channels = bufferD.getNumChannels();
                        m_chain->prepareToPlay(m_rate, m_samplesPerBlock);
                    }
                    if (m_chain->supportsDoublePrecisionProcessing()) {
                        m_chain->processBlock(bufferD, midi);
                    } else {
                        bufferF.makeCopyOf(bufferD);
                        m_chain->processBlock(bufferF, midi);
                        bufferD.makeCopyOf(bufferF);
                    }
                } else if (bufferF.getNumChannels() > 0 && bufferF.getNumSamples() > 0) {
                    if (m_channels > bufferF.getNumChannels()) {
                        dbgln("updating bus layout at processing time due to channel mismatch");
                        m_chain->releaseResources();
                        if (!m_chain->updateChannels(bufferF.getNumChannels())) {
                            logln("failed setting bus layout");
                            m_socket->close();
                            break;
                        }
                        m_channels = bufferF.getNumChannels();
                        m_chain->prepareToPlay(m_rate, m_samplesPerBlock);
                    }
                    m_chain->processBlock(bufferF, midi);
                } else {
                    logln("empty audio message from " << m_socket->getHostName());
                }
                if (msg.isDouble()) {
                    if (!msg.sendToClient(m_socket.get(), bufferD, midi, m_chain->getLatencySamples())) {
                        logln("failed to send audio data to client");
                        m_socket->close();
                    }
                } else {
                    if (!msg.sendToClient(m_socket.get(), bufferF, midi, m_chain->getLatencySamples())) {
                        logln("failed to send audio data to client");
                        m_socket->close();
                    }
                }
            } else {
                m_socket->close();
            }
        }
    }

    clear();

    signalThreadShouldExit();
    if (m_onTerminate) {
        m_onTerminate();
    }
    dbgln("audio processor terminated");
}

void AudioWorker::shutdown() {
    clear();
    signalThreadShouldExit();
}

void AudioWorker::clear() {
    m_chain->releaseResources();
    if (!MessageManager::getInstance()->isThisTheMessageThread()) {
        if (m_chain->getSize() > 0) {
            auto pChain = m_chain;
            MessageManager::callAsync([pChain] { pChain->clear(); });
        }
    } else {
        m_chain->clear();
    }
}

bool AudioWorker::addPlugin(const String& id) { return m_chain->addPluginProcessor(id); }

void AudioWorker::delPlugin(int idx) {
    dbgln("deleting plugin " << idx);
    m_chain->delProcessor(idx);
}

void AudioWorker::exchangePlugins(int idxA, int idxB) {
    dbgln("exchanging plugins idxA=" << idxA << " idxB=" << idxB);
    m_chain->exchangeProcessors(idxA, idxB);
}

AudioWorker::RecentsListType& AudioWorker::getRecentsList(String host) const {
    std::lock_guard<std::mutex> lock(m_recentsMtx);
    return m_recents.getReference(host);
}

void AudioWorker::addToRecentsList(const String& id, const String& host) {
    auto& pluginList = getApp().getPluginList();
    auto plug = pluginList.getTypeForFile(id);
    auto& recents = getRecentsList(host);
    recents.insert(0, *plug);
    for (int i = 1; i < recents.size(); i++) {
        if (!plug->fileOrIdentifier.compare(recents.getReference(i).fileOrIdentifier)) {
            recents.remove(i);
            break;
        }
    }
    while (recents.size() > DEFAULT_NUM_RECENTS) {
        recents.removeLast();
    }
}

}  // namespace e47
