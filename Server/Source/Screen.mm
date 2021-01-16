/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifdef __APPLE__

#import <AVFoundation/AVFoundation.h>

#include "Screen.h"
#include "Utils.hpp"
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>


namespace e47 {

std::shared_ptr<Image> captureScreenNative(Rectangle<int> rect) {
    setLogTagStatic("screen");
    auto r = CGRectMake(rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight());
    auto imgref = CGWindowListCreateImage(r, kCGWindowListOptionAll, kCGNullWindowID, kCGWindowImageNominalResolution);
    if (nullptr != imgref) {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:imgref];
        if (nullptr != rep) {
            NSDictionary *props = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0]
                                                              forKey:NSImageCompressionFactor];
            NSData* data = [rep representationUsingType:NSJPEGFileType properties:props];
            std::shared_ptr<Image> ret;
            if (nullptr != data) {
                ret = std::make_shared<Image>(JPEGImageFormat::loadFrom([data bytes], [data length]));
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

int getCaptureDeviceIndex() {
    setLogTagStatic("screen");
    NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    NSArray* devicesMuxed = [AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed];
    uint64_t numVideoDevices = [devices count] + [devicesMuxed count];
    uint32_t numScreens = 0;
    CGGetActiveDisplayList(0, nullptr, &numScreens);
    int ret = -1;
    logln("iterating over available capture devices...");
    if (numScreens > 0) {
        CGDirectDisplayID screens[32];
        if (numScreens > 32) {
            numScreens = 32;
        }
        CGGetActiveDisplayList(numScreens, screens, &numScreens);
        for (uint32_t i = 0; i < numScreens; i++) {
            auto idx = numVideoDevices + i;
            String desc = "  found device with index " + String(idx);
            if (ret == -1) {
                ret = (int)idx;
                desc << " (selected)";
            }
            logln(desc);
        }
    }
    return ret;
}

void askForScreenRecordingPermission() {
    CGDisplayStreamRef stream = CGDisplayStreamCreate(CGMainDisplayID(), 1, 1, kCVPixelFormatType_32BGRA, nil,
                                                      ^(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef,
                                                        CGDisplayStreamUpdateRef) {});
    if (stream) {
        CFRelease(stream);
    }
}

bool askForAccessibilityPermission() {
    return AXIsProcessTrusted();
}

}

#endif
