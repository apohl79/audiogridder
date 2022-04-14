/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _WINDOWHELPER_HPP_
#define _WINDOWHELPER_HPP_

#include <JuceHeader.h>

namespace e47 {
namespace WindowHelper {

juce::Rectangle<int> getWindowScreenBounds(juce::Component* c);

}
}  // namespace e47

#endif  // _WINDOWHELPER_HPP_
