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
    if (nullptr != m_client) {
        m_client->close();
    }
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
        msg.payload.setString(list);
        if (!msg.send(m_client.get())) {
            logln("failed to send plugin list");
            m_client->close();
            signalThreadShouldExit();
        }

        // enter message loop
        while (!currentThreadShouldExit() && nullptr != m_client && m_client->isConnected()) {
            auto msg = MessageFactory::getNextMessage(m_client.get());
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
                    default:
                        logln("unknown message type " << msg->getType());
                }
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
    if (nullptr != m_client) {
        m_client->close();
        m_client.reset();
    }
    m_audio.shutdown();
    m_screen.shutdown();
    m_audio.waitForThreadToExit(-1);
    m_screen.waitForThreadToExit(-1);
    signalThreadShouldExit();
}

void Worker::handleMessage(std::shared_ptr<Message<Quit>> msg) { shutdown(); }

void Worker::handleMessage(std::shared_ptr<Message<AddPlugin>> msg) {
    bool success = m_audio.addPlugin(msg->payload.getString());
    if (!success) {
        MessageFactory::sendResult(m_client.get(), -1);
    } else {
        // send new updated latency samples back
        MessageFactory::sendResult(m_client.get(), m_audio.getLatencySamples());
        Message<PluginSettings> msgSettings;
        if (msgSettings.read(m_client.get()) && *msgSettings.payload.size > 0) {
            MemoryBlock block;
            block.append(msgSettings.payload.data, *msgSettings.payload.size);
            auto proc = m_audio.getProcessor(m_audio.getSize() - 1);
            proc->setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        }
    }
}

void Worker::handleMessage(std::shared_ptr<Message<DelPlugin>> msg) {
    m_audio.delPlugin(msg->payload.getNumber());
    // send new updated latency samples back
    MessageFactory::sendResult(m_client.get(), m_audio.getLatencySamples());
}

void Worker::handleMessage(std::shared_ptr<Message<EditPlugin>> msg) {
    auto proc = m_audio.getProcessor(msg->payload.getNumber());
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
    auto ev = *msg->payload.data;
    MessageManager::callAsync([ev] {
        auto point = getApp().localPointToGlobal(Point<float>(ev.x, ev.y));
        mouseEvent(ev.type, point.x, point.y);
    });
}

void Worker::handleMessage(std::shared_ptr<Message<Key>> msg) {
    MessageManager::callAsync([msg] {
        auto* codes = msg->payload.getKeyCodes();
        auto num = msg->payload.getKeyCount();
        for (size_t i = 0; i < num; i++) {
            keyEventDown(codes[i]);
        }
        for (size_t i = num - 1; i > -1; i--) {
            keyEventUp(codes[i]);
        }
    });
}

void Worker::handleMessage(std::shared_ptr<Message<GetPluginSettings>> msg) {
    auto proc = m_audio.getProcessor(msg->payload.getNumber());
    if (nullptr != proc) {
        MemoryBlock block;
        proc->getStateInformation(block);
        Message<PluginSettings> ret;
        ret.payload.setData(block.begin(), static_cast<int>(block.getSize()));
        ret.send(m_client.get());
    }
}

void Worker::handleMessage(std::shared_ptr<Message<BypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(msg->payload.getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(true);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg) {
    auto proc = m_audio.getProcessor(msg->payload.getNumber());
    if (nullptr != proc) {
        proc->suspendProcessing(false);
    }
}

void Worker::handleMessage(std::shared_ptr<Message<ExchangePlugins>> msg) {
    m_audio.exchangePlugins(msg->payload.data->idxA, msg->payload.data->idxB);
}

void Worker::handleMessage(std::shared_ptr<Message<RecentsList>> msg) {
    auto& recents = m_audio.getRecentsList();
    String list;
    for (auto& r : recents) {
        list += getStringFrom(r) + "\n";
    }
    msg->payload.setString(list);
    msg->send(m_client.get());
}

}  // namespace e47
