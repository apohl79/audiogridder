/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Worker_hpp
#define Worker_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "AudioWorker.hpp"
#include "Message.hpp"
#include "ScreenWorker.hpp"

#include <thread>

namespace e47 {

class Server;

class Worker : public Thread {
  public:
    Worker(StreamingSocket* clnt) : Thread("Worker"), m_client(clnt) {}
    virtual ~Worker();
    virtual void run() override;

    void shutdown();

    void handleMessage(std::shared_ptr<Message<Quit>> msg);
    void handleMessage(std::shared_ptr<Message<AddPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<DelPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<EditPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<HidePlugin>> msg);
    void handleMessage(std::shared_ptr<Message<Mouse>> msg);
    void handleMessage(std::shared_ptr<Message<Key>> msg);
    void handleMessage(std::shared_ptr<Message<GetPluginSettings>> msg);
    void handleMessage(std::shared_ptr<Message<BypassPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<ExchangePlugins>> msg);
    void handleMessage(std::shared_ptr<Message<RecentsList>> msg);
    void handleMessage(std::shared_ptr<Message<Preset>> msg);
    void handleMessage(std::shared_ptr<Message<ParameterValue>> msg);
    void handleMessage(std::shared_ptr<Message<GetParameterValue>> msg);

  private:
    std::unique_ptr<StreamingSocket> m_client;
    AudioWorker m_audio;
    ScreenWorker m_screen;
    bool m_shouldHideEditor = false;

    String getStringFrom(const PluginDescription& d);
};

}  // namespace e47

#endif /* Worker_hpp */
