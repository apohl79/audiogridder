/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Screen_h
#define Screen_h

#include "../JuceLibraryCode/AppConfig.h"
#include <juce_graphics/juce_graphics.h>

namespace e47 {

std::shared_ptr<juce::Image> captureScreen(juce::Rectangle<int> rect);

}

#endif /* Screen_h */
