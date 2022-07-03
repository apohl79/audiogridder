/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _PROCESSOR_HPP_
#define _PROCESSOR_HPP_

#include "Utils.hpp"
#include "Defaults.hpp"
#include "ProcessorClient.hpp"
#include "ParameterValue.hpp"
#include "ProcessorWindow.hpp"
#include "AudioRingBuffer.hpp"

namespace e47 {

class ProcessorChain;
class SandboxPluginTest;

class Processor : public LogTagDelegate, public std::enable_shared_from_this<Processor> {
  public:
    static std::atomic_uint32_t loadedCount;

    Processor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize);
    Processor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize, bool isClient);
    ~Processor() override;

    //
    // ID calculation methods
    //
    static String createPluginID(const PluginDescription& d);
    // These are for some backwards compatibility
    static String createPluginIDWithName(const PluginDescription& d);
    static String createPluginIDDepricated(const PluginDescription& d);
    static String convertJUCEtoAGPluginID(const String& id);

    inline static json createJson(const PluginDescription& d) {
        return {{"name", d.name.toStdString()},
                {"company", d.manufacturerName.toStdString()},
                {"id", createPluginIDDepricated(d).toStdString()},
                {"id2", createPluginID(d).toStdString()},
                {"type", d.pluginFormatName.toStdString()},
                {"category", d.category.toStdString()},
                {"isInstrument", d.isInstrument}};
    }

    static std::unique_ptr<PluginDescription> findPluginDescritpion(const String& id, String* idNormalized = nullptr);
    static std::unique_ptr<PluginDescription> findPluginDescritpion(const String& id, const KnownPluginList& pluglist,
                                                                    String* idNormalized = nullptr);

    static Array<AudioProcessor::BusesLayout> findSupportedLayouts(std::shared_ptr<AudioPluginInstance> proc,
                                                                   bool checkOnly = true, int srvId = 0);
    static Array<AudioProcessor::BusesLayout> findSupportedLayouts(Processor* proc, bool checkOnly = true,
                                                                   int srvId = 0);

    const Array<AudioProcessor::BusesLayout>& getSupportedBusLayouts() const;

    bool isClient() const { return m_isClient; }

    void updateScreenCaptureArea(int val);
    int getAdditionalScreenCapturingSpace();
    bool isFullscreen();

    static std::shared_ptr<AudioPluginInstance> loadPlugin(const PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize, String& err);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize, String& err, String* idNormalized = nullptr);

    bool load(const String& settings, const String& layout, uint64 monoChannels, String& err,
              const PluginDescription* plugdesc = nullptr);
    void unload();
    bool isLoaded();

    void setChainIndex(int idx) { m_chainIdx = idx; }

    const String& getPluginId() const { return m_id; }

    bool processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages);
    bool processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages);

    void processBlockBypassed(AudioBuffer<float>& buffer);
    void processBlockBypassed(AudioBuffer<double>& buffer);

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
    void releaseResources();
    int getLatencySamples();
    void suspendProcessing(const bool shouldBeSuspended);
    void updateLatencyBuffers();
    void enableAllBuses();
    void setProcessingPrecision(AudioProcessor::ProcessingPrecision p);
    void setMonoChannels(uint64 channels);
    bool isMonoChannelActive(int ch);

    const String& getLayout() const { return m_layout; }
    int getExtraInChannels() const { return m_extraInChannels; }
    int getExtraOutChannels() const { return m_extraOutChannels; }
    void setExtraChannels(int in, int out);

    bool getNeedsDisabledSidechain() const { return m_needsDisabledSidechain; }
    void setNeedsDisabledSidechain(bool b) { m_needsDisabledSidechain = b; }

    // Non client methods to create a plugin UI
    AudioProcessorEditor* createEditorIfNeeded();
    AudioProcessorEditor* getActiveEditor();
    std::shared_ptr<ProcessorWindow> getOrCreateEditorWindow(Thread::ThreadID tid,
                                                             ProcessorWindow::CaptureCallbackFFmpeg func,
                                                             std::function<void()> onHide, int x, int y);
    std::shared_ptr<ProcessorWindow> getOrCreateEditorWindow(Thread::ThreadID tid,
                                                             ProcessorWindow::CaptureCallbackNative func,
                                                             std::function<void()> onHide, int x, int y);
    std::shared_ptr<ProcessorWindow> recreateEditorWindow();
    std::shared_ptr<ProcessorWindow> getEditorWindow() const { return m_windows[getWindowIndex()]; }
    void resetEditorWindow() { m_windows[getWindowIndex()].reset(); }
    void setActiveWindowChannel(int c) {
        logln("setting active window channel to " << c);
        m_activeWindowChannel = c;
    }
    int getActiveWindowChannel() const { return m_activeWindowChannel; }

    // Client methods to tell the sandbox to create the plugin UI
    void showEditor(int x, int y);
    void hideEditor();

    Point<int> getLastPosition() const { return m_lastPosition; }
    void setLastPosition(Point<int> p) { m_lastPosition = p; }
    juce::Rectangle<int> getScreenBounds();

    // AudioProcessorParameter::Listener
    using ParamValueChangeCallback = std::function<void(int idx, int channel, int paramIdx, float val)>;
    using ParamGestureChangeCallback = std::function<void(int idx, int channel, int paramIdx, float val)>;
    using KeysFromSandboxCallback = std::function<void(Message<Key>&)>;
    using StatusChangeFromSandbox = std::function<void(int idx, bool ok, const String& err)>;

    ParamValueChangeCallback onParamValueChange;
    ParamGestureChangeCallback onParamGestureChange;
    KeysFromSandboxCallback onKeysFromSandbox;
    StatusChangeFromSandbox onStatusChangeFromSandbox;

    void setCallbacks(ParamValueChangeCallback valueChangeFn, ParamGestureChangeCallback gestureChangeFn,
                      KeysFromSandboxCallback keysFn, StatusChangeFromSandbox statusChangeFn);

    json getParameters();
    void setParameterValue(int channel, int paramIdx, float value);
    float getParameterValue(int channel, int paramIdx);

    std::vector<Srv::ParameterValue> getAllParamaterValues();

    const String getName();
    bool hasEditor();
    bool supportsDoublePrecisionProcessing();
    bool isSuspended();
    double getTailLengthSeconds();
    void getStateInformation(String& settings);
    void setStateInformation(const String& settings);
    bool checkBusesLayoutSupported(const AudioProcessor::BusesLayout& layout);
    bool setBusesLayout(const AudioProcessor::BusesLayout& layout);
    AudioProcessor::BusesLayout getBusesLayout();
    int getBusCount(bool isInput);
    bool canAddBus(bool isInput);
    bool canRemoveBus(bool isInput);
    bool addBus(bool isInput);
    bool removeBus(bool isInput);
    void setPlayHead(AudioPlayHead* phead);
    int getNumPrograms();
    void setCurrentProgram(int channel, int idx);
    const String getProgramName(int idx);
    int getTotalNumOutputChannels();

    int getChannelInstances();

  private:
    friend class SandboxPluginTest;

    struct Listener : AudioProcessorParameter::Listener {
        Processor* proc;
        int channel;

        Listener(Processor* p, int c) : proc(p), channel(c) {}

        void parameterValueChanged(int parameterIndex, float newValue) override {
            if (proc->onParamValueChange) {
                proc->onParamValueChange(proc->m_chainIdx, channel, parameterIndex, newValue);
            }
        }

        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {
            if (proc->onParamGestureChange) {
                proc->onParamGestureChange(proc->m_chainIdx, channel, parameterIndex, gestureIsStarting);
            }
        }
    };

    ProcessorChain& m_chain;
    int m_chainIdx = -1;
    String m_id;
    String m_idNormalized;
    double m_sampleRate;
    int m_blockSize;
    bool m_isClient;
    std::shared_ptr<ProcessorClient> m_client;
    std::vector<std::shared_ptr<AudioPluginInstance>> m_plugins;
    std::vector<std::shared_ptr<ProcessorWindow>> m_windows;
    std::vector<std::unique_ptr<Listener>> m_listners;
    std::vector<std::unique_ptr<AudioRingBuffer<float>>> m_multiMonoBypassBuffersF;
    std::vector<std::unique_ptr<AudioRingBuffer<double>>> m_multiMonoBypassBuffersD;
    std::mutex m_pluginMtx;
    std::atomic_int m_activeWindowChannel{0};
    int m_additionalScreenSpace = 0;
    bool m_fullscreen = false;
    bool m_prepared = false;
    String m_name;
    String m_layout;
    ChannelSet m_monoChannels;
    std::mutex m_monoChannelsMtx;
    int m_channels = 1;
    int m_extraInChannels = 0;
    int m_extraOutChannels = 0;
    bool m_needsDisabledSidechain = false;
    AudioRingBuffer<float> m_bypassBufferF;
    AudioRingBuffer<double> m_bypassBufferD;
    int m_lastKnownLatency = 0;
    Point<int> m_lastPosition = {0, 0};

    enum FormatType { VST, VST3, AU };
    FormatType m_fmt;

    std::shared_ptr<AudioPluginInstance> getPlugin(int channel) {
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        if ((size_t)channel < m_plugins.size()) {
            return m_plugins[(size_t)channel];
        }
        return nullptr;
    }

    std::shared_ptr<ProcessorClient> getClient() {
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        return m_client;
    }

    template <typename T>
    bool processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages);

    template <typename T>
    void processBlockBypassedInternal(AudioBuffer<T>& buffer, AudioRingBuffer<T>& bypassBuffer);

    template <typename T>
    void processBlockBypassedMultiMonoInternal(AudioBuffer<T>& buffer, AudioRingBuffer<T>& bypassBuffer);

    void processBlockBypassedMultiMono(AudioBuffer<float>& buffer, int ch);
    void processBlockBypassedMultiMono(AudioBuffer<double>& buffer, int ch);

    template <typename T>
    std::shared_ptr<ProcessorWindow> getOrCreateEditorWindowInternal(Thread::ThreadID tid, T func,
                                                                     std::function<void()> onHide, int x, int y);

    inline size_t getWindowIndex() const { return (size_t)(m_channels > 1 ? m_activeWindowChannel.load() : 0); }

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif  // _PROCESSOR_HPP_
