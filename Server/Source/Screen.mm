/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Screen.h"
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

namespace e47 {

std::shared_ptr<juce::Image> captureScreen(juce::Rectangle<int> rect) {
    auto r = CGRectMake(rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight());
    auto imgref = CGWindowListCreateImage(r, kCGWindowListOptionAll, kCGNullWindowID, kCGWindowImageNominalResolution);
    if (nullptr != imgref) {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:imgref];
        if (nullptr != rep) {
            NSData* data = [rep representationUsingType:NSJPEGFileType properties:nil];
            std::shared_ptr<juce::Image> ret;
            if (nullptr != data) {
                ret = std::make_shared<juce::Image>(juce::JPEGImageFormat::loadFrom([data bytes], [data length]));
                ret->duplicateIfShared();
            }
            [rep release];
            CGImageRelease(imgref);
            return ret;
        }
    }
    return nullptr;
}

}
