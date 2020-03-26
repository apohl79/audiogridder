/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Client.hpp"
#include "PluginProcessor.hpp"

namespace e47 {

int Client::NUM_OF_BUFFERS = DEFAULT_NUM_OF_BUFFERS;

Client::Client(AudioGridderAudioProcessor* processor)
    : Thread("Client"),
      m_processor(processor),
      m_ready(false),
      m_cmd_socket(nullptr),
      m_audio_socket(nullptr),
      m_screen_socket(nullptr) {
    startThread();
}

Client::~Client() {
    stopThread(1000);
    close();
    waitForThreadToExit(-1);
}

void Client::run() {
    bool lastState = isReady();
    while (!currentThreadShouldExit()) {
        if (!isReady() && !currentThreadShouldExit()) {
            init();
        } else if (m_needsReconnect && !currentThreadShouldExit()) {
            close();
            init();
        }
        if (lastState != isReady()) {
            lastState = isReady();
            if (!lastState) {
                // disconnected, the callback might have not been called, so do it from here
                if (m_onCloseCallback) {
                    m_onCloseCallback();
                }
            }
        }
        int sleepfor = 20;
        while (!currentThreadShouldExit() && sleepfor-- > 0) {
            Thread::sleep(50);
        }
    }
}

void Client::setServer(const String& host, int port) {
    String currHost = getServerHostAndID();
    std::lock_guard<std::mutex> lock(m_srvMtx);
    if (currHost.compare(host) || m_srvPort != port) {
        auto hostParts = StringArray::fromTokens(host, ":", "");
        if (hostParts.size() > 1) {
            m_srvHost = hostParts[0];
            m_id = hostParts[1].getIntValue();
        } else {
            m_srvHost = host;
            m_id = 0;
        }
        m_srvPort = port;
        m_needsReconnect = true;
    }
}

String Client::getServerHost() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvHost;
}

String Client::getServerHostAndID() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    String h = m_srvHost;
    if (m_id > 0) {
        h << ":" << m_id;
    }
    return h;
}

int Client::getServerPort() {
    std::lock_guard<std::mutex> lock(m_srvMtx);
    return m_srvPort;
}

void Client::setPluginScreenUpdateCallback(ScreenUpdateCallback fn) {
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_pluginScreenUpdateCallback = fn;
}

void Client::setOnConnectCallback(OnConnectCallback fn) {
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_onConnectCallback = fn;
}

void Client::setOnCloseCallback(OnCloseCallback fn) {
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_onCloseCallback = fn;
}

void Client::init(int channels, double rate, int samplesPerBlock, bool doublePrecission) {
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_channels = channels;
    m_rate = rate;
    m_samplesPerBlock = samplesPerBlock;
    m_doublePrecission = doublePrecission;
}

void Client::init() {
    String host;
    int id;
    int port;
    {
        std::lock_guard<std::mutex> lock(m_srvMtx);
        host = m_srvHost;
        id = m_id;
        port = m_srvPort + m_id;
    }
    std::lock_guard<std::mutex> lock(m_clientMtx);
    if (!m_channels || !m_rate || !m_samplesPerBlock) {
        return;
    }
    std::cout << "connecting server " << host << ":" << id << std::endl;
    m_cmd_socket = std::make_unique<StreamingSocket>();
    if (m_cmd_socket->connect(host, port, 1000)) {
        StreamingSocket sock;
        int retry = 0;
        int clientPort;
        do {
            if (sock.createListener(DEFAULT_CLIENT_PORT - retry)) {
                clientPort = DEFAULT_CLIENT_PORT - retry;
                break;
            }
        } while (retry++ < 200);

        if (!sock.isConnected()) {
            std::cerr << "failed to create listener" << std::endl;
            return;
        }

        std::cout << "client listener created, PORT=" << clientPort << std::endl;

        // set master socket non-blocking
        fcntl(sock.getRawSocketHandle(), F_SETFL, fcntl(sock.getRawSocketHandle(), F_GETFL, 0) | O_NONBLOCK);

        Handshake cfg = {1, clientPort, m_channels, m_rate, m_samplesPerBlock, m_doublePrecission};
        if (!e47::send(m_cmd_socket.get(), reinterpret_cast<const char*>(&cfg), sizeof(cfg))) {
            m_cmd_socket->close();
            return;
        }

        m_audio_socket = std::unique_ptr<StreamingSocket>(accept(sock));
        if (nullptr != m_audio_socket) {
            std::cout << "audio connection established" << std::endl;
            if (m_doublePrecission) {
                m_audioStreamerD.reset(new AudioStreamer<double>(this, m_audio_socket.get()));
                m_audioStreamerD->startThread(Thread::realtimeAudioPriority);
            } else {
                m_audioStreamerF.reset(new AudioStreamer<float>(this, m_audio_socket.get()));
                m_audioStreamerF->startThread(Thread::realtimeAudioPriority);
            }
        } else {
            return;
        }

        m_screen_socket = std::unique_ptr<StreamingSocket>(accept(sock));
        if (nullptr != m_screen_socket) {
            std::cout << "screen connection established" << std::endl;
            m_screenWorker = std::make_unique<ScreenReceiver>(this, m_screen_socket.get());
            m_screenWorker->startThread();
        } else {
            return;
        }

        // receive plugin list
        m_plugins.clear();
        Message<PluginList> msg;
        if (!msg.read(m_cmd_socket.get())) {
            std::cerr << "failed reading plugin list" << std::endl;
            return;
        }
        String listChunk(msg.payload.str, *msg.payload.size);
        auto list = StringArray::fromLines(listChunk);
        for (auto& line : list) {
            if (!line.isEmpty()) {
                auto parts = StringArray::fromTokens(line, "|", "");
                m_plugins.push_back(ServerPlugin::fromString(line));
            }
        }
        m_ready = true;
        m_needsReconnect = false;

        if (m_onConnectCallback) {
            m_clientMtx.unlock();
            m_onConnectCallback();
        }
    } else {
        std::cerr << "connection to server failed" << std::endl;
    }
}

bool Client::isReady() {
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_ready = nullptr != m_cmd_socket && m_cmd_socket->isConnected() && nullptr != m_screen_socket &&
              m_screen_socket->isConnected() && nullptr != m_audio_socket && m_audio_socket->isConnected();
    return m_ready;
}

bool Client::isReadyLockFree() { return m_ready; }

void Client::close() {
    m_ready = false;
    std::lock_guard<std::mutex> lock(m_clientMtx);
    m_pluginScreenUpdateCallback = nullptr;
    m_plugins.clear();
    if (nullptr != m_screen_socket) {
        m_screen_socket->close();
    }
    if (nullptr != m_screenWorker && m_screenWorker->isThreadRunning()) {
        if (nullptr != m_screen_socket) {
            m_screen_socket->close();
        }
        m_screenWorker->signalThreadShouldExit();
        m_screenWorker->waitForThreadToExit(100);
        m_screenWorker.reset();
        m_screen_socket.reset();
    }
    if (nullptr != m_cmd_socket) {
        if (m_cmd_socket->isConnected()) {
            quit();
        }
        m_cmd_socket->close();
        m_cmd_socket.reset();
    }
    if (nullptr != m_audio_socket) {
        m_audio_socket->close();
    }
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
    m_audio_socket.reset();
    if (m_onCloseCallback) {
        m_clientMtx.unlock();
        m_onCloseCallback();
    }
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

bool Client::addPlugin(String id, String settings) {
    Message<AddPlugin> msg;
    msg.payload.setString(id);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    if (msg.send(m_cmd_socket.get())) {
        auto result = MessageFactory::getResult(m_cmd_socket.get());
        if (nullptr == result || result->getReturnCode() < 0) {
            return false;
        }
        m_latency = result->getReturnCode();
        Message<PluginSettings> msgSettings;
        if (settings.isNotEmpty()) {
            MemoryBlock block;
            block.fromBase64Encoding(settings);
            msgSettings.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        }
        return msgSettings.send(m_cmd_socket.get());
    }
    return false;
}

void Client::delPlugin(int idx) {
    Message<DelPlugin> msg;
    msg.payload.setNumber(idx);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
    auto result = MessageFactory::getResult(m_cmd_socket.get());
    if (nullptr != result && result->getReturnCode() > -1) {
        m_latency = result->getReturnCode();
    }
}

void Client::editPlugin(int idx) {
    Message<EditPlugin> msg;
    msg.payload.setNumber(idx);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

void Client::hidePlugin() {
    Message<HidePlugin> msg;
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

MemoryBlock Client::getPluginSettings(int idx) {
    MemoryBlock block;
    Message<GetPluginSettings> msg;
    msg.payload.setNumber(idx);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    if (!msg.send(m_cmd_socket.get())) {
        std::cerr << "failed to send GetPluginSettings message" << std::endl;
        m_cmd_socket->close();
    } else {
        Message<PluginSettings> res;
        if (res.read(m_cmd_socket.get())) {
            if (*res.payload.size > 0) {
                block.append(res.payload.data, *res.payload.size);
            }
        } else {
            std::cerr << "failed to read PluginSettings message" << std::endl;
            m_cmd_socket->close();
        }
    }
    return block;
}

void Client::bypassPlugin(int idx) {
    Message<BypassPlugin> msg;
    msg.payload.setNumber(idx);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

void Client::unbypassPlugin(int idx) {
    Message<UnbypassPlugin> msg;
    msg.payload.setNumber(idx);
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

void Client::exchangePlugins(int idxA, int idxB) {
    Message<ExchangePlugins> msg;
    msg.payload.data->idxA = idxA;
    msg.payload.data->idxB = idxB;
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

std::vector<ServerPlugin> Client::getRecents() {
    Message<RecentsList> msg;
    msg.send(m_cmd_socket.get());
    std::vector<ServerPlugin> recents;
    if (msg.read(m_cmd_socket.get())) {
        String listChunk(msg.payload.str, *msg.payload.size);
        auto list = StringArray::fromLines(listChunk);
        for (auto& line : list) {
            if (!line.isEmpty()) {
                recents.push_back(ServerPlugin::fromString(line));
            }
        }
    } else {
        std::cerr << "failed to read RecentsList message" << std::endl;
        m_cmd_socket->close();
    }
    return recents;
}

void Client::ScreenReceiver::run() {
    using MsgType = Message<ScreenCapture>;
    MsgType msg;
    MessageHelper::ReadError e;
    do {
        if (msg.read(m_socket, &e, 200)) {
            if (msg.payload.hdr->size > 0) {
                m_client->setPluginScreen(
                    std::make_shared<Image>(JPEGImageFormat::loadFrom(msg.payload.data, msg.payload.hdr->size)),
                    msg.payload.hdr->width, msg.payload.hdr->height);
            } else {
                m_client->setPluginScreen(nullptr, 0, 0);
            }
        }
    } while (!currentThreadShouldExit() && (e == MessageHelper::E_NONE || e == MessageHelper::E_TIMEOUT));
    signalThreadShouldExit();
    std::cout << "screen receiver terminated" << std::endl;
}

void Client::mouseMove(const MouseEvent& event) { sendMouseEvent(MouseEvType::MOVE, event.position); }

void Client::mouseEnter(const MouseEvent& event) { sendMouseEvent(MouseEvType::MOVE, event.position); }

void Client::mouseDown(const MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_DOWN, event.position);
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_DOWN, event.position);
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_DOWN, event.position);
    } else {
        std::cerr << "unhandled mouseDown event" << std::endl;
    }
}

void Client::mouseDrag(const MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_DRAG, event.position);
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_DRAG, event.position);
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_DRAG, event.position);
    } else {
        std::cerr << "unhandled mouseDrag event" << std::endl;
    }
}

void Client::mouseUp(const MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        sendMouseEvent(MouseEvType::LEFT_UP, event.position);
    } else if (event.mods.isRightButtonDown()) {
        sendMouseEvent(MouseEvType::RIGHT_UP, event.position);
    } else if (event.mods.isMiddleButtonDown()) {
        sendMouseEvent(MouseEvType::OTHER_UP, event.position);
    } else {
        std::cerr << "unhandled mouseUp event" << std::endl;
    }
}

void Client::mouseDoubleClick(const MouseEvent& event) {
    std::cout << "unhandled mouseDoubleClick " << event.position.x << ":" << event.position.y << std::endl;
}

void Client::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    std::cout << "unhanbdled mouseWheelMove " << event.position.x << ":" << event.position.y << std::endl;
}

void Client::sendMouseEvent(MouseEvType ev, Point<float> p) {
    Message<Mouse> msg;
    msg.payload.data->type = ev;
    msg.payload.data->x = p.x;
    msg.payload.data->y = p.y;
    std::lock_guard<std::mutex> lock(m_clientMtx);
    msg.send(m_cmd_socket.get());
}

bool Client::keyPressed(const KeyPress& kp, Component* originatingComponent) {
    if (kp.isValid()) {
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
            String key(&c, 1);
            auto kc = getKeyCode(key.toStdString());
            if (NOKEY != kc) {
                keysToPress.push_back(kc);
            }
        }

        Message<Key> msg;
        msg.payload.setData(reinterpret_cast<const char*>(keysToPress.data()),
                            static_cast<int>(keysToPress.size() * sizeof(uint16_t)));
        std::lock_guard<std::mutex> lock(m_clientMtx);
        msg.send(m_cmd_socket.get());
    }
    return true;
}

StreamingSocket* Client::accept(StreamingSocket& sock) const {
    StreamingSocket* clnt;
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
