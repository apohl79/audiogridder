/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Client.hpp"
#include "PluginProcessor.hpp"
#include "NumberConversion.hpp"

#ifdef JUCE_WINDOWS
#include "windows.h"
#else
#include "fcntl.h"
#endif

namespace e47 {

Client::Client(AudioGridderAudioProcessor* processor) : Thread("Client"), LogTag("client"), m_processor(processor) {
    logln("client created");
}

Client::~Client() {
    signalThreadShouldExit();
    close();
}

void Client::run() {
    logln("entering client loop");
    bool lastState = isReady();
    while (!currentThreadShouldExit()) {
        File cfg(PLUGIN_CONFIG_FILE);
        try {
            if (cfg.exists()) {
                FileInputStream fis(cfg);
                if (fis.openedOk()) {
                    json j = json::parse(fis.readEntireStreamAsString().toStdString());
                    if (j.find("NumberOfBuffers") != j.end()) {
                        int newNum = j["NumberOfBuffers"].get<int>();
                        if (NUM_OF_BUFFERS != newNum) {
                            logln("number of buffers changed from " << NUM_OF_BUFFERS << " to " << newNum);
                            NUM_OF_BUFFERS = newNum;
                            reconnect();
                        }
                    }
                    if (j.find("LoadPluginTimeout") != j.end()) {
                        int newNum = j["LoadPluginTimeout"].get<int>();
                        if (LOAD_PLUGIN_TIMEOUT != newNum) {
                            logln("timeout for leading a plugin changed from " << LOAD_PLUGIN_TIMEOUT << " to "
                                                                               << newNum);
                            LOAD_PLUGIN_TIMEOUT = newNum;
                        }
                    }
                    m_processor->loadConfig(j, true);
                } else {
                    logln("failed to open config file: " << fis.getStatus().getErrorMessage());
                }
            }
        } catch (json::parse_error& e) {
            logln("parsing config failed: " << e.what());
        }
        if (m_srvHost.isEmpty()) {
            auto servers = m_processor->getServersMDNS();
            if (servers.size() > 0) {
                setServer(servers[0]);
            }
        }
        if ((!isReady(LOAD_PLUGIN_TIMEOUT + 5000) || m_needsReconnect) && m_srvHost.isNotEmpty() &&
            !currentThreadShouldExit()) {
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
        if (isReadyLockFree()) {
            updateCPULoad();
        }
        int sleepfor = 20;
        while (!currentThreadShouldExit() && sleepfor-- > 0) {
            Thread::sleep(50);
        }
    }
    logln("client loop terminated");
}

void Client::setServer(const ServerInfo& srv) {
    logln("setting server to " << srv.toString());
    String currHost = getServerHostAndID();
    std::lock_guard<std::mutex> lock(m_srvMtx);
    if (currHost.compare(srv.getHostAndID())) {
        m_srvHost = srv.getHost();
        m_srvId = srv.getID();
        m_needsReconnect = true;
    }
}

String Client::getServerHost() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvHost;
}

int Client::getServerID() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvId;
}

String Client::getServerHostAndID() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    String h = m_srvHost;
    if (m_srvId > 0) {
        h << ":" << m_srvId;
    }
    return h;
}

int Client::getServerPort() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvPort;
}

void Client::setPluginScreenUpdateCallback(ScreenUpdateCallback fn) {
    LockByID lock(*this, SETPLUGINSCREENUPDATECALLBACK);
    m_pluginScreenUpdateCallback = fn;
}

void Client::setOnConnectCallback(OnConnectCallback fn) {
    LockByID lock(*this, SETONCONNECTCALLBACK);
    m_onConnectCallback = fn;
}

void Client::setOnCloseCallback(OnCloseCallback fn) {
    LockByID lock(*this, SETONCLOSECALLBACK);
    m_onCloseCallback = fn;
}

void Client::init(int channelsIn, int channelsOut, double rate, int samplesPerBlock, bool doublePrecission) {
    logln("init: channelsIn=" << channelsIn << " channelsOut=" << channelsOut << " rate=" << rate << " samplesPerBlock="
                              << samplesPerBlock << " doublePrecission=" << as<int>(doublePrecission));
    LockByID lock(*this, INIT1);
    if (!m_ready || m_channelsIn != channelsIn || m_channelsOut != channelsOut || m_rate != rate ||
        m_samplesPerBlock != samplesPerBlock || m_doublePrecission != doublePrecission) {
        m_channelsIn = channelsIn;
        m_channelsOut = channelsOut;
        m_rate = rate;
        m_samplesPerBlock = samplesPerBlock;
        m_doublePrecission = doublePrecission;
        m_needsReconnect = true;
        m_ready = false;
        logln("init: paramater change, requesting reconnect");
    }
}

void Client::init() {
    String host;
    int id;
    int port;
    {
        std::lock_guard<std::mutex> lock(m_srvMtx);
        host = m_srvHost;
        id = m_srvId;
        port = m_srvPort + m_srvId;
    }
    LockByID lock(*this, INIT2);
    m_error = true;
    if (m_channelsOut == 0 || m_rate == 0.0 || m_samplesPerBlock == 0) {
        return;
    }
    logln("connecting server " << host << ":" << id);
    m_cmd_socket = std::make_unique<StreamingSocket>();
    if (m_cmd_socket->connect(host, port, 1000)) {
        StreamingSocket sock;
        int retry = 0;
        int clientPort = DEFAULT_CLIENT_PORT;
        do {
            if (sock.createListener(DEFAULT_CLIENT_PORT - retry)) {
                clientPort = DEFAULT_CLIENT_PORT - retry;
                break;
            }
        } while (retry++ < 200);

        if (!sock.isConnected()) {
            logln("failed to create listener");
            return;
        }

        logln("client listener created, PORT=" << clientPort);

        auto setSocketBlockingState = [](int handle, bool shouldBlock) noexcept -> bool {
#ifdef JUCE_WINDOWS
            DWORD nonBlocking = shouldBlock ? 0 : 1;
            return ioctlsocket(handle, FIONBIO, &nonBlocking) == 0;
#else
            int socketFlags = fcntl(handle, F_GETFL, 0);
            if (socketFlags == -1) {
                return false;
            }
            if (shouldBlock) {
                socketFlags &= ~O_NONBLOCK;
            } else {
                socketFlags |= O_NONBLOCK;
            }
            return fcntl(handle, F_SETFL, socketFlags) == 0;
#endif
        };

        // set master socket non-blocking
        if (!setSocketBlockingState(sock.getRawSocketHandle(), false)) {
            logln("failed to set master socket non-blocking");
        }

        Handshake cfg = {1, clientPort, m_channelsIn, m_channelsOut, m_rate, m_samplesPerBlock, m_doublePrecission};
        if (!e47::send(m_cmd_socket.get(), reinterpret_cast<const char*>(&cfg), sizeof(cfg))) {
            m_cmd_socket->close();
            return;
        }

        auto* audioSock = accept(sock);
        if (nullptr != audioSock) {
            logln("audio connection established");
            std::lock_guard<std::mutex> audiolck(m_audioMtx);
            if (m_doublePrecission) {
                m_audioStreamerD.reset(new AudioStreamer<double>(this, audioSock));
                m_audioStreamerD->startThread(Thread::realtimeAudioPriority);
            } else {
                m_audioStreamerF.reset(new AudioStreamer<float>(this, audioSock));
                m_audioStreamerF->startThread(Thread::realtimeAudioPriority);
            }
        } else {
            return;
        }

        m_screen_socket = std::unique_ptr<StreamingSocket>(accept(sock));
        if (nullptr != m_screen_socket) {
            logln("screen connection established");
            m_screenWorker = std::make_unique<ScreenReceiver>(this, m_screen_socket.get());
            m_screenWorker->startThread();
        } else {
            return;
        }

        // receive plugin list
        m_plugins.clear();
        Message<PluginList> msg;
        MessageHelper::Error err;
        if (!msg.read(m_cmd_socket.get(), &err, 5000)) {
            logln("failed reading plugin list: " << err.toString());
            return;
        }
        String listChunk(PLD(msg).str, as<size_t>(*PLD(msg).size));
        auto list = StringArray::fromLines(listChunk);
        for (auto& line : list) {
            if (!line.isEmpty()) {
                auto parts = StringArray::fromTokens(line, "|", "");
                m_plugins.push_back(ServerPlugin::fromString(line));
            }
        }
        m_ready = true;
        m_error = false;
        m_needsReconnect = false;
    } else {
        logln("connection to server failed");
    }
}

bool Client::isReady(int timeout) {
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
        m_ready = !m_error && !m_needsReconnect && nullptr != m_cmd_socket && m_cmd_socket->isConnected() &&
                  m_screenWorker->isThreadRunning() && nullptr != m_screen_socket && m_screen_socket->isConnected() &&
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
    if (m_ready) {
        logln("closing");
    }
    m_ready = false;
    LockByID lock(*this, CLOSE);
    m_plugins.clear();
    if (nullptr != m_screen_socket && m_screen_socket->isConnected()) {
        m_screen_socket->close();
    }
    if (nullptr != m_screenWorker && m_screenWorker->isThreadRunning()) {
        m_screenWorker->signalThreadShouldExit();
        m_screenWorker->waitForThreadToExit(100);
        m_screenWorker.reset();
        m_screen_socket.reset();
    }
    if (nullptr != m_cmd_socket) {
        if (m_cmd_socket->isConnected()) {
            m_cmd_socket->close();
        }
        m_cmd_socket.reset();
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
    std::lock_guard<std::mutex> lock(m_pluginScreenMtx);
    return *m_pluginScreen;
}

void Client::setPluginScreen(std::shared_ptr<Image> img, int w, int h) {
    std::lock_guard<std::mutex> lock(m_pluginScreenMtx);
    m_pluginScreen = img;
    if (m_pluginScreenUpdateCallback) {
        m_pluginScreenUpdateCallback(m_pluginScreen, w, h);
    }
}

void Client::quit() {
    // called from close which already holds a lock
    Message<Quit> msg;
    msg.send(m_cmd_socket.get());
}

bool Client::addPlugin(String id, StringArray& presets, Array<Parameter>& params, String settings) {
    if (!isReadyLockFree()) {
        return false;
    };
    MessageHelper::Error err;
    Message<AddPlugin> msg;
    PLD(msg).setString(id);
    LockByID lock(*this, ADDPLUGIN);
    if (msg.send(m_cmd_socket.get())) {
        auto result = MessageFactory::getResult(m_cmd_socket.get(), LOAD_PLUGIN_TIMEOUT, &err);
        if (nullptr == result) {
            logln("  failed to get result: " << err.toString());
            return false;
        }
        if (result->getReturnCode() < 0) {
            logln("  negative return code");
            return false;
        }
        m_latency = result->getReturnCode();
        Message<Presets> msgPresets;
        if (!msgPresets.read(m_cmd_socket.get(), &err)) {
            logln("  failed to read presets: " << err.toString());
            return false;
        }
        presets = StringArray::fromTokens(msgPresets.payload.getString(), "|", "");
        Message<Parameters> msgParams;
        if (!msgParams.read(m_cmd_socket.get(), &err)) {
            logln("  failed to read parameters: " << err.toString());
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
        Message<PluginSettings> msgSettings;
        if (settings.isNotEmpty()) {
            MemoryBlock block;
            block.fromBase64Encoding(settings);
            msgSettings.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        }
        if (!msgSettings.send(m_cmd_socket.get())) {
            logln("  failed to send settings");
            return false;
        }
        return true;
    }
    return false;
}

void Client::delPlugin(int idx) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<DelPlugin> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, DELPLUGIN);
    msg.send(m_cmd_socket.get());
    auto result = MessageFactory::getResult(m_cmd_socket.get());
    if (nullptr != result && result->getReturnCode() > -1) {
        m_latency = result->getReturnCode();
    }
}

void Client::editPlugin(int idx) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<EditPlugin> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, EDITPLUGIN);
    msg.send(m_cmd_socket.get());
}

void Client::hidePlugin() {
    if (!isReadyLockFree()) {
        return;
    };
    Message<HidePlugin> msg;
    LockByID lock(*this, HIDEPLUGIN);
    msg.send(m_cmd_socket.get());
}

MemoryBlock Client::getPluginSettings(int idx) {
    MemoryBlock block;
    if (!isReadyLockFree()) {
        return block;
    };
    Message<GetPluginSettings> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, GETPLUGINSETTINGS);
    if (!msg.send(m_cmd_socket.get())) {
        m_error = true;
    } else {
        Message<PluginSettings> res;
        MessageHelper::Error err;
        if (res.read(m_cmd_socket.get(), &err, 5000)) {
            if (*res.payload.size > 0) {
                block.append(res.payload.data, as<size_t>(*res.payload.size));
            }
        } else {
            logln(getLoadedPluginsString() << ": failed to read PluginSettings message: " << err.toString());
            m_error = true;
        }
    }
    return block;
}

void Client::setPluginSettings(int idx, String settings) {
    Message<SetPluginSettings> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, SETPLUGINSETTINGS);
    if (!msg.send(m_cmd_socket.get())) {
        m_error = true;
    } else {
        Message<PluginSettings> msgSettings;
        if (settings.isNotEmpty()) {
            MemoryBlock block;
            block.fromBase64Encoding(settings);
            msgSettings.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        }
        if (!msgSettings.send(m_cmd_socket.get())) {
            logln("failed to send settings");
            m_error = true;
        }
    }
}

void Client::bypassPlugin(int idx) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<BypassPlugin> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, BYPASSPLUGIN);
    msg.send(m_cmd_socket.get());
}

void Client::unbypassPlugin(int idx) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<UnbypassPlugin> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, UNBYPASSPLUGIN);
    msg.send(m_cmd_socket.get());
}

void Client::exchangePlugins(int idxA, int idxB) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<ExchangePlugins> msg;
    DATA(msg)->idxA = idxA;
    DATA(msg)->idxB = idxB;
    LockByID lock(*this, EXCHANGEPLUGINS);
    msg.send(m_cmd_socket.get());
}

std::vector<ServerPlugin> Client::getRecents() {
    std::vector<ServerPlugin> recents;
    if (!isReadyLockFree()) {
        return recents;
    };
    Message<RecentsList> msg;
    MessageHelper::Error err;
    LockByID lock(*this, GETRECENTS);
    msg.send(m_cmd_socket.get());
    if (msg.read(m_cmd_socket.get(), &err, 5000)) {
        String listChunk(PLD(msg).str, as<size_t>(*PLD(msg).size));
        auto list = StringArray::fromLines(listChunk);
        for (auto& line : list) {
            if (!line.isEmpty()) {
                recents.push_back(ServerPlugin::fromString(line));
            }
        }
    } else {
        logln(getLoadedPluginsString() << ": failed to read RecentsList message: " << err.toString());
        m_error = true;
    }
    return recents;
}

void Client::setPreset(int idx, int preset) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<Preset> msg;
    DATA(msg)->idx = idx;
    DATA(msg)->preset = preset;
    LockByID lock(*this, SETPRESET);
    msg.send(m_cmd_socket.get());
}

float Client::getParameterValue(int idx, int paramIdx) {
    if (!isReadyLockFree()) {
        return 0;
    };
    Message<GetParameterValue> msg;
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    LockByID lock(*this, GETPARAMETERVALUE);
    msg.send(m_cmd_socket.get());
    Message<ParameterValue> ret;
    MessageHelper::Error err;
    if (ret.read(m_cmd_socket.get(), &err)) {
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
    if (!isReadyLockFree()) {
        return;
    };
    Message<ParameterValue> msg;
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = val;
    LockByID lock(*this, SETPARAMETERVALUE);
    msg.send(m_cmd_socket.get());
}

Array<Client::ParameterResult> Client::getAllParameterValues(int idx, int count) {
    if (!isReadyLockFree()) {
        return {};
    };
    Message<GetAllParameterValues> msg;
    PLD(msg).setNumber(idx);
    LockByID lock(*this, GETALLPARAMETERVALUES);
    msg.send(m_cmd_socket.get());
    Array<Client::ParameterResult> ret;
    for (int i = 0; i < count; i++) {
        Message<ParameterValue> msgVal;
        MessageHelper::Error err;
        if (msgVal.read(m_cmd_socket.get(), &err)) {
            if (idx == DATA(msgVal)->idx) {
                ret.add({DATA(msgVal)->paramIdx, DATA(msgVal)->value});
            }
        }
    }
    return ret;
}

void Client::ScreenReceiver::run() {
    using MsgType = Message<ScreenCapture>;
    MsgType msg;
    MessageHelper::Error err;
    do {
        if (msg.read(m_socket, &err, 200)) {
            if (PLD(msg).hdr->size > 0) {
                int width = (int)(PLD(msg).hdr->width / PLD(msg).hdr->scale);
                int height = (int)(PLD(msg).hdr->height / PLD(msg).hdr->scale);
                auto img = m_imgReader.read(DATA(msg), PLD(msg).hdr->size, PLD(msg).hdr->width, PLD(msg).hdr->height,
                                            PLD(msg).hdr->scale);
                if (nullptr != img) {
                    m_client->setPluginScreen(img, width, height);
                }
            } else {
                m_client->setPluginScreen(nullptr, 0, 0);
            }
        }
    } while (!currentThreadShouldExit() && (err.code == MessageHelper::E_NONE || err.code == MessageHelper::E_TIMEOUT));
    if (!currentThreadShouldExit()) {
        logln("screen receiver failed to read message: " << err.toString());
        signalThreadShouldExit();
    }
    m_client->m_error = true;
    logln("screen receiver terminated");
}

void Client::mouseMove(const MouseEvent& event) {
    sendMouseEvent(MouseEvType::MOVE, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                   event.mods.isAltDown());
}

void Client::mouseEnter(const MouseEvent& event) {
    sendMouseEvent(MouseEvType::MOVE, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                   event.mods.isAltDown());
}

void Client::mouseDown(const MouseEvent& event) {
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

void Client::mouseDoubleClick(const MouseEvent& /*event*/) {}

void Client::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    if (!wheel.isInertial) {
        sendMouseEvent(MouseEvType::WHEEL, event.position, event.mods.isShiftDown(), event.mods.isCtrlDown(),
                       event.mods.isAltDown(), &wheel);
    }
}

void Client::sendMouseEvent(MouseEvType ev, Point<float> p, bool isShiftDown, bool isCtrlDown, bool isAltDown,
                            const MouseWheelDetails* wheel) {
    if (!isReadyLockFree()) {
        return;
    };
    Message<Mouse> msg;
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
    msg.send(m_cmd_socket.get());
}

bool Client::keyPressed(const KeyPress& kp, Component* /* originatingComponent */) {
    if (!isReadyLockFree() || m_processor->getActivePlugin() == -1) {
        return false;
    };
    bool consumed = true;
    auto modkeys = kp.getModifiers();
    std::vector<uint16_t> keysToPress;
    if (modkeys.isShiftDown()) {
        keysToPress.push_back(getKeyCode("Shift"));
    }
    if (modkeys.isCtrlDown()) {
        keysToPress.push_back(getKeyCode("Control"));
    }
    if (modkeys.isAltDown()) {
        keysToPress.push_back(getKeyCode("Option"));
    }
    if (kp.isKeyCurrentlyDown(KeyPress::escapeKey)) {
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
    } else {
        auto c = static_cast<char>(kp.getKeyCode());
        String key(CharPointer_UTF8(&c), 1);
        auto kc = getKeyCode(key.toStdString());
        if (NOKEY != kc) {
            keysToPress.push_back(kc);
        }
    }

    Message<Key> msg;
    PLD(msg).setData(reinterpret_cast<const char*>(keysToPress.data()),
                     static_cast<int>(keysToPress.size() * sizeof(uint16_t)));
    LockByID lock(*this, KEYPRESSED);
    msg.send(m_cmd_socket.get());

    return consumed;
}

void Client::updateScreenCaptureArea(int val) {
    Message<UpdateScreenCaptureArea> msg;
    PLD(msg).setNumber(val);
    LockByID lock(*this, UPDATESCREENCAPTUREAREA);
    msg.send(m_cmd_socket.get());
}

void Client::rescan(bool wipe) {
    Message<Rescan> msg;
    PLD(msg).setNumber(wipe ? 1 : 0);
    LockByID lock(*this, RESCAN);
    msg.send(m_cmd_socket.get());
}

void Client::updateCPULoad() {
    Message<CPULoad> msg;
    LockByID lock(*this, UPDATECPULOAD);
    msg.send(m_cmd_socket.get());
    msg.read(m_cmd_socket.get());
    m_srvLoad = PLD(msg).getFloat();
    m_processor->setCPULoad(m_srvLoad);
}

StreamingSocket* Client::accept(StreamingSocket& sock) const {
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

String Client::getLoadedPluginsString() {
    LockByID lock(*this, GETLOADEDPLUGINSSTRING, false);
    String ret;
    bool first = true;
    for (auto& p : m_processor->getLoadedPlugins()) {
        if (first) {
            first = false;
        } else {
            ret << " > ";
        }
        ret << p.name;
    }
    return ret;
}

}  // namespace e47
