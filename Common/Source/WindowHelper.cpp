/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "WindowHelper.hpp"

#if !JUCE_MAC

#ifdef JUCE_WINDOWS
#include <windows.h>
#endif

namespace e47 {
namespace WindowHelper {

juce::Rectangle<int> getWindowScreenBounds(juce::Component* c) {
#ifdef JUCE_WINDOWS
    if (HWND hwnd = (HWND)c->getWindowHandle()) {
        RECT rect;
        if (HWND hwndA = GetAncestor(hwnd, GA_ROOT)) {
            if (GetWindowRect(hwndA, &rect)) {
                // scaling
                HDC hDC = GetDC(0);
                float dpi = (GetDeviceCaps(hDC, LOGPIXELSX) + GetDeviceCaps(hDC, LOGPIXELSY)) / 2.0f;
                ReleaseDC(0, hDC);
                float sf = 96 / dpi;
                long left = lroundf(sf * rect.left);
                long top = lroundf(sf * rect.top);
                long right = lroundf(sf * rect.right);
                long bottom = lroundf(sf * rect.bottom);
                return {left, top, right - left, bottom - top};
            }
        }
    }
#else
    ignoreUnused(c);
#endif
    return {};
}

}  // namespace WindowHelper
}  // namespace e47

#endif
