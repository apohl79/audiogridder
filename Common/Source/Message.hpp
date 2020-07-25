/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Message_hpp
#define Message_hpp

#include "KeyAndMouseCommon.hpp"
#include "NumberConversion.hpp"
#include "json.hpp"

namespace e47 {

using json = nlohmann::json;

/*
 * Core I/O functions
 */
struct MessageHelper {
    enum ErrorCode { E_NONE, E_DATA, E_TIMEOUT, E_STATE, E_SYSCALL, E_SIZE };

    struct Error {
        ErrorCode code = E_NONE;
        String str = "";
        String toString() const {
            String ret = "EC=";
            ret << code;
            ret << " STR=" << str;
            return ret;
        }
    };

    static void seterr(Error* e, ErrorCode c, String s = "") {
        if (nullptr != e) {
            e->code = c;
            e->str = s;
        }
    }

    static void seterrstr(Error* e, String s) {
        if (nullptr != e) {
            e->str = s;
        }
    }
};

static bool send(StreamingSocket* socket, const char* data, int size, MessageHelper::Error* e = nullptr) {
    if (nullptr != socket && socket->isConnected()) {
        int toWrite = size;
        do {
            int ret = socket->waitUntilReady(false, 100);
            if (ret < 0) {
                MessageHelper::seterr(e, MessageHelper::E_SYSCALL);
                return false;  // error
            } else if (ret > 0) {
                int len = socket->write(static_cast<const char*>(data) + size - toWrite, toWrite);
                if (len < 0) {
                    MessageHelper::seterr(e, MessageHelper::E_SYSCALL);
                    return false;
                }
                data += len;
                toWrite -= len;
            }
        } while (toWrite > 0);
        return true;
    } else {
        return false;
    }
}

static bool read(StreamingSocket* socket, void* data, int size, int timeoutMilliseconds = 0,
                 MessageHelper::Error* e = nullptr) {
    MessageHelper::seterr(e, MessageHelper::E_NONE);
    if (nullptr != socket && !socket->isConnected()) {
        MessageHelper::seterr(e, MessageHelper::E_STATE);
        return false;
    }
    auto now = Time::getMillisecondCounterHiRes();
    auto until = now;
    if (timeoutMilliseconds > 0) {
        until += timeoutMilliseconds;
    }
    int toRead = size;
    while (toRead > 0 && now <= until) {
        int ret = socket->waitUntilReady(true, 100);
        if (ret < 0) {
            MessageHelper::seterr(e, MessageHelper::E_SYSCALL);
            return false;  // error
        } else if (ret > 0) {
            int len = socket->read(static_cast<char*>(data) + size - toRead, toRead, timeoutMilliseconds == 0);
            if (len < 0) {
                MessageHelper::seterr(e, MessageHelper::E_SYSCALL);
                return false;
            } else if (len == 0) {
                MessageHelper::seterr(e, MessageHelper::E_DATA);
                return false;
            }
            toRead -= len;
        }
        now = Time::getMillisecondCounterHiRes();
    }
    if (toRead == 0) {
        return true;
    } else {
        MessageHelper::seterr(e, MessageHelper::E_TIMEOUT);
        return false;
    }
}

/*
 * Client/Server handshake
 */
struct Handshake {
    int version;
    int clientPort;
    int channelsIn;
    int channelsOut;
    double rate;
    int samplesPerBlock;
    bool doublePrecission;
};

/*
 * Audio streaming
 */
class AudioMessage {
  public:
    struct RequestHeader {
        int channels;
        int samples;
        int channelsRequested;  // If only midi data is sent, let the server know about the expected audio buffer size
        int samplesRequested;   // If only midi data is sent, let the server know about the expected audio buffer size
        int numMidiEvents;
        bool isDouble;
    };

    struct ResponseHeader {
        int channels;
        int samples;
        int numMidiEvents;
        int latencySamples;
    };

    struct MidiHeader {
        int sampleNumber;
        int size;
    };

    int getChannels() const { return m_reqHeader.channels; }
    int getChannelsRequested() const { return m_reqHeader.channelsRequested; }
    int getSamples() const { return m_reqHeader.samples; }
    int getSamplesRequested() const { return m_reqHeader.samplesRequested; }
    bool isDouble() const { return m_reqHeader.isDouble; }

    int getLatencySamples() const { return m_resHeader.latencySamples; }

    template <typename T>
    bool sendToServer(StreamingSocket* socket, AudioBuffer<T>& buffer, MidiBuffer& midi,
                      AudioPlayHead::CurrentPositionInfo& posInfo, int channelsRequested = -1,
                      int samplesRequested = -1) {
        m_reqHeader.channels = buffer.getNumChannels();
        m_reqHeader.samples = buffer.getNumSamples();
        m_reqHeader.channelsRequested = channelsRequested > -1 ? channelsRequested : buffer.getNumChannels();
        m_reqHeader.samplesRequested = samplesRequested > -1 ? samplesRequested : buffer.getNumSamples();
        m_reqHeader.isDouble = std::is_same<T, double>::value;
        m_reqHeader.numMidiEvents = midi.getNumEvents();
        if (socket->isConnected()) {
            if (!send(socket, reinterpret_cast<const char*>(&m_reqHeader), sizeof(m_reqHeader))) {
                return false;
            }
            for (int chan = 0; chan < m_reqHeader.channels; ++chan) {
                if (!send(socket, reinterpret_cast<const char*>(buffer.getReadPointer(chan)),
                          m_reqHeader.samples * as<int>(sizeof(T)))) {
                    return false;
                }
            }
            const uint8* midiData;
            MidiHeader midiHdr;
            MidiBuffer::Iterator midiIt(midi);
            while (midiIt.getNextEvent(midiData, midiHdr.size, midiHdr.sampleNumber)) {
                if (!send(socket, reinterpret_cast<const char*>(&midiHdr), sizeof(midiHdr))) {
                    return false;
                }
                if (!send(socket, reinterpret_cast<const char*>(midiData), midiHdr.size)) {
                    return false;
                }
            }
            if (!send(socket, reinterpret_cast<const char*>(&posInfo), sizeof(posInfo))) {
                return false;
            }
        }
        return true;
    }

    template <typename T>
    bool sendToClient(StreamingSocket* socket, AudioBuffer<T>& buffer, MidiBuffer& midi, int latencySamples,
                      int channelsToSend) {
        m_resHeader.channels = channelsToSend;
        m_resHeader.samples = buffer.getNumSamples();
        m_resHeader.latencySamples = latencySamples;
        m_resHeader.numMidiEvents = midi.getNumEvents();
        if (socket->isConnected()) {
            if (!send(socket, reinterpret_cast<const char*>(&m_resHeader), sizeof(m_resHeader))) {
                return false;
            }
            for (int chan = 0; chan < m_resHeader.channels; ++chan) {
                if (!send(socket, reinterpret_cast<const char*>(buffer.getReadPointer(chan)),
                          m_resHeader.samples * as<int>(sizeof(T)))) {
                    return false;
                }
            }
            const uint8* midiData;
            MidiHeader midiHdr;
            MidiBuffer::Iterator midiIt(midi);
            while (midiIt.getNextEvent(midiData, midiHdr.size, midiHdr.sampleNumber)) {
                if (!send(socket, reinterpret_cast<const char*>(&midiHdr), sizeof(midiHdr))) {
                    return false;
                }
                if (!send(socket, reinterpret_cast<const char*>(midiData), midiHdr.size)) {
                    return false;
                }
            }
        }
        return true;
    }

    template <typename T>
    bool readFromServer(StreamingSocket* socket, AudioBuffer<T>& buffer, MidiBuffer& midi, MessageHelper::Error* e) {
        if (socket->isConnected()) {
            if (!read(socket, &m_resHeader, sizeof(m_resHeader), 1000, e)) {
                MessageHelper::seterrstr(e, "response header");
                return false;
            }
            if (buffer.getNumChannels() < m_resHeader.channels) {
                MessageHelper::seterr(e, MessageHelper::E_SIZE, "buffer has not enough channels");
                return false;
            }
            if (buffer.getNumSamples() < m_resHeader.samples) {
                MessageHelper::seterr(e, MessageHelper::E_SIZE, "buffer has not enough samples");
                return false;
            }
            for (int chan = 0; chan < buffer.getNumChannels(); ++chan) {
                if (!read(socket, buffer.getWritePointer(chan), buffer.getNumSamples() * as<int>(sizeof(T)), 1000, e)) {
                    MessageHelper::seterrstr(e, "audio data");
                    return false;
                }
            }
            midi.clear();
            std::vector<char> midiData;
            MidiHeader midiHdr;
            for (int i = 0; i < m_resHeader.numMidiEvents; i++) {
                if (!read(socket, &midiHdr, sizeof(midiHdr), 1000, e)) {
                    MessageHelper::seterrstr(e, "midi header");
                    return false;
                }
                auto size = as<size_t>(midiHdr.size);
                if (midiData.size() < size) {
                    midiData.resize(size);
                }
                if (!read(socket, midiData.data(), midiHdr.size, 1000, e)) {
                    MessageHelper::seterrstr(e, "midi data");
                    return false;
                }
                midi.addEvent(midiData.data(), midiHdr.size, midiHdr.sampleNumber);
            }
        } else {
            MessageHelper::seterr(e, MessageHelper::E_STATE, "not connected");
            return false;
        }
        MessageHelper::seterr(e, MessageHelper::E_NONE);
        return true;
    }

    template <typename T>
    int prepareBufferForRead(AudioBuffer<T>& buffer, int totalChannels, int totalSamples) {
        buffer.setSize(totalChannels, totalSamples);
        // no data for extra channels
        for (int chan = m_reqHeader.channels; chan < totalChannels; ++chan) {
            buffer.clear(chan, 0, totalSamples);
        }
        // bytes to read
        return m_reqHeader.samples * as<int>(sizeof(T));
    }

    bool readFromClient(StreamingSocket* socket, AudioBuffer<float>& bufferF, AudioBuffer<double>& bufferD,
                        MidiBuffer& midi, AudioPlayHead::CurrentPositionInfo& posInfo, int extraChannels,
                        MessageHelper::Error* e) {
        if (socket->isConnected()) {
            if (!read(socket, &m_reqHeader, sizeof(m_reqHeader), 0, e)) {
                MessageHelper::seterrstr(e, "request header");
                return false;
            }
            // Arbitarry additional channels to support plugins that have more than one input bus. Plugins that don't
            // need it, should ignore the channels.
            int totalChannels = jmax(m_reqHeader.channels, m_reqHeader.channelsRequested) + extraChannels;
            int totalSamples = jmax(m_reqHeader.samples, m_reqHeader.samplesRequested);
            int size;
            if (m_reqHeader.isDouble) {
                size = prepareBufferForRead<double>(bufferD, totalChannels, totalSamples);
            } else {
                size = prepareBufferForRead<float>(bufferF, totalChannels, totalSamples);
            }
            // Read the channel data from the client, if any
            for (int chan = 0; chan < m_reqHeader.channels; ++chan) {
                char* data = m_reqHeader.isDouble ? reinterpret_cast<char*>(bufferD.getWritePointer(chan))
                                                  : reinterpret_cast<char*>(bufferF.getWritePointer(chan));
                if (!read(socket, data, size, 0, e)) {
                    MessageHelper::seterrstr(e, "audio data");
                    return false;
                }
            }
            midi.clear();
            for (int i = 0; i < m_reqHeader.numMidiEvents; i++) {
                std::vector<char> midiData;
                MidiHeader midiHdr;
                if (!read(socket, &midiHdr, sizeof(midiHdr), 0, e)) {
                    MessageHelper::seterrstr(e, "midi header");
                    return false;
                }
                midiData.resize(static_cast<size_t>(midiHdr.size));
                if (!read(socket, midiData.data(), midiHdr.size)) {
                    MessageHelper::seterrstr(e, "midi data");
                    return false;
                }
                midi.addEvent(midiData.data(), midiHdr.size, midiHdr.sampleNumber);
            }
            if (!read(socket, &posInfo, sizeof(posInfo))) {
                MessageHelper::seterrstr(e, "pos info");
                return false;
            }
        } else {
            MessageHelper::seterr(e, MessageHelper::E_STATE, "not connected");
            return false;
        }
        MessageHelper::seterr(e, MessageHelper::E_NONE);
        return true;
    }

  private:
    RequestHeader m_reqHeader;
    ResponseHeader m_resHeader;
};

/*
 * Command I/O
 */
class Payload {
  public:
    using Buffer = std::vector<char>;

    Payload() : payloadType(-1) {}
    Payload(int t, size_t s = 0) : payloadType(t), payloadBuffer(s) { memset(getData(), 0, s); }
    virtual ~Payload() {}
    Payload& operator=(const Payload& other) = delete;
    Payload& operator=(Payload&& other) {
        if (this != &other) {
            payloadType = other.payloadType;
            other.payloadType = -1;
            payloadBuffer = std::move(other.payloadBuffer);
        }
        return *this;
    }

    int getType() const { return payloadType; }
    void setType(int t) { payloadType = t; }
    int getSize() const { return as<int>(payloadBuffer.size()); }
    void setSize(int size) {
        payloadBuffer.resize(as<size_t>(size));
        realign();
    }
    char* getData() { return reinterpret_cast<char*>(payloadBuffer.data()); }
    const char* getData() const { return payloadBuffer.data(); }

    virtual void realign() {}

    int payloadType;
    Buffer payloadBuffer;
};

template <typename T>
class DataPayload : public Payload {
  public:
    T* data;
    DataPayload(int type) : Payload(type, sizeof(T)) { realign(); }
    virtual void realign() override { data = reinterpret_cast<T*>(payloadBuffer.data()); }
};

class NumberPayload : public DataPayload<int> {
  public:
    NumberPayload(int type) : DataPayload<int>(type) {}
    void setNumber(int n) { *data = n; }
    int getNumber() const { return *data; }
};

class StringPayload : public Payload {
  public:
    int* size;
    char* str;

    StringPayload(int type) : Payload(type, sizeof(int)) { realign(); }

    void setString(const String& s) {
        setSize(as<int>(sizeof(int)) + s.length());
        *size = s.length();
        memcpy(str, s.getCharPointer(), as<size_t>(s.length()));
    }

    String getString() const { return String(str, static_cast<size_t>(*size)); }

    virtual void realign() override {
        size = reinterpret_cast<int*>(payloadBuffer.data());
        str =
            as<size_t>(getSize()) > sizeof(int) ? reinterpret_cast<char*>(payloadBuffer.data()) + sizeof(int) : nullptr;
    }
};

class JsonPayload : public StringPayload {
  public:
    JsonPayload(int type) : StringPayload(type) {}

    void setJson(json& j) { setString(j.dump()); }
    json getJson() { return json::parse(getString().toStdString()); }
};

class BinaryPayload : public Payload {
  public:
    int* size;
    char* data;

    BinaryPayload(int type) : Payload(type, sizeof(int)) { realign(); }

    void setData(const char* src, int len) {
        setSize(as<int>(sizeof(int)) + len);
        *size = len;
        memcpy(data, src, static_cast<size_t>(len));
    }

    virtual void realign() override {
        size = reinterpret_cast<int*>(payloadBuffer.data());
        data =
            as<size_t>(getSize()) > sizeof(int) ? reinterpret_cast<char*>(payloadBuffer.data()) + sizeof(int) : nullptr;
    }
};

class Any : public Payload {
  public:
    static constexpr int Type = 0;
    Any() : Payload(Type) {}
};

class Quit : public Payload {
  public:
    static constexpr int Type = 1;
    Quit() : Payload(Type) {}
};

class Result : public Payload {
  public:
    static constexpr int Type = 2;

    struct hdr_t {
        int rc;
        int size;
    };
    hdr_t* hdr;
    char* str;

    Result() : Payload(Type) { realign(); }

    void setResult(int rc, const String& s) {
        setSize(static_cast<int>(sizeof(hdr_t)) + s.length());
        hdr->rc = rc;
        hdr->size = s.length();
        memcpy(str, s.getCharPointer(), as<size_t>(s.length()));
    }

    int getReturnCode() const { return hdr->rc; }
    String getString() const { return String(str, as<size_t>(hdr->size)); }

    virtual void realign() override {
        hdr = reinterpret_cast<hdr_t*>(payloadBuffer.data());
        str = as<size_t>(getSize()) > sizeof(hdr_t) ? reinterpret_cast<char*>(payloadBuffer.data()) + sizeof(hdr_t)
                                                    : nullptr;
    }
};

struct preparetoplay_data_t {
    double rate;
    int samples;
};

class PluginList : public StringPayload {
  public:
    static constexpr int Type = 3;
    PluginList() : StringPayload(Type) {}
};

class AddPlugin : public StringPayload {
  public:
    static constexpr int Type = 4;
    AddPlugin() : StringPayload(Type) {}
};

class DelPlugin : public NumberPayload {
  public:
    static constexpr int Type = 5;
    DelPlugin() : NumberPayload(Type) {}
};

class EditPlugin : public NumberPayload {
  public:
    static constexpr int Type = 6;
    EditPlugin() : NumberPayload(Type) {}
};

class HidePlugin : public Payload {
  public:
    static constexpr int Type = 7;
    HidePlugin() : Payload(Type) {}
};

class ScreenCapture : public Payload {
  public:
    static constexpr int Type = 8;

    struct hdr_t {
        int width;
        int height;
        size_t size;
    };
    hdr_t* hdr;
    char* data;

    ScreenCapture() : Payload(Type) { realign(); }

    void setImage(int width, int height, const void* p, size_t size) {
        setSize(as<int>(sizeof(hdr_t) + size));
        hdr->width = width;
        hdr->height = height;
        hdr->size = size;
        if (nullptr != p) {
            memcpy(data, p, size);
        }
    }

    virtual void realign() override {
        hdr = reinterpret_cast<hdr_t*>(payloadBuffer.data());
        data = as<size_t>(getSize()) > sizeof(hdr_t) ? reinterpret_cast<char*>(payloadBuffer.data()) + sizeof(hdr_t)
                                                     : nullptr;
    }
};

struct mouseevent_t {
    MouseEvType type;
    float x;
    float y;
    bool isShiftDown;
    bool isCtrlDown;
    bool isAltDown;
};

class Mouse : public DataPayload<mouseevent_t> {
  public:
    static constexpr int Type = 9;
    Mouse() : DataPayload<mouseevent_t>(Type) {}
};

class GetPluginSettings : public NumberPayload {
  public:
    static constexpr int Type = 10;
    GetPluginSettings() : NumberPayload(Type) {}
};

class PluginSettings : public BinaryPayload {
  public:
    static constexpr int Type = 11;
    PluginSettings() : BinaryPayload(Type) {}
};

class Key : public BinaryPayload {
  public:
    static constexpr int Type = 12;
    Key() : BinaryPayload(Type) {}

    const uint16_t* getKeyCodes() const { return reinterpret_cast<const uint16_t*>(data); }
    int getKeyCount() const { return *size / as<int>(sizeof(uint16_t)); }
};

class BypassPlugin : public NumberPayload {
  public:
    static constexpr int Type = 13;
    BypassPlugin() : NumberPayload(Type) {}
};

class UnbypassPlugin : public NumberPayload {
  public:
    static constexpr int Type = 14;
    UnbypassPlugin() : NumberPayload(Type) {}
};

struct exchange_t {
    int idxA;
    int idxB;
};

class ExchangePlugins : public DataPayload<exchange_t> {
  public:
    static constexpr int Type = 15;
    ExchangePlugins() : DataPayload<exchange_t>(Type) {}
};

class RecentsList : public StringPayload {
  public:
    static constexpr int Type = 16;
    RecentsList() : StringPayload(Type) {}
};

class Parameters : public JsonPayload {
  public:
    static constexpr int Type = 17;
    Parameters() : JsonPayload(Type) {}
};

struct parametervalue_t {
    int idx;
    int paramIdx;
    float value;
};

class ParameterValue : public DataPayload<parametervalue_t> {
  public:
    static constexpr int Type = 18;
    ParameterValue() : DataPayload<parametervalue_t>(Type) {}
};

struct getparametervalue_t {
    int idx;
    int paramIdx;
};

class GetParameterValue : public DataPayload<getparametervalue_t> {
  public:
    static constexpr int Type = 19;
    GetParameterValue() : DataPayload<getparametervalue_t>(Type) {}
};

class Presets : public StringPayload {
  public:
    static constexpr int Type = 20;
    Presets() : StringPayload(Type) {}
};

struct preset_t {
    int idx;
    int preset;
};

class Preset : public DataPayload<preset_t> {
  public:
    static constexpr int Type = 21;
    Preset() : DataPayload<preset_t>(Type) {}
};

template <typename T>
class Message {
  public:
    static constexpr int MAX_SIZE = 1024 * 1024 * 20;  // 20 MB

    struct Header {
        int type;
        int size;
    };

    virtual ~Message() {}

    bool read(StreamingSocket* socket, MessageHelper::Error* e = nullptr, int timeoutMilliseconds = 1000) {
        bool success = false;
        MessageHelper::seterr(e, MessageHelper::E_NONE);
        if (nullptr != socket && socket->isConnected()) {
            Header hdr;
            success = true;
            int ret = socket->waitUntilReady(true, timeoutMilliseconds);
            if (ret > 0) {
                if (e47::read(socket, &hdr, sizeof(hdr))) {
                    auto t = T::Type;
                    if (t > 0 && hdr.type != t) {
                        success = false;
                        String estr;
                        estr << "invalid message type " << hdr.type << " (" << t << " expected)";
                        MessageHelper::seterr(e, MessageHelper::E_DATA, estr);
                    } else {
                        payload.setType(hdr.type);
                        if (hdr.size > 0) {
                            if (hdr.size > MAX_SIZE) {
                                success = false;
                                String estr;
                                estr << "max size of " << MAX_SIZE << " bytes exceeded (" << hdr.size << " bytes)";
                                MessageHelper::seterr(e, MessageHelper::E_DATA, estr);
                            } else {
                                if (payload.getSize() != hdr.size) {
                                    payload.setSize(hdr.size);
                                }
                                if (!e47::read(socket, payload.getData(), hdr.size)) {
                                    success = false;
                                    MessageHelper::seterr(e, MessageHelper::E_DATA, "failed to read message body");
                                }
                            }
                        }
                    }
                } else {
                    success = false;
                    MessageHelper::seterr(e, MessageHelper::E_DATA, "failed to read message header");
                }
            } else if (ret < 0) {
                success = false;
                MessageHelper::seterr(e, MessageHelper::E_SYSCALL, "failed to wait for message header");
            } else {
                success = false;
                MessageHelper::seterr(e, MessageHelper::E_TIMEOUT);
            }
        } else {
            MessageHelper::seterr(e, MessageHelper::E_STATE);
        }
        return success;
    }

    bool send(StreamingSocket* socket) {
        Header hdr = {payload.getType(), (int)payload.getSize()};
        if (static_cast<size_t>(hdr.size) > MAX_SIZE) {
            std::cerr << "max size of " << MAX_SIZE << " bytes exceeded (" << hdr.size << " bytes)" << std::endl;
            return false;
        }
        if (!e47::send(socket, reinterpret_cast<const char*>(&hdr), sizeof(hdr))) {
            return false;
        }
        if (payload.getSize() > 0 && !e47::send(socket, payload.getData(), (int)payload.getSize())) {
            return false;
        }
        return true;
    }

    int getType() const { return payload.getType(); }
    size_t getSize() const { return payload.getSize(); }
    const char* getData() const { return payload.getData(); }

    template <typename T2>
    static std::shared_ptr<Message<T2>> convert(std::shared_ptr<Message<T>> in) {
        auto out = std::make_shared<Message<T2>>();
        out->payload.payloadBuffer = std::move(in->payload.payloadBuffer);
        out->payload.realign();
        return out;
    }

    T payload;
};

#define PLD(m) m.payload
#define pPLD(m) m->payload
#define DATA(m) PLD(m).data
#define pDATA(m) pPLD(m).data

class MessageFactory {
  public:
    static std::shared_ptr<Message<Any>> getNextMessage(StreamingSocket* socket, MessageHelper::Error* e) {
        if (nullptr != socket) {
            auto msg = std::make_shared<Message<Any>>();
            if (msg->read(socket, e)) {
                return msg;
            }
        }
        return nullptr;
    }

    static std::shared_ptr<Result> getResult(StreamingSocket* socket, int retry = 5,
                                             MessageHelper::Error* e = nullptr) {
        if (nullptr != socket) {
            auto msg = std::make_shared<Message<Result>>();
            MessageHelper::Error err;
            do {
                if (msg->read(socket, &err)) {
                    auto res = std::make_shared<Result>();
                    *res = std::move(msg->payload);
                    return res;
                }
            } while (retry-- > 0 && err.code == MessageHelper::E_TIMEOUT);
            if (nullptr != e) {
                *e = err;
                String m = "unable to retrieve result message after ";
                m << retry;
                m << " attempts";
                MessageHelper::seterrstr(e, m);
            }
        }
        return nullptr;
    }

    static bool sendResult(StreamingSocket* socket, int rc) { return sendResult(socket, rc, ""); }

    static bool sendResult(StreamingSocket* socket, int rc, const String& str) {
        Message<Result> msg;
        msg.payload.setResult(rc, str);
        return msg.send(socket);
    }
};

/*
static inline String toString(MidiMessage& mm, int samplePos) {
    String ret = "Midi message: ";
    ret << "sample=" << samplePos << " ts=" << mm.getTimeStamp()
        << " ch=" << mm.getChannel() << " v=" << mm.getVelocity() << " n=" << mm.getNoteNumber()
        << " desc=" << mm.getDescription();
    return ret;
}

static inline String toString(MidiBuffer& midi) {
    if (midi.getNumEvents() > 0) {
        MidiBuffer::Iterator midiIt(midi);
        MidiMessage mm;
        int samplePos;
        String ret = "Midi buffer:\n";
        while (midiIt.getNextEvent(mm, samplePos)) {
            ret << "  " << toString(mm, samplePos) << "\n";
        }
        return ret;
    }
    return "";
}
*/

}  // namespace e47

#endif /* Message_hpp */
