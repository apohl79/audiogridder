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
          m_queueSize((size_t)clnt->NUM_OF_BUFFERS * 8),
          m_queueHighWaterMark((size_t)clnt->NUM_OF_BUFFERS * 7),
          m_writeQ(m_queueSize),
          m_readQ(m_queueSize),
          m_durationGlobal(TimeStatistic::getDuration("audio_stream")),
          m_durationLocal(TimeStatistic::getDuration(String("audio_stream.") + String(getTagId()), false, false)),
          m_readQMeter((size_t)(clnt->getSampleRate() / clnt->getSamplesPerBlock()) + 1),
          m_readTimeoutMs((int)(clnt->getSamplesPerBlock() / clnt->getSampleRate() * 1000 - 1)) {
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
        if (m_queueSize > 0) {
            notifyWrite();
            notifyRead();
        }
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

    const SizeMeter& getReadQueueMeter() const { return m_readQMeter; }
    int getReadTimeoutMs() const { return m_readTimeoutMs; }
    uint64_t getReadErrors() const { return m_readErrors; }

    void run() {
        traceScope();
        bool isDouble = std::is_same<T, double>::value;
        logln("audio streamer ready, isDouble = " << (int)isDouble);
        while (!threadShouldExit() && !m_error && m_socket->isConnected()) {
            if (m_queueSize > 0) {
                while (m_writeQ.read_available() > 0) {
                    AudioMidiBuffer buf;
                    m_writeQ.pop(buf);
                    if (!buf.skip) {
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
                        // drop samples in case we had read error(s)
                        if (m_dropSamples > 0) {
                            int samples = m_dropSamples.exchange(0);
                            if (samples < buf.workingSamples) {
                                buf.consume(samples);
                            } else {
                                m_dropSamples += samples - buf.workingSamples;
                                buf.workingSamples = 0;
                            }
                        }
                        m_durationLocal.update();
                        m_durationGlobal.update();
                    } else {
                        // add silence
                        buf.audio.setSize(buf.channelsRequested, buf.samplesRequested);
                        buf.audio.clear();
                        buf.workingSamples = buf.samplesRequested;
                    }
                    if (buf.workingSamples > 0) {
                        m_readQ.push(std::move(buf));
                        notifyRead();
                    }
                }
                waitWrite();
            } else {
                if (waitRead()) {
                    m_ioThreadBusy = true;
                    MessageHelper::Error err;
                    if (!readInternal(m_readBuffer, &err)) {
                        logln("error: " << getInstanceString() << ": read failed: " << err.toString());
                        if (err.code != MessageHelper::E_TIMEOUT) {
                            setError();
                        }
                    }
                    m_ioThreadBusy = false;
                    m_ioDataReady.signal();
                }
            }
        }
        m_durationLocal.clear();
        m_durationGlobal.clear();
        logln("audio streamer terminated");
    }

    bool send(AudioBuffer<T>& buffer, MidiBuffer& midi, AudioPlayHead::PositionInfo& posInfo) {
        traceScope();

        if (m_error) {
            return false;
        }

        traceln("  client: numBuffers=" << m_client->NUM_OF_BUFFERS << ", blockSize=" << m_client->getSamplesPerBlock()
                                        << ", fixed=" << (int)m_client->FIXED_OUTBOUND_BUFFER
                                        << ", isFx=" << (int)m_client->isFx());
        traceln("  queues: r.size=" << m_readQ.read_available() << ", w.size=" << m_writeQ.read_available());
        traceln("  buffer (in): channels=" << buffer.getNumChannels() << ", samples=" << buffer.getNumSamples());

        TimeTrace::addTracePoint("as_prep");

        if (m_client->NUM_OF_BUFFERS > 0) {
            if ((m_client->LIVE_MODE && m_writeQ.read_available() > (size_t)m_client->NUM_OF_BUFFERS) ||
                m_writeQ.read_available() > m_queueHighWaterMark) {
                logln("error: " << getInstanceString() << ": write queue full, dropping samples");
                m_readErrors++;
                // add a skip reqord to the queue
                AudioMidiBuffer buf;
                buf.skip = true;
                buf.channelsRequested = buffer.getNumChannels();
                buf.samplesRequested = buffer.getNumSamples();
                m_writeQ.push(std::move(buf));
                notifyWrite();
                TimeTrace::addTracePoint("as_skip");
                return true;
            }

            if (m_client->isFx()) {
                m_writeBuffer.copyFrom(buffer, midi);
            } else {
                m_writeBuffer.copyFrom({}, midi, 0, buffer.getNumSamples());
            }

            TimeTrace::addTracePoint("as_copy_to_wbuf");

            m_writeBuffer.updatePosition(posInfo);

            TimeTrace::addTracePoint("as_upd_pos");

            traceln("  buffer (write, after copy): working samples=" << m_writeBuffer.workingSamples);

            if (!m_client->FIXED_OUTBOUND_BUFFER || m_writeBuffer.workingSamples >= m_client->getSamplesPerBlock()) {
                int samples = m_writeBuffer.workingSamples;
                if (m_client->FIXED_OUTBOUND_BUFFER) {
                    samples = m_client->getSamplesPerBlock();
                }

                AudioMidiBuffer buf;
                buf.posInfo = m_writeBuffer.posInfo;
                buf.copyFromAndConsume(m_writeBuffer, samples);

                TimeTrace::addTracePoint("as_copy_from_wbuf");

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
            if (m_client->LIVE_MODE && m_ioThreadBusy) {
                logln("error: " << getInstanceString() << ": io thread busy, dropping samples");
                m_readErrors++;
                buffer.clear();
                return false;
            }

            AudioMidiBuffer buf;
            buf.posInfo = posInfo;

            if (m_client->isFx()) {
                buf.copyFrom(buffer, midi);
            } else {
                buf.channelsRequested = buffer.getNumChannels();
                buf.samplesRequested = buffer.getNumSamples();
                buf.copyFrom({}, midi, 0, buffer.getNumSamples());
            }

            TimeTrace::addTracePoint("as_copy");

            m_durationLocal.reset();
            m_durationGlobal.reset();

            if (!sendInternal(buf)) {
                logln("error: " << getInstanceString() << ": send failed");
                setError();
                buffer.clear();
                return false;
            }

            TimeTrace::addTracePoint("as_send");
        }

        return true;
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
                    m_dropSamples += buffer.getNumSamples();
                    m_readErrors++;
                    logln("error: " << getInstanceString() << ": waitRead failed");
                    TimeTrace::finishGroup("as_wait_read_failed");
                    return;
                }

                TimeTrace::addTracePoint("as_wait_read");

                if (m_readQ.pop(buf)) {
                    TimeTrace::addTracePoint("as_pop");

                    traceln("  pop buffer: channels=" << buf.audio.getNumChannels() << ", samples="
                                                      << buf.audio.getNumSamples() << ", skip=" << (int)buf.skip);

                    m_readBuffer.copyFrom(buf);

                    TimeTrace::addTracePoint("as_copy_to_rbuf");
                } else {
                    logln("error: " << getInstanceString() << ": read queue empty");
                    return;
                }
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

            m_readBuffer.copyToAndConsume(buffer, midi, maxCh, buffer.getNumSamples());

            traceln("  buffer (read, after consume): working samples=" << m_readBuffer.workingSamples << ",");
            traceln("    channels=" << m_readBuffer.audio.getNumChannels()
                                    << ", samples=" << m_readBuffer.audio.getNumSamples());

            TimeTrace::addTracePoint("as_consume");

            traceln("  consumed " << buffer.getNumSamples() << " samples");
        } else {
            m_readBuffer.channelsRequested = buffer.getNumChannels();
            m_readBuffer.samplesRequested = buffer.getNumSamples();
            m_readBuffer.audio.setSize(buffer.getNumChannels(), buffer.getNumSamples());
            m_readBuffer.midi.clear();

            if (m_client->LIVE_MODE) {
                if (m_ioThreadBusy) {
                    traceln("io thread busy");
                    m_readErrors++;
                    buffer.clear();
                    TimeTrace::addTracePoint("as_io_busy");
                    return;
                } else {
                    notifyRead();
                    if (!m_ioDataReady.wait(m_readTimeoutMs)) {
                        logln("error: " << getInstanceString() << ": read timeout, dropping samples");
                        m_readErrors++;
                        buffer.clear();
                        TimeTrace::addTracePoint("as_io_timeout");
                        return;
                    }
                }
            } else {
                MessageHelper::Error err;
                if (!readInternal(m_readBuffer, &err)) {
                    logln("error: " << getInstanceString() << ": read failed: " << err.toString());
                    setError();
                    return;
                }
            }

            TimeTrace::addTracePoint("as_read");

            m_durationLocal.update();
            m_durationGlobal.update();
            m_readBuffer.copyToAndConsume(buffer, midi, buffer.getNumChannels(), buffer.getNumSamples());

            TimeTrace::addTracePoint("as_consume");
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
        bool skip = false;

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
            traceln("    this: working smpls=" << workingSamples << ", ch req=" << channelsRequested
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
            setLogTagByRef(tag);
            traceScope();
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
            traceln("    this: working smpls=" << workingSamples << ", ch req=" << channelsRequested
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
            setLogTagByRef(tag);
            traceScope();

            numChannels = jmin(audio.getNumChannels(), numChannels);

            traceln("  params: ch=" << numChannels << ", smpls=" << numSamples);
            traceln("    audio.ch=" << audio.getNumChannels() << ", audio.smpls=" << audio.getNumSamples()
                                    << ", midi.events=" << midi.getNumEvents());

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
            samples = jmin(samples, workingSamples);
            if (workingSamples > 0) {
                if (audio.getNumSamples() >= workingSamples) {
                    for (int chan = 0; chan < audio.getNumChannels(); chan++) {
                        for (int s = 0; s < workingSamples; s++) {
                            audio.setSample(chan, s, audio.getSample(chan, samples + s));
                        }
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
    size_t m_queueSize, m_queueHighWaterMark;
    boost::lockfree::spsc_queue<AudioMidiBuffer> m_writeQ, m_readQ;
    std::mutex m_writeMtx, m_readMtx, m_sockMtx;
    std::condition_variable m_writeCv, m_readCv;
    TimeStatistic::Duration m_durationGlobal, m_durationLocal;
    std::shared_ptr<Meter> m_bytesOutMeter, m_bytesInMeter;
    SizeMeter m_readQMeter;
    const int m_readTimeoutMs;
    std::atomic_int m_dropSamples{0};
    std::atomic_uint64_t m_readErrors{0};

    std::atomic_bool m_ioThreadBusy{false};
    WaitableEvent m_ioDataReady;

    AudioMidiBuffer m_readBuffer, m_writeBuffer;

    std::atomic_bool m_error{false};

    void setError() {
        traceScope();
        m_sockMtx.lock();
        m_socket->close();
        m_sockMtx.unlock();
        m_error = true;
        m_client->setError();
        if (m_queueSize > 0) {
            notifyRead();
            notifyWrite();
        }
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
        if (m_queueSize == 0) {
            m_ioDataReady.reset();
        }
        std::lock_guard<std::mutex> lock(m_readMtx);
        m_readCv.notify_one();
    }

    bool waitRead() {
        traceScope();
        if (m_queueSize > 0) {
            m_readQMeter.update(m_readQ.read_available());
            if (m_client->NUM_OF_BUFFERS > 1 && m_readQ.read_available() < (size_t)(m_client->NUM_OF_BUFFERS / 2) &&
                m_readQ.read_available() > 0) {
                logln("warning: " << getInstanceString() << ": input buffer below 50% (" << m_readQ.read_available()
                                  << "/" << m_client->NUM_OF_BUFFERS << ")");
            } else if (m_readQ.read_available() == 0) {
                if (m_client->NUM_OF_BUFFERS > 1) {
                    logln("warning: " << getInstanceString()
                                      << ": read queue empty, waiting for data, try to increase the buffer");
                }
                if (!m_error && !threadShouldExit()) {
                    int timeout = m_client->LIVE_MODE ? m_readTimeoutMs : 1000;
                    std::unique_lock<std::mutex> lock(m_readMtx);
                    return m_readCv.wait_for(lock, std::chrono::milliseconds(timeout),
                                             [this] { return m_readQ.read_available() > 0 || threadShouldExit(); });
                }
            }
        } else {
            if (!m_error && !threadShouldExit()) {
                std::unique_lock<std::mutex> lock(m_readMtx);
                return m_readCv.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::no_timeout;
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
            buffer.workingSamples = buffer.audio.getNumSamples();
            m_client->setLatency(msg.getLatencySamples());
        }
        return success;
    }
};

}  // namespace e47

#endif /* AudioStreamer_hpp */
