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

namespace e47 {

class ProcessorChain;

class AGProcessor {
  public:
    AGProcessor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize)
        : m_chain(chain), m_id(id), m_sampleRate(sampleRate), m_blockSize(blockSize) {}

    std::shared_ptr<AudioPluginInstance> getPlugin() {
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        return m_plugin;
    }

    void updateScreenCaptureArea(int val) {
        m_additionalScreenSpace = m_additionalScreenSpace + val > 0 ? m_additionalScreenSpace + val : 0;
    }

    int getAdditionalScreenCapturingSpace() { return m_additionalScreenSpace; }

    static std::shared_ptr<AudioPluginInstance> loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize);

    bool load();
    void unload();

    template <typename T>
    bool processBlock(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
        auto p = getPlugin();
        if (nullptr != p && !p->isSuspended()) {
            p->processBlock(buffer, midiMessages);
            return true;
        }
        return false;
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
        auto p = getPlugin();
        if (nullptr != p) {
            p->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        }
    }

    void releaseResources() {
        auto p = getPlugin();
        if (nullptr != p) {
            p->releaseResources();
        }
    }

    int getLatencySamples() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getLatencySamples();
        }
        return 0;
    }

    const String getName() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getName();
        }
        return "";
    }

    bool hasEditor() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->hasEditor();
        }
        return false;
    }

    bool isSuspended() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->isSuspended();
        }
        return true;
    }

    double getTailLengthSeconds() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getTailLengthSeconds();
        }
        return 0.0;
    }

    AudioProcessorEditor* createEditorIfNeeded() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->createEditorIfNeeded();
        }
        return nullptr;
    }

    AudioProcessorEditor* getActiveEditor() {
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getActiveEditor();
        }
        return nullptr;
    }

    void getStateInformation(juce::MemoryBlock& destData) {
        auto p = getPlugin();
        if (nullptr != p) {
            p->getStateInformation(destData);
        }
    }

    void setStateInformation(const void* data, int sizeInBytes) {
        auto p = getPlugin();
        if (nullptr != p) {
            p->setStateInformation(data, sizeInBytes);
        }
    }

    void suspendProcessing(const bool shouldBeSuspended) {
        auto p = getPlugin();
        if (nullptr != p) {
            p->suspendProcessing(shouldBeSuspended);
        }
    }

  private:
    ProcessorChain& m_chain;
    String m_id;
    double m_sampleRate;
    int m_blockSize;
    std::shared_ptr<AudioPluginInstance> m_plugin;
    std::mutex m_pluginMtx;
    int m_additionalScreenSpace = 0;

    static std::mutex m_pluginLoaderMtx;
};

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

    bool initPluginInstance(std::shared_ptr<AudioPluginInstance> processor);
    bool addPluginProcessor(const String& id);
    void addProcessor(std::shared_ptr<AGProcessor> processor);
    size_t getSize() const { return m_processors.size(); }
    std::shared_ptr<AGProcessor> getProcessor(int index);

    void delProcessor(int idx);
    void exchangeProcessors(int idxA, int idxB);

    float getParameterValue(int idx, int paramIdx);

    void update();

    void clear();

    String toString();

  private:
    std::vector<std::shared_ptr<AGProcessor>> m_processors;
    std::mutex m_processors_mtx;

    std::atomic_bool m_supportsDoublePrecission{true};
    std::atomic<double> m_tailSecs{0.0};

    int m_extraChannels = 0;

    template <typename T>
    void processBlockReal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
        int latency = 0;
        std::lock_guard<std::mutex> lock(m_processors_mtx);
        for (auto& proc : m_processors) {
            if (proc->processBlock(buffer, midiMessages)) {
                latency += proc->getLatencySamples();
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
