/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include <iostream>
#include <utility>
#include <JuceHeader.h>

#include "KeyAndMouse.hpp"
#include "Utils.hpp"

#if defined(JUCE_MAC) || defined(JUCE_WINDOWS)

namespace e47 {

setLogTagStatic("keyandmouse");

#if defined(JUCE_MAC)
void mouseEventInternal(CGMouseButton button, CGEventType type, CGPoint location, CGEventFlags flags) {
    traceScope();
    CGEventRef event = CGEventCreateMouseEvent(nullptr, type, location, button);
    CGEventSetType(event, type);
    CGEventSetFlags(event, flags | CGEventGetFlags(event));
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}

void mouseDoubleClickEventInternal(CGPoint location, CGEventFlags flags) {
    traceScope();
    CGEventRef event = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown, location, kCGMouseButtonLeft);
    CGEventSetIntegerValueField(event, kCGMouseEventClickState, 2);
    CGEventSetFlags(event, flags | CGEventGetFlags(event));
    CGEventSetType(event, kCGEventLeftMouseDown);
    CGEventPost(kCGSessionEventTap, event);
    CGEventSetType(event, kCGEventLeftMouseUp);
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}

void mouseScrollEventInternal(float deltaX, float deltaY) {
    traceScope();
    if (deltaX == 0 && deltaY == 0) {
        return;
    }
    CGEventRef event = nullptr;
    if (deltaX != 0) {
        event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 2, (int)lround(deltaY),
                                              (int)lround(deltaX));
    } else if (deltaY != 0) {
        event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 1, (int)lround(deltaY));
    }
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}

void keyEventInternal(uint16_t keyCode, uint64_t flags, bool keyDown, bool currentProcessOnly) {
    traceScope();
    CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, keyCode, keyDown);
    CGEventSetFlags(ev, flags | CGEventGetFlags(ev));
    if (currentProcessOnly) {
        CGEventPostToPid(getpid(), ev);
    } else {
        CGEventPost(kCGSessionEventTap, ev);
    }
    CFRelease(ev);
}

inline std::pair<CGMouseButton, CGEventType> toMouseButtonType(MouseEvType t) {
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
        case MouseEvType::WHEEL:
            button = kCGMouseButtonLeft;
            type = kCGEventNull;
            break;
        case MouseEvType::DBL_CLICK:
            break;
    }
    return std::make_pair(button, type);
}

#elif defined(JUCE_WINDOWS)

void sendInput(INPUT* in) {
    traceScope();
    if (SendInput(1, in, sizeof(INPUT)) != 1) {
        logln("SendInput failed: " << GetLastErrorStr());
    }
}

void sendKey(WORD vk, bool keyDown, HWND hwnd = NULL) {
    traceScope();
    if (hwnd) {
        auto msg = keyDown ? WM_KEYDOWN : WM_KEYUP;
        if (SendMessage(hwnd, msg, vk, 1)) {
            logln("SendMessage failed: " << GetLastErrorStr());
        }
    } else {
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
        sendInput(&event);
    }
}

void mouseEventInternal(POINT pos, DWORD evFlags, uint64_t flags) {
    traceScope();

    INPUT event = {0};
    event.type = INPUT_MOUSE;
    event.mi.dx = pos.x;
    event.mi.dy = pos.y;
    event.mi.mouseData = 0;
    event.mi.time = 0;
    event.mi.dwExtraInfo = NULL;
    event.mi.dwFlags = evFlags;

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

    sendInput(&event);

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
}

void mouseScrollEventInternal(POINT pos, DWORD deltaX, DWORD deltaY) {
    traceScope();

    INPUT event = {0};
    event.type = INPUT_MOUSE;
    event.mi.time = 0;
    event.mi.dx = pos.x;
    event.mi.dy = pos.y;
    event.mi.dwExtraInfo = NULL;
    if (deltaX != 0) {
        event.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        event.mi.mouseData = deltaX;
        sendInput(&event);
    }
    if (deltaY != 0) {
        event.mi.dwFlags = MOUSEEVENTF_WHEEL;
        event.mi.mouseData = deltaY;
        sendInput(&event);
    }
}

void keyEventInternal(WORD vk, uint64_t flags, bool keyDown, void* nativeHandle) {
    traceScope();

    auto hwnd = (HWND)nativeHandle;
    if (nullptr != nativeHandle && !IsWindow(hwnd)) {
        logln("nativeHandle is no HWND");
        return;
    }

    // modifiers down
    if (keyDown) {
        if ((flags & VK_SHIFT) == VK_SHIFT) {
            sendKey(VK_SHIFT, true, hwnd);
        }
        if ((flags & VK_CONTROL) == VK_CONTROL) {
            sendKey(VK_CONTROL, true, hwnd);
        }
        if ((flags & VK_MENU) == VK_MENU) {
            sendKey(VK_MENU, true, hwnd);
        }
    }

    // send key
    sendKey(vk, keyDown, hwnd);

    // modifiers up
    if (!keyDown) {
        if ((flags & VK_SHIFT) == VK_SHIFT) {
            sendKey(VK_SHIFT, false, hwnd);
        }
        if ((flags & VK_CONTROL) == VK_CONTROL) {
            sendKey(VK_CONTROL, false, hwnd);
        }
        if ((flags & VK_MENU) == VK_MENU) {
            sendKey(VK_MENU, false, hwnd);
        }
    }
}

inline POINT getScaledPoint(float x, float y) {
    traceScope();
    HDC hDC = GetDC(0);
    float dpi = (GetDeviceCaps(hDC, LOGPIXELSX) + GetDeviceCaps(hDC, LOGPIXELSY)) / 2.0f;
    ReleaseDC(0, hDC);
    if (auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
        float sf = dpi / 96;
        float xf = (float)0xffff / disp->totalArea.getWidth();
        float yf = (float)0xffff / disp->totalArea.getHeight();
        long lx = lroundf(x * sf * xf);
        long ly = lroundf(y * sf * yf);
        return {lx, ly};
    } else {
        return {lroundf(x), lroundf(y)};
    }
}

inline DWORD getMouseFlags(MouseEvType t) {
    DWORD flags = MOUSEEVENTF_ABSOLUTE;
    switch (t) {
        case MouseEvType::LEFT_DRAG:
        case MouseEvType::RIGHT_DRAG:
        case MouseEvType::OTHER_DRAG:
        case MouseEvType::MOVE:
            flags |= MOUSEEVENTF_MOVE;
            break;
        case MouseEvType::LEFT_UP:
            flags |= MOUSEEVENTF_LEFTUP;
            break;
        case MouseEvType::LEFT_DOWN:
            flags |= MOUSEEVENTF_LEFTDOWN;
            break;
        case MouseEvType::RIGHT_UP:
            flags |= MOUSEEVENTF_RIGHTUP;
            break;
        case MouseEvType::RIGHT_DOWN:
            flags |= MOUSEEVENTF_RIGHTDOWN;
            break;
        case MouseEvType::OTHER_UP:
            flags |= MOUSEEVENTF_MIDDLEUP;
            break;
        case MouseEvType::OTHER_DOWN:
            flags |= MOUSEEVENTF_MIDDLEDOWN;
            break;
    }
    return flags;
}

inline WORD getVK(uint16_t keyCode) {
    auto ch = getKeyName(keyCode);
    WORD vk = 0;
    if (ch.length() == 1) {
        vk = VkKeyScanExA(ch[0], GetKeyboardLayout(0));
    } else if (ch == "Space") {
        vk = VK_SPACE;
    } else if (ch == "Return") {
        vk = VK_RETURN;
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
    return vk;
}
#endif

void mouseEvent(MouseEvType t, float x, float y, uint64_t flags) {
#if defined(JUCE_MAC)
    if (t == MouseEvType::DBL_CLICK) {
        CGPoint loc = CGPointMake(x, y);
        mouseDoubleClickEventInternal(loc, flags);
    } else {
        auto bt = toMouseButtonType(t);
        CGPoint loc = CGPointMake(x, y);
        mouseEventInternal(bt.first, bt.second, loc, flags);
    }
#elif defined(JUCE_WINDOWS)
    auto pos = getScaledPoint(x, y);
    if (t == MouseEvType::DBL_CLICK) {
        // No dedicated handling needed, two down/up events will appear which will be detected as double click
    } else {
        auto mouseFlags = getMouseFlags(t);
        mouseEventInternal(pos, mouseFlags, flags);
    }
#endif
}

void mouseScrollEvent(float x, float y, float deltaX, float deltaY, bool isSmooth) {
#if defined(JUCE_MAC)
    ignoreUnused(x);
    ignoreUnused(y);

    if (isSmooth) {
        const float scale = 0.5f / 256.0f;
        mouseScrollEventInternal(deltaX / scale, deltaY / scale);
    } else {
        const float scale = 10.0f / 256.0f;
        mouseScrollEventInternal(deltaX / scale, deltaY / scale);
    }
#elif defined(JUCE_WINDOWS)
    ignoreUnused(isSmooth);

    auto pos = getScaledPoint(x, y);
    mouseScrollEventInternal(pos, lround(deltaX * 512), lround(deltaY * 512));
#endif
}

void keyEvent(uint16_t keyCode, uint64_t flags, bool keyDown, bool currentProcessOnly, void* nativeHandle) {
#if defined(JUCE_MAC)
    ignoreUnused(nativeHandle);
    keyEventInternal(keyCode, flags, keyDown, currentProcessOnly);
#elif defined(JUCE_WINDOWS)
    keyEventInternal(getVK(keyCode), flags, keyDown, currentProcessOnly ? nativeHandle : nullptr);
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
}

void setAltKey(uint64_t& flags) {
#if defined(JUCE_MAC)
    flags |= kCGEventFlagMaskAlternate;
#elif defined(JUCE_WINDOWS)
    flags |= VK_MENU;
#endif
}

void keyEventDown(uint16_t keyCode, uint64_t flags, bool currentProcessOnly, void* nativeHandle) {
    keyEvent(keyCode, flags, true, currentProcessOnly, nativeHandle);
}

void keyEventUp(uint16_t keyCode, uint64_t flags, bool currentProcessOnly, void* nativeHandle) {
    keyEvent(keyCode, flags, false, currentProcessOnly, nativeHandle);
}

}  // namespace e47

#endif
