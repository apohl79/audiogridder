/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _PROCESSORCHAINTEST_HPP_
#define _PROCESSORCHAINTEST_HPP_

#include <JuceHeader.h>

#include "TestsHelper.hpp"
#include "Server.hpp"
#include "ProcessorChain.hpp"
#include "Processor.hpp"

namespace e47 {

class ProcessorChainTest : UnitTest {
  public:
    ProcessorChainTest() : UnitTest("ProcessorChain") {}

    void runTest() override {
        runTestBasic();
        runLoadPlugins();
    }

    void runTestBasic() {
        beginTest("Basic tests");

        int chIn = 2, chOut = 2, chSc = 0;

        LogTag testTag("test");

        auto pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                                   HandshakeRequest());
        expect(pc->updateChannels(chIn, chOut, chSc));
        pc.reset();

        chIn = chOut = 64;
        pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                              HandshakeRequest());
        expect(pc->updateChannels(chIn, chOut, chSc));
        pc.reset();

        chSc = 2;
        pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                              HandshakeRequest());
        expect(pc->updateChannels(chIn, chOut, chSc));
        pc.reset();

        chIn = chOut = chSc = 2;
        pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                              HandshakeRequest());
        expect(pc->updateChannels(chIn, chOut, chSc));
    }

    void runLoadPlugins() {
        beginTest("Load plugins");

        double sampleRate = 48000.0;
        int blockSize = 512, chIn = 2, chOut = 2, chSc = 2;

        LogTag testTag("test");

        auto pc = std::make_unique<ProcessorChain>(&testTag, ProcessorChain::createBussesProperties(chIn, chOut, chSc),
                                                   HandshakeRequest());
        pc->updateChannels(chIn, chOut, chSc);
        pc->prepareToPlay(sampleRate, blockSize);

        KnownPluginList pl;
        json playouts;
        Server::loadKnownPluginList(pl, playouts, 999);

        for (auto desc : pl.getTypes()) {
            auto id = Processor::createPluginID(desc);
            logMessage("Loading " + desc.descriptiveName + " with ID " + id);
            auto proc = std::make_shared<Processor>(*pc, id, sampleRate, blockSize, false);
            String err;
            expect(proc->load({}, err, &desc), "Load failed: " + err);
            pc->addProcessor(std::move(proc));
        }

        expect(pc->getSize() == (size_t)pl.getNumTypes());

        while (pc->getSize() > 0) {
            pc->delProcessor(0);
        }
    }
};

static ProcessorChainTest processorChainTest;

}  // namespace e47

#endif  // _PROCESSORCHAINTEST_HPP_
