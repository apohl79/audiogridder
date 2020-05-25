/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#if defined(JUCE_MAC)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#elif JUCE_WINDOWS
#include <windows.h>
#endif

#include <iostream>
#include <utility>

#include "KeyAndMouse.hpp"

namespace e47 {

#if defined(JUCE_MAC)
void mouseEventReal(CGMouseButton button, CGEventType type, CGPoint location, CGEventFlags flags) {
    CGEventRef event = CGEventCreateMouseEvent(NULL, type, location, button);
    CGEventSetType(event, type);
    CGEventSetFlags(event, flags | CGEventGetFlags(event));
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
#elif defined(JUCE_WINDOWS)
void sendKey(WORD vk, bool keyDown) {
    INPUT event;
    event.type = INPUT_KEYBOARD;
    event.ki.wVk = vk;
    event.ki.wScan = 0;
    if (keyDown) {
        event.ki.dwFlags = 0;
    } else {
        event.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    event.ki.time = 0;
    event.ki.dwExtraInfo = NULL;
    SendInput(1, &event, sizeof(INPUT));
}
#endif

void mouseEvent(MouseEvType t, float x, float y, uint64_t flags) {
#if defined(JUCE_MAC)
    auto bt = toMouseButtonType(t);
    CGPoint loc = CGPointMake(x, y);
    mouseEventReal(bt.first, bt.second, loc, flags);
#elif defined(JUCE_WINDOWS)
    long lx = std::lroundf(x);
    long ly = std::lroundf(y);
    INPUT event;
    event.type = INPUT_MOUSE;
    event.mi.dx = lx;
    event.mi.dy = ly;
    event.mi.mouseData = 0;
    event.mi.time = 0;
    event.mi.dwExtraInfo = NULL;
    event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;
    switch (t) {
        case MouseEvType::MOVE:
            event.mi.dwFlags |= MOUSEEVENTF_MOVE;
            break;
        case MouseEvType::LEFT_UP:
            event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
            break;
        case MouseEvType::LEFT_DOWN:
            event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            break;
        case MouseEvType::RIGHT_UP:
            event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
            break;
        case MouseEvType::RIGHT_DOWN:
            event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
            break;
    }
    // modifiers down
    if ((flags & VK_SHIFT) == VK_SHIFT) {
        sendKey(VK_SHIFT, true);
    }
    if ((flags & VK_CONTROL) == VK_CONTROL) {
        sendKey(VK_CONTROL, true);
    }
    if ((flags & VK_MENU) == VK_MENU) {
        sendKey(VK_MENU, true);
    }

    // Remember: The input desktop must be the current desktop when you call SetCursorPos.
    // Call OpenInputDesktop to determine whether the current desktop is the input desktop. If it
    // is not, call SetThreadDesktop with the HDESK returned by OpenInputDesktop to switch to that
    // desktop.
    // Moves the cursor to the specified screen coordinates.
    SetCursorPos(lx, ly);
    SendInput(1, &event, sizeof(INPUT));

    // modifiers up
    if ((flags & VK_SHIFT) == VK_SHIFT) {
        sendKey(VK_SHIFT, false);
    }
    if ((flags & VK_CONTROL) == VK_CONTROL) {
        sendKey(VK_CONTROL, false);
    }
    if ((flags & VK_MENU) == VK_MENU) {
        sendKey(VK_MENU, false);
    }
#endif
}

void keyEvent(uint16_t keyCode, uint64_t flags, bool keyDown) {
#if defined(JUCE_MAC)
    CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, keyCode, keyDown);
    CGEventSetFlags(ev, flags | CGEventGetFlags(ev));
    CGEventPost(kCGSessionEventTap, ev);
    CFRelease(ev);
#elif JUCE_WINDOWS
    auto ch = getKeyName(keyCode);
    WORD vk = 0;
    if (ch.length() == 1) {
        vk = VkKeyScanExA(ch[0], GetKeyboardLayout(0));
    } else if (ch == "Space") {
        vk = VK_SPACE;
    } else if (ch == "Backspace") {
        vk = VK_BACK;
    } else if (ch == "Escape") {
        vk = VK_ESCAPE;
    } else if (ch == "Delete") {
        vk = VK_DELETE;
    } else if (ch == "Home") {
        vk = VK_HOME;
    } else if (ch == "End") {
        vk = VK_END;
    } else if (ch == "PageUp") {
        vk = VK_PRIOR;
    } else if (ch == "PageDown") {
        vk = VK_NEXT;
    } else if (ch == "LeftArrow") {
        vk = VK_LEFT;
    } else if (ch == "RightArrow") {
        vk = VK_RIGHT;
    } else if (ch == "UpArrow") {
        vk = VK_UP;
    } else if (ch == "DownArrow") {
        vk = VK_DOWN;
    } else if (ch == "F1") {
        vk = VK_F1;
    } else if (ch == "F2") {
        vk = VK_F2;
    } else if (ch == "F3") {
        vk = VK_F3;
    } else if (ch == "F4") {
        vk = VK_F4;
    } else if (ch == "F5") {
        vk = VK_F5;
    } else if (ch == "F6") {
        vk = VK_F6;
    } else if (ch == "F7") {
        vk = VK_F7;
    } else if (ch == "F8") {
        vk = VK_F8;
    } else if (ch == "F9") {
        vk = VK_F9;
    } else if (ch == "F10") {
        vk = VK_F10;
    } else if (ch == "F11") {
        vk = VK_F11;
    } else if (ch == "F12") {
        vk = VK_F12;
    } else if (ch == "F413") {
        vk = VK_F13;
    } else if (ch == "F414") {
        vk = VK_F14;
    } else if (ch == "F15") {
        vk = VK_F15;
    } else if (ch == "F16") {
        vk = VK_F16;
    } else if (ch == "F17") {
        vk = VK_F17;
    } else if (ch == "F18") {
        vk = VK_F18;
    } else if (ch == "F19") {
        vk = VK_F19;
    } else if (ch == "F20") {
        vk = VK_F20;
    }

    // modifiers down
    if (keyDown) {
        if ((flags & VK_SHIFT) == VK_SHIFT) {
            sendKey(VK_SHIFT, true);
        }
        if ((flags & VK_CONTROL) == VK_CONTROL) {
            sendKey(VK_CONTROL, true);
        }
        if ((flags & VK_MENU) == VK_MENU) {
            sendKey(VK_MENU, true);
        }
    }

    // send key
    sendKey(vk, keyDown);

    // modifiers up
    if (!keyDown) {
        if ((flags & VK_SHIFT) == VK_SHIFT) {
            sendKey(VK_SHIFT, false);
        }
        if ((flags & VK_CONTROL) == VK_CONTROL) {
            sendKey(VK_CONTROL, false);
        }
        if ((flags & VK_MENU) == VK_MENU) {
            sendKey(VK_MENU, false);
        }
    }
#endif
}

void setShiftKey(uint64_t& flags) {
#if defined(JUCE_MAC)
    flags |= kCGEventFlagMaskShift;
#elif defined(JUCE_WINDOWS)
    flags |= VK_SHIFT;
#endif
}
void setControlKey(uint64_t& flags) {
#if defined(JUCE_MAC)
    flags |= kCGEventFlagMaskControl;
#elif defined(JUCE_WINDOWS)
    flags |= VK_CONTROL;
#endif
};
void setAltKey(uint64_t& flags) {
#if defined(JUCE_MAC)
    flags |= kCGEventFlagMaskAlternate;
#elif defined(JUCE_WINDOWS)
    flags |= VK_MENU;
#endif
};

void keyEventDown(uint16_t keyCode, uint64_t flags) { keyEvent(keyCode, flags, true); }

void keyEventUp(uint16_t keyCode, uint64_t flags) { keyEvent(keyCode, flags, false); }

}  // namespace e47
