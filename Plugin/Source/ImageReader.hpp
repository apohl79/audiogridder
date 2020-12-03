/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#ifndef ImageReader_hpp
#define ImageReader_hpp

#include <JuceHeader.h>
#include "Utils.hpp"
#include "ImageDiff.hpp"

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wsign-conversion", "-Wconversion")
JUCE_BEGIN_IGNORE_WARNINGS_MSVC(4244)
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
JUCE_END_IGNORE_WARNINGS_MSVC

#ifdef JUCE_WINDOWS
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace e47 {

class ImageReader : public LogTagDelegate {
  public:
    ImageReader();

    std::shared_ptr<Image> read(const char* data, size_t size, int width, int height, double scale);

  private:
    std::shared_ptr<Image> m_image;

    int m_width = 0;
    int m_height = 0;
    double m_scale = 1;
    AVCodec* m_inputCodec = nullptr;
    AVCodecContext* m_inputCodecCtx = nullptr;
    AVFrame* m_inputFrame = nullptr;
    AVFrame* m_outputFrame = nullptr;
    uint8_t* m_outputFrameBuf = nullptr;
    AVPacket* m_inputPacket = nullptr;
    SwsContext* m_swsCtx = nullptr;

    bool initCodec();
    void closeCodec();
};

}  // namespace e47

#endif /* ImageReader_hpp */
