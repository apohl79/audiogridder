/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifdef __APPLE__

#include <AppKit/AppKit.h>

#include "WindowHelper.hpp"

#include <juce_graphics/native/juce_mac_CoreGraphicsHelpers.h>

namespace e47 {
namespace WindowHelper {

juce::Rectangle<int> getWindowScreenBounds(juce::Component* c) {
    if (auto* view = (NSView*)c->getWindowHandle()) {
        if (auto* window = [view window]) {
            auto windowRect = flippedScreenRect([window frame]);
            return juce::Rectangle<double>(windowRect.origin.x, windowRect.origin.y, windowRect.size.width,
                                           windowRect.size.height)
                .toNearestInt();
        }
    }
    return {};
}

}
}  // namespace e47

#endif
