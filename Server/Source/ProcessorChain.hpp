/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ProcessorChain_hpp
#define ProcessorChain_hpp

#include <JuceHeader.h>

#include "Utils.hpp"
#include "Defaults.hpp"
#include "Message.hpp"

namespace e47 {

class Processor;

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

    ProcessorChain(const LogTag* tag, const BusesProperties& props, const HandshakeRequest& cfg)
        : AudioProcessor(props), LogTagDelegate(tag), m_cfg(cfg) {}

    static BusesProperties createBussesProperties(int in, int out, int sc) {
        setLogTagStatic("processorchain");
        traceScope();
        auto props = BusesProperties().withOutput("Output", AudioChannelSet::discreteChannels(out), false);
        if (in > 0) {
            props = props.withInput("Input", AudioChannelSet::discreteChannels(in), false);
        }
        if (sc > 0) {
            props = props.withInput("Sidechain", AudioChannelSet::discreteChannels(sc), false);
        }
        return props;
    }

    const HandshakeRequest& getConfig() const { return m_cfg; }

    bool isSidechainDisabled() const { return m_sidechainDisabled; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void setPlayHead(AudioPlayHead* ph) override;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override;
    const String getName() const override { return "ProcessorChain"; }
    double getTailLengthSeconds() const override;
    bool supportsDoublePrecisionProcessing() const override;
    bool isBusesLayoutSupported(const BusesLayout& /*layouts*/) const override { return true; }

    bool updateChannels(int channelsIn, int channelsOut, int channelsSC);
    bool setProcessorBusesLayout(Processor* proc);
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

    bool initPluginInstance(Processor* proc, String& err);
    bool addPluginProcessor(const String& id, const String& settings, String& err);
    void addProcessor(std::shared_ptr<Processor> processor);
    size_t getSize() const { return m_processors.size(); }
    std::shared_ptr<Processor> getProcessor(int index);

    void delProcessor(int idx);
    void exchangeProcessors(int idxA, int idxB);
    float getParameterValue(int idx, int paramIdx);
    void update();
    void clear();
    String toString();

  private:
    std::vector<std::shared_ptr<Processor>> m_processors;
    std::mutex m_processorsMtx;

    std::atomic_bool m_supportsDoublePrecision{true};
    std::atomic<double> m_tailSecs{0.0};

    HandshakeRequest m_cfg;

    int m_extraChannels = 0;
    bool m_hasSidechain = false;
    bool m_sidechainDisabled = false;

    template <typename T>
    void processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages);

    template <typename T>
    void preProcessBlocks(Processor* proc);

    void updateNoLock();
};

}  // namespace e47

#endif /* ProcessorChain_hpp */
