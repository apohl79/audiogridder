/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _SCANPLUGINSTEST_HPP_
#define _SCANPLUGINSTEST_HPP_

#include <JuceHeader.h>

#include "TestsHelper.hpp"
#include "Server.hpp"
#include "Utils.hpp"

namespace e47 {

class ScanPluginsTest : UnitTest {
  public:
    ScanPluginsTest() : UnitTest("Scan Plugins") {}

    void runTest() override {
        beginTest("Scan");

        std::vector<String> vst2plugins, vst3plugins;
        auto datadir = TestsHelper::getTestsDataDir();

#if JUCE_MAC
        vst2plugins.push_back(datadir.getChildFile("dreverb_1.0_mac_86_64")
                                  .getChildFile("VST2")
                                  .getChildFile("DReverb.vst")
                                  .getFullPathName());
        vst3plugins.push_back(datadir.getChildFile("dreverb_1.0_mac_86_64")
                                  .getChildFile("VST3")
                                  .getChildFile("DReverb.vst3")
                                  .getFullPathName());
#elif JUCE_WINDOWS
        vst2plugins.push_back(datadir.getChildFile("dreverb_1.0_win_86_64")
                                  .getChildFile("VST2")
                                  .getChildFile("DReverb.dll")
                                  .getFullPathName());
        vst3plugins.push_back(datadir.getChildFile("dreverb_1.0_win_86_64")
                                  .getChildFile("VST3")
                                  .getChildFile("DReverb.vst3")
                                  .getFullPathName());
#endif
        vst3plugins.push_back(datadir.getChildFile("2RuleSynth.vst3").getFullPathName());
        vst3plugins.push_back(datadir.getChildFile("LoudMax.vst3").getFullPathName());

        for (auto& p : vst2plugins) {
            runOnMsgThreadSync([&] { expect(Server::scanPlugin(p, "VST", 999)); });
        }

        for (auto& p : vst3plugins) {
            runOnMsgThreadSync([&] { expect(Server::scanPlugin(p, "VST3", 999)); });
        }
    }
};

static ScanPluginsTest scanPluginsTest;

}  // namespace e47

#endif  // _SCANPLUGINSTEST_HPP_
