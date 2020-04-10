/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ProcessorChain.hpp"
#include "Utils.hpp"

namespace e47 {

void ProcessorChain::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    setRateAndBufferSizeDetails(sampleRate, maximumExpectedSamplesPerBlock);
    for (auto& p : m_processors) {
        p->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }
}

void ProcessorChain::releaseResources() {
    for (auto& p : m_processors) {
        p->releaseResources();
    }
}

void ProcessorChain::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    auto start_proc = Time::getHighResolutionTicks();
    processBlockReal(buffer, midiMessages);
    auto end_proc = Time::getHighResolutionTicks();
    double time_proc = Time::highResolutionTicksToSeconds(end_proc - start_proc);
    if (time_proc > 0.02) {
        dbgln("warning: chain (" << toString() << "): high audio processing time: " << time_proc);
    }
}

void ProcessorChain::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    auto start_proc = Time::getHighResolutionTicks();
    processBlockReal(buffer, midiMessages);
    auto end_proc = Time::getHighResolutionTicks();
    double time_proc = Time::highResolutionTicksToSeconds(end_proc - start_proc);
    if (time_proc > 0.02) {
        dbgln("warning: chain (" << toString() << "): high audio processing time: " << time_proc);
    }
}

double ProcessorChain::getTailLengthSeconds() const {
    if (!m_processors.empty()) {
        return m_processors.back()->getTailLengthSeconds();
    }
    return 0;
}

bool ProcessorChain::supportsDoublePrecisionProcessing() const {
    for (auto& p : m_processors) {
        if (!p->supportsDoublePrecisionProcessing()) {
            return false;
        }
    }
    return true;
}

bool ProcessorChain::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo() &&
        layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
    return true;
}

void ProcessorChain::setLatency() {
    int latency = 0;
    for (auto& proc : m_processors) {
        latency += proc->getLatencySamples();
    }
    if (latency != getLatencySamples()) {
        dbgln("updating latency samples to " << latency);
        setLatencySamples(latency);
    }
}

// Async version.
// std::shared_ptr<AudioPluginInstance> ProcessorChain::loadPlugin(PluginDescription& plugdesc, double sampleRate,
//                                                                int blockSize) {
//    std::shared_ptr<AudioPluginInstance> inst;
//    std::mutex mtx;
//    std::condition_variable cv;
//    bool done = false;
//    auto fn = [&](std::unique_ptr<AudioPluginInstance> p, const String& err) {
//        std::lock_guard<std::mutex> lock(mtx);
//        if (nullptr == p) {
//            logln("failed loading plugin " << plugdesc.fileOrIdentifier << ": " << err);
//        } else {
//            inst = std::move(p);
//        }
//        done = true;
//        cv.notify_one();
//    };
//
//    AudioPluginFormatManager plugmgr;
//    plugmgr.addDefaultFormats();
//    plugmgr.createPluginInstanceAsync(plugdesc, sampleRate, blockSize, fn);
//
//    std::unique_lock<std::mutex> lock(mtx);
//    cv.wait(lock, [&done] { return done; });
//    return inst;
//}

// Sync version.
std::shared_ptr<AudioPluginInstance> ProcessorChain::loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                                int blockSize) {
    String err;
    AudioPluginFormatManager plugmgr;
    plugmgr.addDefaultFormats();
    auto inst =
        std::shared_ptr<AudioPluginInstance>(plugmgr.createPluginInstance(plugdesc, sampleRate, blockSize, err));
    if (nullptr == inst) {
        logln("failed loading plugin " << plugdesc.fileOrIdentifier << ": " << err);
    }
    return inst;
}

std::shared_ptr<AudioPluginInstance> ProcessorChain::loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                                int blockSize) {
    auto& pluglist = getApp().getPluginList();
    auto plugdesc = pluglist.getTypeForFile(fileOrIdentifier);
    if (nullptr != plugdesc) {
        return loadPlugin(*plugdesc, sampleRate, blockSize);
    } else {
        logln("failed to find plugin descriptor");
    }
    return nullptr;
}

bool ProcessorChain::addPluginProcessor(const String& fileOrIdentifier) {
    auto inst = loadPlugin(fileOrIdentifier, getSampleRate(), getBlockSize());
    if (nullptr != inst) {
        if (!inst->setBusesLayout(getBusesLayout())) {
            logln("I/O layout not supported by plugin: " << fileOrIdentifier);
            return false;
        }
        if (!inst->enableAllBuses()) {
            logln("failed to enable busses for plugin: " << fileOrIdentifier);
            return false;
        }
        AudioProcessor::ProcessingPrecision prec = AudioProcessor::singlePrecision;
        if (isUsingDoublePrecision()) {
            if (inst->supportsDoublePrecisionProcessing()) {
                prec = AudioProcessor::doublePrecision;
            } else {
                logln("host wants double precission but plugin (" << fileOrIdentifier << ") does not support it");
            }
        }
        inst->setProcessingPrecision(prec);
        inst->prepareToPlay(getSampleRate(), getBlockSize());
        if (prec == AudioProcessor::doublePrecision) {
            preProcessBlocks<double>(inst);
        } else {
            preProcessBlocks<float>(inst);
        }
        return addProcessor(inst);
    }
    return false;
}

bool ProcessorChain::addProcessor(std::shared_ptr<AudioPluginInstance> processor) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    m_processors.push_back(processor);
    setLatency();
    return true;
}

void ProcessorChain::delProcessor(int idx) {
    int i = 0;
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    for (auto it = m_processors.begin(); it < m_processors.end(); it++) {
        if (i++ == idx) {
            m_processors.erase(it);
            break;
        }
    }
}

std::shared_ptr<AudioPluginInstance> ProcessorChain::getProcessor(int index) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (index > -1 && index < m_processors.size()) {
        return m_processors[index];
    }
    return nullptr;
}

void ProcessorChain::exchangeProcessors(int idxA, int idxB) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (idxA > -1 && idxB < m_processors.size() && idxB > -1 && idxB < m_processors.size()) {
        std::swap(m_processors[idxA], m_processors[idxB]);
    }
}

String ProcessorChain::toString() {
    String ret;
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    bool first = true;
    for (auto& p : m_processors) {
        if (!first) {
            ret << " > ";
        } else {
            first = false;
        }
        ret << p->getName();
    }
    return ret;
}

}  // namespace e47
