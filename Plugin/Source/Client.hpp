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

#include <boost/lockfree/spsc_queue.hpp>

#if (JUCE_DEBUG && !JUCE_DISABLE_ASSERTIONS)
#define dbgln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     std::cout << __str << std::endl;)
#else
#define dbgln(M)
#endif

#define logln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     std::cerr << __str << std::endl;)

#define logln_clnt(C, M)                                                                     \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)C << "] " << M; \
                                     std::cerr << __str << std::endl;)

class AudioGridderAudioProcessor;

namespace e47 {

class Client : public Thread, public MouseListener, public KeyListener {
  public:
    Client(AudioGridderAudioProcessor* processor);
    ~Client() override;

    struct Parameter {
        int idx = -1;
        String name;
        float defaultValue = 0;
        AudioProcessorParameter::Category category = AudioProcessorParameter::genericParameter;
        String label;
        int numSteps = 0x7fffffff;
        bool isBoolean = false;
        bool isDiscrete = false;
        bool isMeta = false;
        bool isOrientInv = false;

        int automationSlot = -1;

        static Parameter fromJson(const json& j) {
            Parameter p;
            p.idx = j["idx"].get<int>();
            p.name = j["name"].get<std::string>();
            p.defaultValue = j["defaultValue"].get<float>();
            p.category = j["category"].get<AudioProcessorParameter::Category>();
            p.label = j["label"].get<std::string>();
            p.numSteps = j["numSteps"].get<int>();
            p.isBoolean = j["isBoolean"].get<bool>();
            p.isDiscrete = j["isDiscrete"].get<bool>();
            p.isMeta = j["isMeta"].get<bool>();
            p.isOrientInv = j["isOrientInv"].get<bool>();
            if (j.find("automationSlot") != j.end()) {
                p.automationSlot = j["automationSlot"].get<int>();
            }
            return p;
        }

        json toJson() const {
            json j = {{"idx", idx},
                      {"name", name.toStdString()},
                      {"defaultValue", defaultValue},
                      {"category", category},
                      {"label", label.toStdString()},
                      {"numSteps", numSteps},
                      {"isBoolean", isBoolean},
                      {"isDiscrete", isDiscrete},
                      {"isMeta", isMeta},
                      {"isOrientInv", isOrientInv},
                      {"automationSlot", automationSlot}};
            return j;
        }
    };

    int NUM_OF_BUFFERS = DEFAULT_NUM_OF_BUFFERS;

    void run() override;

    void setServer(const String& host, int port = DEFAULT_SERVER_PORT);
    String getServerHost();
    String getServerHostAndID();
    int getServerPort();
    int getId() const { return m_id; }
    int getChannelsIn() const { return m_channelsIn; }
    int getChannelsOut() const { return m_channelsOut; }
    double getSampleRate() const { return m_rate; }
    int getSamplesPerBlock() const { return m_samplesPerBlock; }
    int getLatencySamples() const { return m_latency + NUM_OF_BUFFERS * m_samplesPerBlock; }

    bool isReady();
    bool isReadyLockFree();
    void init(int channelsIn, int channelsOut, double rate, int samplesPerBlock, bool doublePrecission);

    void reconnect() { m_needsReconnect = true; }
    void close();

    struct dbglock {
        Client& client;
        dbglock(Client& c, int id) : client(c) {
            client.m_clientMtx.lock();
            client.m_clientMtxId = id;
        }
        ~dbglock() {
            client.m_clientMtx.unlock();
            client.m_clientMtxId = 0;
        }
    };
    friend dbglock;

    void send(AudioBuffer<float>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        dbglock(*this, 1);
        m_audioStreamerF->send(buffer, midi, posInfo);
    }

    void send(AudioBuffer<double>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        dbglock(*this, 2);
        m_audioStreamerD->send(buffer, midi, posInfo);
    }

    void read(AudioBuffer<float>& buffer, MidiBuffer& midi) {
        dbglock(*this, 3);
        m_audioStreamerF->read(buffer, midi);
    }

    void read(AudioBuffer<double>& buffer, MidiBuffer& midi) {
        dbglock(*this, 4);
        m_audioStreamerD->read(buffer, midi);
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

    bool addPlugin(String id, StringArray& presets, Array<Parameter>& params, String settings = "");
    void delPlugin(int idx);
    void editPlugin(int idx);
    void hidePlugin();
    MemoryBlock getPluginSettings(int idx);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::vector<ServerPlugin> getRecents();
    void setPreset(int idx, int preset);

    float getParameterValue(int idx, int paramIdx);
    void setParameterValue(int idx, int paramIdx, float val);

    // MouseListener
    void mouseMove(const MouseEvent& event) override;
    void mouseEnter(const MouseEvent& event) override;
    void mouseExit(const MouseEvent& /* event */) override {}
    void mouseDown(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void mouseDoubleClick(const MouseEvent& event) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;

    void sendMouseEvent(MouseEvType ev, Point<float> p, bool isShiftDown, bool isCtrlDown, bool isAltDown);

    // KeyListener
    bool keyPressed(const KeyPress& kp, Component* originatingComponent) override;

  private:
    AudioGridderAudioProcessor* m_processor;
    std::mutex m_srvMtx;
    String m_srvHost = "127.0.0.1";
    int m_srvPort = DEFAULT_SERVER_PORT;
    int m_id = 0;
    bool m_needsReconnect = false;
    int m_channelsIn = 0;
    int m_channelsOut = 0;
    double m_rate = 0;
    int m_samplesPerBlock = 0;
    bool m_doublePrecission = false;
    int m_latency = 0;

    std::mutex m_clientMtx;
    int m_clientMtxId = 0;
    std::atomic_bool m_ready;
    std::atomic_bool m_error{false};
    std::unique_ptr<StreamingSocket> m_cmd_socket;
    std::unique_ptr<StreamingSocket> m_audio_socket;
    std::unique_ptr<StreamingSocket> m_screen_socket;
    std::vector<ServerPlugin> m_plugins;

    class ScreenReceiver : public Thread {
      public:
        ScreenReceiver(Client* clnt, StreamingSocket* sock) : Thread("ScreenWorker"), m_client(clnt), m_socket(sock) {}
        ~ScreenReceiver() { stopThread(100); }
        void run();

      private:
        Client* m_client;
        StreamingSocket* m_socket;
        std::shared_ptr<Image> m_image;
    };

    friend ScreenReceiver;

    std::unique_ptr<ScreenReceiver> m_screenWorker;
    std::shared_ptr<Image> m_pluginScreen;
    ScreenUpdateCallback m_pluginScreenUpdateCallback;
    std::mutex m_pluginScreenMtx;

    OnConnectCallback m_onConnectCallback;
    OnCloseCallback m_onCloseCallback;

    void quit();
    void init();

    StreamingSocket* accept(StreamingSocket& sock) const;

    String getLoadedPluginsString();

#include "AudioStreamer.hpp"

    friend AudioStreamer<float>;
    friend AudioStreamer<double>;

    std::unique_ptr<AudioStreamer<float>> m_audioStreamerF;
    std::unique_ptr<AudioStreamer<double>> m_audioStreamerD;
};

}  // namespace e47

#endif /* Client_hpp */
