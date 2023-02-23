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

ProcessorClient::~ProcessorClient() {
    {
        std::lock_guard<std::mutex> lock(m_cmdMtx);
        m_sockCmdOut.reset();
        m_sockCmdIn.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_audioMtx);
        m_sockAudio.reset();
    }
    removeWorkerPort(m_port);
}

bool ProcessorClient::init() {
    traceScope();
    if (!startSandbox()) {
        setAndLogError("fatal error: failed to start sandbox process");
        return false;
    }

    if (!connectSandbox()) {
        setAndLogError("fatal error: failed to connect to sandbox process");
        if (m_process.isRunning()) {
            m_process.kill();
        }
        return false;
    }

    m_error.clear();

    return true;
}

void ProcessorClient::shutdown() {
    logln("shutting down sandbox");

    signalThreadShouldExit();

    {
        std::lock_guard<std::mutex> lock(m_cmdMtx);

        if (nullptr != m_sockCmdOut) {
            if (m_sockCmdOut->isConnected()) {
                m_sockCmdOut->close();
            }
        }

        if (nullptr != m_sockCmdIn) {
            if (m_sockCmdIn->isConnected()) {
                m_sockCmdIn->close();
            }
        }

        if (m_process.isRunning()) {
            m_process.kill();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_audioMtx);

        if (nullptr != m_sockAudio) {
            if (m_sockAudio->isConnected()) {
                m_sockAudio->close();
            }
        }
    }
}

bool ProcessorClient::isOk() {
    bool ok;

    {
        std::lock_guard<std::mutex> lock(m_cmdMtx);
        ok = m_process.isRunning() && nullptr != m_sockCmdIn && m_sockCmdIn->isConnected() &&
             nullptr != m_sockCmdOut && m_sockCmdOut->isConnected();
    }

    {
        std::lock_guard<std::mutex> lock(m_audioMtx);
        ok = ok && nullptr != m_sockAudio && m_sockAudio->isConnected();
    }

    return ok;
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
    try {
        std::lock_guard<std::mutex> lock(m_cmdMtx);

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
        if (auto srv = getApp()->getServer()) {
            args.addArray({"-id", String(srv->getId())});
        } else {
            throw std::runtime_error("no server object");
        }
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
    } catch (const std::exception& e) {
        setAndLogError("failed to start sandbox: " + String(e.what()));
    } catch (...) {
        setAndLogError("failed to start sandbox: unknown error");
    }

    return false;
}

bool ProcessorClient::connectSandbox() {
    logln("connecting to sandbox at port " << m_port);

    bool success = true;

    bool hasUnixDomainSockets = Defaults::unixDomainSocketsSupported();
    auto socketPath = Defaults::getSocketPath(Defaults::SANDBOX_PLUGIN_SOCK, {{"n", String(m_port)}});

    {
        std::lock_guard<std::mutex> lock(m_audioMtx);
        m_sockAudio.reset();
    }

    {
        std::lock_guard<std::mutex> lock(m_cmdMtx);

        m_sockCmdIn.reset();
        m_sockCmdOut = std::make_unique<StreamingSocket>();

        // let the process come up and bind to the port
        int maxTries = 100;
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

        if (m_sockCmdOut->isConnected()) {
            m_sockCmdIn = std::make_unique<StreamingSocket>();

            if (hasUnixDomainSockets) {
                if (!m_sockCmdIn->connect(socketPath)) {
                    setAndLogError("failed to setup sandbox command-in connection");
                    success = false;
                }
            } else {
                if (!m_sockCmdIn->connect("127.0.0.1", m_port)) {
                    setAndLogError("failed to setup sandbox command-in connection");
                    success = false;
                }
            }
        } else {
            setAndLogError("failed to setup sandbox command-out connection");
            success = false;
        }

        if (!success) {
            m_sockCmdOut.reset();
            m_sockCmdIn.reset();
        }
    }

    if (success) {
        std::lock_guard<std::mutex> lock(m_audioMtx);

        m_sockAudio = std::make_unique<StreamingSocket>();

        if (hasUnixDomainSockets) {
            if (!m_sockAudio->connect(socketPath)) {
                setAndLogError("failed to setup sandbox audio connection");
                success = false;
            }
        } else {
            if (!m_sockAudio->connect("127.0.0.1", m_port)) {
                setAndLogError("failed to setup sandbox audio connection");
                success = false;
            }
        }

        if (success) {
            m_bytesOutMeter = Metrics::getStatistic<Meter>("SandboxBytesOut");
            m_bytesInMeter = Metrics::getStatistic<Meter>("SandboxBytesIn");
        } else {
            m_sockAudio.reset();
        }
    }

    if (success) {
        logln("connected to sandbox successfully");
    }

    return success;
}

void ProcessorClient::run() {
    traceScope();
    MessageFactory msgFactory(this);

    bool lastOk = true;

    while (!threadShouldExit()) {
        if (!isOk()) {
            if (lastOk) {
                lastOk = false;
                if (onStatusChange) {
                    onStatusChange(false, m_error);
                }
            }
            if (!init()) {
                return;
            }
            if (!isOk()) {
                sleepExitAware(1000);
                continue;
            }
            if (m_loaded) {
                String err;
                if (!load(m_lastSettings, m_lastLayout, m_lastMonoChannels, err)) {
                    setAndLogError("reload failed: " + err);
                }
            }
        }

        if (!lastOk) {
            lastOk = true;
            if (onStatusChange) {
                onStatusChange(m_error.isEmpty(), m_error);
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
                    case ScreenBounds::Type:
                        handleMessage(Message<Any>::convert<ScreenBounds>(msg));
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
        onParamValueChange(pDATA(msg)->channel, pDATA(msg)->paramIdx, pDATA(msg)->value);
    }
}

void ProcessorClient::handleMessage(std::shared_ptr<Message<ParameterGesture>> msg) {
    traceScope();
    if (nullptr != onParamGestureChange) {
        onParamGestureChange(pDATA(msg)->channel, pDATA(msg)->paramIdx, pDATA(msg)->gestureIsStarting);
    }
}

void ProcessorClient::handleMessage(std::shared_ptr<Message<ScreenBounds>> msg) {
    std::lock_guard<std::mutex> lock(m_cmdMtx);
    m_lastScreenBounds = {pDATA(msg)->x, pDATA(msg)->y, pDATA(msg)->w, pDATA(msg)->h};
}

bool ProcessorClient::load(const String& settings, const String& layout, uint64 monoChannels, String& err) {
    traceScope();

    if (!isOk()) {
        err = "load failed: client not ok";
        return false;
    }

    logln("loading " << m_id << "...");

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    TimeStatistic::Timeout timeout(15000);
    MessageHelper::Error e;

    Message<AddPlugin> msgAddPlugin(this);
    PLD(msgAddPlugin)
        .setJson({{"id", m_id.toStdString()},
                  {"settings", settings.toStdString()},
                  {"layout", layout.toStdString()},
                  {"monoChannels", monoChannels}});

    if (msgAddPlugin.send(m_sockCmdOut.get())) {
        Message<AddPluginResult> msgResult(this);
        if (!msgResult.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "seems like the plugin did not load or crash: " + e.toString();
            setAndLogError(err);
            m_sockCmdOut->close();
            return false;
        }

        auto jresult = msgResult.payload.getJson();
        if (!jresult["success"].get<bool>()) {
            err = jresult["err"].get<std::string>();
            setAndLogError("load failed: " + err);
            m_sockCmdOut->close();
            return false;
        }

        if (timeout.getMillisecondsLeft() == 0) {
            err = "load failed: timeout";
            setAndLogError(err);
            m_sockCmdOut->close();
            return false;
        }

        logln("reading presets...");

        Message<Presets> msgPresets(this);
        if (!msgPresets.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read presets: " + e.toString();
            setAndLogError(err);
            m_sockCmdOut->close();
            return false;
        }
        m_presets = StringArray::fromTokens(msgPresets.payload.getString(), "|", "");

        if (timeout.getMillisecondsLeft() == 0) {
            err = "load failed: timeout";
            setAndLogError(err);
            m_sockCmdOut->close();
            return false;
        }

        logln("...ok");
        logln("reading parameters...");

        Message<Parameters> msgParams(this);
        if (!msgParams.read(m_sockCmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read parameters: " + e.toString();
            logln(err);
            m_sockCmdOut->close();
            return false;
        }

        logln("...ok");

        m_parameters = msgParams.payload.getJson();

        try {
            m_name = jresult["name"].get<std::string>();
            m_latency = jresult["latency"].get<int>();
            m_hasEditor = jresult["hasEditor"].get<bool>();
            m_scDisabled = jresult["disabledSideChain"].get<bool>();
            m_supportsDoublePrecision = jresult["supportsDoublePrecision"].get<bool>();
            m_tailSeconds = jresult["tailSeconds"].get<double>();
            m_numOutputChannels = jresult["numOutputChannels"].get<int>();
            m_lastChannelInstances = jresult["channelInstances"].get<int>();
        } catch (const json::parse_error& ex) {
            err = "json error when reading result: " + String(ex.what());
            setAndLogError(err);
            return false;
        } catch (const std::exception& ex) {
            err = "std error when reading result: " + String(ex.what());
            setAndLogError(err);
            return false;
        }

        m_lastSettings = settings;
        m_lastLayout = layout;
        m_lastMonoChannels = monoChannels;
        m_lastScreenBounds = {};
        m_loaded = true;
        m_error.clear();
        logln("load was successful");

        return true;
    } else {
        err = "load failed: send failed";
        m_sockCmdOut->close();
    }

    return false;
}

void ProcessorClient::unload() {
    m_loaded = false;

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<DelPlugin> msg(this);
    PLD(msg).setNumber(0);
    msg.send(m_sockCmdOut.get());

    MessageHelper::Error e;
    MessageFactory msgFactory(this);
    auto result = msgFactory.getResult(m_sockCmdOut.get(), 5, &e);
    if (nullptr != result && result->getReturnCode() > -1) {
        m_latency = result->getReturnCode();
    } else {
        logln("unload failed: can read result message: " << e.toString());
        m_sockCmdOut->close();
    }
}

const String ProcessorClient::getName() { return m_name; }

bool ProcessorClient::hasEditor() { return m_hasEditor; }

void ProcessorClient::showEditor(int channel, int x, int y) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<EditPlugin> msg(this);
    DATA(msg)->index = 0;
    DATA(msg)->channel = channel;
    DATA(msg)->x = x;
    DATA(msg)->y = y;
    msg.send(m_sockCmdOut.get());
}

void ProcessorClient::hideEditor() {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<HidePlugin> msg(this);
    msg.send(m_sockCmdOut.get());

    m_lastScreenBounds = {};
}

bool ProcessorClient::supportsDoublePrecisionProcessing() { return m_supportsDoublePrecision; }

bool ProcessorClient::isSuspended() { return m_suspended; }

double ProcessorClient::getTailLengthSeconds() { return m_tailSeconds; }

void ProcessorClient::getStateInformation(String& settings) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<GetPluginSettings> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("getStateInformation failed: can't send message");
        return;
    }

    Message<PluginSettings> res(this);
    MessageHelper::Error err;
    if (res.read(m_sockCmdOut.get(), &err, 5000)) {
        m_lastSettings = PLD(res).getString();
        settings = m_lastSettings;
    } else {
        logln("getStateInformation failed: failed to read PluginSettings message: " << err.toString());
        m_sockCmdOut->close();
        return;
    }
}

void ProcessorClient::setStateInformation(const String& settings) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<SetPluginSettings> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("setStateInformation failed: can't send announcement message");
        return;
    }

    Message<PluginSettings> msgSettings(this);
    PLD(msgSettings).setString(settings);
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

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<Preset> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->preset = i;
    msg.send(m_sockCmdOut.get());
}

void ProcessorClient::suspendProcessing(bool b) {
    m_suspended = b;

    std::lock_guard<std::mutex> lock(m_cmdMtx);

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
    std::lock_guard<std::mutex> lock(m_cmdMtx);

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

    AudioPlayHead::PositionInfo posInfo;
    if (nullptr != m_playhead) {
        if (auto optPosInfo = m_playhead->getPosition()) {
            posInfo = *optPosInfo;
        }
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

    TimeTrace::addTracePoint("pc_prep_buffer");

    m_channelMapper.map(&buffer, sendBuffer);
    TimeTrace::addTracePoint("pc_ch_map");

    {
        std::lock_guard<std::mutex> lock(m_audioMtx);

        if (nullptr != m_sockAudio) {
            TimeTrace::addTracePoint("pc_lock");

            if (!msg.sendToServer(m_sockAudio.get(), *sendBuffer, midiMessages, posInfo, sendBuffer->getNumChannels(),
                                  sendBuffer->getNumSamples(), &e, *m_bytesOutMeter)) {
                logln("error while sending audio message to sandbox: " << e.toString());
                m_sockAudio->close();
                return;
            }

            TimeTrace::addTracePoint("pc_send");

            if (!msg.readFromServer(m_sockAudio.get(), *sendBuffer, midiMessages, &e, *m_bytesInMeter)) {
                logln("error while reading audio message from sandbox: " << e.toString());
                m_sockAudio->close();
                return;
            }

            TimeTrace::addTracePoint("pc_read");
        } else {
            logln("error while sending audio message: no socket");
            return;
        }
    }

    m_channelMapper.mapReverse(sendBuffer, &buffer);

    TimeTrace::addTracePoint("pc_ch_map_reverse");
}

void ProcessorClient::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

void ProcessorClient::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    processBlockInternal(buffer, midiMessages);
}

juce::Rectangle<int> ProcessorClient::getScreenBounds() {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<GetScreenBounds> msg(this);
    PLD(msg).setNumber(0);
    if (!msg.send(m_sockCmdOut.get())) {
        logln("getScreenBounds failed: can't send message");
    }

    return m_lastScreenBounds;
}

void ProcessorClient::setParameterValue(int channel, int paramIdx, float value) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<ParameterValue> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->channel = channel;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = value;
    msg.send(m_sockCmdOut.get());
}

float ProcessorClient::getParameterValue(int channel, int paramIdx) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<GetParameterValue> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->channel = channel;
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
    m_sockCmdOut->close();

    return 0.0f;
}

std::vector<Srv::ParameterValue> ProcessorClient::getAllParameterValues() {
    traceScope();

    if (m_parameters.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<GetAllParameterValues> msg(this);
    PLD(msg).setNumber(0);
    msg.send(m_sockCmdOut.get());

    std::vector<Srv::ParameterValue> ret;
    for (int i = 0; i < (int)m_parameters.size(); i++) {
        Message<ParameterValue> msgVal(this);
        MessageHelper::Error err;
        if (msgVal.read(m_sockCmdOut.get(), &err, 2000)) {
            ret.push_back({DATA(msgVal)->paramIdx, DATA(msgVal)->value});
        } else {
            logln("getAllParameterValues failed: " << err.toString());
            m_sockCmdOut->close();
            break;
        }
    }
    return ret;
}

void ProcessorClient::setMonoChannels(uint64 channels) {
    std::lock_guard<std::mutex> lock(m_cmdMtx);

    Message<SetMonoChannels> msg(this);
    DATA(msg)->idx = 0;
    DATA(msg)->channels = channels;
    msg.send(m_sockCmdOut.get());
}

}  // namespace e47
