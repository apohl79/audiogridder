/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ProcessorChain_hpp
#define ProcessorChain_hpp

#include "../JuceLibraryCode/JuceHeader.h"

namespace e47 {

class ProcessorChain : public AudioProcessor {
  public:
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override;
    const String getName() const override { return "ProcessorChain"; }
    double getTailLengthSeconds() const override;
    bool supportsDoublePrecisionProcessing() const override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    bool acceptsMidi() const override { return false; };
    bool producesMidi() const override { return false; };
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const String getProgramName(int index) override { return ""; }
    void changeProgramName(int index, const String& newName) override {}
    void getStateInformation(juce::MemoryBlock& destData) override {}
    void setStateInformation(const void* data, int sizeInBytes) override {}

    void setLatency();

    static std::shared_ptr<AudioPluginInstance> loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize);
    bool addPluginProcessor(const String& fileOrIdentifier);
    bool addProcessor(std::shared_ptr<AudioProcessor> processor);
    size_t getSize() const { return m_processors.size(); }
    std::shared_ptr<AudioProcessor> getProcessor(int index);

    void delProcessor(int idx);
    void exchangeProcessors(int idxA, int idxB);

  private:
    std::vector<std::shared_ptr<AudioProcessor>> m_processors;
    std::mutex m_processors_mtx;
};

}  // namespace e47

#endif /* ProcessorChain_hpp */
