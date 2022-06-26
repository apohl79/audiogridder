/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Worker.hpp"
#include "Server.hpp"
#include "Processor.hpp"
#include "KeyAndMouse.hpp"
#include "Defaults.hpp"
#include "App.hpp"
#include "CPUInfo.hpp"
#include "ChannelSet.hpp"

#ifdef JUCE_MAC
#include <sys/socket.h>
#include <fcntl.h>
#else
#include <windows.h>
#endif

namespace e47 {

std::atomic_uint32_t Worker::count{0};
std::atomic_uint32_t Worker::runCount{0};

Worker::Worker(std::shared_ptr<StreamingSocket> masterSocket, const HandshakeRequest& cfg, int sandboxModeRuntime)
    : Thread("Worker"),
      LogTag("worker"),
      m_masterSocket(masterSocket),
      m_cfg(cfg),
      m_audio(std::make_shared<AudioWorker>(this)),
      m_screen(std::make_shared<ScreenWorker>(this)),
      m_msgFactory(this),
      m_sandboxModeRuntime(sandboxModeRuntime),
      m_keyWatcher(std::make_unique<KeyWatcher>(this)),
      m_clipboardTracker(std::make_unique<ClipboardTracker>(this)) {
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
    m_cmdIn.reset();
    m_cmdOut.reset();
    m_audio.reset();
    m_screen.reset();
    m_keyWatcher.reset();
    m_clipboardTracker.reset();
    count--;
}

void Worker::run() {
    traceScope();
    runCount++;
    setLogTagExtra("client:" + String::toHexString(m_cfg.clientId));

    getApp()->setWorkerErrorCallback(getThreadId(), [this](const String& err) {
        if (isThreadRunning()) {
            sendError(err);
        }
    });

    m_noPluginListFilter = m_cfg.isFlag(HandshakeRequest::NO_PLUGINLIST_FILTER);

    // set master socket non-blocking
    if (!setNonBlocking(m_masterSocket->getRawSocketHandle())) {
        logln("failed to set master socket non-blocking");
    }

    m_cmdIn.reset(accept(m_masterSocket.get(), 5000));
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
        m_audio->init(std::move(sock), m_cfg);
        m_audio->startThread(Thread::realtimeAudioPriority);
    } else {
        logln("failed to establish audio connection");
    }

    // start screen capturing
    if (m_sandboxModeRuntime != Server::SANDBOX_PLUGIN) {
        sock.reset(accept(m_masterSocket.get(), 2000));
        if (nullptr != sock && sock->isConnected()) {
            m_screen->init(std::move(sock));
            m_screen->startThread();
        } else {
            logln("failed to establish screen connection");
        }
    }

    m_masterSocket->close();
    m_masterSocket.reset();

    // send list of plugins
    if (m_sandboxModeRuntime != Server::SANDBOX_PLUGIN) {
        auto msgPL = std::make_shared<Message<PluginList>>(this);
        handleMessage(msgPL);
    }

    // enter message loop
    logln("command processor started");
    while (!threadShouldExit() && nullptr != m_cmdIn && m_cmdIn->isConnected() && m_audio->isOkNoLock() &&
           m_screen->isOkNoLock()) {
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
                case GetScreenBounds::Type:
                    handleMessage(Message<Any>::convert<GetScreenBounds>(msg));
                    break;
                case Clipboard::Type:
                    handleMessage(Message<Any>::convert<Clipboard>(msg));
                    break;
                case SetMonoChannels::Type:
                    handleMessage(Message<Any>::convert<SetMonoChannels>(msg));
                    break;
                default:
                    logln("unknown message type " << msg->getType());
            }
        } else if (e.code != MessageHelper::E_TIMEOUT) {
            logln("failed to get next message: " << e.toString());
            break;
        }
    }

    getApp()->setWorkerErrorCallback(getThreadId(), nullptr);

    if (nullptr != m_screen) {
        if (m_activeEditorIdx > -1) {
            m_screen->hideEditor();
        }
        m_screen->shutdown();
        m_screen->waitForThreadToExit(-1);
    }

    if (nullptr != m_audio) {
        m_audio->shutdown();
        m_audio->waitForThreadToExit(-1);
    }

    logln("command processor terminated");
    runCount--;
}

void Worker::shutdown() {
    traceScope();
    signalThreadShouldExit();
}

void Worker::handleMessage(std::shared_ptr<Message<Quit>> /* msg */) {
    traceScope();
    shutdown();
}

void Worker::handleMessage(std::shared_ptr<Message<AddPlugin>> msg) {
    traceScope();
    auto jmsg = pPLD(msg).getJson();
    auto id = jsonGetValue(jmsg, "id", String());
    auto settings = jsonGetValue(jmsg, "settings", String());
    auto layout = jsonGetValue(jmsg, "layout", String());
    auto monoChannels = jsonGetValue(jmsg, "monoChannels", 0ull);

    logln("adding plugin " << id << "...");

    String err;
    bool wasSidechainDisabled = m_audio->isSidechainDisabled();
    bool success = m_audio->addPlugin(id, settings, layout, monoChannels, err);
    if (!success) {
        logln("error: " << err);
    }
    std::shared_ptr<Processor> proc;
    json jresult;
    jresult["success"] = success;
    jresult["err"] = err.toStdString();
    if (success) {
        proc = m_audio->getProcessor(m_audio->getSize() - 1);
        jresult["latency"] = m_audio->getLatencySamples();
        jresult["disabledSideChain"] = !wasSidechainDisabled && m_audio->isSidechainDisabled();
        jresult["name"] = proc->getName().toStdString();
        jresult["hasEditor"] = proc->hasEditor();
        jresult["supportsDoublePrecision"] = proc->supportsDoublePrecisionProcessing();
        jresult["channelInstances"] = proc->getChannelInstances();
        auto ts = proc->getTailLengthSeconds();
        if (ts == std::numeric_limits<double>::infinity()) {
            ts = 0.0;
        }
        jresult["tailSeconds"] = ts;
        jresult["numOutputChannels"] = proc->getTotalNumOutputChannels();
        proc->setCallbacks(
            [this, ctx = getAsyncContext()](int idx, int channel, int paramIdx, float val) mutable {
                ctx.execute([this, idx, channel, paramIdx, val] { sendParamValueChange(idx, channel, paramIdx, val); });
            },
            [this, ctx = getAsyncContext()](int idx, int channel, int paramIdx, bool gestureIsStarting) mutable {
                ctx.execute([this, idx, channel, paramIdx, gestureIsStarting] {
                    sendParamGestureChange(idx, channel, paramIdx, gestureIsStarting);
                });
            },
            [this, ctx = getAsyncContext()](Message<Key>& m) mutable {
                ctx.execute([this, &m] {
                    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
                    m.send(m_cmdOut.get());
                });
            },
            [this, ctx = getAsyncContext()](int idx, bool ok, const String& procErr) mutable {
                ctx.execute([this, idx, ok, &procErr] { sendStatusChange(idx, ok, procErr); });
            });
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
        return;
    }
    logln("sending presets...");
    String presets;
    bool first = true;
    for (int i = 0; i < proc->getNumPrograms(); i++) {
        if (first) {
            first = false;
        } else {
            presets << "|";
        }
        presets << proc->getProgramName(i);
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
    Message<Parameters> msgParams(this);
    PLD(msgParams).setJson(proc->getParameters());
    if (!msgParams.send(m_cmdIn.get())) {
        logln("failed to send Parameters message");
        m_cmdIn->close();
        return;
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
        m_clipboardTracker->stop();
        m_activeEditorIdx = -1;
    }
    m_audio->delPlugin(idx);
    // send new updated latency samples back
    m_msgFactory.sendResult(m_cmdIn.get(), m_audio->getLatencySamples());
}

void Worker::handleMessage(std::shared_ptr<Message<EditPlugin>> msg) {
    traceScope();
    int idx = pDATA(msg)->index;
    if (auto proc = m_audio->getProcessor(idx)) {
        getApp()->getServer()->sandboxShowEditor();
        m_screen->showEditor(getThreadId(), proc, pDATA(msg)->channel, pDATA(msg)->x, pDATA(msg)->y,
                             [this, idx] { sendHideEditor(idx); });
        m_activeEditorIdx = idx;
        if (getApp()->getServer()->getScreenLocalMode()) {
            runOnMsgThreadAsync([this] { getApp()->addKeyListener(getThreadId(), m_keyWatcher.get()); });
        } else if (!getApp()->getServer()->getScreenCapturingOff()) {
            m_clipboardTracker->start();
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
        m_clipboardTracker->stop();
        m_activeEditorIdx = -1;
    }
    logln("hiding done (worker)");
}

void Worker::handleMessage(std::shared_ptr<Message<Mouse>> msg) {
    traceScope();
    auto ev = *pDATA(msg);
    runOnMsgThreadAsync([this, ev] {
        traceScope();
        if (m_activeEditorIdx > -1) {
            auto point = getApp()->localPointToGlobal(getThreadId(), Point<float>(ev.x, ev.y));
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
        }
    });
}

void Worker::handleMessage(std::shared_ptr<Message<Key>> msg) {
    traceScope();
    runOnMsgThreadAsync([this, msg] {
        traceScope();
        if (m_activeEditorIdx > -1) {
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
                } else if (isCopyKey(codes[i])) {
                    setCopyKeys(key, flags);
                } else if (isPasteKey(codes[i])) {
                    setPasteKeys(key, flags);
                } else if (isCutKey(codes[i])) {
                    setCutKeys(key, flags);
                } else if (isSelectAllKey(codes[i])) {
                    setSelectAllKeys(key, flags);
                } else {
                    key = codes[i];
                }
            }
            keyEventDown(key, flags);
            keyEventUp(key, flags);
        }
    });
}

void Worker::handleMessage(std::shared_ptr<Message<GetPluginSettings>> msg) {
    traceScope();
    String settings;
    if (auto proc = m_audio->getProcessor(pPLD(msg).getNumber())) {
        // Load plugin state on the message thread
        proc->getStateInformation(settings);
    } else {
        logln("error: failed to read plugin settings: invalid index " << pPLD(msg).getNumber());
    }
    Message<PluginSettings> ret(this);
    PLD(ret).setString(settings);
    ret.send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<SetPluginSettings>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pPLD(msg).getNumber())) {
        Message<PluginSettings> msgSettings(this);
        if (!msgSettings.read(m_cmdIn.get())) {
            logln("failed to read PluginSettings message");
            m_cmdIn->close();
            return;
        }
        auto settings = PLD(msgSettings).getString();
        if (settings.length() > 0) {
            proc->setStateInformation(settings);
        } else {
            logln("warning: empty settings message");
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<BypassPlugin>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pPLD(msg).getNumber())) {
        proc->suspendProcessing(true);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pPLD(msg).getNumber())) {
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
    if (auto proc = m_audio->getProcessor(pDATA(msg)->idx)) {
        proc->setCurrentProgram(pDATA(msg)->channel, pDATA(msg)->preset);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pDATA(msg)->idx)) {
        proc->setParameterValue(pDATA(msg)->channel, pDATA(msg)->paramIdx, pDATA(msg)->value);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<GetParameterValue>> msg) {
    traceScope();
    Message<ParameterValue> ret(this);
    DATA(ret)->idx = pDATA(msg)->idx;
    DATA(ret)->paramIdx = pDATA(msg)->paramIdx;
    DATA(ret)->value = m_audio->getParameterValue(pDATA(msg)->channel, pDATA(msg)->idx, pDATA(msg)->paramIdx);
    ret.send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<GetAllParameterValues>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pPLD(msg).getNumber())) {
        for (auto& param : proc->getAllParamaterValues()) {
            Message<ParameterValue> ret(this);
            DATA(ret)->idx = pPLD(msg).getNumber();
            DATA(ret)->paramIdx = param.paramIdx;
            DATA(ret)->value = param.value;
            DATA(ret)->channel = param.channel;
            ret.send(m_cmdIn.get());
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UpdateScreenCaptureArea>> msg) {
    traceScope();
    getApp()->updateScreenCaptureArea(getThreadId(), pPLD(msg).getNumber());
}

void Worker::handleMessage(std::shared_ptr<Message<Rescan>> msg) {
    traceScope();
    bool wipe = pPLD(msg).getNumber() == 1;
    runOnMsgThreadAsync([this, wipe] {
        traceScope();
        if (wipe) {
            getApp()->getServer()->saveKnownPluginList(true);
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
    auto& pluginList = getApp()->getPluginList();
    json jlist = json::array();
    for (auto& plugin : pluginList.getTypes()) {
        auto jplug = Processor::createJson(plugin);
        auto pluginId = Processor::createPluginID(plugin);
        int pluginChIn = 0, pluginChOut = 0;
        bool hasMono = false;

        // add layouts, that match the number of output channels
        auto& layouts = getApp()->getServer()->getPluginLayouts(pluginId);
        if (layouts.isEmpty()) {
            logln("warning: no known layouts for '" << plugin.name << "' (" << pluginId << ")");
        }
        StringArray slayouts;
        for (auto& l : layouts) {
            int chIn = getLayoutNumChannels(l, true);
            int chOut = getLayoutNumChannels(l, false);

            pluginChIn = jmax(pluginChIn, chIn);
            pluginChOut = jmax(pluginChOut, chOut);

            bool isFxChain = m_cfg.channelsIn > 0;
            bool match = false;

            if (plugin.name == "LoudMax") {
                logln("-- " << describeLayout(l));
            }

            if (isFxChain) {
                if (l.inputBuses == l.outputBuses /* same inputs and outputs */ ||
                    (l.inputBuses.size() == 2 /* main input bus and sidechain  */ &&
                     l.outputBuses.size() == 1 /* single main output bus */ &&
                     l.inputBuses[0] == l.outputBuses[0] /* main in and out buses are the same */)) {
                    // the layout should match the outs exactly
                    match = m_cfg.channelsOut == chOut;
                }
                if (chOut == 1) {
                    hasMono = true;
                }
            } else {
                match = plugin.isInstrument && m_cfg.channelsOut >= chOut;
            }

            if (match) {
                slayouts.add(LogTag::getStrWithLeadingZero(chOut) + ":" + describeLayout(l, false, true, true));
            }
        }

        if (hasMono && m_cfg.channelsOut > 1) {
            slayouts.add("01:Multi-Mono");
        }

        auto jlayouts = json::array();

        if (slayouts.isEmpty()) {
            jlayouts.push_back("Default");
        } else {
            slayouts.sort(false);
            for (auto& l : slayouts) {
                auto parts = StringArray::fromTokens(l, ":", "");
                jlayouts.push_back(parts[1].toStdString());
            }
        }

        jplug["layouts"] = jlayouts;

        bool match = m_noPluginListFilter;
        // exact match is fine
        match = (m_cfg.channelsIn == pluginChIn) || match;
        // hide plugins with no inputs if we have inputs
        match = (m_cfg.channelsIn > 0 && plugin.numInputChannels > 0) || match;
        // for instruments (no inputs) allow any plugin with the isInstrument flag
        match = (m_cfg.channelsIn == 0 && plugin.isInstrument) || match;

        if (match) {
            jlist.push_back(jplug);
        }
    }
    pPLD(msg).setJson({{"plugins", jlist}});
    msg->send(m_cmdIn.get());
}

void Worker::handleMessage(std::shared_ptr<Message<GetScreenBounds>> /*msg*/) {
    traceScope();
    Message<ScreenBounds> res(this);
    if (auto proc = getApp()->getCurrentWindowProc(getThreadId())) {
        auto rect = proc->getScreenBounds();
        DATA(res)->x = rect.getX();
        DATA(res)->y = rect.getY();
        DATA(res)->w = rect.getWidth();
        DATA(res)->h = rect.getHeight();
    } else {
        logln("failed to get processor screen bounds: no active editor");
        DATA(res)->x = 0;
        DATA(res)->y = 0;
        DATA(res)->w = 0;
        DATA(res)->h = 0;
    }
    if (getApp()->getServer()->getSandboxModeRuntime() == Server::SANDBOX_PLUGIN) {
        // We don't want to block for updating the bounds of a plugin UI in a plugin isolation sandbox, so the response
        // goes back on the "command out" channel.
        std::lock_guard<std::mutex> lock(m_cmdOutMtx);
        res.send(m_cmdOut.get());
    } else {
        res.send(m_cmdIn.get());
    }
}

void Worker::handleMessage(std::shared_ptr<Message<Clipboard>> msg) {
    traceScope();
    SystemClipboard::copyTextToClipboard(pPLD(msg).getString());
}

void Worker::handleMessage(std::shared_ptr<Message<SetMonoChannels>> msg) {
    traceScope();
    if (auto proc = m_audio->getProcessor(pDATA(msg)->idx)) {
        proc->setMonoChannels(pDATA(msg)->channels);
    }
}

void Worker::sendKeys(const std::vector<uint16_t>& keysToPress) {
    Message<Key> msg(this);
    PLD(msg).setData(reinterpret_cast<const char*>(keysToPress.data()),
                     static_cast<int>(keysToPress.size() * sizeof(uint16_t)));
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

void Worker::sendClipboard(const String& val) {
    Message<Clipboard> msg(this);
    PLD(msg).setString(val);
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

bool Worker::KeyWatcher::keyPressed(const KeyPress& kp, Component*) {
    std::vector<uint16_t> keysToPress;
    auto modkeys = kp.getModifiers();
    auto c = static_cast<char>(kp.getKeyCode());
    String key(CharPointer_UTF8(&c), 1);
    auto keyCode = getKeyCode(key.toUpperCase().toStdString());

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
#if JUCE_MAC
        if (key.toUpperCase() == "Q") {
            // don't shut down the server
            return true;
        }
#endif
        keysToPress.push_back(getKeyCode("Command"));
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
        if (NOKEY != keyCode) {
            keysToPress.push_back(keyCode);
        }
    }
    worker->sendKeys(keysToPress);

    return true;
}

void Worker::sendParamValueChange(int idx, int channel, int paramIdx, float val) {
    logln("sending parameter update (index=" << idx << ", channel=" << channel << ", param index=" << paramIdx
                                             << ") new value is " << val);
    Message<ParameterValue> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->value = val;
    DATA(msg)->channel = channel;
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

void Worker::sendParamGestureChange(int idx, int channel, int paramIdx, bool guestureIsStarting) {
    logln("sending gesture change (index=" << idx << ", channel=" << channel << ", param index=" << paramIdx << ") "
                                           << (guestureIsStarting ? "starting" : "end"));
    Message<ParameterGesture> msg(this);
    DATA(msg)->idx = idx;
    DATA(msg)->paramIdx = paramIdx;
    DATA(msg)->gestureIsStarting = guestureIsStarting;
    DATA(msg)->channel = channel;
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

void Worker::sendStatusChange(int idx, bool ok, const String& err) {
    logln("sending plugin status (index=" << idx << ", ok=" << (int)ok << ", err=" << err << ")");
    Message<PluginStatus> msg(this);
    PLD(msg).setJson({{"idx", idx}, {"ok", ok}, {"err", err.toStdString()}});
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

void Worker::sendHideEditor(int idx) {
    logln("sending hide editor (index=" << idx << ")");
    Message<HidePlugin> msg(this);
    PLD(msg).setNumber(idx);
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

void Worker::sendError(const String& error) {
    Message<ServerError> msg(this);
    PLD(msg).setString(error);
    std::lock_guard<std::mutex> lock(m_cmdOutMtx);
    msg.send(m_cmdOut.get());
}

}  // namespace e47
