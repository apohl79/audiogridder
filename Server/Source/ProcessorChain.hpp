/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ProcessorChain_hpp
#define ProcessorChain_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "Utils.hpp"

namespace e47 {

class ProcessorChain : public AudioProcessor, public LogTagDelegate {
  public:
    class PlayHead : public AudioPlayHead {
      public:
        PlayHead(AudioPlayHead::CurrentPositionInfo* posInfo) : m_posInfo(posInfo) {}
        bool getCurrentPosition(CurrentPositionInfo& result) {
            result = *m_posInfo;
            return true;
        }

      private:
        AudioPlayHead::CurrentPositionInfo* m_posInfo;
    };

    ProcessorChain(const BusesProperties& props) : AudioProcessor(props) {}

    static BusesProperties createBussesProperties(bool instrument) {
        if (instrument) {
            return BusesProperties().withOutput("Output", AudioChannelSet::stereo(), false);
        } else {
            return BusesProperties()
                .withInput("Input", AudioChannelSet::stereo(), false)
                .withOutput("Output", AudioChannelSet::stereo(), false);
        }
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override;
    const String getName() const override { return "ProcessorChain"; }
    double getTailLengthSeconds() const override;
    bool supportsDoublePrecisionProcessing() const override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    bool updateChannels(int channelsIn, int channelsOut);
    bool setProcessorBusesLayout(std::shared_ptr<AudioPluginInstance> proc);
    int getExtraChannels();

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int /* index */) override {}
    const String getProgramName(int /* index */) override { return ""; }
    void changeProgramName(int /* index */, const String& /* newName */) override {}
    void getStateInformation(juce::MemoryBlock& /* destData */) override {}
    void setStateInformation(const void* /* data */, int /* sizeInBytes */) override {}

    static std::shared_ptr<AudioPluginInstance> loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize);
    bool addPluginProcessor(const String& id);
    void addProcessor(std::shared_ptr<AudioPluginInstance> processor);
    size_t getSize() const { return m_processors.size(); }
    std::shared_ptr<AudioPluginInstance> getProcessor(int index);

    void delProcessor(int idx);
    void exchangeProcessors(int idxA, int idxB);

    float getParameterValue(int idx, int paramIdx);

    void update();

    void clear();

    String toString();

  private:
    std::vector<std::shared_ptr<AudioPluginInstance>> m_processors;
    std::mutex m_processors_mtx;

    static std::mutex m_pluginLoaderMtx;

    std::atomic_bool m_supportsDoublePrecission{true};
    std::atomic<double> m_tailSecs{0.0};

    int m_extraChannels = 0;

    template <typename T>
    void processBlockReal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
        int latency = 0;
        std::lock_guard<std::mutex> lock(m_processors_mtx);
        for (auto& p : m_processors) {
            if (!p->isSuspended()) {
                p->processBlock(buffer, midiMessages);
                latency += p->getLatencySamples();
            }
        }
        if (latency != getLatencySamples()) {
            logln("updating latency samples to " << latency);
            setLatencySamples(latency);
        }
    }

    template <typename T>
    void preProcessBlocks(std::shared_ptr<AudioPluginInstance> inst) {
        MidiBuffer midi;
        int channels = jmax(getMainBusNumInputChannels(), getMainBusNumOutputChannels()) + m_extraChannels;
        AudioBuffer<T> buf(channels, getBlockSize());
        buf.clear();
        int samplesProcessed = 0;
        do {
            inst->processBlock(buf, midi);
            samplesProcessed += getBlockSize();
        } while (samplesProcessed < 8192);
    }

    void updateNoLock();
};

}  // namespace e47

#endif /* ProcessorChain_hpp */
