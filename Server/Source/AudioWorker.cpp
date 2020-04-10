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

AudioWorker::~AudioWorker() {
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
}

void AudioWorker::init(std::unique_ptr<StreamingSocket> s, int channels, double rate, int samplesPerBlock,
                       bool doublePrecission, std::function<void()> fn) {
    m_socket = std::move(s);
    m_channels = channels;
    m_rate = rate;
    m_samplesPerBlock = samplesPerBlock;
    m_doublePrecission = doublePrecission;
    m_onTerminate = fn;
    AudioProcessor::BusesLayout layout;
    if (channels == 1) {
        layout.inputBuses.add(AudioChannelSet::mono());
        layout.outputBuses.add(AudioChannelSet::mono());
    } else if (channels == 2) {
        layout.inputBuses.add(AudioChannelSet::stereo());
        layout.outputBuses.add(AudioChannelSet::stereo());
    }
    m_chain->setBusesLayout(layout);
    if (m_doublePrecission && m_chain->supportsDoublePrecisionProcessing()) {
        m_chain->setProcessingPrecision(AudioProcessor::doublePrecision);
    }
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
                    if (m_chain->supportsDoublePrecisionProcessing()) {
                        m_chain->processBlock(bufferD, midi);
                    } else {
                        bufferF.makeCopyOf(bufferD);
                        m_chain->processBlock(bufferF, midi);
                        bufferD.makeCopyOf(bufferF);
                    }
                } else if (bufferF.getNumChannels() > 0 && bufferF.getNumSamples() > 0) {
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

    m_chain->releaseResources();
    m_chain->clear();

    signalThreadShouldExit();
    if (m_onTerminate) {
        m_onTerminate();
    }
    dbgln("audio processor terminated");
}

void AudioWorker::shutdown() { signalThreadShouldExit(); }

bool AudioWorker::addPlugin(const String& id) {
    dbgln("adding plugin " << id << "...");
    bool success = m_chain->addPluginProcessor(id);
    if (success) {
        // update recents list
        auto& pluginList = getApp().getPluginList();
        auto plug = pluginList.getTypeForFile(id);
        m_recents.insert(0, *plug);
        for (int i = 1; i < m_recents.size(); i++) {
            if (plug->fileOrIdentifier == m_recents[i].fileOrIdentifier) {
                m_recents.remove(i);
                break;
            }
        }
        while (m_recents.size() > 5) {
            m_recents.removeLast();
        }
    } else {
        dbgln("failed to add plugin");
    }
    dbgln("..." << (success ? "ok" : "failed"));
    return success;
}

void AudioWorker::delPlugin(int idx) {
    dbgln("deleting plugin " << idx);
    m_chain->delProcessor(idx);
}

void AudioWorker::exchangePlugins(int idxA, int idxB) {
    dbgln("exchanging plugins idxA=" << idxA << " idxB=" << idxB);
    m_chain->exchangeProcessors(idxA, idxB);
}

}  // namespace e47
