/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Client.hpp"
#include <memory>
#include "PluginProcessor.hpp"
#include "ServiceReceiver.hpp"
#include "AudioStreamer.hpp"
#include "KeyAndMouse.hpp"

#ifdef JUCE_WINDOWS
#include "windows.h"
#else
#include "fcntl.h"
#endif

namespace e47 {

std::atomic_uint32_t Client::count{0};

Client::Client(PluginProcessor* processor)
    : Thread("Client"), LogTag("client"), m_processor(processor), m_msgFactory(this) {
    initAsyncFunctors();
    count++;
}

Client::~Client() {
    traceScope();
    stopAsyncFunctors();
    signalThreadShouldExit();
    close();
    count--;
}

void Client::run() {
    traceScope();
    logln("entering client loop");
    uint32 cpuUpdateSeconds = 5;
    uint32 syncSeconds = 10;
    uint32 loops = 0;
    MessageFactory msgFactory(this);
    bool lastState = isReady();
    while (!threadShouldExit()) {
#ifndef AG_UNIT_TESTS
        // Check for config updates from other clients
        auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
        int newNum;
        if (!jsonGetValue(cfg, "BufferSettingByPlugin", false)) {
            newNum = jsonGetValue(cfg, "NumberOfBuffers", NUM_OF_BUFFERS.load());
            if (NUM_OF_BUFFERS != newNum) {
                logln("number of buffers changed from " << NUM_OF_BUFFERS << " to " << newNum);
                NUM_OF_BUFFERS = newNum;
                reconnect();
            }
        }
        newNum = jsonGetValue(cfg, "LoadPluginTimeoutMS", LOAD_PLUGIN_TIMEOUT.load());
        if (LOAD_PLUGIN_TIMEOUT != newNum) {
            logln("timeout for leading a plugin changed from " << LOAD_PLUGIN_TIMEOUT << " to " << newNum);
            LOAD_PLUGIN_TIMEOUT = newNum;
        }
        m_processor->loadConfig(cfg, true);
#endif

        // Start/stop tray connection, if the setting changed
        m_processor->setDisableTray(m_processor->getDisableTray());

        auto srvInfo = getServer();
        auto servers = m_processor->getServersMDNS();

        // Try to auto connect to the first available host discovered via mDNS
        if (!srvInfo.isValid()) {
            if (servers.size() > 0) {
                setServer(servers[0]);
                srvInfo = servers[0];
            }
        } else {
            for (auto& si : servers) {
                if (si.matches(srvInfo) && si != srvInfo) {
                    bool reconnect = si.getHostAndID() != srvInfo.getHostAndID();
                    srvInfo = si;
                    std::lock_guard<std::mutex> lock(m_srvMtx);
                    m_srvInfo = si;
                    if (reconnect) {
                        m_needsReconnect = true;
                    }
                }
            }
        }

        // Health check & reconnect
        if ((!isReady(LOAD_PLUGIN_TIMEOUT + 5000) || m_needsReconnect) && srvInfo.isValid() && !threadShouldExit()) {
            logln("(re)connecting...");
            close();
            init();
            bool newState = m_ready;
            if (newState) {
                if (m_onConnectCallback) {
                    m_onConnectCallback();
                }
            } else if (lastState) {
                if (m_onCloseCallback) {
                    m_onCloseCallback();
                }
            }
            lastState = newState;
        }

        // CPU load update
        if ((loops % cpuUpdateSeconds == 0) && isReadyLockFree()) {
            updateCPULoad();
        }

        // Trigger sync
        if ((loops % syncSeconds == 0) && isReadyLockFree()) {
            m_processor->sync();
        }

        // Check for auto reconnect
        m_processor->autoRetry();

        if (isReadyLockFree()) {
            TimeStatistic::Timeout timeout(1000);
            while (isReadyLockFree() && timeout.getMillisecondsLeft() > 0 && !threadShouldExit()) {
                MessageHelper::Error err;
                auto msg = msgFactory.getNextMessage(m_cmdIn.get(), &err, 100);
                if (nullptr != msg) {
                    switch (msg->getType()) {
                        case Key::Type:
                            handleMessage(Message<Any>::convert<Key>(msg));
                            break;
                        case Clipboard::Type:
                            handleMessage(Message<Any>::convert<Clipboard>(msg));
                            break;
                        case ParameterValue::Type:
                            handleMessage(Message<Any>::convert<ParameterValue>(msg));
                            break;
                        case ParameterGesture::Type:
                            handleMessage(Message<Any>::convert<ParameterGesture>(msg));
                            break;
                        case PluginStatus::Type:
                            handleMessage(Message<Any>::convert<PluginStatus>(msg));
                            break;
                        default:
                            logln("unknown message type " << msg->getType());
                    }
                }
            }
        } else {
            // Relax
            sleepExitAware(1000);
        }

        loops++;
    }
    logln("client loop terminated");
}

void Client::handleMessage(std::shared_ptr<Message<Key>> msg) {
#if defined(JUCE_MAC) || defined(JUCE_WINDOWS)
    traceScope();
    runOnMsgThreadAsync([this, msg] {
        traceScope();
        void* nativeHandle = nullptr;
#ifdef JUCE_WINDOWS
        auto* e = m_processor->getActiveEditor();
        if (nullptr != e) {
            nativeHandle = e->getPeer()->getNativeHandle();
        }
        if (nullptr == nativeHandle) {
            logln("unable to get native handle");
            return;
        }
#endif
        auto* codes = pPLD(msg).getKeyCodes();
        auto num = pPLD(msg).getKeyCount();
        uint16_t key = 0;
        uint64_t flags = 0;
        for (int i = 0; i < num; i++) {
            if (isShiftKey(codes[i])) {
                setShiftKey(flags);
            } else if (isControlKey(codes[i])) {
                setControlKey(flags);
            } else if (isAltKey(codes[i])) {
                setAltKey(flags);
            } else {
                key = codes[i];
            }
        }
        keyEventDown(key, flags, true, nativeHandle);
        keyEventUp(key, flags, true, nativeHandle);
    });
#else
    ignoreUnused(msg);
#endif
}

void Client::handleMessage(std::shared_ptr<Message<Clipboard>> msg) {
    SystemClipboard::copyTextToClipboard(pPLD(msg).getString());
}

void Client::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    m_processor->updateParameterValue(pDATA(msg)->idx, pDATA(msg)->paramIdx, pDATA(msg)->value, false);
}

void Client::handleMessage(std::shared_ptr<Message<ParameterGesture>> msg) {
    m_processor->updateParameterGestureTracking(pDATA(msg)->idx, pDATA(msg)->paramIdx, pDATA(msg)->gestureIsStarting);
}

void Client::handleMessage(std::shared_ptr<Message<PluginStatus>> msg) {
    auto jstatus = pPLD(msg).getJson();
    try {
        logln("updating plugin status: " << jstatus.dump());
        m_processor->updatePluginStatus(jstatus["idx"].get<int>(), jstatus["ok"].get<bool>(),
                                        jstatus["err"].get<std::string>());
    } catch (const json::exception& e) {
        logln("failed to update plugin status: " << e.what());
    }
}

void Client::setServer(const ServerInfo& srv) {
    traceScope();
    logln("setting server to " << srv.toString());
    std::lock_guard<std::mutex> lock(m_srvMtx);
    if (m_srvInfo != srv) {
        m_srvInfo = srv;
        m_needsReconnect = true;
    }
}

ServerInfo Client::getServer() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvInfo;
}

void Client::setPluginScreenUpdateCallback(ScreenUpdateCallback fn) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_pluginScreenMtx);
    m_pluginScreenUpdateCallback = fn;
}

void Client::setOnConnectCallback(OnConnectCallback fn) {
    traceScope();
    LockByID lock(*this, SETONCONNECTCALLBACK);
    m_onConnectCallback = fn;
}

void Client::setOnCloseCallback(OnCloseCallback fn) {
    traceScope();
    LockByID lock(*this, SETONCLOSECALLBACK);
    m_onCloseCallback = fn;
}

void Client::init(int channelsIn, int channelsOut, int channelsSC, double rate, int samplesPerBlock,
                  bool doublePrecission) {
    traceScope();
    logln("init: channelsIn=" << channelsIn << " channelsOut=" << channelsOut << " channelsSC=" << channelsSC
                              << " rate=" << rate << " samplesPerBlock=" << samplesPerBlock
                              << " doublePrecission=" << (int)doublePrecission);
    LockByID lock(*this, INIT1);
    if (!m_ready || m_channelsIn != channelsIn || m_channelsOut != channelsOut || m_channelsSC != channelsSC ||
        m_rate != rate || m_samplesPerBlock != samplesPerBlock || m_doublePrecission != doublePrecission) {
        m_channelsIn = channelsIn;
        m_channelsOut = channelsOut;
        m_channelsSC = channelsSC;
        m_rate = rate;
        m_samplesPerBlock = samplesPerBlock;
        m_doublePrecission = doublePrecission;
        m_needsReconnect = true;
        m_ready = false;
        logln("init: paramater change, requesting reconnect");
    }
}

void Client::init() {
    traceScope();
    auto srvInfo = getServer();
    bool useUnixDomain = srvInfo.getLocalMode() && Defaults::unixDomainSocketsSupported();
    int port = Defaults::SERVER_PORT + srvInfo.getID();

    LockByID lock(*this, INIT2);

#if !JucePlugin_IsMidiEffect
    if (m_channelsOut == 0 || m_rate == 0.0 || m_samplesPerBlock == 0) {
        return;
    }
#endif

    m_error = true;
    m_cmdOut = std::make_unique<StreamingSocket>();

    if (useUnixDomain) {
        auto socketPath = Defaults::getSocketPath(Defaults::SERVER_SOCK, {{"id", String(srvInfo.getID())}});
        logln("connecting server: " << socketPath.getFullPathName());
        if (!m_cmdOut->connect(socketPath, 1000)) {
            logln("local connection to server failed");
            useUnixDomain = false;
        }
    }

    if (!m_cmdOut->isConnected()) {
        logln("connecting server: " << srvInfo.getHostAndID());
        m_cmdOut->connect(srvInfo.getHost(), port, 1000);
    }

    if (m_cmdOut->isConnected()) {
        HandshakeRequest cfg = {AG_PROTOCOL_VERSION,
                                m_channelsIn,
                                m_channelsOut,
                                m_channelsSC,
                                m_rate,
                                m_samplesPerBlock,
                                m_doublePrecission,
                                getTagId(),
                                0,
                                0,
                                m_processor->getActiveChannels().toInt(),
                                0};
        if (m_processor->getNoSrvPluginListFilter()) {
            cfg.setFlag(HandshakeRequest::NO_PLUGINLIST_FILTER);
        }

        if (!send(m_cmdOut.get(), reinterpret_cast<const char*>(&cfg), sizeof(cfg))) {
            m_cmdOut->close();
            return;
        }

        HandshakeResponse resp;
        MessageHelper::Error err;
        if (!read(m_cmdOut.get(), &resp, sizeof(resp), LOAD_PLUGIN_TIMEOUT, &err)) {
            logln("handshake error: " << err.toString());
            m_cmdOut->close();
            return;
        }
        m_cmdOut->close();

        m_srvLocalMode = resp.isFlag(HandshakeResponse::LOCAL_MODE);
        logln("server local mode is " << (int)m_srvLocalMode);

        File workerSocketPath;

        if (useUnixDomain) {
            workerSocketPath = Defaults::getSocketPath(Defaults::WORKER_SOCK,
                                                       {{"id", String(srvInfo.getID())}, {"n", String(resp.port)}});
            logln("connecting worker: " << workerSocketPath.getFullPathName());
            m_cmdOut->connect(workerSocketPath);
        } else {
            logln("connecting worker: " << srvInfo.getHost() << ":" << resp.port);
            m_cmdOut->connect(srvInfo.getHost(), resp.port);
        }

        if (!m_cmdOut->isConnected()) {
            logln("connection to server failed");
            m_cmdOut.reset();
            return;
        }

        m_cmdIn = std::make_unique<StreamingSocket>();
        if (useUnixDomain ? !m_cmdIn->connect(workerSocketPath) : !m_cmdIn->connect(srvInfo.getHost(), resp.port)) {
            logln("failed to setup command receive connection");
            m_cmdIn.reset();
        }
        logln("command connection established");

        StreamingSocket* audioSock = nullptr;
        audioSock = new StreamingSocket;
        if (useUnixDomain ? !audioSock->connect(workerSocketPath) : !audioSock->connect(srvInfo.getHost(), resp.port)) {
            logln("failed to setup audio connection");
            delete audioSock;
            audioSock = nullptr;
        }

        m_screenSocket = std::make_unique<StreamingSocket>();
        if (useUnixDomain ? !m_screenSocket->connect(workerSocketPath)
                          : !m_screenSocket->connect(srvInfo.getHost(), resp.port)) {
            logln("failed to setup screen connection");
            m_screenSocket.reset();
        }

        if (nullptr != audioSock) {
            logln("audio connection established");
            std::lock_guard<std::mutex> audiolck(m_audioMtx);
            if (m_doublePrecission) {
                m_audioStreamerD = std::make_shared<AudioStreamer<double>>(this, audioSock);
                m_audioStreamerD->startThread(Thread::realtimeAudioPriority);
            } else {
                m_audioStreamerF = std::make_shared<AudioStreamer<float>>(this, audioSock);
                m_audioStreamerF->startThread(Thread::realtimeAudioPriority);
            }
        } else {
            return;
        }

        if (nullptr != m_screenSocket) {
            logln("screen connection established");
            m_screenWorker = std::make_unique<ScreenReceiver>(this, m_screenSocket.get());
            m_screenWorker->startThread();
        } else {
            return;
        }

        // receive plugin list
        updatePluginList();

        m_ready = true;
        m_error = false;
        m_needsReconnect = false;
    } else {
        logln("connection to server failed");
    }
}

bool Client::isReady(int timeout) {
    traceScope();
    int retry = timeout / 10;
    bool locked = false;
    while (retry-- > 0) {
        if ((locked = m_clientMtx.try_lock()) == true) {
            break;
        } else {
            sleep(10);
        }
    }
    if (locked) {
        m_ready = !m_error && !m_needsReconnect && nullptr != m_cmdOut && m_cmdOut->isConnected() &&
                  m_screenWorker->isThreadRunning() && nullptr != m_screenSocket && m_screenSocket->isConnected() &&
                  audioConnectionOk();
        m_clientMtx.unlock();
    } else {
        logln(getLoadedPluginsString() << ": error: isReady can't acquire lock, locked by " << m_clientMtxId);
        m_error = true;
    }
    return !m_error && m_ready;
}

bool Client::isReadyLockFree() { return !m_error && m_ready; }

void Client::close() {
    traceScope();
    if (m_ready) {
        logln("closing");
        if (m_onCloseCallback) {
            m_onCloseCallback();
        }
    }
    m_ready = false;
    LockByID lock(*this, CLOSE);
    m_plugins.clear();
    if (nullptr != m_screenSocket && m_screenSocket->isConnected()) {
        m_screenSocket->close();
    }
    if (nullptr != m_screenWorker && m_screenWorker->isThreadRunning()) {
        m_screenWorker->signalThreadShouldExit();
        m_screenWorker->waitForThreadToExit(100);
        m_screenWorker.reset();
        m_screenSocket.reset();
    }
    if (nullptr != m_cmdOut) {
        if (m_cmdOut->isConnected()) {
            m_cmdOut->close();
        }
        m_cmdOut.reset();
    }
    m_audioMtx.lock();
    if (nullptr != m_audioStreamerD && m_audioStreamerD->isThreadRunning()) {
        m_audioStreamerD->signalThreadShouldExit();
        m_audioStreamerD->waitForThreadToExit(100);
        m_audioStreamerD.reset();
    }
    if (nullptr != m_audioStreamerF && m_audioStreamerF->isThreadRunning()) {
        m_audioStreamerF->signalThreadShouldExit();
        m_audioStreamerF->waitForThreadToExit(100);
        m_audioStreamerF.reset();
    }
    m_audioMtx.unlock();
}

Image Client::getPluginScreen() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_pluginScreenMtx);
    return *m_pluginScreen;
}

void Client::setPluginScreen(std::shared_ptr<Image> img, int w, int h) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_pluginScreenMtx);
    m_pluginScreen = img;
    if (m_pluginScreenUpdateCallback) {
        m_pluginScreenUpdateCallback(m_pluginScreen, w, h);
    }
}

void Client::quit() {
    traceScope();
    // called from close which already holds a lock
    Message<Quit> msg(this);
    msg.send(m_cmdOut.get());
}

bool Client::addPlugin(String id, StringArray& presets, Array<Parameter>& params, bool& hasEditor, bool& scDisabled,
                       String settings, String& err) {
    traceScope();

    if (!isReadyLockFree()) {
        err = "client not ready";
        return false;
    };

    err.clear();
    MessageHelper::Error e;
    Message<AddPlugin> msg(this);
    PLD(msg).setJson({{"id", id.toStdString()}, {"settings", settings.toStdString()}});

    LockByID lock(*this, ADDPLUGIN);

    TimeStatistic::Timeout timeout(LOAD_PLUGIN_TIMEOUT);

    if (msg.send(m_cmdOut.get())) {
        Message<AddPluginResult> msgResult(this);
        if (!msgResult.read(m_cmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "seems like the plugin crashed the server or did not load (" + e.toString() + ")";
            logln("error: " << err);
            return false;
        }
        auto jresult = PLD(msgResult).getJson();
        if (!jresult["success"].get<bool>()) {
            err = jresult["err"].get<std::string>();
            logln("load error: " << err);
            return false;
        }

        if (timeout.getMillisecondsLeft() == 0) {
            err = "failed to finish load: timeout before getting presets";
            logln(err);
            return false;
        }

        Message<Presets> msgPresets(this);
        if (!msgPresets.read(m_cmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read presets: " + e.toString();
            logln(err);
            return false;
        }
        presets = StringArray::fromTokens(msgPresets.payload.getString(), "|", "");
        if (timeout.getMillisecondsLeft() == 0) {
            err = "failed to finish load: timeout before getting parameters";
            logln(err);
            return false;
        }

        Message<Parameters> msgParams(this);
        if (!msgParams.read(m_cmdOut.get(), &e, timeout.getMillisecondsLeft())) {
            err = "failed to read parameters: " + e.toString();
            logln(err);
            return false;
        }
        auto jparams = msgParams.payload.getJson();
        Array<Parameter> paramsBak(std::move(params));
        for (auto& jparam : jparams) {
            auto newParam = Parameter::fromJson(jparam);
            for (auto& oldParam : paramsBak) {
                if (newParam.idx == oldParam.idx) {
                    newParam.automationSlot = oldParam.automationSlot;
                    break;
                }
            }
            params.add(std::move(newParam));
        }

        m_latency = jresult["latency"].get<int>();
        hasEditor = jresult["hasEditor"].get<bool>();
        scDisabled = jresult["disabledSideChain"].get<bool>();

        return true;
    }

    return false;
}

void Client::delPlugin(int idx) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<DelPlugin> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, DELPLUGIN);
    msg.send(m_cmdOut.get());
    auto result = m_msgFactory.getResult(m_cmdOut.get());
    if (nullptr != result && result->getReturnCode() > -1) {
        m_latency = result->getReturnCode();
    }
}

void Client::editPlugin(int idx, int x, int y) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<EditPlugin> msg(this);
    DATA(msg)->index = idx;
    DATA(msg)->x = x;
    DATA(msg)->y = y;
    LockByID lock(*this, EDITPLUGIN);
    msg.send(m_cmdOut.get());
}

void Client::hidePlugin() {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<HidePlugin> msg(this);
    LockByID lock(*this, HIDEPLUGIN);
    msg.send(m_cmdOut.get());
}

MemoryBlock Client::getPluginSettings(int idx) {
    traceScope();
    MemoryBlock block;
    if (!isReadyLockFree()) {
        return block;
    };
    Message<GetPluginSettings> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, GETPLUGINSETTINGS);
    if (!msg.send(m_cmdOut.get())) {
        m_error = true;
    } else {
        Message<PluginSettings> res(this);
        MessageHelper::Error err;
        if (res.read(m_cmdOut.get(), &err, LOAD_PLUGIN_TIMEOUT)) {
            if (*res.payload.size > 0) {
                block.append(res.payload.data, (size_t)*res.payload.size);
            }
        } else {
            logln(getLoadedPluginsString()
                  << ": failed to read PluginSettings message for idx " << idx << ": " << err.toString());
            m_error = true;
        }
    }
    return block;
}

void Client::setPluginSettings(int idx, String settings) {
    traceScope();
    Message<SetPluginSettings> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, SETPLUGINSETTINGS);
    if (!msg.send(m_cmdOut.get())) {
        m_error = true;
    } else {
        Message<PluginSettings> msgSettings(this);
        if (settings.isNotEmpty()) {
            MemoryBlock block;
            block.fromBase64Encoding(settings);
            msgSettings.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        }
        if (!msgSettings.send(m_cmdOut.get())) {
            logln("failed to send settings");
            m_error = true;
        }
    }
}

void Client::bypassPlugin(int idx) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<BypassPlugin> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, BYPASSPLUGIN);
    msg.send(m_cmdOut.get());
}

void Client::unbypassPlugin(int idx) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<UnbypassPlugin> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, UNBYPASSPLUGIN);
    msg.send(m_cmdOut.get());
}

void Client::exchangePlugins(int idxA, int idxB) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<ExchangePlugins> msg(this);
    DATA(msg)->idxA = idxA;
    DATA(msg)->idxB = idxB;
    LockByID lock(*this, EXCHANGEPLUGINS);
    msg.send(m_cmdOut.get());
}

Array<ServerPlugin> Client::getRecents() {
    traceScope();
    Array<ServerPlugin> recents;
    if (!isReadyLockFree()) {
        return recents;
    };
    Message<RecentsList> msg(this);
    MessageHelper::Error err;
    LockByID lock(*this, GETRECENTS);
    msg.send(m_cmdOut.get());
    if (msg.read(m_cmdOut.get(), &err, 5000)) {
        String listChunk(PLD(msg).str, (size_t)*PLD(msg).size);
        auto list = StringArray::fromLines(listChunk);
        for (auto& line : list) {
            if (!line.isEmpty()) {
                recents.add(ServerPlugin::fromString(line));
            }
        }
    } else {
        logln(getLoadedPluginsString() << ": failed to read RecentsList message: " << err.toString());
        m_error = true;
    }
    return recents;
}

void Client::setPreset(int idx, int preset) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<Preset> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->preset = preset;
    LockByID lock(*this, SETPRESET);
    msg.send(m_cmdOut.get());
}

float Client::getParameterValue(int idx, int paramIdx) {
    traceScope();
    if (!isReadyLockFree()) {
        return 0;
    };
    Message<GetParameterValue> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    LockByID lock(*this, GETPARAMETERVALUE);
    msg.send(m_cmdOut.get());
    Message<ParameterValue> ret(this);
    MessageHelper::Error err;
    if (ret.read(m_cmdOut.get(), &err)) {
        if (DATA(msg)->idx == DATA(ret)->idx && DATA(msg)->paramIdx == DATA(ret)->paramIdx) {
            return DATA(ret)->value;
        }
    }
    logln(getLoadedPluginsString() << ": failed to read parameter value idx=" << idx << " paramIdx=" << paramIdx << ": "
                                   << err.toString());
    m_error = true;
    return 0;
}

void Client::setParameterValue(int idx, int paramIdx, float val) {
    traceScope();
    if (!isReadyLockFree()) {
        return;
    };
    Message<ParameterValue> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = val;
    LockByID lock(*this, SETPARAMETERVALUE);
    msg.send(m_cmdOut.get());
}

Array<Client::ParameterResult> Client::getAllParameterValues(int idx, int cnt) {
    traceScope();
    if (!isReadyLockFree()) {
        return {};
    };
    Message<GetAllParameterValues> msg(this);
    PLD(msg).setNumber(idx);
    LockByID lock(*this, GETALLPARAMETERVALUES);
    msg.send(m_cmdOut.get());
    Array<Client::ParameterResult> ret;
    for (int i = 0; i < cnt; i++) {
        Message<ParameterValue> msgVal(this);
        MessageHelper::Error err;
        if (msgVal.read(m_cmdOut.get(), &err)) {
            if (idx == DATA(msgVal)->idx) {
                ret.add({DATA(msgVal)->paramIdx, DATA(msgVal)->value});
            }
        } else {
            break;
        }
    }
    return ret;
}

void Client::ScreenReceiver::run() {
    traceScope();
    Message<ScreenCapture> msg(getLogTagSource());
    MessageHelper::Error err;
    do {
        if (msg.read(m_socket, &err, 200)) {
            if (PLD(msg).hdr->size > 0) {
                int width = (int)(PLD(msg).hdr->width / PLD(msg).hdr->scale);
                int height = (int)(PLD(msg).hdr->height / PLD(msg).hdr->scale);
                auto img = m_imgReader.read(DATA(msg), PLD(msg).hdr->size, PLD(msg).hdr->width, PLD(msg).hdr->height,
                                            PLD(msg).hdr->widthPadded, PLD(msg).hdr->heightPadded, PLD(msg).hdr->scale);
                if (nullptr != img) {
                    m_client->setPluginScreen(img, width, height);
                }
            } else {
                m_client->setPluginScreen(nullptr, 0, 0);
            }
        }
    } while (!threadShouldExit() && (err.code == MessageHelper::E_NONE || err.code == MessageHelper::E_TIMEOUT));
    if (!threadShouldExit()) {
        logln("screen receiver failed to read message: " << err.toString());
    }
    m_client->m_error = true;
    logln("screen receiver terminated");
}

void Client::mouseMove(const MouseEvent& event) {
    traceScope();
    sendMouseEvent(MouseEvType::MOVE, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                   event.mods.isAltDown());
}

void Client::mouseEnter(const MouseEvent& event) {
    traceScope();
    sendMouseEvent(MouseEvType::MOVE, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                   event.mods.isAltDown());
}

void Client::mouseDown(const MouseEvent& event) {
    traceScope();
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_DOWN, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_DOWN, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_DOWN, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    }
}

void Client::mouseDrag(const MouseEvent& event) {
    traceScope();
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_DRAG, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_DRAG, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_DRAG, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    }
}

void Client::mouseUp(const MouseEvent& event) {
    traceScope();
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_UP, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_UP, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_UP, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown());
    }
}

void Client::mouseDoubleClick(const MouseEvent& event) {
    traceScope();
    sendMouseEvent(MouseEvType::DBL_CLICK, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                   event.mods.isAltDown());
}

void Client::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    traceScope();
    if (!wheel.isInertial) {
        sendMouseEvent(MouseEvType::WHEEL, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown(), &wheel);
    }
}

void Client::sendMouseEvent(MouseEvType ev, Point<float> p, bool isShiftDown, bool isCtrlDown, bool isAltDown,
                            const MouseWheelDetails* wheel) {
    traceScope();
    if (!isReadyLockFree() || m_processor->getActivePlugin() == -1) {
        return;
    };
    Message<Mouse> msg(this);
    DATA(msg)->type = ev;
    DATA(msg)->x = p.x;
    DATA(msg)->y = p.y;
    DATA(msg)->isShiftDown = isShiftDown;
    DATA(msg)->isCtrlDown = isCtrlDown;
    DATA(msg)->isAltDown = isAltDown;
    if (ev == MouseEvType::WHEEL && nullptr != wheel) {
        DATA(msg)->deltaX = wheel->deltaX;
        DATA(msg)->deltaY = wheel->isReversed ? -wheel->deltaY : wheel->deltaY;
        DATA(msg)->isSmooth = wheel->isSmooth;
    } else {
        DATA(msg)->deltaX = 0;
        DATA(msg)->deltaY = 0;
        DATA(msg)->isSmooth = false;
    }
    LockByID lock(*this, SENDMOUSEEVENT);
    msg.send(m_cmdOut.get());
}

bool Client::keyPressed(const KeyPress& kp, Component* /* originatingComponent */) {
    traceScope();
    if (!isReadyLockFree() || m_processor->getActivePlugin() == -1) {
        return false;
    };
    auto modkeys = kp.getModifiers();
    bool consumed = true, isCopy = false, isPaste = false, isCut = false, isSelectAll = false;

#if JUCE_MAC
    isCopy = modkeys.isCommandDown() && kp.isKeyCode('C');
    isPaste = modkeys.isCommandDown() && kp.isKeyCode('V');
    isCut = modkeys.isCommandDown() && kp.isKeyCode('X');
    isSelectAll = modkeys.isCommandDown() && kp.isKeyCode('A');
#else
    isCopy = modkeys.isCtrlDown() && kp.isKeyCode('C');
    isPaste = modkeys.isCtrlDown() && kp.isKeyCode('V');
    isCut = modkeys.isCtrlDown() && kp.isKeyCode('X');
    isSelectAll = modkeys.isCtrlDown() && kp.isKeyCode('A');
#endif

    std::vector<uint16_t> keysToPress;
    if (!isCopy && !isPaste) {
        if (modkeys.isShiftDown()) {
            keysToPress.push_back(getKeyCode("Shift"));
        }
        if (modkeys.isCtrlDown()) {
            keysToPress.push_back(getKeyCode("Control"));
        }
        if (modkeys.isAltDown()) {
            keysToPress.push_back(getKeyCode("Option"));
        }
        if (modkeys.isCommandDown()) {
            keysToPress.push_back(getKeyCode("Command"));
        }
    }
    if (isCopy || isPaste || isCut || isSelectAll) {
        keysToPress.push_back(getKeyCode(isCopy ? "Copy" : isPaste ? "Paste" : isCut ? "Cut" : "SelectAll"));

        if (isPaste) {
            auto cbStr = SystemClipboard::getTextFromClipboard();
            Message<Clipboard> msg(this);
            PLD(msg).setString(cbStr);
            LockByID lock(*this, KEYPRESSED);
            msg.send(m_cmdOut.get());
        }
    } else if (kp.isKeyCurrentlyDown(KeyPress::escapeKey)) {
        keysToPress.push_back(getKeyCode("Escape"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::spaceKey)) {
        keysToPress.push_back(getKeyCode("Space"));
        consumed = false;  // don't consume the space key
    } else if (kp.isKeyCurrentlyDown(KeyPress::returnKey)) {
        keysToPress.push_back(getKeyCode("Return"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::tabKey)) {
        keysToPress.push_back(getKeyCode("Tab"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::deleteKey)) {
        keysToPress.push_back(getKeyCode("Delete"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::backspaceKey)) {
        keysToPress.push_back(getKeyCode("Backspace"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::upKey)) {
        keysToPress.push_back(getKeyCode("UpArrow"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::downKey)) {
        keysToPress.push_back(getKeyCode("DownArrow"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::leftKey)) {
        keysToPress.push_back(getKeyCode("LeftArrow"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::rightKey)) {
        keysToPress.push_back(getKeyCode("RightArrow"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::pageUpKey)) {
        keysToPress.push_back(getKeyCode("PageUp"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::pageDownKey)) {
        keysToPress.push_back(getKeyCode("PageDown"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::homeKey)) {
        keysToPress.push_back(getKeyCode("Home"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::endKey)) {
        keysToPress.push_back(getKeyCode("End"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F1Key)) {
        keysToPress.push_back(getKeyCode("F1"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F2Key)) {
        keysToPress.push_back(getKeyCode("F2"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F3Key)) {
        keysToPress.push_back(getKeyCode("F3"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F4Key)) {
        keysToPress.push_back(getKeyCode("F4"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F5Key)) {
        keysToPress.push_back(getKeyCode("F5"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F6Key)) {
        keysToPress.push_back(getKeyCode("F6"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F7Key)) {
        keysToPress.push_back(getKeyCode("F7"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F8Key)) {
        keysToPress.push_back(getKeyCode("F8"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F9Key)) {
        keysToPress.push_back(getKeyCode("F9"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F10Key)) {
        keysToPress.push_back(getKeyCode("F10"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F11Key)) {
        keysToPress.push_back(getKeyCode("F11"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F12Key)) {
        keysToPress.push_back(getKeyCode("F12"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F13Key)) {
        keysToPress.push_back(getKeyCode("F13"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F14Key)) {
        keysToPress.push_back(getKeyCode("F14"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F15Key)) {
        keysToPress.push_back(getKeyCode("F15"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F16Key)) {
        keysToPress.push_back(getKeyCode("F16"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F17Key)) {
        keysToPress.push_back(getKeyCode("F17"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F18Key)) {
        keysToPress.push_back(getKeyCode("F18"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::F19Key)) {
        keysToPress.push_back(getKeyCode("F19"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad0)) {
        keysToPress.push_back(getKeyCode("Numpad0"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad1)) {
        keysToPress.push_back(getKeyCode("Numpad1"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad2)) {
        keysToPress.push_back(getKeyCode("Numpad2"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad3)) {
        keysToPress.push_back(getKeyCode("Numpad3"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad4)) {
        keysToPress.push_back(getKeyCode("Numpad4"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad5)) {
        keysToPress.push_back(getKeyCode("Numpad5"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad6)) {
        keysToPress.push_back(getKeyCode("Numpad6"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad7)) {
        keysToPress.push_back(getKeyCode("Numpad7"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad8)) {
        keysToPress.push_back(getKeyCode("Numpad8"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPad9)) {
        keysToPress.push_back(getKeyCode("Numpad9"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadMultiply)) {
        keysToPress.push_back(getKeyCode("Numpad*"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadDelete)) {
        keysToPress.push_back(getKeyCode("NumpadClear"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadAdd)) {
        keysToPress.push_back(getKeyCode("Numpad+"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadSubtract)) {
        keysToPress.push_back(getKeyCode("Numpad-"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadEquals)) {
        keysToPress.push_back(getKeyCode("Numpad="));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadDivide)) {
        keysToPress.push_back(getKeyCode("Numpad/"));
    } else if (kp.isKeyCurrentlyDown(KeyPress::numberPadDecimalPoint)) {
        keysToPress.push_back(getKeyCode("Numpad."));
    } else {
        auto c = static_cast<char>(kp.getKeyCode());
        String key(CharPointer_UTF8(&c), 1);
        auto kc = getKeyCode(key.toUpperCase().toStdString());
        if (NOKEY != kc) {
            keysToPress.push_back(kc);
        }
    }

    Message<Key> msg(this);
    PLD(msg).setData(reinterpret_cast<const char*>(keysToPress.data()),
                     static_cast<int>(keysToPress.size() * sizeof(uint16_t)));
    LockByID lock(*this, KEYPRESSED);
    msg.send(m_cmdOut.get());

    return consumed;
}

void Client::updateScreenCaptureArea(int val) {
    traceScope();
    Message<UpdateScreenCaptureArea> msg(this);
    PLD(msg).setNumber(val);
    LockByID lock(*this, UPDATESCREENCAPTUREAREA);
    msg.send(m_cmdOut.get());
}

void Client::rescan(bool wipe) {
    traceScope();
    Message<Rescan> msg(this);
    PLD(msg).setNumber(wipe ? 1 : 0);
    LockByID lock(*this, RESCAN);
    msg.send(m_cmdOut.get());
}

void Client::restart() {
    traceScope();
    Message<Restart> msg(this);
    LockByID lock(*this, RESTART);
    msg.send(m_cmdOut.get());
}

void Client::updatePluginList(bool sendRequest) {
    traceScope();
    Message<PluginList> msg(this);
    LockByID lock(*this, UPDATEPLUGINLIST, false);  // NOT enforcing the lock as this is called from init()
    if (sendRequest) {
        msg.send(m_cmdOut.get());
    }
    m_plugins.clear();
    MessageHelper::Error err;
    if (!msg.read(m_cmdOut.get(), &err, LOAD_PLUGIN_TIMEOUT)) {
        logln("failed reading plugin list: " << err.toString());
        return;
    }
    String listChunk(PLD(msg).str, (size_t)*PLD(msg).size);
    auto list = StringArray::fromLines(listChunk);
    for (auto& line : list) {
        if (!line.isEmpty()) {
            m_plugins.push_back(ServerPlugin::fromString(line));
        }
    }
}

void Client::updateCPULoad() {
    traceScope();
    auto srvInfo = ServiceReceiver::hostToServerInfo(getServer().getHost());
    bool updated = false;
    int now = Time::getCurrentTime().getUTCOffsetSeconds();
    if (srvInfo.isValid()) {
        traceln("updating cpu load from mDNS");
        LockByID lock(*this, UPDATECPULOAD1);
        if (m_srvLoad != srvInfo.getLoad()) {
            m_srvLoad = srvInfo.getLoad();
            updated = true;
        }
        m_srvLoadLastUpdated = now;
    } else if (m_srvLoadLastUpdated + 10 < now) {
        traceln("updating cpu load via server request");
        Message<CPULoad> msg(this);
        LockByID lock(*this, UPDATECPULOAD2);
        msg.send(m_cmdOut.get());
        msg.read(m_cmdOut.get());
        if (m_srvLoad != PLD(msg).getFloat()) {
            m_srvLoad = PLD(msg).getFloat();
            updated = true;
        }
        m_srvLoadLastUpdated = now;
    }
    if (updated) {
        m_processor->setCPULoad(m_srvLoad);
    }
}

StreamingSocket* Client::accept(StreamingSocket& sock) const {
    traceScope();
    StreamingSocket* clnt = nullptr;
    int retry = 100;
    do {
        if (sock.waitUntilReady(true, 200) > 0) {
            clnt = sock.waitForNextConnection();
            if (nullptr != clnt) {
                break;
            }
        }
    } while (--retry > 0);
    return clnt;
}

template <>
std::shared_ptr<AudioStreamer<float>> Client::getStreamer() {
    std::lock_guard<std::mutex> lock(m_audioMtx);
    return m_audioStreamerF;
}

template <>
std::shared_ptr<AudioStreamer<double>> Client::getStreamer() {
    std::lock_guard<std::mutex> lock(m_audioMtx);
    return m_audioStreamerD;
}

bool Client::audioConnectionOk() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_audioMtx);
    return (nullptr != m_audioStreamerF && m_audioStreamerF->isOk()) ||
           (nullptr != m_audioStreamerD && m_audioStreamerD->isOk());
}

int Client::getNumActiveChannels() const {
    return (int)m_processor->getActiveChannels().getNumActiveChannelsCombined();
}

}  // namespace e47
