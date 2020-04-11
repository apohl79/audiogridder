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

class AudioGridderAudioProcessor;

namespace e47 {

class Client : public Thread, public MouseListener, public KeyListener {
  public:
    Client(AudioGridderAudioProcessor* processor);
    ~Client();

    static int NUM_OF_BUFFERS;

    void run();

    void setServer(const String& host, int port = DEFAULT_SERVER_PORT);
    String getServerHost();
    String getServerHostAndID();
    int getServerPort();
    int getId() const { return m_id; }
    int getChannels() const { return m_channels; }
    double getSampleRate() const { return m_rate; }
    int getSamplesPerBlock() const { return m_samplesPerBlock; }
    int getLatencySamples() const { return m_latency + NUM_OF_BUFFERS * m_samplesPerBlock; }

    bool isReady();
    bool isReadyLockFree();
    void init(int channels, double rate, int samplesPerBlock, bool doublePrecission);

    void reconnect() { m_needsReconnect = true; }
    void close();

    void send(AudioBuffer<float>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        std::lock_guard<std::mutex> lock(m_clientMtx);
        m_audioStreamerF->send(buffer, midi, posInfo);
    }

    void send(AudioBuffer<double>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        std::lock_guard<std::mutex> lock(m_clientMtx);
        m_audioStreamerD->send(buffer, midi, posInfo);
    }

    void read(AudioBuffer<float>& buffer, MidiBuffer& midi) {
        std::lock_guard<std::mutex> lock(m_clientMtx);
        m_audioStreamerF->read(buffer, midi);
    }

    void read(AudioBuffer<double>& buffer, MidiBuffer& midi) {
        std::lock_guard<std::mutex> lock(m_clientMtx);
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

    bool addPlugin(String id, StringArray& presets, String settings = "");
    void delPlugin(int idx);
    void editPlugin(int idx);
    void hidePlugin();
    MemoryBlock getPluginSettings(int idx);
    void bypassPlugin(int idx);
    void unbypassPlugin(int idx);
    void exchangePlugins(int idxA, int idxB);
    std::vector<ServerPlugin> getRecents();
    void setPreset(int idx, int preset);

    class ScreenReceiver : public Thread {
      public:
        ScreenReceiver(Client* clnt, StreamingSocket* sock) : Thread("ScreenWorker"), m_client(clnt), m_socket(sock) {}
        ~ScreenReceiver() { stopThread(100); }
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
    AudioGridderAudioProcessor* m_processor;
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

    String getLoadedPluginsString();

    template <typename T>
    class AudioStreamer : public Thread {
      public:
        struct AudioMidiBuffer {
            AudioBuffer<T> audio;
            MidiBuffer midi;
            AudioPlayHead::CurrentPositionInfo posInfo;
        };

        Client* client;
        StreamingSocket* socket;
        boost::lockfree::spsc_queue<AudioMidiBuffer> writeQ, readQ;
        std::mutex writeMtx, readMtx;
        std::condition_variable writeCv, readCv;

        AudioStreamer(Client* clnt, StreamingSocket* sock)
            : Thread("AudioStreamer"),
              client(clnt),
              socket(sock),
              writeQ(clnt->NUM_OF_BUFFERS * 2),
              readQ(clnt->NUM_OF_BUFFERS * 2) {
            for (int i = 0; i < clnt->NUM_OF_BUFFERS; i++) {
                AudioMidiBuffer buf;
                buf.audio.setSize(clnt->m_channels, clnt->m_samplesPerBlock);
                buf.audio.clear();
                readQ.push(std::move(buf));
            }
        }

        ~AudioStreamer() {
            signalThreadShouldExit();
            notifyWrite();
            notifyRead();
            stopThread(100);
        }

        void notifyWrite() {
            std::lock_guard<std::mutex> lock(writeMtx);
            writeCv.notify_one();
        }

        void waitWrite() {
            if (writeQ.read_available() == 0) {
                std::unique_lock<std::mutex> lock(writeMtx);
                writeCv.wait(lock, [this] { return writeQ.read_available() > 0 || currentThreadShouldExit(); });
            }
        }

        void notifyRead() {
            std::lock_guard<std::mutex> lock(readMtx);
            readCv.notify_one();
        }

        void waitRead() {
            if (readQ.read_available() < (client->NUM_OF_BUFFERS / 2) && readQ.read_available() > 0) {
                std::cerr << "warning: instance (" << client->getLoadedPluginsString() << "): input buffer below 50% ("
                          << readQ.read_available() << "/" << client->NUM_OF_BUFFERS << ")" << std::endl;
            } else if (readQ.read_available() == 0) {
                std::cerr << "warning: instance (" << client->getLoadedPluginsString()
                          << "): read queue empty, waiting for data, try increasing the NumberOfBuffers value"
                          << std::endl;
                std::unique_lock<std::mutex> lock(readMtx);
                readCv.wait(lock, [this] { return readQ.read_available() > 0 || currentThreadShouldExit(); });
            }
        }

        void run() {
            while (!currentThreadShouldExit() && socket->isConnected()) {
                while (writeQ.read_available() > 0) {
                    AudioMidiBuffer buf;
                    writeQ.pop(buf);
                    if (!sendReal(buf)) {
                        std::cerr << "error: instance (" << client->getLoadedPluginsString() << "): send failed"
                                  << std::endl;
                        socket->close();
                        break;
                    }
                    MessageHelper::ReadError err;
                    if (!readReal(buf, &err)) {
                        std::cerr << "error: instance (" << client->getLoadedPluginsString()
                                  << "): read failed, error code " << err << std::endl;
                        socket->close();
                        break;
                    }
                    readQ.push(std::move(buf));
                    notifyRead();
                }
                waitWrite();
            }
        }

        void send(AudioBuffer<T>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
            AudioMidiBuffer buf;
            buf.audio.makeCopyOf(buffer);
            buf.midi.addEvents(midi, 0, midi.getNumEvents(), 0);
            buf.posInfo = posInfo;
            writeQ.push(std::move(buf));
            notifyWrite();
        }

        void read(AudioBuffer<T>& buffer, MidiBuffer& midi) {
            waitRead();
            AudioMidiBuffer buf;
            readQ.pop(buf);
            buffer.makeCopyOf(buf.audio);
            midi.clear();
            midi.addEvents(buf.midi, 0, buf.midi.getNumEvents(), 0);
        }

        bool sendReal(AudioMidiBuffer& buffer) {
            AudioMessage msg;
            if (nullptr != socket) {
                return msg.sendToServer(socket, buffer.audio, buffer.midi, buffer.posInfo);
            } else {
                return nullptr;
            }
        }

        bool readReal(AudioMidiBuffer& buffer, MessageHelper::ReadError* e) {
            AudioMessage msg;
            bool success = false;
            if (nullptr != socket) {
                success = msg.readFromServer(socket, buffer.audio, buffer.midi, e);
            }
            if (success) {
                client->m_latency = msg.getLatencySamples();
            }
            return success;
        }
    };
    friend AudioStreamer<float>;
    friend AudioStreamer<double>;

    std::unique_ptr<AudioStreamer<float>> m_audioStreamerF;
    std::unique_ptr<AudioStreamer<double>> m_audioStreamerD;
};

}  // namespace e47

#endif /* Client_hpp */
