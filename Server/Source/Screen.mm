/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Screen.h"
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

#define logln(M) JUCE_BLOCK_WITH_FORCED_SEMICOLON(juce::String __str; __str << M; juce::Logger::writeToLog(__str);)

namespace e47 {

std::shared_ptr<juce::Image> captureScreen(juce::Rectangle<int> rect) {
    auto r = CGRectMake(rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight());
    auto imgref = CGWindowListCreateImage(r, kCGWindowListOptionAll, kCGNullWindowID, kCGWindowImageNominalResolution);
    if (nullptr != imgref) {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:imgref];
        if (nullptr != rep) {
            NSDictionary *props = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0]
                                                              forKey:NSImageCompressionFactor];
            NSData* data = [rep representationUsingType:NSJPEGFileType properties:props];
            std::shared_ptr<juce::Image> ret;
            if (nullptr != data) {
                ret = std::make_shared<juce::Image>(juce::JPEGImageFormat::loadFrom([data bytes], [data length]));
                ret->duplicateIfShared();
            } else {
                logln("representationUsingType failed");
            }
            [rep release];
            CGImageRelease(imgref);
            return ret;
        } else {
            logln("initWithCGImage failed");
        }
    } else {
        logln("CGWindowListCreateImage failed");
    }
    return nullptr;
}

}
