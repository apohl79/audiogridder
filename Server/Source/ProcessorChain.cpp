/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ProcessorChain.hpp"
#include "Processor.hpp"
#include "App.hpp"

namespace e47 {

void ProcessorChain::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    traceScope();
    setRateAndBufferSizeDetails(sampleRate, maximumExpectedSamplesPerBlock);
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    for (auto& proc : m_processors) {
        proc->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }
}

void ProcessorChain::releaseResources() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    for (auto& proc : m_processors) {
        proc->releaseResources();
    }
}

void ProcessorChain::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

void ProcessorChain::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

double ProcessorChain::getTailLengthSeconds() const { return m_tailSecs; }

bool ProcessorChain::supportsDoublePrecisionProcessing() const { return m_supportsDoublePrecision; }

bool ProcessorChain::updateChannels(int channelsIn, int channelsOut, int channelsSC) {
    traceScope();
    AudioProcessor::BusesLayout layout;
    if (channelsIn == 1) {
        layout.inputBuses.add(AudioChannelSet::mono());
    } else if (channelsIn == 2) {
        layout.inputBuses.add(AudioChannelSet::stereo());
    } else if (channelsIn > 0) {
        layout.inputBuses.add(AudioChannelSet::discreteChannels(channelsIn));
    }
    if (channelsSC == 1) {
        layout.inputBuses.add(AudioChannelSet::mono());
    } else if (channelsSC == 2) {
        layout.inputBuses.add(AudioChannelSet::stereo());
    } else if (channelsSC > 0) {
        layout.inputBuses.add(AudioChannelSet::discreteChannels(channelsIn));
    }
    if (channelsOut == 1) {
        layout.outputBuses.add(AudioChannelSet::mono());
    } else if (channelsOut == 2) {
        layout.outputBuses.add(AudioChannelSet::stereo());
    } else if (channelsOut > 0) {
        layout.outputBuses.add(AudioChannelSet::discreteChannels(channelsOut));
    }
    logln("setting chain layout");
    printBusesLayout(layout);
    if (!setBusesLayout(layout)) {
        logln("failed to set layout");
    }
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    m_extraChannels = 0;
    m_hasSidechain = channelsSC > 0;
    m_sidechainDisabled = false;
    for (auto& proc : m_processors) {
        setProcessorBusesLayout(proc.get());
    }
    return true;
}

bool ProcessorChain::setProcessorBusesLayout(Processor* proc) {
    traceScope();

    if (!proc->isLoaded()) {
        return false;
    }

    auto layout = getBusesLayout();

    if (m_hasSidechain && m_sidechainDisabled) {
        logln("the sidechain has been disabled, removing it from the standard layout");
        layout.inputBuses.remove(1);
    }

    bool hasSidechain = m_hasSidechain && !m_sidechainDisabled;
    bool supported = proc->checkBusesLayoutSupported(layout) && proc->setBusesLayout(layout);

    if (!supported) {
        logln("standard layout not supported");

        // try with mono or without sidechain
        if (hasSidechain) {
            if (layout.getChannelSet(true, 1).size() > 1) {
                logln("trying with mono sidechain bus");
                layout.inputBuses.remove(1);
                layout.inputBuses.add(AudioChannelSet::mono());
                supported = proc->checkBusesLayoutSupported(layout) && proc->setBusesLayout(layout);
            }
            if (!supported) {
                logln("trying without sidechain bus");
                layout.inputBuses.remove(1);
                supported = proc->checkBusesLayoutSupported(layout) && proc->setBusesLayout(layout);
                if (supported) {
                    proc->setNeedsDisabledSidechain(true);
                    m_sidechainDisabled = true;
                }
            }
        }
        if (!supported && layout.getMainOutputChannels() > 2 && layout.getMainInputChannels() == 0) {
            logln("trying multi-mono-bus layout for instrument");
            AudioProcessor::BusesLayout layout2;
            for (int ch = 0; ch < layout.getMainOutputChannels(); ch++) {
                layout2.outputBuses.add(AudioChannelSet::mono());
            }
            supported = proc->checkBusesLayoutSupported(layout2) && proc->setBusesLayout(layout2);
            if (!supported) {
                logln("trying multi-stereo-bus layout for instrument");
                layout2.outputBuses.clear();
                for (int ch = 0; ch + 1 < layout.getMainOutputChannels(); ch += 2) {
                    layout2.outputBuses.add(AudioChannelSet::stereo());
                }
                supported = proc->checkBusesLayoutSupported(layout2) && proc->setBusesLayout(layout2);
            }
            if (!supported) {
                logln("trying multi-mono-bus layout with stereo main for instrument");
                layout2.outputBuses.clear();
                layout2.outputBuses.add(AudioChannelSet::stereo());
                for (int ch = 2; ch < layout.getMainOutputChannels(); ch++) {
                    layout2.outputBuses.add(AudioChannelSet::mono());
                }
                supported = proc->checkBusesLayoutSupported(layout2) && proc->setBusesLayout(layout2);
            }
        }
        if (!supported) {
            if (hasSidechain) {
                logln("disabling sidechain input to use the plugins I/O layout");
                m_sidechainDisabled = true;
            }

            // when getting here, make sure we always disable the sidechain for this plugin
            proc->setNeedsDisabledSidechain(true);

            logln("falling back to the plugins default layout");

            // keep the processor's layout and calculate the neede extra channels
            auto procLayout = proc->getBusesLayout();

            // main bus IN
            int extraInChannels = procLayout.getMainInputChannels() - layout.getMainInputChannels();
            // check extra busses IN
            for (int busIdx = 1; busIdx < procLayout.inputBuses.size(); busIdx++) {
                extraInChannels += procLayout.inputBuses[busIdx].size();
            }
            // main bus OUT
            int extraOutChannels = procLayout.getMainOutputChannels() - layout.getMainOutputChannels();
            // check extra busses OUT
            for (int busIdx = 1; busIdx < procLayout.outputBuses.size(); busIdx++) {
                extraOutChannels += procLayout.outputBuses[busIdx].size();
            }

            proc->setExtraChannels(extraInChannels, extraOutChannels);

            m_extraChannels = jmax(m_extraChannels, extraInChannels, extraOutChannels);

            logln(extraInChannels << " extra input(s), " << extraOutChannels << " extra output(s) -> "
                                  << m_extraChannels << " extra channel(s) in total");

            layout = procLayout;
            supported = true;
        }
    }

    if (supported) {
        logln("using I/O layout:");
        printBusesLayout(layout);
    } else {
        logln("no working I/O layout found");
    }

    return supported;
}

int ProcessorChain::getExtraChannels() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    return m_extraChannels;
}

bool ProcessorChain::initPluginInstance(Processor* proc, String& err) {
    traceScope();
    if (!setProcessorBusesLayout(proc)) {
        err = "failed to find working I/O configuration";
        return false;
    }
    AudioProcessor::ProcessingPrecision prec = AudioProcessor::singlePrecision;
    if (isUsingDoublePrecision() && supportsDoublePrecisionProcessing()) {
        if (proc->supportsDoublePrecisionProcessing()) {
            prec = AudioProcessor::doublePrecision;
        } else {
            logln("host wants double precission but plugin '" << proc->getName() << "' does not support it");
        }
    }
    proc->setProcessingPrecision(prec);
    proc->prepareToPlay(getSampleRate(), getBlockSize());
    proc->setPlayHead(getPlayHead());
    proc->enableAllBuses();
    if (prec == AudioProcessor::doublePrecision) {
        preProcessBlocks<double>(proc);
    } else {
        preProcessBlocks<float>(proc);
    }
    return true;
}

bool ProcessorChain::addPluginProcessor(const String& id, const String& settings, String& err) {
    traceScope();
    auto proc = std::make_shared<Processor>(*this, id, getSampleRate(), getBlockSize());
    if (proc->load(settings, err)) {
        addProcessor(std::move(proc));
        return true;
    }
    return false;
}

void ProcessorChain::addProcessor(std::shared_ptr<Processor> processor) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    processor->setChainIndex((int)m_processors.size());
    m_processors.push_back(processor);
    updateNoLock();
}

void ProcessorChain::delProcessor(int idx) {
    traceScope();
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

void ProcessorChain::update() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    updateNoLock();
}

void ProcessorChain::updateNoLock() {
    traceScope();
    int latency = 0;
    bool supportsDouble = true;
    m_extraChannels = 0;
    m_sidechainDisabled = false;
    for (auto& proc : m_processors) {
        if (nullptr != proc) {
            latency += proc->getLatencySamples();
            if (!proc->supportsDoublePrecisionProcessing()) {
                supportsDouble = false;
            }
            m_extraChannels = jmax(m_extraChannels, proc->getExtraInChannels(), proc->getExtraOutChannels());
            m_sidechainDisabled = m_hasSidechain && (m_sidechainDisabled || proc->getNeedsDisabledSidechain());
        }
    }
    if (latency != getLatencySamples()) {
        logln("updating latency samples to " << latency);
        setLatencySamples(latency);
    }
    m_supportsDoublePrecision = supportsDouble;
    auto it = m_processors.rbegin();
    while (it != m_processors.rend() && (*it)->isSuspended()) {
        it++;
    }
    if (it != m_processors.rend()) {
        m_tailSecs = (*it)->getTailLengthSeconds();
    } else {
        m_tailSecs = 0.0;
    }
}

std::shared_ptr<Processor> ProcessorChain::getProcessor(int index) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (index > -1 && (size_t)index < m_processors.size()) {
        return m_processors[(size_t)index];
    }
    return nullptr;
}

void ProcessorChain::exchangeProcessors(int idxA, int idxB) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (idxA > -1 && (size_t)idxA < m_processors.size() && idxB > -1 && (size_t)idxB < m_processors.size()) {
        std::swap(m_processors[(size_t)idxA], m_processors[(size_t)idxB]);
        m_processors[(size_t)idxA]->setChainIndex(idxA);
        m_processors[(size_t)idxB]->setChainIndex(idxB);
    }
}

float ProcessorChain::getParameterValue(int idx, int paramIdx) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    if (idx > -1 && (size_t)idx < m_processors.size()) {
        if (auto p = m_processors[(size_t)idx]) {
            return p->getParameterValue(paramIdx);
        }
    }
    return 0.0f;
}

void ProcessorChain::clear() {
    traceScope();
    releaseResources();
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    m_processors.clear();
}

String ProcessorChain::toString() {
    traceScope();
    String ret;
    std::lock_guard<std::mutex> lock(m_processors_mtx);
    bool first = true;
    for (auto& proc : m_processors) {
        if (!first) {
            ret << " > ";
        } else {
            first = false;
        }
        if (proc->isSuspended()) {
            ret << "<bypassed>";
        } else {
            ret << proc->getName();
        }
    }
    return ret;
}

template <typename T>
void ProcessorChain::processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();

    int latency = 0;
    if (getBusCount(true) > 1 && m_sidechainDisabled) {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        sidechainBuffer.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_processors_mtx);
        for (auto& proc : m_processors) {
            if (proc->processBlock(buffer, midiMessages)) {
                latency += proc->getLatencySamples();
            }
        }
    }

    if (latency != getLatencySamples()) {
        logln("updating latency samples to " << latency);
        setLatencySamples(latency);
    }
}

template <typename T>
void ProcessorChain::preProcessBlocks(Processor* proc) {
    traceScope();
    MidiBuffer midi;
    int channels = jmax(getTotalNumInputChannels(), getTotalNumOutputChannels()) + m_extraChannels;
    AudioBuffer<T> buf(channels, getBlockSize());
    buf.clear();
    int samplesProcessed = 0;
    do {
        proc->processBlock(buf, midi);
        samplesProcessed += getBlockSize();
    } while (samplesProcessed < 16384);
}

}  // namespace e47
