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

namespace e47 {

class ProcessorChain;

class AGProcessor : public LogTagDelegate, public AudioProcessorParameter::Listener {
  public:
    static std::atomic_uint32_t loadedCount;

    AGProcessor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize);
    ~AGProcessor() override;

    static String createPluginID(const PluginDescription& d);
    static String convertJUCEtoAGPluginID(const String& id);

    inline static String createString(const PluginDescription& d) {
        json j = {{"name", d.name.toStdString()},          {"company", d.manufacturerName.toStdString()},
                  {"id", createPluginID(d).toStdString()}, {"type", d.pluginFormatName.toStdString()},
                  {"category", d.category.toStdString()},  {"isInstrument", d.isInstrument}};
        return String(j.dump()) + "\n";
    }

    static std::unique_ptr<PluginDescription> findPluginDescritpion(const String& id);

    std::shared_ptr<AudioPluginInstance> getPlugin() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        return m_plugin;
    }

    void updateScreenCaptureArea(int val) {
        traceScope();
        if (val == Defaults::SCAREA_FULLSCREEN) {
            m_fullscreen = !m_fullscreen;
        } else {
            m_additionalScreenSpace = m_additionalScreenSpace + val > 0 ? m_additionalScreenSpace + val : 0;
        }
    }

    int getAdditionalScreenCapturingSpace() {
        traceScope();
        return m_additionalScreenSpace;
    }

    bool isFullscreen() {
        traceScope();
        return m_fullscreen;
    }

    static std::shared_ptr<AudioPluginInstance> loadPlugin(PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize, String& err);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize, String& err);

    bool load(String& err);
    void unload();

    void setChainIndex(int idx) { m_chainIdx = idx; }

    template <typename T>
    bool processBlock(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            if (!p->isSuspended()) {
                p->processBlock(buffer, midiMessages);
            } else {
                if (m_lastKnownLatency > 0) {
                    processBlockBypassed(buffer);
                }
            }
            return true;
        }
        return false;
    }

    void processBlockBypassed(AudioBuffer<float>& buffer);
    void processBlockBypassed(AudioBuffer<double>& buffer);

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            p->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
            m_prepared = true;
        }
    }

    void releaseResources() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            p->releaseResources();
            m_prepared = false;
        }
    }

    int getLatencySamples() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            int latency = p->getLatencySamples();
            if (latency != m_lastKnownLatency) {
                m_lastKnownLatency = latency;
                updateLatencyBuffers();
            }
            return latency;
        }
        return 0;
    }

    const String getName() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getName();
        }
        return "";
    }

    bool hasEditor() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->hasEditor();
        }
        return false;
    }

    bool isSuspended() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->isSuspended();
        }
        return true;
    }

    double getTailLengthSeconds() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getTailLengthSeconds();
        }
        return 0.0;
    }

    AudioProcessorEditor* createEditorIfNeeded() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->createEditorIfNeeded();
        }
        return nullptr;
    }

    AudioProcessorEditor* getActiveEditor() {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            return p->getActiveEditor();
        }
        return nullptr;
    }

    void getStateInformation(juce::MemoryBlock& destData) {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            p->getStateInformation(destData);
        }
    }

    void setStateInformation(const void* data, int sizeInBytes) {
        traceScope();
        auto p = getPlugin();
        if (nullptr != p) {
            p->setStateInformation(data, sizeInBytes);
        }
    }

    void suspendProcessing(const bool shouldBeSuspended);
    void updateLatencyBuffers();

    int getExtraInChannels() const { return m_extraInChannels; }
    int getExtraOutChannels() const { return m_extraOutChannels; }
    void setExtraChannels(int in, int out) {
        m_extraInChannels = in;
        m_extraOutChannels = out;
    }

    bool getNeedsDisabledSidechain() const { return m_needsDisabledSidechain; }
    void setNeedsDisabledSidechain(bool b) { m_needsDisabledSidechain = b; }

    Point<int> getLastPosition() const { return m_lastPosition; }
    void setLastPosition(Point<int> p) { m_lastPosition = p; }

    // AudioProcessorParameter::Listener
    std::function<void(int idx, int paramIdx, float val)> onParamValueChange;
    std::function<void(int idx, int paramIdx, bool gestureIsStarting)> onParamGestureChange;

    void parameterValueChanged(int parameterIndex, float newValue) override {
        traceScope();
        if (onParamValueChange) {
            onParamValueChange(m_chainIdx, parameterIndex, newValue);
        }
    }

    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {
        traceScope();
        if (onParamGestureChange) {
            onParamGestureChange(m_chainIdx, parameterIndex, gestureIsStarting);
        }
    }

  private:
    ProcessorChain& m_chain;
    int m_chainIdx = -1;
    String m_id;
    double m_sampleRate;
    int m_blockSize;
    bool m_parallelLoadAllowed;
    static std::mutex m_pluginLoaderMtx;
    std::shared_ptr<AudioPluginInstance> m_plugin;
    std::mutex m_pluginMtx;
    int m_additionalScreenSpace = 0;
    bool m_fullscreen = false;
    bool m_prepared = false;
    int m_extraInChannels = 0;
    int m_extraOutChannels = 0;
    bool m_needsDisabledSidechain = false;
    Array<Array<float>> m_bypassBufferF;
    Array<Array<double>> m_bypassBufferD;
    int m_lastKnownLatency = 0;
    Point<int> m_lastPosition = {0, 0};
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

    bool isSidechainDisabled() const { return m_sidechainDisabled; }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override;
    const String getName() const override { return "ProcessorChain"; }
    double getTailLengthSeconds() const override;
    bool supportsDoublePrecisionProcessing() const override;
    bool isBusesLayoutSupported(const BusesLayout& /*layouts*/) const override { return true; }

    bool updateChannels(int channelsIn, int channelsOut, int channelsSC);
    bool setProcessorBusesLayout(AGProcessor* proc);
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

    bool initPluginInstance(AGProcessor* proc, String& err);
    bool addPluginProcessor(const String& id, String& err);
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
    bool m_hasSidechain = false;
    bool m_sidechainDisabled = false;

    template <typename T>
    void processBlockReal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
        traceScope();
        int latency = 0;
        if (getBusCount(true) > 1 && m_sidechainDisabled) {
            auto sidechainBuffer = getBusBuffer(buffer, true, 1);
            sidechainBuffer.clear();
        }
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
        traceScope();
        MidiBuffer midi;
        int channels = jmax(getTotalNumInputChannels(), getTotalNumOutputChannels()) + m_extraChannels;
        AudioBuffer<T> buf(channels, getBlockSize());
        buf.clear();
        int samplesProcessed = 0;
        do {
            inst->processBlock(buf, midi);
            samplesProcessed += getBlockSize();
        } while (samplesProcessed < 16384);
    }

    void updateNoLock();

    void printBusesLayout(const AudioProcessor::BusesLayout& l) const {
        logln("input buses: " << l.inputBuses.size());
        for (int i = 0; i < l.inputBuses.size(); i++) {
            logln("  [" << i << "] " << l.inputBuses[i].size() << " channel(s)");
            for (auto ct : l.inputBuses[i].getChannelTypes()) {
                logln("    <- " << AudioChannelSet::getAbbreviatedChannelTypeName(ct));
            }
        }
        logln("output buses: " << l.outputBuses.size());
        for (int i = 0; i < l.outputBuses.size(); i++) {
            logln("  [" << i << "] " << l.outputBuses[i].size() << " channel(s)");
            for (auto ct : l.outputBuses[i].getChannelTypes()) {
                logln("    -> " << AudioChannelSet::getAbbreviatedChannelTypeName(ct));
            }
        }
    }
};

}  // namespace e47

#endif /* ProcessorChain_hpp */
