/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef AudioStreamer_hpp
#define AudioStreamer_hpp

template <typename T>
class AudioStreamer : public Thread, public LogTagDelegate {
  public:
    struct AudioMidiBuffer {
        int channelsRequested = -1;
        int samplesRequested = -1;
        AudioBuffer<T> audio;
        MidiBuffer midi;
        AudioPlayHead::CurrentPositionInfo posInfo;
    };

    Client* client;
    StreamingSocket* socket;
    boost::lockfree::spsc_queue<AudioMidiBuffer> writeQ, readQ;
    std::mutex writeMtx, readMtx;
    std::condition_variable writeCv, readCv;
    TimeStatistics::Duration duration;

    AudioMidiBuffer workingSendBuf, workingReadBuf;
    int workingSendSamples = 0;
    int workingReadSamples = 0;

    std::atomic_bool error{false};

    AudioStreamer(Client* clnt, StreamingSocket* sock)
        : Thread("AudioStreamer"),
          client(clnt),
          socket(sock),
          writeQ(as<size_t>(clnt->NUM_OF_BUFFERS * 2)),
          readQ(as<size_t>(clnt->NUM_OF_BUFFERS * 2)),
          duration(TimeStatistics::getDuration()) {
        setLogTagSource(client);

        for (int i = 0; i < clnt->NUM_OF_BUFFERS; i++) {
            AudioMidiBuffer buf;
            buf.audio.setSize(clnt->m_channelsOut, clnt->m_samplesPerBlock);
            buf.audio.clear();
            readQ.push(std::move(buf));
        }
        workingSendBuf.audio.clear();
        workingReadBuf.audio.clear();
    }

    ~AudioStreamer() {
        logln("audio streamer cleaning up");
        signalThreadShouldExit();
        notifyWrite();
        notifyRead();
        waitForThreadAndLog(getLogTagSource(), this);
        logln("audio streamer cleanup done");
    }

    void run() {
        logln("audio streamer ready");
        while (!currentThreadShouldExit() && !error && socket->isConnected()) {
            while (writeQ.read_available() > 0) {
                AudioMidiBuffer buf;
                writeQ.pop(buf);
                duration.reset();
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
                duration.update();
                readQ.push(std::move(buf));
                notifyRead();
            }
            while (!waitWrite()) {}
        }
        logln("audio streamer terminated");
    }

    void send(AudioBuffer<T>& buffer, MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo) {
        if (error) {
            return;
        }
        if (client->NUM_OF_BUFFERS > 1) {
            if (buffer.getNumSamples() == client->m_samplesPerBlock && workingSendSamples == 0) {
                AudioMidiBuffer buf;
                if (client->m_channelsIn > 0) {
                    buf.audio.makeCopyOf(buffer);
                } else {
                    buf.channelsRequested = client->m_channelsOut;
                    buf.samplesRequested = client->m_samplesPerBlock;
                }
                buf.midi.addEvents(midi, 0, buffer.getNumSamples(), 0);
                buf.posInfo = posInfo;
                writeQ.push(std::move(buf));
                notifyWrite();
            } else {
                if (!copyToWorkingBuffer(workingSendBuf, workingSendSamples, buffer, midi, client->m_channelsIn == 0)) {
                    logln("error: " << getInstanceString() << ": send error");
                    setError();
                    return;
                }
                if (workingSendSamples >= client->m_samplesPerBlock) {
                    AudioMidiBuffer buf;
                    if (client->m_channelsIn > 0) {
                        buf.audio.setSize(client->m_channelsIn, client->m_samplesPerBlock);
                        for (int chan = 0; chan < client->m_channelsIn; chan++) {
                            buf.audio.copyFrom(chan, 0, workingSendBuf.audio, chan, 0, client->m_samplesPerBlock);
                        }
                    } else {
                        buf.channelsRequested = client->m_channelsOut;
                        buf.samplesRequested = client->m_samplesPerBlock;
                    }
                    buf.midi.addEvents(workingSendBuf.midi, 0, client->m_samplesPerBlock, 0);
                    workingSendBuf.midi.clear(0, client->m_samplesPerBlock);
                    buf.posInfo = posInfo;
                    writeQ.push(std::move(buf));
                    notifyWrite();
                    workingSendSamples -= client->m_samplesPerBlock;
                    if (workingSendSamples > 0) {
                        shiftSamplesToFront(workingSendBuf, client->m_samplesPerBlock, workingSendSamples);
                    }
                }
            }
        } else {
            AudioMidiBuffer buf;
            if (client->m_channelsIn > 0) {
                buf.audio.makeCopyOf(buffer);
            } else {
                buf.channelsRequested = buffer.getNumChannels();
                buf.samplesRequested = buffer.getNumSamples();
            }
            buf.midi.addEvents(midi, 0, buffer.getNumSamples(), 0);
            buf.posInfo = posInfo;
            duration.reset();
            if (!sendReal(buf)) {
                logln("error: " << getInstanceString() << ": send failed");
                setError();
            }
        }
    }

    void read(AudioBuffer<T>& buffer, MidiBuffer& midi) {
        if (error) {
            return;
        }
        AudioMidiBuffer buf;
        if (client->NUM_OF_BUFFERS > 1) {
            if (buffer.getNumSamples() == client->m_samplesPerBlock && workingReadSamples == 0) {
                if (!waitRead()) {
                    logln("error: " << getInstanceString() << ": waitRead failed");
                    return;
                }
                readQ.pop(buf);
                buffer.makeCopyOf(buf.audio);
                midi.clear();
                midi.addEvents(buf.midi, 0, buffer.getNumSamples(), 0);
            } else {
                while (workingReadSamples < buffer.getNumSamples()) {
                    if (!waitRead()) {
                        logln("error: " << getInstanceString() << ": waitRead failed");
                        return;
                    }
                    readQ.pop(buf);
                    if (!copyToWorkingBuffer(workingReadBuf, workingReadSamples, buf.audio, buf.midi)) {
                        logln("error: " << getInstanceString() << ": read error");
                        setError();
                        return;
                    }
                }
                for (int chan = 0; chan < buffer.getNumChannels(); chan++) {
                    buffer.copyFrom(chan, 0, workingReadBuf.audio, chan, 0, buffer.getNumSamples());
                }
                midi.clear();
                midi.addEvents(workingReadBuf.midi, 0, buffer.getNumSamples(), 0);
                workingReadBuf.midi.clear(0, buffer.getNumSamples());
                workingReadSamples -= buffer.getNumSamples();
                if (workingReadSamples > 0) {
                    shiftSamplesToFront(workingReadBuf, buffer.getNumSamples(), workingReadSamples);
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
            duration.update();
            buffer.makeCopyOf(buf.audio);
            midi.clear();
            midi.addEvents(buf.midi, 0, buffer.getNumSamples(), 0);
        }
    }

  private:
    void setError() {
        socket->close();
        error = true;
        client->m_error = true;
        notifyRead();
        notifyWrite();
    }

    String getInstanceString() const {
        String ret = "instance (";
        ret << client->getLoadedPluginsString() << ")";
        return ret;
    }

    void notifyWrite() {
        std::lock_guard<std::mutex> lock(writeMtx);
        writeCv.notify_one();
    }

    bool waitWrite() {
        if (error || currentThreadShouldExit()) {
            return false;
        }
        if (writeQ.read_available() == 0) {
            std::unique_lock<std::mutex> lock(writeMtx);
            return writeCv.wait_for(lock, std::chrono::seconds(1), [this] { return writeQ.read_available() > 0 || currentThreadShouldExit(); });
        }
        return true;
    }

    void notifyRead() {
        std::lock_guard<std::mutex> lock(readMtx);
        readCv.notify_one();
    }

    bool waitRead() {
        if (client->NUM_OF_BUFFERS > 1 && readQ.read_available() < as<size_t>(client->NUM_OF_BUFFERS / 2) &&
            readQ.read_available() > 0) {
            logln("warning: " << getInstanceString() << ": input buffer below 50% (" << readQ.read_available() << "/"
                              << client->NUM_OF_BUFFERS << ")");
        } else if (readQ.read_available() == 0) {
            if (client->NUM_OF_BUFFERS > 1) {
                logln("warning: " << getInstanceString()
                                  << ": read queue empty, waiting for data, try increasing the NumberOfBuffers value");
            }
            if (!error && !threadShouldExit()) {
                std::unique_lock<std::mutex> lock(readMtx);
                return readCv.wait_for(lock, std::chrono::seconds(1), [this] { return readQ.read_available() > 0 || threadShouldExit(); });
            }
        }
        return true;
    }

    bool copyToWorkingBuffer(AudioMidiBuffer& dst, int& workingSamples, AudioBuffer<T>& src, MidiBuffer& midi,
                             bool midiOnly = false) {
        if (!midiOnly) {
            if (src.getNumChannels() < 1) {
                logln("error: " << getInstanceString() << ": copy failed, source audio buffer empty");
                return false;
            }
            if ((dst.audio.getNumSamples() - workingSamples) < src.getNumSamples()) {
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
        AudioMessage msg;
        if (nullptr != socket) {
            return msg.sendToServer(socket, buffer.audio, buffer.midi, buffer.posInfo, buffer.channelsRequested,
                                    buffer.samplesRequested);
        } else {
            return false;
        }
    }

    bool readReal(AudioMidiBuffer& buffer, MessageHelper::Error* e) {
        AudioMessage msg;
        bool success = false;
        if (nullptr != socket) {
            if (buffer.audio.getNumChannels() < buffer.channelsRequested ||
                buffer.audio.getNumSamples() < buffer.samplesRequested) {
                buffer.audio.setSize(buffer.channelsRequested, buffer.samplesRequested);
            }
            success = msg.readFromServer(socket, buffer.audio, buffer.midi, e);
        }
        if (success) {
            client->m_latency = msg.getLatencySamples();
        }
        return success;
    }
};

#endif /* AudioStreamer_hpp */
