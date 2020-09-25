/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Worker.hpp"
#include "KeyAndMouse.hpp"
#include "Defaults.hpp"
#include "NumberConversion.hpp"
#include "App.hpp"
#include "CPUInfo.hpp"

#ifdef JUCE_MAC
#include <sys/socket.h>
#endif

namespace e47 {

Worker::Worker(StreamingSocket* clnt) : Thread("Worker"), LogTag("worker"), m_client(clnt) {
    m_audio.setLogTagSource(this);
    m_screen.setLogTagSource(this);
}

Worker::~Worker() {
    if (nullptr != m_client && m_client->isConnected()) {
        m_client->close();
    }
    waitForThreadAndLog(this, this);
}

String Worker::getStringFrom(const PluginDescription& d) {
    String s = d.name + ";" + d.manufacturerName + ";" + d.createIdentifierString() + ";" + d.pluginFormatName + ";" +
               d.category + "\n";
    return s;
}

void Worker::run() {
    Handshake cfg;
    std::unique_ptr<StreamingSocket> sock;
    int len;
    len = m_client->read(&cfg, sizeof(cfg), true);
    if (len > 0) {
        logln("  version          = " << cfg.version);
        logln("  clientPort       = " << cfg.clientPort);
        logln("  channelsIn       = " << cfg.channelsIn);
        logln("  channelsOut      = " << cfg.channelsOut);
        logln("  rate             = " << cfg.rate);
        logln("  samplesPerBlock  = " << cfg.samplesPerBlock);
        logln("  doublePrecission = " << static_cast<int>(cfg.doublePrecission));

        // start audio processing
        sock = std::make_unique<StreamingSocket>();
#ifdef JUCE_MAC
        setsockopt(sock->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif
        if (sock->connect(m_client->getHostName(), cfg.clientPort)) {
            m_audio.init(std::move(sock), cfg.channelsIn, cfg.channelsOut, cfg.rate, cfg.samplesPerBlock,
                         cfg.doublePrecission);
            m_audio.startThread(Thread::realtimeAudioPriority);
        } else {
            logln("failed to establish audio connection to " << m_client->getHostName() << ":" << cfg.clientPort);
        }

        // start screen capturing
        sock = std::make_unique<StreamingSocket>();
#ifdef JUCE_MAC
        setsockopt(sock->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif
        if (sock->connect(m_client->getHostName(), cfg.clientPort)) {
            m_screen.init(std::move(sock));
            m_screen.startThread();
        } else {
            logln("failed to establish screen connection to " << m_client->getHostName() << ":" << cfg.clientPort);
        }

        // send list of plugins
        auto& pluginList = getApp()->getPluginList();
        String list;
        for (auto& plugin : pluginList.getTypes()) {
            if ((plugin.numInputChannels > 0 && cfg.channelsIn > 0) ||
                (plugin.numInputChannels == 0 && cfg.channelsIn == 0) || (plugin.isInstrument && cfg.channelsIn == 0)) {
                list += getStringFrom(plugin) + "\n";
            }
        }
        Message<PluginList> msgPL;
        PLD(msgPL).setString(list);
        if (!msgPL.send(m_client.get())) {
            logln("failed to send plugin list");
            m_client->close();
            signalThreadShouldExit();
        }

        // enter message loop
        logln("command processor started");
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
                    case CPULoad::Type:
                        handleMessage(Message<Any>::convert<CPULoad>(msg));
                        break;
                    default:
                        logln("unknown message type " << msg->getType());
                }
            } else if (e.code != MessageHelper::E_TIMEOUT) {
                break;
            }
        }
    } else {
        logln("handshake error with client " << m_client->getHostName());
    }
    shutdown();
    logln("command processor terminated");
}

void Worker::shutdown() {
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    if (m_shouldHideEditor) {
        m_screen.hideEditor();
    }
    m_audio.shutdown();
    m_screen.shutdown();
    m_audio.waitForThreadToExit(-1);
    m_screen.waitForThreadToExit(-1);
    signalThreadShouldExit();
}

void Worker::handleMessage(std::shared_ptr<Message<Quit>> /* msg */) { shutdown(); }

void Worker::handleMessage(std::shared_ptr<Message<AddPlugin>> msg) {
    auto id = pPLD(msg).getString();
    logln("adding plugin " << id << "...");
    bool success = m_audio.addPlugin(id);
    logln("..." << (success ? "ok" : "failed"));
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
    logln("sending presets...");
    auto proc = m_audio.getProcessor(m_audio.getSize() - 1)->getPlugin();
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
    logln("...ok");
    logln("sending parameters...");
    json jparams = json::array();
    for (auto& param : proc->getParameters()) {
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
    Message<Parameters> msgParams;
    msgParams.payload.setJson(jparams);
    if (!msgParams.send(m_client.get())) {
        logln("failed to send Parameters message");
        m_client->close();
        return;
    }
    logln("...ok");
    logln("reading plugin settings...");
    Message<PluginSettings> msgSettings;
    if (!msgSettings.read(m_client.get())) {
        logln("failed to read PluginSettings message");
        m_client->close();
        return;
    }
    if (*msgSettings.payload.size > 0) {
        MemoryBlock block;
        block.append(msgSettings.payload.data, as<size_t>(*msgSettings.payload.size));
        proc->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    }
    logln("...ok");
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

void Worker::handleMessage(std::shared_ptr<Message<HidePlugin>> /* msg */) {
    m_screen.hideEditor();
    m_shouldHideEditor = false;
}

void Worker::handleMessage(std::shared_ptr<Message<Mouse>> msg) {
    auto ev = *pDATA(msg);
    MessageManager::callAsync([ev] {
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

void Worker::handleMessage(std::shared_ptr<Message<SetPluginSettings>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        Message<PluginSettings> msgSettings;
        if (!msgSettings.read(m_client.get())) {
            logln("failed to read PluginSettings message");
            m_client->close();
            return;
        }
        if (*msgSettings.payload.size > 0) {
            MemoryBlock block;
            block.append(msgSettings.payload.data, as<size_t>(*msgSettings.payload.size));
            proc->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<BypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        bool runOnMsgThread = false;
        if (m_shouldHideEditor) {
            m_screen.hideEditor();
            m_shouldHideEditor = false;
            // Hiding an editor has to run on the msg thread. This has to be finished before unloading the plugin. By
            // adding a task to the message queue, we can ensure this.
            runOnMsgThread = true;
        }
        auto unloadfunc = [this, proc] {
            proc->unload();
            m_audio.update();
        };
        if (runOnMsgThread) {
            MessageManager::callAsync(unloadfunc);
        } else {
            unloadfunc();
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(pPLD(msg).getNumber());
    if (nullptr != proc) {
        if (proc->load()) {
            m_audio.update();
        } else {
            logln("unbypass failed: can't load plugin");
        }
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
    auto p = m_audio.getProcessor(pDATA(msg)->idx)->getPlugin();
    if (nullptr != p) {
        p->setCurrentProgram(pDATA(msg)->preset);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ParameterValue>> msg) {
    auto p = m_audio.getProcessor(pDATA(msg)->idx)->getPlugin();
    if (nullptr != p) {
        for (auto* param : p->getParameters()) {
            if (pDATA(msg)->paramIdx == param->getParameterIndex()) {
                param->setValue(pDATA(msg)->value);
                return;
            }
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

void Worker::handleMessage(std::shared_ptr<Message<GetAllParameterValues>> msg) {
    auto p = m_audio.getProcessor(pPLD(msg).getNumber())->getPlugin();
    if (nullptr != p) {
        for (auto* param : p->getParameters()) {
            Message<ParameterValue> ret;
            DATA(ret)->idx = pPLD(msg).getNumber();
            DATA(ret)->paramIdx = param->getParameterIndex();
            DATA(ret)->value = param->getValue();
            ret.send(m_client.get());
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UpdateScreenCaptureArea>> msg) {
    getApp()->updateScreenCaptureArea(pPLD(msg).getNumber());
}

void Worker::handleMessage(std::shared_ptr<Message<Rescan>> msg) {
    bool wipe = pPLD(msg).getNumber() == 1;
    MessageManager::callAsync([wipe] {
        if (wipe) {
            getApp()->getServer().getPluginList().clear();
            getApp()->getServer().saveKnownPluginList();
        }
        getApp()->restartServer(true);
    });
}

void Worker::handleMessage(std::shared_ptr<Message<CPULoad>> msg) {
    pPLD(msg).setFloat(CPUInfo::getUsage());
    msg->send(m_client.get());
}

}  // namespace e47
