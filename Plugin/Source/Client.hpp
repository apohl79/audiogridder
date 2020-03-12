/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Client_hpp
#define Client_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "Message.hpp"
#include "ServerPlugin.hpp"
#include "Defaults.hpp"

namespace e47 {

class Client : public Thread, public MouseListener, public KeyListener {
  public:
    Client();
    ~Client();

    void run();

    void setServer(const String& host, int port = DEFAULT_SERVER_PORT);
    String getServerHost();
    String getServerHostAndID();
    int getServerPort();
    int getId() const { return m_id; }
    int getChannels() const { return m_channels; }
    double getSampleRate() const { return m_rate; }
    int getSamplesPerBlock() const { return m_samplesPerBlock; }
    int getLatencySamples() const { return m_latency; }

    bool isReady();
    bool isReadyLockFree();
    void init(int channels, double rate, int samplesPerBlock, bool doublePrecission);
    void close();

    template <typename T>
    bool send(AudioBuffer<T>& buffer) {
        AudioMessage msg;
        std::lock_guard<std::mutex> lock(m_clientMtx);
        return msg.sendToServer(m_audio_socket.get(), buffer);
    }

    template <typename T>
    bool read(AudioBuffer<T>& buffer) {
        AudioMessage msg;
        std::lock_guard<std::mutex> lock(m_clientMtx);
        bool success = msg.readFromServer(m_audio_socket.get(), buffer);
        if (success) {
            m_latency = msg.getLatencySamples();
        }
        return success;
    }

    const auto& getPlugins() const { return m_plugins; }
    Image getPluginScreen();  // create copy
    void setPluginScreen(std::shared_ptr<Image> img, int w, int h);

    using ScreenUpdateCallback = std::function<void(std::shared_ptr<Image>, int, int)>;
    void setPluginScreenUpdateCallback(ScreenUpdateCallback fn);

    using OnConnectCallback = std::function<void()>;
    void setOnConnectCallback(OnConnectCallback fn);

    using OnCloseCallback = std::function<void()>;
    void setOnCloseCallback(OnCloseCallback fn);

    bool addPlugin(String id, String settings = "");
    void delPlugin(int idx);
    void editPlugin(int idx);
    void hidePlugin();
    MemoryBlock getPluginSettings(int idx);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::vector<ServerPlugin> getRecents();

    class ScreenReceiver : public Thread {
      public:
        ScreenReceiver(Client* clnt, StreamingSocket* sock) : Thread("ScreenWorker"), m_client(clnt), m_socket(sock) {}
        void run();

      private:
        Client* m_client;
        StreamingSocket* m_socket;
    };

    // MouseListener
    virtual void mouseMove(const MouseEvent& event);
    virtual void mouseEnter(const MouseEvent& event);
    virtual void mouseExit(const MouseEvent& event) {}
    virtual void mouseDown(const MouseEvent& event);
    virtual void mouseDrag(const MouseEvent& event);
    virtual void mouseUp(const MouseEvent& event);
    virtual void mouseDoubleClick(const MouseEvent& event);
    virtual void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel);

    void sendMouseEvent(MouseEvType ev, Point<float> p);

    // KeyListener
    virtual bool keyPressed(const KeyPress& kp, Component* originatingComponent);

  private:
    std::mutex m_srvMtx;
    String m_srvHost = "127.0.0.1";
    int m_srvPort = DEFAULT_SERVER_PORT;
    int m_id = 0;
    bool m_needsReconnect = false;
    int m_channels = 0;
    double m_rate = 0;
    int m_samplesPerBlock = 0;
    bool m_doublePrecission = false;
    int m_latency = 0;

    std::mutex m_clientMtx;
    std::atomic_bool m_ready;
    std::unique_ptr<StreamingSocket> m_cmd_socket;
    std::unique_ptr<StreamingSocket> m_audio_socket;
    std::unique_ptr<StreamingSocket> m_screen_socket;
    std::vector<ServerPlugin> m_plugins;

    std::unique_ptr<ScreenReceiver> m_screenWorker;
    std::shared_ptr<Image> m_pluginScreen;
    ScreenUpdateCallback m_pluginScreenUpdateCallback;
    std::mutex m_pluginScreenMtx;

    OnConnectCallback m_onConnectCallback;
    OnCloseCallback m_onCloseCallback;

    void quit();
    void init();

    StreamingSocket* accept(StreamingSocket& sock) const;
};

}  // namespace e47

#endif /* Client_hpp */
