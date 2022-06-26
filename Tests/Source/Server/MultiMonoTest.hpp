/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _MULTIMONOTEST_HPP_
#define _MULTIMONOTEST_HPP_

#include <JuceHeader.h>

#include "TestsHelper.hpp"
#include "Server.hpp"
#include "ProcessorChain.hpp"
#include "Processor.hpp"
#include "ChannelSet.hpp"

namespace e47 {

class MultiMonoTest : UnitTest {
  public:
    MultiMonoTest() : UnitTest("MultiMono") {}

    void runTest() override {
        beginTest("Setup");

        double sampleRate = 48000.0;
        int blockSize = 512, chIn = 2, chOut = 2, chSc = 2;

        LogTag testTag("test");
        String err;

        auto pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                                   HandshakeRequest());
        pc->updateChannels(chIn, chOut, chSc);
        pc->prepareToPlay(sampleRate, blockSize);

        KnownPluginList pl;
        json playouts;
        Server::loadKnownPluginList(pl, playouts, 999);

        String id = "VST3-66155f87";
        auto desc = Processor::findPluginDescritpion(id, pl);
        auto proc = std::make_shared<Processor>(*pc, id, sampleRate, blockSize, false);
        expect(proc->load("|", "Multi-Mono", 0, err, desc.get()), "Load failed: " + err);
        pc->addProcessor(proc);
        expect(proc->getLatencySamples() == 60);
        expect(pc->getLatencySamples() == 60);

        TestsHelper::TestPlayHead phead;
        pc->setPlayHead(&phead);

        ChannelSet cs(0, 0, 2);
        MidiBuffer midi;
        AudioBuffer<float> buf(chIn + chSc, blockSize);

        beginTest("All channels on");

        setBufferSamples(buf, 0.5f);
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.0f, 0, 2, 0, 60);
        checkBufferSamples2(buf, 0.5f, 0, 2, 60, blockSize - 60);

        buf.clear();
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.5f, 0, 2, 0, 60);  // leftover
        checkBufferSamples2(buf, 0.0f, 0, 2, 60, blockSize - 60);

        beginTest("Right OFF");

        cs.setOutputActive(0);
        proc->setMonoChannels(cs.toInt());

        setBufferSamples(buf, 0.5f);
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.0f, 0, 2, 0, 60);
        checkBufferSamples2(buf, 0.5f, 0, 2, 60, blockSize - 60);

        buf.clear();
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.5f, 0, 2, 0, 60);  // leftover
        checkBufferSamples2(buf, 0.0f, 0, 2, 60, blockSize - 60);

        beginTest("Left OFF");

        cs.setOutputRangeActive(false);
        cs.setOutputActive(1);
        proc->setMonoChannels(cs.toInt());

        setBufferSamples(buf, 0.5f);
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.0f, 0, 2, 0, 60);
        checkBufferSamples2(buf, 0.5f, 0, 2, 60, blockSize - 60);

        buf.clear();
        pc->processBlock(buf, midi);
        checkBufferSamples2(buf, 0.5f, 0, 2, 0, 60);  // leftover
        checkBufferSamples2(buf, 0.0f, 0, 2, 60, blockSize - 60);

        pc->delProcessor(0);
        pc->releaseResources();
    }
};

static MultiMonoTest multiMonoTest;

}  // namespace e47

#endif  // _MULTIMONOTEST_HPP_
