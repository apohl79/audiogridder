/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Worker_hpp
#define Worker_hpp

#include <JuceHeader.h>
#include <thread>

#include "AudioWorker.hpp"
#include "Message.hpp"
#include "ScreenWorker.hpp"
#include "Utils.hpp"

namespace e47 {

class Server;

class Worker : public Thread, public LogTag {
  public:
    static std::atomic_uint32_t count;
    static std::atomic_uint32_t runCount;

    Worker(std::shared_ptr<StreamingSocket> masterSocket, const HandshakeRequest& cfg);

    ~Worker() override;
    void run() override;

    void shutdown();

    void handleMessage(std::shared_ptr<Message<Quit>> msg);
    void handleMessage(std::shared_ptr<Message<AddPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<DelPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<EditPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<HidePlugin>> msg, bool fromMaster = false);
    void handleMessage(std::shared_ptr<Message<Mouse>> msg);
    void handleMessage(std::shared_ptr<Message<Key>> msg);
    void handleMessage(std::shared_ptr<Message<GetPluginSettings>> msg);
    void handleMessage(std::shared_ptr<Message<SetPluginSettings>> msg);
    void handleMessage(std::shared_ptr<Message<BypassPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<UnbypassPlugin>> msg);
    void handleMessage(std::shared_ptr<Message<ExchangePlugins>> msg);
    void handleMessage(std::shared_ptr<Message<RecentsList>> msg);
    void handleMessage(std::shared_ptr<Message<Preset>> msg);
    void handleMessage(std::shared_ptr<Message<ParameterValue>> msg);
    void handleMessage(std::shared_ptr<Message<GetParameterValue>> msg);
    void handleMessage(std::shared_ptr<Message<GetAllParameterValues>> msg);
    void handleMessage(std::shared_ptr<Message<UpdateScreenCaptureArea>> msg);
    void handleMessage(std::shared_ptr<Message<Rescan>> msg);
    void handleMessage(std::shared_ptr<Message<Restart>> msg);
    void handleMessage(std::shared_ptr<Message<CPULoad>> msg);
    void handleMessage(std::shared_ptr<Message<PluginList>> msg);

  private:
    std::shared_ptr<StreamingSocket> m_masterSocket;
    std::unique_ptr<StreamingSocket> m_cmdIn;
    std::unique_ptr<StreamingSocket> m_cmdOut;
    HandshakeRequest m_cfg;
    std::shared_ptr<AudioWorker> m_audio;
    std::shared_ptr<ScreenWorker> m_screen;
    std::atomic_int m_activeEditorIdx{-1};
    std::atomic_bool m_shutdown{false};
    MessageFactory m_msgFactory;

    bool m_noPluginListFilter = false;

    struct KeyWatcher : KeyListener {
        Worker* worker;
        KeyWatcher(Worker* w) : worker(w) {}
        bool keyPressed(const KeyPress& kp, Component*);
    };

    std::unique_ptr<KeyWatcher> m_keyWatcher;

    void sendKeys(const std::vector<uint16_t>& keysToPress);
    void sendParamValueChange(int idx, int paramIdx, float val);
    void sendParamGestureChange(int idx, int paramIdx, bool guestureIsStarting);

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* Worker_hpp */
