/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ImageDiff_hpp
#define ImageDiff_hpp

#include "NumberConversion.hpp"

namespace e47 {

namespace ImageDiff {

static bool operator==(const PixelARGB& lhs, const PixelARGB& rhs) {
    return lhs.getNativeARGB() == rhs.getNativeARGB();
}

static bool operator!=(const PixelARGB& lhs, const PixelARGB& rhs) { return !(lhs == rhs); }

using PerPixelFn = std::function<void(const PixelARGB& px)>;

inline uint64_t getDelta(const uint8_t* imgFrom, const uint8_t* imgTo, uint8_t* imgDelta, int width, int height,
                         PerPixelFn fn = nullptr) {
    uint64_t count = 0;
    auto* pxFrom = reinterpret_cast<const PixelARGB*>(imgFrom);
    auto* pxTo = reinterpret_cast<const PixelARGB*>(imgTo);
    auto* pxDelta = reinterpret_cast<PixelARGB*>(imgDelta);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto p = as<size_t>(y * width + x);
            if (pxFrom[p] != pxTo[p]) {
                count++;
                pxDelta[p].set(pxTo[p]);
                pxDelta[p].setAlpha(255);
            } else {
                pxDelta[p] = {0, 0, 0, 0};
            }
            if (fn) {
                fn(pxTo[p]);
            }
        }
    }
    return count;
}

inline uint64_t getDelta(const Image& imgFrom, const Image& imgTo, const Image& imgDelta, PerPixelFn fn = nullptr) {
    if (imgFrom.getBounds() == imgTo.getBounds() && imgDelta.getBounds() == imgTo.getBounds()) {
        int width = imgTo.getWidth();
        int height = imgTo.getHeight();
        const Image::BitmapData bdFrom(imgFrom, 0, 0, width, height);
        const Image::BitmapData bdTo(imgTo, 0, 0, width, height);
        Image::BitmapData bdDelta(imgDelta, 0, 0, width, height);
        return getDelta(bdFrom.data, bdTo.data, bdDelta.data, width, height, fn);
    }
    return 0;
}

inline uint64_t applyDelta(uint8_t* imgDst, const uint8_t* imgDelta, int width, int height) {
    uint64_t count = 0;
    auto* pxDst = reinterpret_cast<PixelARGB*>(imgDst);
    auto* pxDelta = reinterpret_cast<const PixelARGB*>(imgDelta);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto p = as<size_t>(y * width + x);
            if (pxDelta[p].getAlpha() == 255) {
                pxDst[p].set(pxDelta[p]);
                count++;
            }
        }
    }
    return count;
}

inline uint64_t applyDelta(Image& imgDst, const Image& imgDelta) {
    if (imgDelta.getBounds() == imgDst.getBounds()) {
        int width = imgDelta.getWidth();
        int height = imgDelta.getHeight();
        const Image::BitmapData bdDelta(imgDelta, 0, 0, width, height);
        Image::BitmapData bdDst(imgDst, 0, 0, width, height);
        return applyDelta(bdDst.data, bdDelta.data, width, height);
    }
    return 0;
}

inline float getBrightness(const PixelARGB& px) {
    auto col = Colour::fromRGBA(px.getRed(), px.getGreen(), px.getBlue(), px.getAlpha());
    return col.getFloatRed() / 3 + col.getFloatGreen() / 3 + col.getFloatBlue() / 3;
}

inline float getBrightness(const uint8_t* img, int width, int height) {
    auto* px = reinterpret_cast<const PixelARGB*>(img);
    float brightness = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto p = as<size_t>(y * width + x);
            brightness += getBrightness(px[p]);
        }
    }
    return brightness;
}

inline float getBrightness(const Image& img) {
    int width = img.getWidth();
    int height = img.getHeight();
    const Image::BitmapData bd(img, 0, 0, width, height);
    return getBrightness(bd.data, width, height);
}

}  // namespace ImageDiff

}  // namespace e47

#endif
