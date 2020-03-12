/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <iostream>
#include <utility>

#include "KeyAndMouse.hpp"

namespace e47 {

void mouseEventReal(CGMouseButton button, CGEventType type, CGPoint location) {
    CGEventRef event = CGEventCreateMouseEvent(NULL, type, location, button);
    CGEventSetType(event, type);
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}

std::pair<CGMouseButton, CGEventType> toMouseButtonType(MouseEvType t) {
    CGMouseButton button;
    CGEventType type;
    switch (t) {
        case MouseEvType::MOVE:
            button = kCGMouseButtonLeft;
            type = kCGEventMouseMoved;
            break;
        case MouseEvType::LEFT_UP:
            button = kCGMouseButtonLeft;
            type = kCGEventLeftMouseUp;
            break;
        case MouseEvType::LEFT_DOWN:
            button = kCGMouseButtonLeft;
            type = kCGEventLeftMouseDown;
            break;
        case MouseEvType::LEFT_DRAG:
            button = kCGMouseButtonLeft;
            type = kCGEventLeftMouseDragged;
            break;
        case MouseEvType::RIGHT_UP:
            button = kCGMouseButtonRight;
            type = kCGEventRightMouseUp;
            break;
        case MouseEvType::RIGHT_DOWN:
            button = kCGMouseButtonRight;
            type = kCGEventRightMouseDown;
            break;
        case MouseEvType::RIGHT_DRAG:
            button = kCGMouseButtonRight;
            type = kCGEventRightMouseDragged;
            break;
        case MouseEvType::OTHER_UP:
            button = kCGMouseButtonCenter;
            type = kCGEventOtherMouseUp;
            break;
        case MouseEvType::OTHER_DOWN:
            button = kCGMouseButtonCenter;
            type = kCGEventOtherMouseDown;
            break;
        case MouseEvType::OTHER_DRAG:
            button = kCGMouseButtonCenter;
            type = kCGEventOtherMouseDragged;
            break;
    }
    return std::make_pair(button, type);
}

void mouseEvent(MouseEvType t, float x, float y) {
    auto bt = toMouseButtonType(t);
    CGPoint loc = CGPointMake(x, y);
    mouseEventReal(bt.first, bt.second, loc);
}

void keyEvent(uint16_t keyCode, uint64_t flags, bool keyDown) {
    CGEventRef ev = CGEventCreateKeyboardEvent(NULL, keyCode, keyDown);
    CGEventSetFlags(ev, flags);
    CGEventPost(kCGSessionEventTap, ev);
    CFRelease(ev);
}

void keyEventDown(uint16_t keyCode, uint64_t flags) { keyEvent(keyCode, flags, true); }

void keyEventUp(uint16_t keyCode, uint64_t flags) { keyEvent(keyCode, flags, false); }

}  // namespace e47
