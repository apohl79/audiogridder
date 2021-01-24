/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ScreenRecorder_hpp
#define ScreenRecorder_hpp

#include <JuceHeader.h>

#include "Utils.hpp"

#include <thread>

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

class ScreenRecorder : public LogTag {
  public:
    using CaptureCallback = std::function<void(const uint8_t* data, int size, int width, int height, double scale)>;

    enum EncoderMode { WEBP, MJPEG };

    ScreenRecorder();
    ~ScreenRecorder();

    void start(Rectangle<int> rect, CaptureCallback fn);
    void resume(Rectangle<int> rect = {});
    void stop();

    bool isRecording() const { return m_capture; }

    enum EncoderQuality : int { ENC_QUALITY_LOW = 0, ENC_QUALITY_MEDIUM = 1, ENC_QUALITY_HIGH = 2 };

    static void initialize(EncoderMode encMode = WEBP, EncoderQuality quality = ENC_QUALITY_MEDIUM);

  private:
    static String m_inputFmtName;
    static String m_inputStreamUrl;
    static AVInputFormat* m_inputFmt;
    static AVFormatContext* m_captureFmtCtx;
    AVCodec* m_captureCodec = nullptr;
    AVCodecContext* m_captureCodecCtx = nullptr;
    AVFrame* m_captureFrame = nullptr;
    AVFrame* m_cropFrame = nullptr;
    AVPacket* m_capturePacket = nullptr;
    AVStream* m_captureStream = nullptr;
    int m_captureStreamIndex = -1;

    static AVCodec* m_outputCodec;
    AVCodecContext* m_outputCodecCtx = nullptr;
    AVFrame* m_outputFrame = nullptr;
    uint8_t* m_outputFrameBuf = nullptr;
    AVPacket* m_outputPacket = nullptr;

    static bool m_initialized;

    SwsContext* m_swsCtx = nullptr;

    Rectangle<int> m_captureRect;
    int m_pxSize = 0;

    static EncoderMode m_encMode;
    static double m_scale;
    static int m_quality;
    static bool m_downScale;

    std::unique_ptr<std::thread> m_thread;
    std::atomic_bool m_threadRunning{false};
    std::atomic_bool m_capture{false};

    CaptureCallback m_callback;

    bool updateArea(Rectangle<int> rect);

    bool prepareInput();
    bool prepareOutput();

    void cleanupInput();
    void cleanupOutput();

    void record();
};

}  // namespace e47
#endif /* ScreenRecorder_hpp */
