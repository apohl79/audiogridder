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

using namespace juce;

namespace e47 {

std::shared_ptr<Image> captureScreenNative(Rectangle<int> rect);

#ifdef JUCE_MAC
int getCaptureDeviceIndex();
void askForScreenRecordingPermission();
bool askForAccessibilityPermission();
#endif

}

#endif /* Screen_h */
