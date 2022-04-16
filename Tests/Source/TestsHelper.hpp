/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _TESTSHELPER_HPP_
#define _TESTSHELPER_HPP_

#include <JuceHeader.h>

#ifndef AG_TESTS_DATA
#define AG_TESTS_DATA ""
#endif

namespace e47 {
namespace TestsHelper {

inline File getTestsDataDir() {
#if JUCE_MAC
    return File(AG_TESTS_DATA).getChildFile("macos");
#elif JUCE_WINDOWS
    return File(AG_TESTS_DATA).getChildFile("windows");
#else
    return {};
#endif
}

struct TestPlayHead : AudioPlayHead {
    AudioPlayHead::CurrentPositionInfo posInfo;
    TestPlayHead() { posInfo.resetToDefault(); }
    bool getCurrentPosition(CurrentPositionInfo& result) {
        result = posInfo;
        return true;
    }
};

#define setBufferSamples(b, v)                            \
    do {                                                  \
        for (int c = 0; c < b.getNumChannels(); c++) {    \
            for (int s = 0; s < b.getNumSamples(); s++) { \
                b.setSample(c, s, v);                     \
            }                                             \
        }                                                 \
    } while (0)

#define checkBufferSamples(b, v)                                                                                   \
    do {                                                                                                           \
        bool __fail = false;                                                                                       \
        for (int __c = 0; __c < b.getNumChannels() && !__fail; __c++) {                                            \
            for (int __s = 0; __s < b.getNumSamples() && !__fail; __s++) {                                         \
                auto __x = b.getSample(0, __s);                                                                    \
                __fail = __x != v;                                                                                 \
                expect(!__fail, "sample at channel " + String(__c) + ", position " + String(__s) + " should be " + \
                                    String(v) + " but is " + String(__x));                                         \
            }                                                                                                      \
        }                                                                                                          \
    } while (0)

}  // namespace TestsHelper
}  // namespace e47

#endif  // _TESTSHELPER_HPP_