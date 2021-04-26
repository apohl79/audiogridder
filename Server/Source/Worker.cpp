/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Worker.hpp"
#include "KeyAndMouse.hpp"
#include "Defaults.hpp"
#include "App.hpp"
#include "CPUInfo.hpp"

#ifdef JUCE_MAC
#include <sys/socket.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

namespace e47 {

std::atomic_uint32_t Worker::count{0};
std::atomic_uint32_t Worker::runCount{0};

Worker::Worker(std::shared_ptr<StreamingSocket> masterSocket, const HandshakeRequest& cfg)
    : Thread("Worker"),
      LogTag("worker"),
      m_masterSocket(masterSocket),
      m_cfg(cfg),
      m_audio(std::make_shared<AudioWorker>(this)),
      m_screen(std::make_shared<ScreenWorker>(this)),
      m_msgFactory(this),
      m_keyWatcher(std::make_unique<KeyWatcher>(this)) {
    traceScope();
    initAsyncFunctors();
    count++;
}

Worker::~Worker() {
    traceScope();
    stopAsyncFunctors();
    if (nullptr != m_cmdIn && m_cmdIn->isConnected()) {
        m_cmdIn->close();
    }
    waitForThreadAndLog(this, this);
    count--;
}

void Worker::run() {
    traceScope();
    runCount++;
    setLogTagExtra("client:" + String::toHexString(m_cfg.clientId));

    m_noPluginListFilter = m_cfg.isFlag(HandshakeRequest::NO_PLUGINLIST_FILTER);

    // set master socket non-blocking
    if (!setNonBlocking(m_masterSocket->getRawSocketHandle())) {
        logln("failed to set master socket non-blocking");
    }

    m_cmdIn.reset(accept(m_masterSocket.get(), 2000));
    if (nullptr != m_cmdIn && m_cmdIn->isConnected()) {
        logln("client connected " << m_cmdIn->getHostName());
    } else {
        logln("no client, giving up");
        return;
    }

    // command sending socket
    m_cmdOut.reset(accept(m_masterSocket.get(), 2000));
    if (nullptr == m_cmdIn || !m_cmdIn->isConnected()) {
        logln("failed to establish command connection");
        return;
    }

    std::unique_ptr<StreamingSocket> sock;

    // start audio processing
    sock.reset(accept(m_masterSocket.get(), 2000));
    if (nullptr != sock && sock->isConnected()) {
        m_audio->init(std::move(sock), m_cfg.channelsIn, m_cfg.channelsOut, m_cfg.channelsSC, m_cfg.rate,
                      m_cfg.samplesPerBlock, m_cfg.doublePrecission,
                      m_cfg.isFlag(HandshakeRequest::CAN_DISABLE_SIDECHAIN));
        m_audio->startThread(Thread::realtimeAudioPriority);
    } else {
        logln("failed to establish audio connection");
    }

    // start screen capturing
    sock.reset(accept(m_masterSocket.get(), 2000));
    if (nullptr != sock && sock->isConnected()) {
        m_screen->init(std::move(sock));
        m_screen->startThread();
    } else {
        logln("failed to establish screen connection");
    }

    m_masterSocket->close();
    m_masterSocket.reset();

    // send list of plugins
    auto msgPL = std::make_shared<Message<PluginList>>(this);
    handleMessage(msgPL);

    // enter message loop
    logln("command processor started");
    while (!currentThreadShouldExit() && nullptr != m_cmdIn && m_cmdIn->isConnected() && m_audio->isThreadRunning() &&
           m_screen->isThreadRunning()) {
        MessageHelper::Error e;
        std::shared_ptr<Message<Any>> msg;
        msg = m_msgFactory.getNextMessage(m_cmdIn.get(), &e);
        if (nullptr != msg) {
            switch (msg->getType()) {
                case Quit::Type:
                    handleMessage(Message<Any>::convert<Quit>(msg));
                    break;
                case AddPlugin::Type:
                    handleMessage(Message<Any>::convert<AddPlugin>(msg));
                    break;
                case DelPlugin::Type:
                    handleMessage(Message<Any>::convert<DelPlugin>(msg));
                    break;
                case EditPlugin::Type:
                    handleMessage(Message<Any>::convert<EditPlugin>(msg));
                    break;
                case HidePlugin::Type:
                    handleMessage(Message<Any>::convert<HidePlugin>(msg));
                    break;
                case Mouse::Type:
                    handleMessage(Message<Any>::convert<Mouse>(msg));
                    break;
                case Key::Type:
                    handleMessage(Message<Any>::convert<Key>(msg));
                    break;
                case GetPluginSettings::Type:
                    handleMessage(Message<Any>::convert<GetPluginSettings>(msg));
                    break;
                case SetPluginSettings::Type:
                    handleMessage(Message<Any>::convert<SetPluginSettings>(msg));
                    break;
                case BypassPlugin::Type:
                    handleMessage(Message<Any>::convert<BypassPlugin>(msg));
                    break;
                case UnbypassPlugin::Type:
                    handleMessage(Message<Any>::convert<UnbypassPlugin>(msg));
                    break;
                case ExchangePlugins::Type:
                    handleMessage(Message<Any>::convert<ExchangePlugins>(msg));
                    break;
                case RecentsList::Type:
                    handleMessage(Message<Any>::convert<RecentsList>(msg));
                    break;
                case Preset::Type:
                    handleMessage(Message<Any>::convert<Preset>(msg));
                    break;
                case ParameterValue::Type:
                    handleMessage(Message<Any>::convert<ParameterValue>(msg));
                    break;
                case GetParameterValue::Type:
                    handleMessage(Message<Any>::convert<GetParameterValue>(msg));
                    break;
                case GetAllParameterValues::Type:
                    handleMessage(Message<Any>::convert<GetAllParameterValues>(msg));
                    break;
                case UpdateScreenCaptureArea::Type:
                    handleMessage(Message<Any>::convert<UpdateScreenCaptureArea>(msg));
                    break;
                case Rescan::Type:
                    handleMessage(Message<Any>::convert<Rescan>(msg));
                    break;
                case Restart::Type:
                    handleMessage(Message<Any>::convert<Restart>(msg));
                    break;
                case CPULoad::Type:
                    handleMessage(Message<Any>::convert<CPULoad>(msg));
                    break;
                case PluginList::Type:
                    handleMessage(Message<Any>::convert<PluginList>(msg));
                    break;
                default:
                    logln("unknown message type " << msg->getType());
            }
        } else if (e.code != MessageHelper::E_TIMEOUT) {
            logln("failed to get next message: " << e.toString());
            break;
        }
    }

    shutdown();
    m_audio->waitForThreadToExit(-1);
    m_audio.reset();
    m_screen->waitForThreadToExit(-1);
    m_screen.reset();
    logln("command processor terminated");
    runCount--;
}

void Worker::shutdown() {
    traceScope();
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    if (m_activeEditorIdx > -1) {
        m_screen->hideEditor();
    }
    if (nullptr != m_audio) {
        m_audio->shutdown();
    }
    if (nullptr != m_screen) {
        m_screen->shutdown();
    }
    signalThreadShouldExit();
}

void Worker::handleMessage(std::shared_ptr<Message<Quit>> /* msg */) {
    traceScope();
    shutdown();
}

void Worker::handleMessage(std::shared_ptr<Message<AddPlugin>> msg) {
    traceScope();
    auto id = pPLD(msg).getString();
    logln("adding plugin " << id << "...");
    String err;
    bool success = m_audio->addPlugin(id, err);
    std::shared_ptr<AGProcessor> proc;
    std::shared_ptr<AudioPluginInstance> plugin;
    json jresult;
    jresult["success"] = success;
    jresult["err"] = err.toStdString();
    if (success) {
        proc = m_audio->getProcessor(m_audio->getSize() - 1);
        plugin = proc->getPlugin();
        jresult["latency"] = m_audio->getLatencySamples();
        runOnMsgThreadSync([&] { jresult["hasEditor"] = plugin->hasEditor(); });
        proc->onParamValueChange = [this](int idx, int paramIdx, float val) {
            sendParamValueChanged(idx, paramIdx, val);
        };
        proc->onParamGestureChange = [this](int idx, int paramIdx, bool gestureIsStarting) {
            sendParamGestureChange(idx, paramIdx, gestureIsStarting);
        };
    }
    Message<AddPluginResult> msgResult(this);
    PLD(msgResult).setJson(jresult);
    if (!msgResult.send(m_cmdIn.get())) {
        logln("failed to send result");
        m_cmdIn->close();
        return;
    }
    logln("..." << (success ? "ok" : "failed"));
    if (!success) {
        m_cmdIn->close();
        return;
    }
    logln("sending presets...");
    String presets;
    bool first = true;
    for (int i = 0; i < plugin->getNumPrograms(); i++) {
        if (first) {
            first = false;
        } else {
            presets << "|";
        }
        presets << plugin->getProgramName(i);
    }
    Message<Presets> msgPresets(this);
    msgPresets.payload.setString(presets);
    if (!msgPresets.send(m_cmdIn.get())) {
        logln("failed to send Presets message");
        m_cmdIn->close();
        return;
    }
    logln("...ok");
    logln("sending parameters...");
    json jparams = json::array();
    for (auto& param : plugin->getParameters()) {
        json jparam = {{"idx", param->getParameterIndex()},
                       {"name", param->getName(32).toStdString()},
                       {"defaultValue", param->getDefaultValue()},
                       {"currentValue", param->getValue()},
                       {"category", param->getCategory()},
                       {"label", param->getLabel().toStdString()},
                       {"numSteps", param->getNumSteps()},
                       {"isBoolean", param->isBoolean()},
                       {"isDiscrete", param->isDiscrete()},
                       {"isMeta", param->isMetaParameter()},
                       {"isOrientInv", param->isOrientationInverted()},
                       {"minValue", param->getText(0.0f, 20).toStdString()},
                       {"maxValue", param->getText(1.0f, 20).toStdString()}};
        jparam["allValues"] = json::array();
        for (auto& val : param->getAllValueStrings()) {
            jparam["allValues"].push_back(val.toStdString());
        }
        if (jparam["allValues"].size() == 0 && param->isDiscrete() && param->getNumSteps() < 64) {
            // try filling values manually
            float step = 1.0f / (param->getNumSteps() - 1);
            for (int i = 0; i < param->getNumSteps(); i++) {
                auto val = param->getText(step * i, 32);
                if (val.isEmpty()) {
                    break;
                }
                jparam["allValues"].push_back(val.toStdString());
            }
        }
        jparams.push_back(jparam);
    }
    Message<Parameters> msgParams(this);
    PLD(msgParams).setJson(jparams);
    if (!msgParams.send(m_cmdIn.get())) {
        logln("failed to send Parameters message");
        m_cmdIn->close();
        return;
    }
    logln("...ok");
    logln("reading plugin settings...");
    Message<PluginSettings> msgSettings(this);
    MessageHelper::Error e;
    if (!msgSettings.read(m_cmdIn.get(), &e, 10000)) {
        logln("failed to read PluginSettings message:" << e.toString());
        m_cmdIn->close();
        return;
    }
    if (*msgSettings.payload.size > 0) {
        MemoryBlock block;
        block.append(msgSettings.payload.data, (size_t)*msgSettings.payload.size);
        plugin->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    }
    logln("...ok");
    m_audio->addToRecentsList(id, m_cmdIn->getHostName());
}

void Worker::handleMessage(std::shared_ptr<Message<DelPlugin>> msg) {
    traceScope();
    int idx = pPLD(msg).getNumber();
    if (idx == m_activeEditorIdx) {
        getApp()->getServer()->sandboxHideEditor();
        m_screen->hideEditor();
        m_activeEditorIdx = -1;
    }
    m_audio->delPlugin(idx);
    // send new updated latency samples back
    m_msgFactory.sendResult(m_cmdIn.get(), m_audio->getLatencySamples());
}

void Worker::handleMessage(std::shared_ptr<Message<EditPlugin>> msg) {
    traceScope();
    int idx = pDATA(msg)->index;
    auto proc = m_audio->getProcessor(idx);
    if (nullptr != proc) {
        getApp()->getServer()->sandboxShowEditor();
        m_screen->showEditor(proc, pDATA(msg)->x, pDATA(msg)->y);
        m_activeEditorIdx = idx;
        if (getApp()->getServer()->getScreenLocalMode()) {
            runOnMsgThreadAsync([this] { getApp()->addKeyListener(m_keyWatcher.get()); });
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<HidePlugin>> /* msg */, bool fromMaster) {
    traceScope();
    if (m_activeEditorIdx > -1) {
        if (!fromMaster) {
            getApp()->getServer()->sandboxHideEditor();
        }
        m_screen->hideEditor();
        m_activeEditorIdx = -1;
    }
    logln("hiding done (worker)");
}

void Worker::handleMessage(std::shared_ptr<Message<Mouse>> msg) {
    traceScope();
    auto ev = *pDATA(msg);
    runOnMsgThreadAsync([this, ev] {
        traceScope();
        auto point = getApp()->localPointToGlobal(Point<float>(ev.x, ev.y));
        if (ev.type == MouseEvType::WHEEL) {
            mouseScrollEvent(point.x, point.y, ev.deltaX, ev.deltaY, ev.isSmooth);
        } else {
            uint64_t flags = 0;
            if (ev.isShiftDown) {
                setShiftKey(flags);
            }
            if (ev.isCtrlDown) {
                setControlKey(flags);
            }
            if (ev.isAltDown) {
                setAltKey(flags);
            }
            mouseEvent(ev.type, point.x, point.y, flags);
        }
    });
}

void Worker::handleMessage(std::shared_ptr<Message<Key>> msg) {
    traceScope();
    runOnMsgThreadAsync([this, msg] {
        traceScope();
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
        keyEventDown(key, flags);
        keyEventUp(key, flags);
    });
}

void Worker::handleMessage(std::shared_ptr<Message<GetPluginSettings>> msg) {
    traceScope();
    auto proc = m_audio->getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        MemoryBlock block;
        proc->getStateInformation(block);
        Message<PluginSettings> ret(this);
        ret.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        ret.send(m_cmdIn.get());
    }
}

void Worker::handleMessage(std::shared_ptr<Message<SetPluginSettings>> msg) {
    traceScope();
    auto proc = m_audio->getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        Message<PluginSettings> msgSettings(this);
        if (!msgSettings.read(m_cmdIn.get())) {
            logln("failed to read PluginSettings message");
            m_cmdIn->close();
            return;
        }
        if (*msgSettings.payload.size > 0) {
            MemoryBlock block;
            block.append(msgSettings.payload.data, (size_t)*msgSettings.payload.size);
            proc->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<BypassPlugin>> msg) {
    traceScope();
    auto proc = m_audio->getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(true);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg) {
    traceScope();
    auto proc = m_audio->getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(false);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ExchangePlugins>> msg) {
    traceScope();
    m_audio->exchangePlugins(pDATA(msg)->idxA, pDATA(msg)->idxB);
}

void Worker::handleMessage(std::shared_ptr<Message<RecentsList>> msg) {
    traceScope();
    auto list = m_audio->getRecentsList(m_cmdIn->getHostName());
    pPLD(msg).setString(list);
    msg->send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<Preset>> msg) {
    traceScope();
    auto p = m_audio->getProcessor(pDATA(msg)->idx)->getPlugin();
    if (nullptr != p) {
        p->setCurrentProgram(pDATA(msg)->preset);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    traceScope();
    auto p = m_audio->getProcessor(pDATA(msg)->idx)->getPlugin();
    if (nullptr != p) {
        if (auto* param = p->getParameters()[pDATA(msg)->paramIdx]) {
            param->setValue(pDATA(msg)->value);
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<GetParameterValue>> msg) {
    traceScope();
    Message<ParameterValue> ret(this);
    DATA(ret)->idx = pDATA(msg)->idx;
    DATA(ret)->paramIdx = pDATA(msg)->paramIdx;
    DATA(ret)->value = m_audio->getParameterValue(pDATA(msg)->idx, pDATA(msg)->paramIdx);
    ret.send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<GetAllParameterValues>> msg) {
    traceScope();
    auto p = m_audio->getProcessor(pPLD(msg).getNumber())->getPlugin();
    if (nullptr != p) {
        for (auto* param : p->getParameters()) {
            Message<ParameterValue> ret(this);
            DATA(ret)->idx = pPLD(msg).getNumber();
            DATA(ret)->paramIdx = param->getParameterIndex();
            DATA(ret)->value = param->getValue();
            ret.send(m_cmdIn.get());
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UpdateScreenCaptureArea>> msg) {
    traceScope();
    getApp()->updateScreenCaptureArea(pPLD(msg).getNumber());
}

void Worker::handleMessage(std::shared_ptr<Message<Rescan>> msg) {
    traceScope();
    bool wipe = pPLD(msg).getNumber() == 1;
    runOnMsgThreadAsync([this, wipe] {
        traceScope();
        if (wipe) {
            getApp()->getServer()->getPluginList().clear();
            getApp()->getServer()->saveKnownPluginList();
        }
        getApp()->restartServer(true);
    });
}

void Worker::handleMessage(std::shared_ptr<Message<Restart>> /*msg*/) {
    traceScope();
    runOnMsgThreadAsync([this] {
        traceScope();
        getApp()->prepareShutdown(App::EXIT_RESTART);
    });
}

void Worker::handleMessage(std::shared_ptr<Message<CPULoad>> msg) {
    traceScope();
    pPLD(msg).setFloat(CPUInfo::getUsage());
    msg->send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<PluginList>> msg) {
    traceScope();
    String filterStr = pPLD(msg).getString();
    auto& pluginList = getApp()->getPluginList();
    String list;
    for (auto& plugin : pluginList.getTypes()) {
        bool inputMatch = m_noPluginListFilter;
        // exact match is fine
        inputMatch = (m_audio->getChannelsIn() == plugin.numInputChannels) || inputMatch;
        // hide plugins with no inputs if we have inputs
        inputMatch = (m_audio->getChannelsIn() > 0 && plugin.numInputChannels > 0) || inputMatch;
        // for instruments (no inputs) allow any plugin with the isInstrument flag
        inputMatch = (m_audio->getChannelsIn() == 0 && plugin.isInstrument) || inputMatch;
        // match filter string
        if (inputMatch && filterStr.isNotEmpty()) {
            inputMatch = plugin.descriptiveName.containsIgnoreCase(filterStr);
        }
        if (inputMatch) {
            list += AGProcessor::createString(plugin) + "\n";
        }
    }
    pPLD(msg).setString(list);
    msg->send(m_cmdIn.get());
}

void Worker::sendKeys(const std::vector<uint16_t>& keysToPress) {
    Message<Key> msg(this);
    PLD(msg).setData(reinterpret_cast<const char*>(keysToPress.data()),
                     static_cast<int>(keysToPress.size() * sizeof(uint16_t)));
    msg.send(m_cmdOut.get());
}

bool Worker::KeyWatcher::keyPressed(const KeyPress& kp, Component*) {
    std::vector<uint16_t> keysToPress;
    auto modkeys = kp.getModifiers();
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
        String key(CharPointer_UTF8(&c), 1);
        auto kc = getKeyCode(key.toUpperCase().toStdString());
        if (NOKEY != kc) {
            keysToPress.push_back(kc);
        }
    }
    worker->sendKeys(keysToPress);
    return true;
}

void Worker::sendParamValueChanged(int idx, int paramIdx, float val) {
    Message<ParameterValue> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = val;
    msg.send(m_cmdOut.get());
}

void Worker::sendParamGestureChange(int idx, int paramIdx, bool guestureIsStarting) {
    Message<ParameterGesture> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->gestureIsStarting = guestureIsStarting;
    msg.send(m_cmdOut.get());
}

}  // namespace e47
