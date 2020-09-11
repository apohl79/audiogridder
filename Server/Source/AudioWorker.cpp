/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "AudioWorker.hpp"
#include "Message.hpp"
#include "Defaults.hpp"
#include "App.hpp"
#include "Metrics.hpp"

namespace e47 {

HashMap<String, AudioWorker::RecentsListType> AudioWorker::m_recents;
std::mutex AudioWorker::m_recentsMtx;

AudioWorker::~AudioWorker() {
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
    m_recents.clear();
    waitForThreadAndLog(getLogTagSource(), this);
}

void AudioWorker::init(std::unique_ptr<StreamingSocket> s, int channelsIn, int channelsOut, double rate,
                       int samplesPerBlock, bool doublePrecission) {
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
    logln("audio processor started");

    AudioBuffer<float> bufferF;
    AudioBuffer<double> bufferD;
    MidiBuffer midi;
    AudioMessage msg;
    AudioPlayHead::CurrentPositionInfo posInfo;
    auto duration = TimeStatistics::getDuration("audio");

    ProcessorChain::PlayHead playHead(&posInfo);
    m_chain->prepareToPlay(m_rate, m_samplesPerBlock);
    bool hasToSetPlayHead = true;

    MessageHelper::Error e;
    while (!currentThreadShouldExit() && nullptr != m_socket && m_socket->isConnected()) {
        // Read audio chunk
        if (m_socket->waitUntilReady(true, 1000)) {
            if (msg.readFromClient(m_socket.get(), bufferF, bufferD, midi, posInfo, m_chain->getExtraChannels(), &e)) {
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
                    sendOk =
                        msg.sendToClient(m_socket.get(), bufferD, midi, m_chain->getLatencySamples(), m_channelsOut);
                } else {
                    m_chain->processBlock(bufferF, midi);
                    sendOk =
                        msg.sendToClient(m_socket.get(), bufferF, midi, m_chain->getLatencySamples(), m_channelsOut);
                }
                if (!sendOk) {
                    logln("error: failed to send audio data to client");
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

    clear();
    signalThreadShouldExit();
    logln("audio processor terminated");
}

void AudioWorker::shutdown() { signalThreadShouldExit(); }

void AudioWorker::clear() {
    if (nullptr != m_chain) {
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
}

bool AudioWorker::addPlugin(const String& id) { return m_chain->addPluginProcessor(id); }

void AudioWorker::delPlugin(int idx) {
    logln("deleting plugin " << idx);
    m_chain->delProcessor(idx);
}

void AudioWorker::exchangePlugins(int idxA, int idxB) {
    logln("exchanging plugins idxA=" << idxA << " idxB=" << idxB);
    m_chain->exchangeProcessors(idxA, idxB);
}

AudioWorker::RecentsListType& AudioWorker::getRecentsList(String host) const {
    std::lock_guard<std::mutex> lock(m_recentsMtx);
    return m_recents.getReference(host);
}

void AudioWorker::addToRecentsList(const String& id, const String& host) {
    auto& pluginList = getApp()->getPluginList();
    auto plug = pluginList.getTypeForIdentifierString(id);
    if (plug != nullptr) {
        auto& recents = getRecentsList(host);
        recents.insert(0, *plug);
        for (int i = 1; i < recents.size(); i++) {
            if (!plug->createIdentifierString().compare(recents.getReference(i).createIdentifierString())) {
                recents.remove(i);
                break;
            }
        }
        while (recents.size() > DEFAULT_NUM_RECENTS) {
            recents.removeLast();
        }
    }
}

}  // namespace e47
