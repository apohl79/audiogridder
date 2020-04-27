/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Worker.hpp"
#include "KeyAndMouse.hpp"
#include "Utils.hpp"
#include "Defaults.hpp"

#include <sys/socket.h>

namespace e47 {

Worker::~Worker() {
    if (nullptr != m_client && m_client->isConnected()) {
        m_client->close();
    }
    stopThread(-1);
}

String Worker::getStringFrom(const PluginDescription& d) {
    String s = d.name + "|" + d.manufacturerName + "|" + d.fileOrIdentifier + "|" + d.pluginFormatName + "\n";
    return s;
}

void Worker::run() {
    Handshake cfg;
    std::unique_ptr<StreamingSocket> sock;
    int len;
    len = m_client->read(&cfg, sizeof(cfg), true);
    if (len > 0) {
        dbgln("  version          = " << cfg.version);
        dbgln("  clientPort       = " << cfg.clientPort);
        dbgln("  channels         = " << cfg.channels);
        dbgln("  rate             = " << cfg.rate);
        dbgln("  samplesPerBlock  = " << cfg.samplesPerBlock);
        dbgln("  doublePrecission = " << static_cast<int>(cfg.doublePrecission));

        // start audio processing
        sock = std::make_unique<StreamingSocket>();
        setsockopt(sock->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
        if (sock->connect(m_client->getHostName(), cfg.clientPort)) {
            m_audio.init(std::move(sock), cfg.channels, cfg.rate, cfg.samplesPerBlock, cfg.doublePrecission,
                         [this] { /*m_client->close();*/ });
            m_audio.startThread(Thread::realtimeAudioPriority);
        } else {
            logln("failed to establish audio connection to " << m_client->getHostName() << ":" << cfg.clientPort);
        }

        // start screen capturing
        sock = std::make_unique<StreamingSocket>();
        setsockopt(sock->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
        if (sock->connect(m_client->getHostName(), cfg.clientPort)) {
            m_screen.init(std::move(sock));
            m_screen.startThread();
        } else {
            logln("failed to establish screen connection to " << m_client->getHostName() << ":" << cfg.clientPort);
        }

        // send list of plugins
        auto& pluginList = getApp().getPluginList();
        String list;
        for (auto& plugin : pluginList.getTypes()) {
            list += getStringFrom(plugin) + "\n";
        }
        Message<PluginList> msg;
        PLD(msg).setString(list);
        if (!msg.send(m_client.get())) {
            logln("failed to send plugin list");
            m_client->close();
            signalThreadShouldExit();
        }

        // enter message loop
        while (!currentThreadShouldExit() && nullptr != m_client && m_client->isConnected()) {
            MessageHelper::Error e;
            auto msg = MessageFactory::getNextMessage(m_client.get(), &e);
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
                    default:
                        logln("unknown message type " << msg->getType());
                }
            } else if (e != MessageHelper::E_TIMEOUT) {
                break;
            }
        }
    } else {
        logln("handshake error with client " << m_client->getHostName());
    }
    shutdown();
    dbgln("command processor terminated");
}

void Worker::shutdown() {
    if (m_shouldHideEditor) {
        m_screen.hideEditor();
    }
    dbgln("shutdown: terminating audio worker");
    m_audio.shutdown();
    dbgln("shutdown: terminating screen worker");
    m_screen.shutdown();
    dbgln("shutdown: waiting for audio worker");
    m_audio.waitForThreadToExit(-1);
    dbgln("shutdown: waiting for screen worker");
    m_screen.waitForThreadToExit(-1);
    signalThreadShouldExit();
}

void Worker::handleMessage(std::shared_ptr<Message<Quit>> msg) { shutdown(); }

void Worker::handleMessage(std::shared_ptr<Message<AddPlugin>> msg) {
    auto id = pPLD(msg).getString();
    bool success = m_audio.addPlugin(id);
    if (!success) {
        MessageFactory::sendResult(m_client.get(), -1);
        return;
    }
    m_audio.addToRecentsList(id, m_client->getHostName());
    // send new updated latency samples back
    if (!MessageFactory::sendResult(m_client.get(), m_audio.getLatencySamples())) {
        logln("failed to send result");
        m_client->close();
        return;
    }
    auto proc = m_audio.getProcessor(m_audio.getSize() - 1);
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
    Message<Presets> msgPresets;
    msgPresets.payload.setString(presets);
    if (!msgPresets.send(m_client.get())) {
        logln("failed to send Presets message");
        m_client->close();
        return;
    }
    json jparams = json::array();
    for (auto& param : proc->getParameters()) {
        json jparam = {{"idx", param->getParameterIndex()},        {"name", param->getName(32).toStdString()},
                       {"defaultValue", param->getDefaultValue()}, {"category", param->getCategory()},
                       {"label", param->getLabel().toStdString()}, {"numSteps", param->getNumSteps()},
                       {"isBoolean", param->isBoolean()},          {"isDiscrete", param->isDiscrete()},
                       {"isMeta", param->isMetaParameter()},       {"isOrientInv", param->isOrientationInverted()}};
        jparams.push_back(jparam);
    }
    Message<Parameters> msgParams;
    msgParams.payload.setJson(jparams);
    if (!msgParams.send(m_client.get())) {
        logln("failed to send Parameters message");
        m_client->close();
        return;
    }
    Message<PluginSettings> msgSettings;
    if (!msgSettings.read(m_client.get())) {
        logln("failed to read PluginSettings message");
        m_client->close();
        return;
    }
    if (*msgSettings.payload.size > 0) {
        MemoryBlock block;
        block.append(msgSettings.payload.data, *msgSettings.payload.size);
        auto proc = m_audio.getProcessor(m_audio.getSize() - 1);
        proc->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    }
}

void Worker::handleMessage(std::shared_ptr<Message<DelPlugin>> msg) {
    m_audio.delPlugin(pPLD(msg).getNumber());
    // send new updated latency samples back
    MessageFactory::sendResult(m_client.get(), m_audio.getLatencySamples());
}

void Worker::handleMessage(std::shared_ptr<Message<EditPlugin>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        m_screen.showEditor(proc);
        m_shouldHideEditor = true;
    }
}

void Worker::handleMessage(std::shared_ptr<Message<HidePlugin>> msg) {
    m_screen.hideEditor();
    m_shouldHideEditor = false;
}

void Worker::handleMessage(std::shared_ptr<Message<Mouse>> msg) {
    auto ev = *pDATA(msg);
    MessageManager::callAsync([ev] {
        auto point = getApp().localPointToGlobal(Point<float>(ev.x, ev.y));
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
    });
}

void Worker::handleMessage(std::shared_ptr<Message<Key>> msg) {
    MessageManager::callAsync([msg] {
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
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        MemoryBlock block;
        proc->getStateInformation(block);
        Message<PluginSettings> ret;
        ret.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        ret.send(m_client.get());
    }
}

void Worker::handleMessage(std::shared_ptr<Message<BypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(true);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(false);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ExchangePlugins>> msg) {
    m_audio.exchangePlugins(pDATA(msg)->idxA, pDATA(msg)->idxB);
}

void Worker::handleMessage(std::shared_ptr<Message<RecentsList>> msg) {
    auto& recents = m_audio.getRecentsList(m_client->getHostName());
    String list;
    for (auto& r : recents) {
        list += getStringFrom(r) + "\n";
    }
    pPLD(msg).setString(list);
    msg->send(m_client.get());
}

void Worker::handleMessage(std::shared_ptr<Message<Preset>> msg) {
    m_audio.getProcessor(pDATA(msg)->idx)->setCurrentProgram(pDATA(msg)->preset);
}

void Worker::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    for (auto* p : m_audio.getProcessor(pDATA(msg)->idx)->getParameters()) {
        if (pDATA(msg)->paramIdx == p->getParameterIndex()) {
            p->setValue(pDATA(msg)->value);
            return;
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<GetParameterValue>> msg) {
    Message<ParameterValue> ret;
    DATA(ret)->idx = pDATA(msg)->idx;
    DATA(ret)->paramIdx = pDATA(msg)->paramIdx;
    DATA(ret)->value = m_audio.getParameterValue(pDATA(msg)->idx, pDATA(msg)->paramIdx);
    ret.send(m_client.get());
}

}  // namespace e47
