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
          m_durationLocal(TimeStatistic::getDuration(String("audio.") + String(getTagId()), false)) {
        traceScope();

        for (int i = 0; i < clnt->NUM_OF_BUFFERS; i++) {
            AudioMidiBuffer buf;
            buf.channelsRequested = clnt->getChannelsIn();
            buf.samplesRequested = clnt->getSamplesPerBlock();
            buf.audio.setSize(clnt->getChannelsIn(), clnt->getSamplesPerBlock());
            buf.audio.clear();
            m_readQ.push(std::move(buf));
        }
        m_readBuffer.audio.clear();

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
        bool isDouble = std::is_same<T, double>::value;
        logln("audio streamer ready, isDouble = " << (int)isDouble);
        while (!threadShouldExit() && !m_error && m_socket->isConnected()) {
            while (m_writeQ.read_available() > 0) {
                AudioMidiBuffer buf;
                m_writeQ.pop(buf);
                m_durationLocal.reset();
                m_durationGlobal.reset();
                if (!sendInternal(buf)) {
                    logln("error: " << getInstanceString() << ": send failed");
                    setError();
                    return;
                }
                MessageHelper::Error err;
                if (!readInternal(buf, &err)) {
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

    void send(AudioBuffer<T>& buffer, MidiBuffer& midi, AudioPlayHead::PositionInfo& posInfo) {
        traceScope();

        if (m_error) {
            return;
        }

        traceln("  client: numBuffers=" << m_client->NUM_OF_BUFFERS << ", blockSize=" << m_client->getSamplesPerBlock()
                                        << ", fixed=" << (int)m_client->FIXED_OUTBOUND_BUFFER
                                        << ", isFx=" << (int)m_client->isFx());
        traceln("  queues: r.size=" << m_readQ.read_available() << ", w.size=" << m_writeQ.read_available());
        traceln("  buffer (in): channels=" << buffer.getNumChannels() << ", samples=" << buffer.getNumSamples());

        TimeTrace::addTracePoint("as_prep");

        if (m_client->NUM_OF_BUFFERS > 0) {
            if (m_client->isFx()) {
                m_writeBuffer.copyFrom(buffer, midi);
            } else {
                m_writeBuffer.copyFrom({}, midi, 0, buffer.getNumSamples());
            }

            m_writeBuffer.updatePosition(posInfo);

            traceln("  buffer (write, after copy): working samples=" << m_writeBuffer.workingSamples);

            if (!m_client->FIXED_OUTBOUND_BUFFER || m_writeBuffer.workingSamples >= m_client->getSamplesPerBlock()) {
                int samples = m_writeBuffer.workingSamples;
                if (m_client->FIXED_OUTBOUND_BUFFER) {
                    samples = m_client->getSamplesPerBlock();
                }

                AudioMidiBuffer buf;
                buf.posInfo = m_writeBuffer.posInfo;
                buf.copyFromAndConsume(m_writeBuffer, samples);

                if (!m_client->isFx()) {
                    buf.channelsRequested = buffer.getNumChannels();
                    buf.samplesRequested = samples;
                }

                traceln("  buffer (out): ch req=" << buf.channelsRequested << ", smpls req=" << buf.samplesRequested
                                                  << ", smpls out=" << buf.audio.getNumSamples() << ",");
                traceln("    midi.events=" << buf.midi.getNumEvents());
                traceln("  buffer (write, after send): working samples=" << m_writeBuffer.workingSamples);

                m_writeQ.push(std::move(buf));
                TimeTrace::addTracePoint("as_push");
                notifyWrite();
                TimeTrace::addTracePoint("as_notify");
            }
        } else {
            AudioMidiBuffer buf;
            buf.posInfo = posInfo;
            if (m_client->isFx()) {
                buf.copyFrom(buffer, midi);
            } else {
                buf.channelsRequested = buffer.getNumChannels();
                buf.samplesRequested = buffer.getNumSamples();
                buf.copyFrom({}, midi, 0, buffer.getNumSamples());
            }
            m_durationLocal.reset();
            m_durationGlobal.reset();
            if (!sendInternal(buf)) {
                logln("error: " << getInstanceString() << ": send failed");
                setError();
            }
            TimeTrace::addTracePoint("as_send");
        }
    }

    void read(AudioBuffer<T>& buffer, MidiBuffer& midi) {
        traceScope();

        if (m_error) {
            return;
        }

        midi.clear();

        traceln("  client: num buffers=" << m_client->NUM_OF_BUFFERS);
        traceln("  queues: r.size=" << m_readQ.read_available() << ", w.size=" << m_writeQ.read_available());

        AudioMidiBuffer buf;

        if (m_client->NUM_OF_BUFFERS > 0) {
            if (m_readBuffer.workingSamples < buffer.getNumSamples()) {
                traceln("  buffer (read): working samples=" << m_readBuffer.workingSamples << ",");
                traceln("    channels=" << m_readBuffer.audio.getNumChannels()
                                        << ", samples=" << m_readBuffer.audio.getNumSamples());
            }

            TimeTrace::startGroup();

            while (m_readBuffer.workingSamples < buffer.getNumSamples()) {
                traceln("  waiting for data...");
                if (!waitRead()) {
                    logln("error: " << getInstanceString() << ": waitRead failed");
                    TimeTrace::finishGroup("as_wait_read_failed");
                    return;
                }
                TimeTrace::addTracePoint("as_wait_read");
                if (m_readQ.pop(buf)) {
                    traceln("  pop buffer: channels=" << buf.audio.getNumChannels()
                                                      << ", samples=" << buf.audio.getNumSamples());
                    m_readBuffer.copyFrom(buf);
                } else {
                    logln("error: " << getInstanceString() << ": read queue empty");
                    return;
                }
                TimeTrace::addTracePoint("as_pop");
            }

            TimeTrace::finishGroup("as_get_buffer");

            traceln("  buffer (read, after pop): working samples=" << m_readBuffer.workingSamples << ",");
            traceln("    channels=" << m_readBuffer.audio.getNumChannels()
                                    << ", samples=" << m_readBuffer.audio.getNumSamples());

            int maxCh = jmin(buffer.getNumChannels(), m_readBuffer.audio.getNumChannels());

            // clear channels of the target buffer, that we have no data for in the src buffer
            for (int chan = maxCh; chan < buffer.getNumChannels(); chan++) {
                traceln("  clearing channel " << chan << "...");
                buffer.clear(chan, 0, buffer.getNumSamples());
            }

            m_readBuffer.copyToAndConsume(buffer, midi, buffer.getNumChannels(), buffer.getNumSamples());

            traceln("  buffer (read, after consume): working samples=" << m_readBuffer.workingSamples << ",");
            traceln("    channels=" << m_readBuffer.audio.getNumChannels()
                                    << ", samples=" << m_readBuffer.audio.getNumSamples());

            TimeTrace::addTracePoint("as_consume");

            traceln("  consumed " << buffer.getNumSamples() << " samples");
        } else {
            buf.channelsRequested = buffer.getNumChannels();
            buf.samplesRequested = buffer.getNumSamples();
            buf.audio.setSize(buffer.getNumChannels(), buffer.getNumSamples());
            MessageHelper::Error err;
            if (!readInternal(buf, &err)) {
                logln("error: " << getInstanceString() << ": read failed: " << err.toString());
                setError();
                return;
            }
            TimeTrace::addTracePoint("as_read");
            m_durationLocal.update();
            m_durationGlobal.update();
            buf.copyToAndConsume(buffer, midi, buffer.getNumChannels(), buffer.getNumSamples());
        }
    }

  private:
    struct AudioMidiBuffer {
        int channelsRequested = -1;
        int samplesRequested = -1;
        int workingSamples = 0;
        AudioBuffer<T> audio;
        MidiBuffer midi;
        AudioPlayHead::PositionInfo posInfo;
        bool needsPositionUpdate = true;

        LogTag tag = LogTag("audiomidibuffer");

        void copyFrom(const AudioMidiBuffer& src, int numChannels = -1, int numSamples = -1) {
            copyFrom(src.audio, src.midi, numChannels, numSamples);
        }

        void copyFrom(const AudioBuffer<T>& srcBuffer, const MidiBuffer& srcMidi, int numChannels = -1,
                      int numSamples = -1) {
            setLogTagByRef(tag);
            traceScope();

            if (numChannels == -1) {
                numChannels = srcBuffer.getNumChannels();
            }
            if (numSamples == -1) {
                numSamples = srcBuffer.getNumSamples();
            }

            traceln("  params: channels=" << numChannels << ", samples=" << numSamples);
            traceln("    src: channels=" << srcBuffer.getNumChannels() << ", samples=" << srcBuffer.getNumSamples());
            traceln("    midi: events=" << srcMidi.getNumEvents());
            traceln("  this: working smpls=" << workingSamples << ", ch req=" << channelsRequested
                                             << ", smpls req=" << samplesRequested << ",");
            traceln("    audio.ch=" << audio.getNumChannels() << ", audio.smpls=" << audio.getNumSamples()
                                    << ", midi.events=" << midi.getNumEvents());

            if (numChannels > 0 && numSamples > 0 && srcBuffer.getNumChannels() > 0 && srcBuffer.getNumSamples() > 0) {
                if ((audio.getNumSamples() - workingSamples) < numSamples || audio.getNumChannels() < numChannels) {
                    audio.setSize(numChannels, workingSamples + numSamples, true);
                }
                for (int chan = 0; chan < numChannels; chan++) {
                    audio.copyFrom(chan, workingSamples, srcBuffer, chan, 0, numSamples);
                }
            }
            midi.addEvents(srcMidi, 0, numSamples, workingSamples);
            workingSamples += numSamples;
        }

        void copyFromAndConsume(AudioMidiBuffer& src, int numSamples = -1) {
            if (numSamples == -1) {
                numSamples = src.audio.getNumSamples();
            }
            moveOrCopyFrom(src.audio, src.midi, numSamples);
            src.consume(numSamples);
        }

        void moveOrCopyFrom(AudioBuffer<T>& srcBuffer, MidiBuffer& srcMidi, int numSamples) {
            setLogTagByRef(tag);
            traceScope();

            traceln("  params: samples=" << numSamples);
            traceln("    src: channels=" << srcBuffer.getNumChannels() << ", samples=" << srcBuffer.getNumSamples());
            traceln("    midi: events=" << srcMidi.getNumEvents());
            traceln("  this: working smpls=" << workingSamples << ", ch req=" << channelsRequested
                                             << ", smpls req=" << samplesRequested << ",");
            traceln("    audio.ch=" << audio.getNumChannels() << ", audio.smpls=" << audio.getNumSamples()
                                    << ", midi.events=" << midi.getNumEvents());

            if (srcBuffer.getNumChannels() > 0 && srcBuffer.getNumSamples() > 0) {
                if (numSamples == srcBuffer.getNumSamples()) {
                    traceln("  moving audio buffer");
                    audio = std::move(srcBuffer);
                } else {
                    if ((audio.getNumSamples() - workingSamples) < numSamples ||
                        audio.getNumChannels() < srcBuffer.getNumChannels()) {
                        audio.setSize(srcBuffer.getNumChannels(), workingSamples + numSamples, true);
                    }
                    for (int chan = 0; chan < srcBuffer.getNumChannels(); chan++) {
                        traceln("  copying channel " << chan);
                        audio.copyFrom(chan, workingSamples, srcBuffer, chan, 0, numSamples);
                    }
                }
            }
            midi.addEvents(srcMidi, 0, numSamples, workingSamples);
            workingSamples += numSamples;
        }

        void copyToAndConsume(AudioBuffer<T>& dstBuffer, MidiBuffer& dstMidi, int numChannels, int numSamples) {
            if (numChannels > 0 && numSamples > 0 && audio.getNumChannels() > 0 && audio.getNumSamples() > 0) {
                if (dstBuffer.getNumSamples() < numSamples || dstBuffer.getNumChannels() < numChannels) {
                    dstBuffer.setSize(numChannels, numSamples, true);
                }
                for (int chan = 0; chan < numChannels; chan++) {
                    dstBuffer.copyFrom(chan, 0, audio, chan, 0, numSamples);
                }
            }
            dstMidi.addEvents(midi, 0, numSamples, 0);
            if (workingSamples > 0) {
                consume(numSamples);
            }
        }

        void updatePosition(const AudioPlayHead::PositionInfo& pos) {
            if (needsPositionUpdate) {
                posInfo = pos;
                needsPositionUpdate = false;
            }
        }

        void consume(int samples) {
            setLogTagByRef(tag);
            traceScope();

            traceln("  params: samples=" << samples);
            traceln("  this: working smpls=" << workingSamples << ", ch req=" << channelsRequested
                                             << ", smpls req=" << samplesRequested << ",");
            traceln("    audio.ch=" << audio.getNumChannels() << ", audio.smpls=" << audio.getNumSamples()
                                    << ", midi.events=" << midi.getNumEvents());

            workingSamples -= samples;
            shiftAndResize(samples);

            needsPositionUpdate = true;
        }

        void shiftAndResize(int samples) {
            if (workingSamples > 0) {
                for (int chan = 0; chan < audio.getNumChannels(); chan++) {
                    for (int s = 0; s < workingSamples; s++) {
                        audio.setSample(chan, s, audio.getSample(chan, samples + s));
                    }
                }
                if (midi.getNumEvents() > 0) {
                    MidiBuffer midiCpy;
                    midiCpy.addEvents(midi, 0, -1, -samples);
                    midi.clear();
                    midi.addEvents(midiCpy, 0, -1, 0);
                }
            } else {
                midi.clear();
            }
            audio.setSize(audio.getNumChannels(), workingSamples, true);
        }
    };

    Client* m_client;
    std::unique_ptr<StreamingSocket> m_socket;
    boost::lockfree::spsc_queue<AudioMidiBuffer> m_writeQ, m_readQ;
    std::mutex m_writeMtx, m_readMtx, m_sockMtx;
    std::condition_variable m_writeCv, m_readCv;
    TimeStatistic::Duration m_durationGlobal, m_durationLocal;
    std::shared_ptr<Meter> m_bytesOutMeter, m_bytesInMeter;

    AudioMidiBuffer m_readBuffer, m_writeBuffer;

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
        if (m_error || threadShouldExit()) {
            return false;
        }
        if (m_writeQ.read_available() == 0) {
            std::unique_lock<std::mutex> lock(m_writeMtx);
            return m_writeCv.wait_for(lock, std::chrono::seconds(1),
                                      [this] { return m_writeQ.read_available() > 0 || threadShouldExit(); });
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

    bool sendInternal(AudioMidiBuffer& buffer) {
        traceScope();
        AudioMessage msg(m_client);
        return msg.sendToServer(m_socket.get(), buffer.audio, buffer.midi, buffer.posInfo, buffer.channelsRequested,
                                buffer.samplesRequested, nullptr, *m_bytesOutMeter);
    }

    bool readInternal(AudioMidiBuffer& buffer, MessageHelper::Error* e) {
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
