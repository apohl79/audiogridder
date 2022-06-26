/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _SANDBOXPLUGINTEST_HPP_
#define _SANDBOXPLUGINTEST_HPP_

#include <JuceHeader.h>

#include "TestsHelper.hpp"
#include "Defaults.hpp"
#include "Server.hpp"
#include "Processor.hpp"
#include "ProcessorChain.hpp"
#include "ChannelSet.hpp"

namespace e47 {

class SandboxPluginTest : UnitTest {
  public:
    SandboxPluginTest() : UnitTest("Sandbox (Plugin Isolation)") {}

    void runTest() override {
        logMessage("Setting up server config");
        auto serverConfig = Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", "999"}});
        configWriteFile(serverConfig, {{"ID", 999},
                                       {"NAME", "Test"},
                                       {"CrashReporting", false},
                                       {"SandboxMode", Server::SANDBOX_PLUGIN},
                                       {"Tracer", true}});

        beginTest("Load plugins");

        double sampleRate = 48000.0;
        int blockSize = 512, chIn = 2, chOut = 2, chSc = 2;
        ChannelSet activeChannels;
        activeChannels.setNumChannels(chIn + chSc, chOut);
        activeChannels.setRangeActive();
        HandshakeRequest cfg = {AG_PROTOCOL_VERSION,    chIn, chOut, chSc, sampleRate, blockSize, false, 0, 0, 0,
                                activeChannels.toInt(), 0};

        LogTag testTag("test");

        auto pc =
            std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc), cfg);
        pc->setProcessingPrecision(AudioProcessor::singlePrecision);
        pc->updateChannels(chIn, chOut, chSc);
        pc->prepareToPlay(sampleRate, blockSize);

        KnownPluginList pl;
        json playouts;
        Server::loadKnownPluginList(pl, playouts, 999);

        for (auto desc : pl.getTypes()) {
            auto id = Processor::createPluginID(desc);
            logMessage("Loading " + desc.descriptiveName + " with ID " + id);
            auto proc = std::make_shared<Processor>(*pc, id, sampleRate, blockSize, true);
            String err;
            expect(proc->load({}, {}, 0, err, &desc), "Load failed: " + err);
            expect(proc->isClient());
            expect(proc->isLoaded());
            pc->addProcessor(proc);
            // bypass the plugin in the sandbox so we can do some audio tests
            proc->getClient()->suspendProcessingRemoteOnly(true);
        }

        expect(pc->getSize() == (size_t)pl.getNumTypes());

        beginTest("Send audio");

        int latency = pc->getLatencySamples();

        TestsHelper::TestPlayHead phead;
        pc->setPlayHead(&phead);

        AudioBuffer<float> buf(chIn + chSc, blockSize);
        setBufferSamples(buf, 0.5f);
        MidiBuffer midi;
        pc->processBlock(buf, midi);

        if (latency == 0) {
            checkBufferSamples(buf, 0.5f);
        } else {
            checkBufferSamples2(buf, 0.0f, 0, buf.getNumChannels(), 0, latency);
            checkBufferSamples2(buf, 0.5f, 0, buf.getNumChannels(), latency, buf.getNumSamples() - latency);
        }

        beginTest("Unload plugins");

        while (pc->getSize() > 0) {
            logMessage("Unloading " + pc->getProcessor(0)->getName());
            pc->delProcessor(0);
        }

        pc->releaseResources();
    }
};

static SandboxPluginTest sandboxPluginTest;

}  // namespace e47

#endif  // _SANDBOXPLUGINTEST_HPP_
