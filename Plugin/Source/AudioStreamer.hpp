/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef AudioStreamer_hpp
#define AudioStreamer_hpp

#include <memory>
#include "Client.hpp"
#include "Metrics.hpp"

namespace e47 {

template <typename T>
class AudioStreamer : public Thread, public LogTagDelegate {
  public:
    AudioStreamer(Client* clnt, StreamingSocket* sock)
        : Thread("AudioStreamer"),
          LogTagDelegate(clnt),
          m_client(clnt),
          m_socket(std::unique_ptr<StreamingSocket>(sock)),
          m_writeQ((size_t)clnt->NUM_OF_BUFFERS * 2),
          m_readQ((size_t)clnt->NUM_OF_BUFFERS * 2),
          m_durationGlobal(TimeStatistic::getDuration("audio")),
          m_durationLocal(TimeStatistic::getDuration(String("audio.") + String(getId()), false)) {
        traceScope();

        for (int i = 0; i < clnt->NUM_OF_BUFFERS; i++) {
            AudioMidiBuffer buf;
            buf.audio.setSize(clnt->getChannelsIn(), clnt->getSamplesPerBlock());
            buf.audio.clear();
            m_readQ.push(std::move(buf));
        }
        m_workingSendBuf.audio.clear();
        m_workingReadBuf.audio.clear();

        m_bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
        m_bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");
    }

    ~AudioStreamer() {
        traceScope();
        logln("audio streamer cleaning up");
        signalThreadShouldExit();
        notifyWrite();
        notifyRead();
        waitForThreadAndLog(getLogTagSource(), this);
        logln("audio streamer cleanup done");
    }

    bool isOk() {
        traceScope();
        if (!m_error) {
            return m_socket->isConnected();
        }
        return false;
    }

    void run() {
        traceScope();
        logln("audio streamer ready");
        while (!currentThreadShouldExit() && !m_error && m_socket->isConnected()) {
            while (m_writeQ.read_available() > 0) {
                AudioMidiBuffer buf;
                m_writeQ.pop(buf);
                m_durationLocal.reset();
                m_durationGlobal.reset();
                if (!sendReal(buf)) {
                    logln("error: " << getInstanceString() << ": send failed");
                    setError();
                    return;
                }
                MessageHelper::Error err;
                if (!readReal(buf, &err)) {
                    logln("error: " << getInstanceString() << ": read failed: " << err.toString());
                    setError();
                    return;
                }
                m_durationLocal.update();
                m_durationGlobal.update();
                m_readQ.push(std::move(buf));
                notifyRead();
            }
            waitWrite();
        }
        m_durationLocal.clear();
        m_durationGlobal.clear();
        logln("audio streamer terminated");
    }

    void send(AudioBuffer<T>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        traceScope();
        if (m_error) {
            return;
        }
        if (m_client->NUM_OF_BUFFERS > 0) {
            if (buffer.getNumSamples() == m_client->getSamplesPerBlock() && m_workingSendSamples == 0) {
                AudioMidiBuffer buf;
                if (m_client->getChannelsIn() > 0) {  // fx
                    buf.audio.makeCopyOf(buffer);
                } else {  // inst
                    buf.channelsRequested = m_client->getNumActiveChannels();
                    buf.samplesRequested = m_client->getSamplesPerBlock();
                }
                buf.midi.addEvents(midi, 0, buffer.getNumSamples(), 0);
                buf.posInfo = posInfo;
                m_writeQ.push(std::move(buf));
                notifyWrite();
            } else {
                if (!copyToWorkingBuffer(m_workingSendBuf, m_workingSendSamples, buffer, midi)) {
                    logln("error: " << getInstanceString() << ": send error");
                    setError();
                    return;
                }
                if (m_workingSendSamples >= m_client->getSamplesPerBlock()) {
                    AudioMidiBuffer buf;
                    if (m_client->getChannelsIn() > 0) {  // fx
                        buf.audio.setSize(m_client->getNumActiveChannels(), m_client->getSamplesPerBlock());
                        for (int chan = 0; chan < m_client->getNumActiveChannels(); chan++) {
                            buf.audio.copyFrom(chan, 0, m_workingSendBuf.audio, chan, 0,
                                               m_client->getSamplesPerBlock());
                        }
                    } else {  // inst
                        buf.channelsRequested = m_client->getNumActiveChannels();
                        buf.samplesRequested = m_client->getSamplesPerBlock();
                    }
                    buf.midi.addEvents(m_workingSendBuf.midi, 0, m_client->getSamplesPerBlock(), 0);
                    m_workingSendBuf.midi.clear(0, m_client->getSamplesPerBlock());
                    buf.posInfo = posInfo;
                    m_writeQ.push(std::move(buf));
                    notifyWrite();
                    m_workingSendSamples -= m_client->getSamplesPerBlock();
                    if (m_workingSendSamples > 0) {
                        shiftSamplesToFront(m_workingSendBuf, m_client->getSamplesPerBlock(), m_workingSendSamples);
                    }
                }
            }
        } else {
            AudioMidiBuffer buf;
            if (m_client->getChannelsIn() > 0) {  // fx
                buf.audio.makeCopyOf(buffer);
            } else {  // inst
                buf.channelsRequested = m_client->getNumActiveChannels();
                buf.samplesRequested = buffer.getNumSamples();
            }
            buf.midi.addEvents(midi, 0, buffer.getNumSamples(), 0);
            buf.posInfo = posInfo;
            m_durationLocal.reset();
            m_durationGlobal.reset();
            if (!sendReal(buf)) {
                logln("error: " << getInstanceString() << ": send failed");
                setError();
            }
        }
    }

    void read(AudioBuffer<T>& buffer, MidiBuffer& midi) {
        traceScope();
        if (m_error) {
            return;
        }
        AudioMidiBuffer buf;
        if (m_client->NUM_OF_BUFFERS > 0) {
            if (buffer.getNumSamples() == m_client->getSamplesPerBlock() && m_workingReadSamples == 0) {
                if (!waitRead()) {
                    logln("error: " << getInstanceString() << ": waitRead failed");
                    return;
                }
                m_readQ.pop(buf);
                buffer.makeCopyOf(buf.audio);
                midi.clear();
                midi.addEvents(buf.midi, 0, buffer.getNumSamples(), 0);
            } else {
                while (m_workingReadSamples < buffer.getNumSamples()) {
                    if (!waitRead()) {
                        logln("error: " << getInstanceString() << ": waitRead failed");
                        return;
                    }
                    m_readQ.pop(buf);
                    if (!copyToWorkingBuffer(m_workingReadBuf, m_workingReadSamples, buf.audio, buf.midi)) {
                        logln("error: " << getInstanceString() << ": read error");
                        setError();
                        return;
                    }
                }
                int maxCh = jmin(buffer.getNumChannels(), m_workingReadBuf.audio.getNumChannels());
                // clear channels of the target buffer, that we have no data for in the src buffer
                for (int chan = maxCh; chan < buffer.getNumChannels(); chan++) {
                    buffer.clear(chan, 0, buffer.getNumSamples());
                }
                for (int chan = 0; chan < maxCh; chan++) {
                    buffer.copyFrom(chan, 0, m_workingReadBuf.audio, chan, 0, buffer.getNumSamples());
                }
                midi.clear();
                midi.addEvents(m_workingReadBuf.midi, 0, buffer.getNumSamples(), 0);
                m_workingReadBuf.midi.clear(0, buffer.getNumSamples());
                m_workingReadSamples -= buffer.getNumSamples();
                if (m_workingReadSamples > 0) {
                    shiftSamplesToFront(m_workingReadBuf, buffer.getNumSamples(), m_workingReadSamples);
                }
            }
        } else {
            buf.audio.setSize(buffer.getNumChannels(), buffer.getNumSamples());
            MessageHelper::Error err;
            if (!readReal(buf, &err)) {
                logln("error: " << getInstanceString() << ": read failed: " << err.toString());
                setError();
                return;
            }
            m_durationLocal.update();
            m_durationGlobal.update();
            buffer.makeCopyOf(buf.audio);
            midi.clear();
            midi.addEvents(buf.midi, 0, buffer.getNumSamples(), 0);
        }
    }

  private:
    struct AudioMidiBuffer {
        int channelsRequested = -1;
        int samplesRequested = -1;
        AudioBuffer<T> audio;
        MidiBuffer midi;
        AudioPlayHead::CurrentPositionInfo posInfo;
    };

    Client* m_client;
    std::unique_ptr<StreamingSocket> m_socket;
    boost::lockfree::spsc_queue<AudioMidiBuffer> m_writeQ, m_readQ;
    std::mutex m_writeMtx, m_readMtx, m_sockMtx;
    std::condition_variable m_writeCv, m_readCv;
    TimeStatistic::Duration m_durationGlobal, m_durationLocal;
    std::shared_ptr<Meter> m_bytesOutMeter, m_bytesInMeter;

    AudioMidiBuffer m_workingSendBuf, m_workingReadBuf;
    int m_workingSendSamples = 0;
    int m_workingReadSamples = 0;

    std::atomic_bool m_error{false};

    void setError() {
        traceScope();
        m_sockMtx.lock();
        m_socket->close();
        m_sockMtx.unlock();
        m_error = true;
        m_client->setError();
        notifyRead();
        notifyWrite();
    }

    String getInstanceString() const {
        traceScope();
        String ret = "instance (";
        ret << m_client->getLoadedPluginsString() << ")";
        return ret;
    }

    void notifyWrite() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_writeMtx);
        m_writeCv.notify_one();
    }

    bool waitWrite() {
        traceScope();
        if (m_error || currentThreadShouldExit()) {
            return false;
        }
        if (m_writeQ.read_available() == 0) {
            std::unique_lock<std::mutex> lock(m_writeMtx);
            return m_writeCv.wait_for(lock, std::chrono::seconds(1),
                                      [this] { return m_writeQ.read_available() > 0 || currentThreadShouldExit(); });
        }
        return true;
    }

    void notifyRead() {
        traceScope();
        std::lock_guard<std::mutex> lock(m_readMtx);
        m_readCv.notify_one();
    }

    bool waitRead() {
        traceScope();
        if (m_client->NUM_OF_BUFFERS > 1 && m_readQ.read_available() < (size_t)(m_client->NUM_OF_BUFFERS / 2) &&
            m_readQ.read_available() > 0) {
            logln("warning: " << getInstanceString() << ": input buffer below 50% (" << m_readQ.read_available() << "/"
                              << m_client->NUM_OF_BUFFERS << ")");
        } else if (m_readQ.read_available() == 0) {
            if (m_client->NUM_OF_BUFFERS > 1) {
                logln("warning: " << getInstanceString()
                                  << ": read queue empty, waiting for data, try increasing the NumberOfBuffers value");
            }
            if (!m_error && !threadShouldExit()) {
                std::unique_lock<std::mutex> lock(m_readMtx);
                return m_readCv.wait_for(lock, std::chrono::seconds(1),
                                         [this] { return m_readQ.read_available() > 0 || threadShouldExit(); });
            }
        }
        return true;
    }

    bool copyToWorkingBuffer(AudioMidiBuffer& dst, int& workingSamples, AudioBuffer<T>& src, MidiBuffer& midi) {
        traceScope();
        if (src.getNumChannels() > 0) {
            if ((dst.audio.getNumSamples() - workingSamples) < src.getNumSamples() ||
                dst.audio.getNumChannels() < src.getNumChannels()) {
                dst.audio.setSize(src.getNumChannels(), workingSamples + src.getNumSamples(), true);
            }
            for (int chan = 0; chan < src.getNumChannels(); chan++) {
                dst.audio.copyFrom(chan, workingSamples, src, chan, 0, src.getNumSamples());
            }
        }
        dst.midi.addEvents(midi, 0, src.getNumSamples(), workingSamples);
        workingSamples += src.getNumSamples();
        return true;
    }

    void shiftSamplesToFront(AudioMidiBuffer& buf, int start, int num) {
        traceScope();
        if (start + num <= buf.audio.getNumSamples()) {
            for (int chan = 0; chan < buf.audio.getNumChannels(); chan++) {
                for (int s = 0; s < num; s++) {
                    buf.audio.setSample(chan, s, buf.audio.getSample(chan, start + s));
                }
            }
        }
        if (buf.midi.getNumEvents() > 0) {
            MidiBuffer midiCpy;
            midiCpy.addEvents(buf.midi, 0, -1, -start);
            buf.midi.clear();
            buf.midi.addEvents(midiCpy, 0, -1, 0);
        }
    }

    bool sendReal(AudioMidiBuffer& buffer) {
        traceScope();
        AudioMessage msg(m_client);
        return msg.sendToServer(m_socket.get(), buffer.audio, buffer.midi, buffer.posInfo, buffer.channelsRequested,
                                buffer.samplesRequested, nullptr, *m_bytesOutMeter);
    }

    bool readReal(AudioMidiBuffer& buffer, MessageHelper::Error* e) {
        traceScope();
        AudioMessage msg(m_client);
        if (buffer.audio.getNumChannels() < buffer.channelsRequested ||
            buffer.audio.getNumSamples() < buffer.samplesRequested) {
            buffer.audio.setSize(buffer.channelsRequested, buffer.samplesRequested);
        }
        bool success = msg.readFromServer(m_socket.get(), buffer.audio, buffer.midi, e, *m_bytesInMeter);
        if (success) {
            m_client->setLatency(msg.getLatencySamples());
        }
        return success;
    }
};

}  // namespace e47

#endif /* AudioStreamer_hpp */
