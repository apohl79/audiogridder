/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ProcessorChain.hpp"
#include "Utils.hpp"
#include "NumberConversion.hpp"

namespace e47 {

std::mutex ProcessorChain::m_pluginLoaderMtx;

void ProcessorChain::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    setRateAndBufferSizeDetails(sampleRate, maximumExpectedSamplesPerBlock);
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    for (auto& p : m_processors) {
        p->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }
}

void ProcessorChain::releaseResources() {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
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
        logln("warning: chain (" << toString() << "): high audio processing time: " << time_proc);
    }
}

void ProcessorChain::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    auto start_proc = Time::getHighResolutionTicks();
    processBlockReal(buffer, midiMessages);
    auto end_proc = Time::getHighResolutionTicks();
    double time_proc = Time::highResolutionTicksToSeconds(end_proc - start_proc);
    if (time_proc > 0.02) {
        logln("warning: chain (" << toString() << "): high audio processing time: " << time_proc);
    }
}

double ProcessorChain::getTailLengthSeconds() const { return m_tailSecs; }

bool ProcessorChain::supportsDoublePrecisionProcessing() const { return m_supportsDoublePrecission; }

bool ProcessorChain::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo() &&
        layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
    return true;
}

bool ProcessorChain::updateChannels(int channelsIn, int channelsOut) {
    AudioProcessor::BusesLayout layout;
    if (channelsIn == 1) {
        layout.inputBuses.add(AudioChannelSet::mono());
    } else if (channelsIn == 2) {
        layout.inputBuses.add(AudioChannelSet::stereo());
    }
    if (channelsOut == 1) {
        layout.outputBuses.add(AudioChannelSet::mono());
    } else if (channelsOut == 2) {
        layout.outputBuses.add(AudioChannelSet::stereo());
    }
    setBusesLayout(layout);
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    m_extraChannels = 0;
    for (auto& proc : m_processors) {
        if (!setProcessorBusesLayout(proc)) {
            return false;
        }
    }
    return true;
}

bool ProcessorChain::setProcessorBusesLayout(std::shared_ptr<AudioPluginInstance> proc) {
    auto layout = getBusesLayout();
    if (proc->checkBusesLayoutSupported(layout)) {
        return proc->setBusesLayout(layout);
    } else {
        // try with extra channels
        auto procLayout = proc->getBusesLayout();
        int extraInChannels = 0;
        for (int busIdx = 1; busIdx < procLayout.inputBuses.size(); busIdx++) {
            auto bus = procLayout.inputBuses[busIdx];
            extraInChannels += bus.size();
            layout.inputBuses.add(bus);
        }
        int extraOutChannels = 0;
        for (int busIdx = 1; busIdx < procLayout.outputBuses.size(); busIdx++) {
            auto bus = procLayout.outputBuses[busIdx];
            extraOutChannels += bus.size();
            layout.outputBuses.add(bus);
        }
        if (proc->checkBusesLayoutSupported(layout) && proc->setBusesLayout(layout)) {
            m_extraChannels = jmax(m_extraChannels, extraInChannels, extraOutChannels);
            logln(extraInChannels << " extra input(s), " << extraOutChannels << " extra output(s)");
            return true;
        }
    }
    return false;
}

int ProcessorChain::getExtraChannels() {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    return m_extraChannels;
}

// Sync version.
std::shared_ptr<AudioPluginInstance> ProcessorChain::loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                                int blockSize) {
    String err;
    AudioPluginFormatManager plugmgr;
    plugmgr.addDefaultFormats();
    std::lock_guard<std::mutex> lock(m_pluginLoaderMtx);  // don't load plugins in parallel
    auto inst =
        std::shared_ptr<AudioPluginInstance>(plugmgr.createPluginInstance(plugdesc, sampleRate, blockSize, err));
    if (nullptr == inst) {
        logln_static("failed loading plugin " << plugdesc.fileOrIdentifier << ": " << err);
    }
    return inst;
}

std::shared_ptr<AudioPluginInstance> ProcessorChain::loadPlugin(const String& id, double sampleRate, int blockSize) {
    auto& pluglist = getApp().getPluginList();
    auto plugdesc = pluglist.getTypeForIdentifierString(id);
    // try fallback
    if (nullptr == plugdesc) {
        plugdesc = pluglist.getTypeForFile(id);
    }
    if (nullptr != plugdesc) {
        return loadPlugin(*plugdesc, sampleRate, blockSize);
    } else {
        logln_static("failed to find plugin descriptor");
    }
    return nullptr;
}

bool ProcessorChain::addPluginProcessor(const String& id) {
    auto inst = loadPlugin(id, getSampleRate(), getBlockSize());
    if (nullptr != inst) {
        if (!setProcessorBusesLayout(inst)) {
            logln("I/O layout (" << getMainBusNumInputChannels() << "," << getMainBusNumOutputChannels() << " +"
                                 << m_extraChannels << ") not supported by plugin: " << inst->getName() << " (" << id
                                 << ")");
            return false;
        }
        AudioProcessor::ProcessingPrecision prec = AudioProcessor::singlePrecision;
        if (isUsingDoublePrecision() && supportsDoublePrecisionProcessing()) {
            if (inst->supportsDoublePrecisionProcessing()) {
                prec = AudioProcessor::doublePrecision;
            } else {
                logln("host wants double precission but plugin '" << inst->getName() << "' (" << id
                                                                  << ") does not support it");
            }
        }
        inst->setProcessingPrecision(prec);
        inst->prepareToPlay(getSampleRate(), getBlockSize());
        inst->setPlayHead(getPlayHead());
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
    updateNoLock();
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
    updateNoLock();
}

void ProcessorChain::updateNoLock() {
    int latency = 0;
    bool supportsDouble = true;
    m_extraChannels = 0;
    for (auto& proc : m_processors) {
        latency += proc->getLatencySamples();
        if (!proc->supportsDoublePrecisionProcessing()) {
            supportsDouble = false;
        }
        int extraInChannels = proc->getTotalNumInputChannels() - proc->getMainBusNumInputChannels();
        int extraOutChannels = proc->getTotalNumOutputChannels() - proc->getMainBusNumOutputChannels();
        m_extraChannels = jmax(m_extraChannels, extraInChannels, extraOutChannels);
    }
    if (latency != getLatencySamples()) {
        logln("updating latency samples to " << latency);
        setLatencySamples(latency);
    }
    m_supportsDoublePrecission = supportsDouble;
    if (m_processors.size() > 0) {
        m_tailSecs = m_processors.back()->getTailLengthSeconds();
    } else {
        m_tailSecs = 0.0;
    }
}

std::shared_ptr<AudioPluginInstance> ProcessorChain::getProcessor(int index) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (index > -1 && as<size_t>(index) < m_processors.size()) {
        return m_processors[as<size_t>(index)];
    }
    return nullptr;
}

void ProcessorChain::exchangeProcessors(int idxA, int idxB) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (idxA > -1 && as<size_t>(idxA) < m_processors.size() && idxB > -1 && as<size_t>(idxB) < m_processors.size()) {
        std::swap(m_processors[as<size_t>(idxA)], m_processors[as<size_t>(idxB)]);
    }
}

float ProcessorChain::getParameterValue(int idx, int paramIdx) {
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (idx > -1 && as<size_t>(idx) < m_processors.size()) {
        for (auto& p : m_processors[as<size_t>(idx)]->getParameters()) {
            if (paramIdx == p->getParameterIndex()) {
                return p->getValue();
            }
        }
    }
    return 0;
}

void ProcessorChain::clear() {
    releaseResources();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    m_processors.clear();
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
