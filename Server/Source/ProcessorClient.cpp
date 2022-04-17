/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ProcessorClient.hpp"
#include "Message.hpp"
#include "App.hpp"
#include "Server.hpp"
#include "Defaults.hpp"

namespace e47 {

std::unordered_set<int> ProcessorClient::m_workerPorts;
std::mutex ProcessorClient::m_workerPortsMtx;

bool ProcessorClient::init() {
    traceScope();
    if (startSandbox()) {
        if (!connectSandbox()) {
            if (m_process.isRunning()) {
                logln("error: failed to connect to sandbox process (retrying in a bit)");
                m_process.kill();
                return true;
            } else {
                logln("fatal error: failed to connect to sandbox process (process died, giving up)");
                return false;
            }
        }
    } else {
        logln("fatal error: failed to start sandbox process");
        return false;
    }
    return true;
}

void ProcessorClient::shutdown() {
    logln("shutting down sandbox");

    signalThreadShouldExit();

    std::lock_guard<std::mutex> lock(m_mtx);

    if (nullptr != m_sockCmdOut) {
        if (m_sockCmdOut->isConnected()) {
            m_sockCmdOut->close();
        }
        m_sockCmdOut.reset();
    }

    if (nullptr != m_sockCmdIn) {
        if (m_sockCmdIn->isConnected()) {
            m_sockCmdIn->close();
        }
        m_sockCmdIn.reset();
    }

    if (nullptr != m_sockAudio) {
        if (m_sockAudio->isConnected()) {
            m_sockAudio->close();
        }
        m_sockAudio.reset();
    }

    if (m_process.isRunning()) {
        m_process.kill();
    }
}

bool ProcessorClient::isOk() {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_process.isRunning() && nullptr != m_sockCmdIn && m_sockCmdIn->isConnected() &&
           nullptr != m_sockCmdOut & m_sockCmdOut->isConnected() && nullptr != m_sockAudio &&
           m_sockAudio->isConnected();
}

int ProcessorClient::getWorkerPort() {
    std::lock_guard<std::mutex> lock(m_workerPortsMtx);
    int port = Defaults::SANDBOX_PLUGIN_PORT;
    while (m_workerPorts.count(port) > 0) {
        port++;
    }
    m_workerPorts.insert(port);
    return port;
}

void ProcessorClient::removeWorkerPort(int port) {
    std::lock_guard<std::mutex> lock(m_workerPortsMtx);
    m_workerPorts.erase(port);
}

bool ProcessorClient::startSandbox() {
    std::lock_guard<std::mutex> lock(m_mtx);

    if (m_process.isRunning()) {
        logln("killing already running sandbox");
        m_process.kill();
        m_process.waitForProcessToFinish(-1);
    }

    auto cfgDump = m_cfg.toJson().dump();
    MemoryBlock config(cfgDump.c_str(), cfgDump.size());

    StringArray args;

#ifndef AG_UNIT_TESTS
    args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    args.addArray({"-id", String(getApp()->getServer()->getId())});
#else
    auto exe = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory();
#if JUCE_WINDOWS
    exe = exe.getChildFile("AudioGridderServer.exe");
#else
    exe = exe.getChildFile("AudioGridderServer.app")
              .getChildFile("Contents")
              .getChildFile("MacOS")
              .getChildFile("AudioGridderServer");
#endif
    // args.add("lldb");
    args.add(exe.getFullPathName());
    // args.addArray({"-o", "process launch --tty", "--"});
    args.addArray({"-id", "999"});
#endif

    args.add("-load");
    args.addArray({"-pluginid", m_id});
    args.addArray({"-workerport", String(m_port)});
    args.addArray({"-config", config.toBase64Encoding()});

    logln("starting sandbox process: " << args.joinIntoString(" "));

    return m_process.start(args, 0);
}

bool ProcessorClient::connectSandbox() {
    std::lock_guard<std::mutex> lock(m_mtx);

    logln("connecting to sandbox at port " << m_port);

    bool hasUnixDomainSockets = Defaults::unixDomainSocketsSupported();
    auto socketPath = Defaults::getSocketPath(Defaults::SANDBOX_PLUGIN_SOCK, {{"n", String(m_port)}});

    m_sockCmdOut = std::make_unique<StreamingSocket>();

    // let the process come up and bind to the port
    int maxTries = 50;
    while (!m_sockCmdOut->isConnected() && maxTries-- > 0 && m_process.isRunning()) {
        if (hasUnixDomainSockets) {
            if (!m_sockCmdOut->connect(socketPath, 100)) {
                sleep(100);
            }
        } else {
            if (!m_sockCmdOut->connect("127.0.0.1", m_port, 100)) {
                sleep(100);
            }
        }
    }

    if (!m_sockCmdOut->isConnected()) {
        logln("failed to setup sandbox command-out connection");
        return false;
    }

    m_sockCmdIn = std::make_unique<StreamingSocket>();
    if (hasUnixDomainSockets) {
        if (!m_sockCmdIn->connect(socketPath)) {
            logln("failed to setup sandbox command-in connection");
            return false;
        }
    } else {
        if (!m_sockCmdIn->connect("127.0.0.1", m_port)) {
            logln("failed to setup sandbox command-in connection");
            return false;
        }
    }

    m_sockAudio = std::make_unique<StreamingSocket>();
    if (hasUnixDomainSockets) {
        if (!m_sockAudio->connect(socketPath)) {
            logln("failed to setup sandbox audio connection");
            return false;
        }
    } else {
        if (!m_sockAudio->connect("127.0.0.1", m_port)) {
            logln("failed to setup sandbox audio connection");
            return false;
        }
    }

    m_bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
    m_bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");

    logln("connected to sandbox successfully");

    return true;
}

void ProcessorClient::run() {
    traceScope();
    MessageFactory msgFactory(this);

    while (!threadShouldExit()) {
        if (!isOk()) {
            if (!init()) {
                return;
            }
            if (!isOk()) {
                sleepExitAware(1000);
                continue;
            }
            if (m_loaded) {
                String err;
                if (!load(m_lastSettings, err)) {
                    logln("reload failed: " << err);
                }
            }
        }

        TimeStatistic::Timeout timeout(1000);
        while (timeout.getMillisecondsLeft() > 0 && !threadShouldExit()) {
            MessageHelper::Error err;
            auto msg = msgFactory.getNextMessage(m_sockCmdIn.get(), &err, 100);
            if (nullptr != msg) {
                switch (msg->getType()) {
                    case Key::Type:
                        handleMessage(Message<Any>::convert<Key>(msg));
                        break;
                    case ParameterValue::Type:
                        handleMessage(Message<Any>::convert<ParameterValue>(msg));
                        break;
                    case ParameterGesture::Type:
                        handleMessage(Message<Any>::convert<ParameterGesture>(msg));
                        break;
                    default:
                        logln("unknown message type " << msg->getType());
                }
            }
        }
    }
}

void ProcessorClient::handleMessage(std::shared_ptr<Message<Key>> msg) {
    traceScope();
    if (nullptr != onKeysFromSandbox) {
        onKeysFromSandbox(*msg);
    }
}

void ProcessorClient::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    traceScope();
    if (nullptr != onParamValueChange) {
        onParamValueChange(pDATA(msg)->paramIdx, pDATA(msg)->value);
    }
}

void ProcessorClient::handleMessage(std::shared_ptr<Message<ParameterGesture>> msg) {
    traceScope();
    if (nullptr != onParamGestureChange) {
        onParamGestureChange(pDATA(msg)->paramIdx, pDATA(msg)->gestureIsStarting);
    }
}

bool ProcessorClient::load(const String& settings, String& err) {
    traceScope();

    if (!isOk()) {
        err = "load failed: client not ok";
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mtx);

    TimeStatistic::Timeout timeout(15000);
    MessageHelper::Error e;

    Message<AddPlugin> msgAddPlugin(this);
    PLD(msgAddPlugin).setJson({{"id", m_id.toStdString()}, {"settings", settings.toStdString()}});

    if (msgAddPlugin.send(m_sockCmdOut.get())) {
        Message<AddPluginResult> msgResult(this);
        if (!msgResult.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to get result: " + e.toString();
            logln(err);
            return false;
        }
        auto jresult = msgResult.payload.getJson();
        if (!jresult["success"].get<bool>()) {
            err = jresult["err"].get<std::string>();
            logln(err);
            return false;
        }
        if (timeout.getMillisecondsLeft() == 0) {
            err = "timeout";
            logln(err);
            return false;
        }
        Message<Presets> msgPresets(this);
        if (!msgPresets.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read presets: " + e.toString();
            logln(err);
            return false;
        }
        m_presets = StringArray::fromTokens(msgPresets.payload.getString(), "|", "");
        if (timeout.getMillisecondsLeft() == 0) {
            err = "timeout";
            logln(err);
            return false;
        }
        Message<Parameters> msgParams(this);
        if (!msgParams.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read parameters: " + e.toString();
            logln(err);
            return false;
        }
        m_parameters = msgParams.payload.getJson();
        m_name = jresult["name"].get<std::string>();
        m_latency = jresult["latency"].get<int>();
        m_hasEditor = jresult["hasEditor"].get<bool>();
        m_scDisabled = jresult["disabledSideChain"].get<bool>();
        m_supportsDoublePrecision = jresult["supportsDoublePrecision"].get<bool>();
        m_tailSeconds = jresult["tailSeconds"].get<double>();
        m_numOutputChannels = jresult["numOutputChannels"].get<int>();
        m_lastSettings = settings;
        m_loaded = true;
        return true;
    } else {
        err = "load failed: send failed";
    }

    return false;
}

void ProcessorClient::unload() {
    m_loaded = false;

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<DelPlugin> msg(this);
    PLD(msg).setNumber(0);
    msg.send(m_sockCmdOut.get());

    MessageFactory msgFactory(this);
    auto result = msgFactory.getResult(m_sockCmdOut.get());
    if (nullptr != result && result->getReturnCode() > -1) {
        m_latency = result->getReturnCode();
    }
}

const String ProcessorClient::getName() { return m_name; }

bool ProcessorClient::hasEditor() { return m_hasEditor; }

void ProcessorClient::showEditor(int x, int y) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<EditPlugin> msg(this);
    DATA(msg)->index = 0;
    DATA(msg)->x = x;
    DATA(msg)->y = y;
    msg.send(m_sockCmdOut.get());
}

void ProcessorClient::hideEditor() {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<HidePlugin> msg(this);
    msg.send(m_sockCmdOut.get());
}

bool ProcessorClient::supportsDoublePrecisionProcessing() { return m_supportsDoublePrecision; }

bool ProcessorClient::isSuspended() { return m_suspended; }

double ProcessorClient::getTailLengthSeconds() { return m_tailSeconds; }

void ProcessorClient::getStateInformation(juce::MemoryBlock& block) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<GetPluginSettings> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("getStateInformation failed: can't send message");
        return;
    }

    Message<PluginSettings> res(this);
    MessageHelper::Error err;
    if (res.read(m_sockCmdOut.get(), &err, 5000)) {
        if (*res.payload.size > 0) {
            block.append(res.payload.data, (size_t)*res.payload.size);
            m_lastSettings = block.toBase64Encoding();
        }
    } else {
        logln("getStateInformation failed: failed to read PluginSettings message: " << err.toString());
        return;
    }
}

void ProcessorClient::setStateInformation(const void* data, int size) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<SetPluginSettings> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("setStateInformation failed: can't send announcement message");
        return;
    }

    Message<PluginSettings> msgSettings(this);
    msgSettings.payload.setData((const char*)data, size);
    if (!msgSettings.send(m_sockCmdOut.get())) {
        logln("setStateInformation failed: can't send payload message");
        return;
    }
}

void ProcessorClient::setPlayHead(AudioPlayHead* p) { m_playhead = p; }

const json& ProcessorClient::getParameters() { return m_parameters; }

int ProcessorClient::getNumPrograms() { return m_presets.size(); }

const String ProcessorClient::getProgramName(int i) {
    if (i > -1 && i < m_presets.size()) {
        return m_presets[i];
    }
    return {};
}

void ProcessorClient::setCurrentProgram(int i) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<Preset> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->preset = i;
    msg.send(m_sockCmdOut.get());
}

void ProcessorClient::suspendProcessing(bool b) {
    m_suspended = b;

    std::lock_guard<std::mutex> lock(m_mtx);

    if (b) {
        Message<BypassPlugin> msg(this);
        PLD(msg).setNumber(0);
        msg.send(m_sockCmdOut.get());
    } else {
        Message<UnbypassPlugin> msg(this);
        PLD(msg).setNumber(0);
        msg.send(m_sockCmdOut.get());
    }
}

void ProcessorClient::suspendProcessingRemoteOnly(bool b) {
    std::lock_guard<std::mutex> lock(m_mtx);

    if (b) {
        Message<BypassPlugin> msg(this);
        PLD(msg).setNumber(0);
        msg.send(m_sockCmdOut.get());
    } else {
        Message<UnbypassPlugin> msg(this);
        PLD(msg).setNumber(0);
        msg.send(m_sockCmdOut.get());
    }
}

int ProcessorClient::getTotalNumOutputChannels() { return m_numOutputChannels; }

int ProcessorClient::getLatencySamples() { return m_latency; }

template <typename T>
void ProcessorClient::processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();

    if (!isOk()) {
        return;
    }

    AudioPlayHead::CurrentPositionInfo posInfo;
    if (nullptr != m_playhead) {
        m_playhead->getCurrentPosition(posInfo);
    }

    int sendBufChannels = m_activeChannels.getNumActiveChannelsCombined();
    AudioBuffer<T>* sendBuffer;
    std::unique_ptr<AudioBuffer<T>> tmpBuffer;

    if (sendBufChannels != buffer.getNumChannels()) {
        tmpBuffer = std::make_unique<AudioBuffer<T>>(sendBufChannels, buffer.getNumSamples());
        sendBuffer = tmpBuffer.get();
    } else {
        sendBuffer = &buffer;
    }

    MessageHelper::Error e;
    AudioMessage msg(this);

    m_channelMapper.map(&buffer, sendBuffer);

    {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (!msg.sendToServer(m_sockAudio.get(), *sendBuffer, midiMessages, posInfo, sendBuffer->getNumChannels(),
                              sendBuffer->getNumSamples(), &e, *m_bytesOutMeter)) {
            logln("error while sending audio message to sandbox: " << e.toString());
            m_sockAudio->close();
            return;
        }

        if (!msg.readFromServer(m_sockAudio.get(), *sendBuffer, midiMessages, &e, *m_bytesInMeter)) {
            logln("error while reading audio message from sandbox: " << e.toString());
            m_sockAudio->close();
            return;
        }
    }

    m_channelMapper.mapReverse(sendBuffer, &buffer);
}

void ProcessorClient::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

void ProcessorClient::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

juce::Rectangle<int> ProcessorClient::getScreenBounds() {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<GetScreenBounds> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("getScreenBounds failed: can't send message");
        return {};
    }

    Message<ScreenBounds> res(this);
    MessageHelper::Error err;
    if (res.read(m_sockCmdOut.get(), &err)) {
        return {DATA(res)->x, DATA(res)->y, DATA(res)->w, DATA(res)->h};
    } else {
        logln("getScreenBounds failed: failed to read ScreenBounds message: " << err.toString());
        return {};
    }
}

void ProcessorClient::setParameterValue(int paramIdx, float value) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<ParameterValue> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = value;
    msg.send(m_sockCmdOut.get());
}

float ProcessorClient::getParameterValue(int paramIdx) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<GetParameterValue> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->paramIdx = paramIdx;
    msg.send(m_sockCmdOut.get());

    Message<ParameterValue> ret(this);
    MessageHelper::Error err;
    if (ret.read(m_sockCmdOut.get(), &err)) {
        if (paramIdx == DATA(ret)->paramIdx) {
            return DATA(ret)->value;
        }
    }

    logln("getParameterValue failed: failed to read parameter value for paramIdx=" << paramIdx << ": "
                                                                                   << err.toString());

    return 0.0f;
}

std::vector<Srv::ParameterValue> ProcessorClient::getAllParameterValues() {
    traceScope();

    if (m_parameters.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_mtx);

    Message<GetAllParameterValues> msg(this);
    PLD(msg).setNumber(0);
    msg.send(m_sockCmdOut.get());

    std::vector<Srv::ParameterValue> ret;
    for (int i = 0; i < (int)m_parameters.size(); i++) {
        Message<ParameterValue> msgVal(this);
        MessageHelper::Error err;
        if (msgVal.read(m_sockCmdOut.get(), &err)) {
            ret.push_back({DATA(msgVal)->paramIdx, DATA(msgVal)->value});
        } else {
            break;
        }
    }
    return ret;
}

}  // namespace e47
