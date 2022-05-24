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

namespace e47 {

class ProcessorChain;
class SandboxPluginTest;

class Processor : public LogTagDelegate, public AudioProcessorParameter::Listener {
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

    inline static String createString(const PluginDescription& d) {
        json j = {{"name", d.name.toStdString()},
                  {"company", d.manufacturerName.toStdString()},
                  {"id", createPluginIDDepricated(d).toStdString()},
                  {"id2", createPluginID(d).toStdString()},
                  {"type", d.pluginFormatName.toStdString()},
                  {"category", d.category.toStdString()},
                  {"isInstrument", d.isInstrument}};
        return String(j.dump());
    }

    static std::unique_ptr<PluginDescription> findPluginDescritpion(const String& id);
    static std::unique_ptr<PluginDescription> findPluginDescritpion(const String& id, const KnownPluginList& pluglist);

    bool isClient() const { return m_isClient; }

    void updateScreenCaptureArea(int val);
    int getAdditionalScreenCapturingSpace();
    bool isFullscreen();

    static std::shared_ptr<AudioPluginInstance> loadPlugin(const PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize, String& err);
    static std::shared_ptr<AudioPluginInstance> loadPlugin(const String& fileOrIdentifier, double sampleRate,
                                                           int blockSize, String& err);

    bool load(const String& settings, String& err, const PluginDescription* plugdesc = nullptr);
    void unload();
    bool isLoaded(std::shared_ptr<AudioPluginInstance>* plugin = nullptr,
                  std::shared_ptr<ProcessorClient>* client = nullptr);

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

    int getExtraInChannels() const { return m_extraInChannels; }
    int getExtraOutChannels() const { return m_extraOutChannels; }
    void setExtraChannels(int in, int out);

    bool getNeedsDisabledSidechain() const { return m_needsDisabledSidechain; }
    void setNeedsDisabledSidechain(bool b) { m_needsDisabledSidechain = b; }

    // Non client methods to create a plugin UI
    AudioProcessorEditor* createEditorIfNeeded();
    AudioProcessorEditor* getActiveEditor();

    // Client methods to tell the sandbox to create the plugin UI
    void showEditor(int x, int y);
    void hideEditor();

    Point<int> getLastPosition() const { return m_lastPosition; }
    void setLastPosition(Point<int> p) { m_lastPosition = p; }
    juce::Rectangle<int> getScreenBounds();

    // AudioProcessorParameter::Listener
    using ParamValueChangeCallback = std::function<void(int idx, int paramIdx, float val)>;
    using ParamGestureChangeCallback = std::function<void(int idx, int paramIdx, float val)>;
    using KeysFromSandboxCallback = std::function<void(Message<Key>&)>;
    using StatusChangeFromSandbox = std::function<void(int idx, bool ok, const String& err)>;

    ParamValueChangeCallback onParamValueChange;
    ParamGestureChangeCallback onParamGestureChange;
    KeysFromSandboxCallback onKeysFromSandbox;
    StatusChangeFromSandbox onStatusChangeFromSandbox;

    void setCallbacks(ParamValueChangeCallback valueChangeFn, ParamGestureChangeCallback gestureChangeFn,
                      KeysFromSandboxCallback keysFn, StatusChangeFromSandbox statusChangeFn);

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

    json getParameters();
    void setParameterValue(int paramIdx, float value);
    float getParameterValue(int paramIdx);

    std::vector<Srv::ParameterValue> getAllParamaterValues();

#define callBackend(method)            \
    do {                               \
        if (isLoaded()) {              \
            if (m_isClient) {          \
                getClient()->method(); \
            } else {                   \
                getPlugin()->method(); \
            }                          \
        }                              \
    } while (0)

#define callBackendWithReturn(method)         \
    do {                                      \
        if (isLoaded()) {                     \
            if (m_isClient) {                 \
                return getClient()->method(); \
            } else {                          \
                return getPlugin()->method(); \
            }                                 \
        }                                     \
    } while (0)

#define callBackendWithArgs1(method, arg1) \
    do {                                   \
        if (isLoaded()) {                  \
            if (m_isClient) {              \
                getClient()->method(arg1); \
            } else {                       \
                getPlugin()->method(arg1); \
            }                              \
        }                                  \
    } while (0)

#define callBackendWithArgs1MT(method, arg1)                            \
    do {                                                                \
        if (isLoaded()) {                                               \
            if (m_isClient) {                                           \
                getClient()->method(arg1);                              \
            } else {                                                    \
                runOnMsgThreadSync([&] { getPlugin()->method(arg1); }); \
            }                                                           \
        }                                                               \
    } while (0)

#define callBackendWithArgs2(method, arg1, arg2) \
    do {                                         \
        if (isLoaded()) {                        \
            if (m_isClient) {                    \
                getClient()->method(arg1, arg2); \
            } else {                             \
                getPlugin()->method(arg1, arg2); \
            }                                    \
        }                                        \
    } while (0)

#define callBackendWithArgs2MT(method, arg1, arg2)                            \
    do {                                                                      \
        if (isLoaded()) {                                                     \
            if (m_isClient) {                                                 \
                getClient()->method(arg1, arg2);                              \
            } else {                                                          \
                runOnMsgThreadSync([&] { getPlugin()->method(arg1, arg2); }); \
            }                                                                 \
        }                                                                     \
    } while (0)

#define callBackendWithArgs1Return(method, arg1)  \
    do {                                          \
        if (isLoaded()) {                         \
            if (m_isClient) {                     \
                return getClient()->method(arg1); \
            } else {                              \
                return getPlugin()->method(arg1); \
            }                                     \
        }                                         \
    } while (0)

#define backendMethod(method)              \
    void method() { callBackend(method); } \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithReturn(method, ReturnType, defaultReturn) \
    ReturnType method() {                                          \
        callBackendWithReturn(method);                             \
        return defaultReturn;                                      \
    }                                                              \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithReturnMT(method, ReturnType, defaultReturn)      \
    ReturnType method() {                                                 \
        if (isLoaded()) {                                                 \
            if (m_isClient) {                                             \
                return getClient()->method();                             \
            } else {                                                      \
                ReturnType ret;                                           \
                runOnMsgThreadSync([&] { ret = getPlugin()->method(); }); \
                return ret;                                               \
            }                                                             \
        }                                                                 \
        return defaultReturn;                                             \
    }                                                                     \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithReturnRef(method, ReturnType) \
    ReturnType& method() {                             \
        callBackendWithReturn(method);                 \
        static ReturnType ret;                         \
        return ret;                                    \
    }                                                  \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithArgs1(method, Arg1Type)                       \
    void method(Arg1Type arg1) { callBackendWithArgs1(method, arg1); } \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithArgs1MT(method, Arg1Type)                       \
    void method(Arg1Type arg1) { callBackendWithArgs1MT(method, arg1); } \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithArgs2(method, Arg1Type, Arg2Type)                                  \
    void method(Arg1Type arg1, Arg2Type arg2) { callBackendWithArgs2(method, arg1, arg2); } \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithArgs2MT(method, Arg1Type, Arg2Type)                                  \
    void method(Arg1Type arg1, Arg2Type arg2) { callBackendWithArgs2MT(method, arg1, arg2); } \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

#define backendMethodWithReturnArgs1(method, ReturnType, defaultReturn, Arg1Type) \
    ReturnType method(Arg1Type arg1) {                                            \
        callBackendWithArgs1Return(method, arg1);                                 \
        return defaultReturn;                                                     \
    }                                                                             \
    static constexpr char _createUniqueVar(__forceColon, __COUNTER__) = ' '

    backendMethodWithReturn(getName, const String, {});
    backendMethodWithReturnMT(hasEditor, bool, false);
    backendMethodWithReturn(supportsDoublePrecisionProcessing, bool, false);
    backendMethodWithReturn(isSuspended, bool, true);
    backendMethodWithReturn(getTailLengthSeconds, double, 0.0);
    backendMethodWithArgs1MT(getStateInformation, juce::MemoryBlock&);
    backendMethodWithArgs2MT(setStateInformation, const void*, int);
    backendMethodWithReturnArgs1(checkBusesLayoutSupported, bool, false, const AudioProcessor::BusesLayout&);
    backendMethodWithReturnArgs1(setBusesLayout, bool, false, const AudioProcessor::BusesLayout&);
    backendMethodWithReturn(getBusesLayout, AudioProcessor::BusesLayout, {});
    backendMethodWithArgs1(setPlayHead, AudioPlayHead*);
    backendMethodWithReturn(getNumPrograms, int, 1);
    backendMethodWithReturnArgs1(getProgramName, const String, {}, int);
    backendMethodWithArgs1(setCurrentProgram, int);
    backendMethodWithReturn(getTotalNumOutputChannels, int, 0);

  private:
    friend class SandboxPluginTest;

    ProcessorChain& m_chain;
    int m_chainIdx = -1;
    String m_id;
    double m_sampleRate;
    int m_blockSize;
    bool m_isClient;
    std::shared_ptr<AudioPluginInstance> m_plugin;
    std::shared_ptr<ProcessorClient> m_client;
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

    enum FormatType { VST, VST3, AU };
    FormatType m_fmt;

    std::shared_ptr<AudioPluginInstance> getPlugin() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        return m_plugin;
    }

    std::shared_ptr<ProcessorClient> getClient() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_pluginMtx);
        return m_client;
    }

    template <typename T>
    bool processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages);

    template <typename T>
    void processBlockBypassedInternal(AudioBuffer<T>& buffer, Array<Array<T>>& bypassBuffer);

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif  // _PROCESSOR_HPP_
