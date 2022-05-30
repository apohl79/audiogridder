/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _AUDIOSTREAMERTEST_HPP_
#define _AUDIOSTREAMERTEST_HPP_

#include <JuceHeader.h>

#include "TestsHelper.hpp"
#include "Utils.hpp"
#include "Defaults.hpp"
#include "Message.hpp"

#include "PluginProcessor.hpp"

namespace e47 {

class AudioStreamerTest : public UnitTest {
  public:
    AudioStreamerTest() : UnitTest("AudioStremer") {}

    void runTest() override {
        // setup a mock server
        FnThread mock;
        mock.fn = [this] {
            StreamingSocket master;
            setNonBlocking(master.getRawSocketHandle());

            if (master.createListener(Defaults::getSocketPath(Defaults::SERVER_SOCK, {{"id", "999"}}, true))) {
                logMessage("mock listener created");
                while (!FnThread::currentThreadShouldExit()) {
                    if (auto* clnt = accept(&master, 3000)) {
                        logMessage("new client");

                        HandshakeRequest cfg;
                        int len = clnt->read(&cfg, sizeof(cfg), true);
                        if (len > 0) {
                            if (cfg.version >= AG_PROTOCOL_VERSION) {
                                auto workerMasterSocket = std::make_shared<StreamingSocket>();
                                setNonBlocking(workerMasterSocket->getRawSocketHandle());

                                int workerPort = Defaults::CLIENT_PORT;
                                File socketPath = Defaults::getSocketPath(Defaults::WORKER_SOCK,
                                                                          {{"id", "999"}, {"n", String(workerPort)}});
                                if (workerMasterSocket->createListener(socketPath)) {
                                    HandshakeResponse resp = {AG_PROTOCOL_VERSION, 0, 0};
                                    resp.setFlag(HandshakeResponse::LOCAL_MODE);
                                    resp.port = workerPort;
                                    send(clnt, (const char*)&resp, sizeof(resp));

                                    auto* cmdIn = accept(workerMasterSocket.get(), 2000);
                                    auto* cmdOut = accept(workerMasterSocket.get(), 2000);
                                    auto* audio = accept(workerMasterSocket.get(), 2000);
                                    auto* screen = accept(workerMasterSocket.get(), 2000);

                                    workerMasterSocket->close();
                                    workerMasterSocket.reset();

                                    expect(cmdIn && cmdOut && audio && screen, "could not establish all connections");

                                    Message<PluginList> msg;
                                    msg.send(cmdIn);

                                    LogTag testTag("test");

                                    AudioMessage amsg(&testTag);
                                    AudioBuffer<float> bufferF;
                                    AudioBuffer<double> bufferD;
                                    MidiBuffer midi;
                                    AudioPlayHead::CurrentPositionInfo posInfo;
                                    auto bytesIn = Metrics::getStatistic<Meter>("NetBytesIn");
                                    auto bytesOut = Metrics::getStatistic<Meter>("NetBytesOut");
                                    Uuid traceId = Uuid::null();
                                    MessageHelper::Error e;

                                    while (!FnThread::currentThreadShouldExit() && audio->isConnected()) {
                                        if (audio->waitUntilReady(true, 100) != 0) {
                                            if (amsg.readFromClient(audio, bufferF, bufferD, midi, posInfo, &e,
                                                                    *bytesIn, traceId)) {
                                                if (amsg.isDouble()) {
                                                    amsg.sendToClient(audio, bufferD, midi, 0, bufferD.getNumChannels(),
                                                                      &e, *bytesOut);
                                                } else {
                                                    amsg.sendToClient(audio, bufferF, midi, 0, bufferF.getNumChannels(),
                                                                      &e, *bytesOut);
                                                }
                                            }
                                        }
                                    }

                                    delete cmdIn;
                                    delete cmdOut;
                                    delete audio;
                                    delete screen;
                                } else {
                                    expect(false, "can't create worker listener");
                                    delete clnt;
                                    return;
                                }
                            }
                        }
                        delete clnt;
                    }
                }
            } else {
                expect(false, "can't create listener");
                return;
            }
            logMessage("mock server terminated");
        };

        mock.startThread();

        beginTest("Plugin Init");

        double sampleRate = 48000.0;
        int blockSize = 512;
        int blockSizeHalf = blockSize / 2;

        PluginProcessor proc(AudioProcessor::wrapperType_Undefined);
        proc.getClient().setServer(String("127.0.0.1:999:test:0:0:1"));
        proc.getClient().NUM_OF_BUFFERS = 2;

        while (proc.getBusCount(true) > 1 && proc.canRemoveBus(true)) {
            proc.removeBus(true);
        }

        AudioProcessor::BusesLayout l;
        l.inputBuses.add(AudioChannelSet::stereo());
        l.outputBuses.add(AudioChannelSet::stereo());
        proc.setBusesLayout(l);

        proc.prepareToPlay(sampleRate, blockSize);

        int max = 15;
        while (!proc.getClient().isReadyLockFree() && max-- > 0) {
            Thread::sleep(1000);
        }

        expect(proc.getClient().isReadyLockFree(), "client not ready");
        expect(proc.getLatencySamples() == 1024,
               "lantency samples should be 1024 but is " + String(proc.getLatencySamples()));

        proc.setBypassWhenNotConnected(false);
        proc.setTransferMode(PluginProcessor::TM_ALWAYS);

        auto sendReadAndCheck = [&](float valOut, float valExepcted, int samples) {
            logMessage("sending " + String(samples) + " samples: valOut = " + String(valOut) +
                       ", valExpected = " + String(valExepcted));
            int channels = jmax(proc.getClient().getChannelsOut(),
                                proc.getClient().getChannelsIn() + proc.getClient().getChannelsSC());
            AudioBuffer<float> buf(channels, samples);
            MidiBuffer midi;
            setBufferSamples(buf, valOut);
            proc.processBlock(buf, midi);
            checkBufferSamples(buf, valExepcted);
        };

        beginTest("Send + Receive - Matching block size");

        // we send 1 at the first two calls and expect 0 to come back, because we have two blocks buffered
        sendReadAndCheck(1.0f, 0.0f, blockSize);  // 512
        sendReadAndCheck(0.0f, 0.0f, blockSize);  // 1024
        sendReadAndCheck(0.0f, 1.0f, blockSize);
        sendReadAndCheck(0.0f, 0.0f, blockSize);

        beginTest("Send + Receive - Smaller block size");

        // smaller block size
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);

        sendReadAndCheck(1.0f, 0.0f, blockSizeHalf);  // 256
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);  // 512
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);  // 768
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);  // 1024
        sendReadAndCheck(0.0f, 1.0f, blockSizeHalf);
        sendReadAndCheck(0.0f, 0.0f, blockSizeHalf);

        sendReadAndCheck(1.0f, 0.0f, 128);  // 128
        sendReadAndCheck(0.0f, 0.0f, 512);  // 640
        sendReadAndCheck(0.0f, 0.0f, 384);  // 1024
        sendReadAndCheck(0.0f, 1.0f, 128);

        proc.releaseResources();

        mock.stopThread(-1);
    }
};

static AudioStreamerTest audioStreamerTest;

}  // namespace e47

#endif  // _AUDIOSTREAMERTEST_HPP_
