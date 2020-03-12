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
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:imgref];
    NSData* data;
    // data = [[rep representationUsingType:NSJPEGFileType properties:nil] autorelease];
    data = [rep representationUsingType:NSJPEGFileType properties:nil];
    auto ret = std::make_shared<juce::Image>(juce::JPEGImageFormat::loadFrom([data bytes], [data length]));
    [rep release];
    CGImageRelease(imgref);
    return ret;
}

}
