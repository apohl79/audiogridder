/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ScreenRecorder.hpp"
#include "Screen.h"
#include "Metrics.hpp"

namespace e47 {

String ScreenRecorder::m_inputFmtName;
String ScreenRecorder::m_inputStreamUrl;
const AVInputFormat* ScreenRecorder::m_inputFmt = nullptr;
const AVCodec* ScreenRecorder::m_outputCodec = nullptr;
AVFormatContext* ScreenRecorder::m_captureFmtCtx = nullptr;
bool ScreenRecorder::m_initialized = false;

ScreenRecorder::EncoderMode ScreenRecorder::m_encMode = ScreenRecorder::WEBP;
double ScreenRecorder::m_scale;
int ScreenRecorder::m_quality;
bool ScreenRecorder::m_downScale = false;

int WEBP_QUALITY[3] = {4000, 8000, 16000};
int MJPEG_QUALITY[3] = {9000000, 14000000, 20000000};

void ScreenRecorder::initialize(ScreenRecorder::EncoderMode encMode, EncoderQuality quality) {
    setLogTagStatic("screenrec");
    traceScope();

    SharedInstance<ScreenRecorder>::initialize();

    av_log_set_level(AV_LOG_QUIET);

    m_encMode = encMode;
    const char* encName = "unset";
    switch (m_encMode) {
        case WEBP:
            encName = "libwebp";
            m_quality = WEBP_QUALITY[quality];
            break;
        case MJPEG:
            encName = "mjpeg";
            m_quality = MJPEG_QUALITY[quality];
            break;
    }
    m_outputCodec = avcodec_find_encoder_by_name(encName);
    if (nullptr == m_outputCodec) {
        logln("unable to find output codec " << encName);
        return;
    }

    if (m_initialized) {
        return;
    }

    if (auto disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
        m_scale = disp->scale;
    } else {
        m_scale = 1.0;
    }

    avdevice_register_all();

#ifdef JUCE_MAC
    askForScreenRecordingPermission();
    m_inputFmtName = "avfoundation";
    m_inputStreamUrl = String(getCaptureDeviceIndex()) + ":none";
    m_downScale = quality != ENC_QUALITY_HIGH;
#else
    m_inputFmtName = "gdigrab";
    m_inputStreamUrl = "desktop";
#endif

    logln("using device " << m_inputFmtName << " with stream URL '" << m_inputStreamUrl << "'");

    m_inputFmt = av_find_input_format(m_inputFmtName.getCharPointer());
    if (nullptr == m_inputFmt) {
        logln("unable to find " << m_inputFmtName << " input format");
        return;
    }

    m_initialized = true;
}

ScreenRecorder::ScreenRecorder() : LogTag("screenrec") { traceScope(); }

ScreenRecorder::~ScreenRecorder() {
    traceScope();
    cleanupInput();
    cleanupOutput();
}

void ScreenRecorder::cleanupInput() {
    traceScope();
    if (nullptr != m_capturePacket) {
        av_free(m_capturePacket);
        m_capturePacket = nullptr;
    }

    if (nullptr != m_captureFrame) {
        av_frame_unref(m_captureFrame);
        av_frame_free(&m_captureFrame);
    }

    if (nullptr != m_cropFrame) {
        av_frame_unref(m_cropFrame);
        av_frame_free(&m_cropFrame);
    }

    if (nullptr != m_captureCodecCtx) {
        avcodec_close(m_captureCodecCtx);
        avcodec_free_context(&m_captureCodecCtx);
    }

    if (nullptr != m_captureFmtCtx) {
        avformat_close_input(&m_captureFmtCtx);
        if (nullptr != m_captureFmtCtx) {
            logln("avformat_close_input failed");
            avformat_free_context(m_captureFmtCtx);
            m_captureFmtCtx = nullptr;
        }
    }
}

void ScreenRecorder::cleanupOutput() {
    traceScope();
    if (nullptr != m_outputPacket) {
        av_free(m_outputPacket);
        m_outputPacket = nullptr;
    }

    if (nullptr != m_outputFrameBuf) {
        av_free(m_outputFrameBuf);
        m_outputFrameBuf = nullptr;
    }

    if (nullptr != m_outputFrame) {
        av_frame_unref(m_outputFrame);
        av_frame_free(&m_outputFrame);
    }

    if (nullptr != m_outputCodecCtx) {
        avcodec_close(m_outputCodecCtx);
        avcodec_free_context(&m_outputCodecCtx);
    }

    if (nullptr != m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

void ScreenRecorder::start(juce::Rectangle<int> rect, CaptureCallback callbackFn, ErrorCallback errorFn) {
    traceScope();

    if (!m_initialized) {
        logError("screen recording not possible: initialization failed");
        return;
    }

    bool shouldResume = false;

    {
        std::lock_guard<std::mutex> lock(m_startStopMtx);

        m_captureRect = rect * m_scale;

        if (nullptr != callbackFn) {
            m_captureCallback = callbackFn;
            shouldResume = true;
        }

        if (nullptr != errorFn) {
            m_errorCallback = errorFn;
        }
    }

    if (shouldResume) {
        resume();
    }
}

void ScreenRecorder::stop() {
    traceScope();

    m_capture = false;

    std::lock_guard<std::mutex> lock(m_startStopMtx);

    if (nullptr != m_thread) {
        while (m_threadRunning) {
            Thread::sleep(5);
        }
        if (m_thread->joinable()) {
            m_thread->join();
        } else {
            logln("error in stop: thread is not joinable");
            m_thread->detach();
        }
        m_thread.reset();
    }

    m_threadRunning = false;
}

void ScreenRecorder::resume(Rectangle<int> rect) {
    traceScope();

    if (!m_initialized) {
        logError("screen recording not possible: initialization failed");
        return;
    }

    m_capture = true;

    std::lock_guard<std::mutex> lock(m_startStopMtx);

    if (nullptr != m_thread) {
        logln("error in resume: detaching thread");
        m_thread->detach();
    }

    m_threadRunning = true;

    m_thread = std::make_unique<std::thread>([this, rect] {
        Thread::setCurrentThreadName("ScreenRecorder");
        traceScope();
        if (!rect.isEmpty()) {
            m_captureRect = rect * m_scale;
        }
        if (prepareInput() && prepareOutput()) {
            record();
        }
        cleanupInput();
        cleanupOutput();
        m_threadRunning = false;
    });
}

bool ScreenRecorder::prepareInput() {
    traceScope();
    AVDictionary* opts = nullptr;

#ifdef JUCE_MAC
    // This works only when building ffmpeg for OSX 10.8+. Thats why there is a separate 10.7 build.
    // av_dict_set(&opts, "capture_cursor", "0", 0); // the default is 0 anyways, so we don't need to call this.
    av_dict_set(&opts, "pixel_format", "yuyv422", 0);
#else
    av_dict_set(&opts, "draw_mouse", "0", 0);
    av_dict_set(&opts, "framerate", "30", 0);
    String vidSize;
    vidSize << m_captureRect.getWidth() << "x" << m_captureRect.getHeight();
    av_dict_set(&opts, "video_size", vidSize.getCharPointer(), 0);
    av_dict_set(&opts, "offset_x", String(m_captureRect.getX()).getCharPointer(), 0);
    av_dict_set(&opts, "offset_y", String(m_captureRect.getY()).getCharPointer(), 0);
#endif

    int ret;

    m_captureFmtCtx = avformat_alloc_context();
    ret = avformat_open_input(&m_captureFmtCtx, m_inputStreamUrl.getCharPointer(), m_inputFmt, &opts);
    if (ret != 0) {
        logError("prepareInput: avformat_open_input failed: err = " + String(ret));
        return false;
    }

    ret = avformat_find_stream_info(m_captureFmtCtx, nullptr);
    if (ret < 0) {
        logError("prepareInput: avformat_find_stream_info failed: " + String(ret));
        return false;
    }

    for (uint16 i = 0; i < m_captureFmtCtx->nb_streams; i++) {
        m_captureStream = m_captureFmtCtx->streams[i];
        if (m_captureStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_captureStreamIndex = i;
            break;
        }
    }
    if (m_captureStreamIndex < 0) {
        logError("prepareInput: unable to find video stream");
        return false;
    }

    m_captureCodec = avcodec_find_decoder(m_captureStream->codecpar->codec_id);
    if (nullptr == m_captureCodec) {
        logError("prepareInput: unable to find capture codec");
        return false;
    }
    m_captureCodecCtx = avcodec_alloc_context3(m_captureCodec);
    if (nullptr == m_captureCodecCtx) {
        logError("prepareInput: unable to allocate codec context");
        return false;
    }
    ret = avcodec_parameters_to_context(m_captureCodecCtx, m_captureStream->codecpar);
    if (ret < 0) {
        logError("prepareInput: avcodec_parameters_to_context failed: err = " + String(ret));
        return false;
    }
    ret = avcodec_open2(m_captureCodecCtx, m_captureCodec, nullptr);
    if (ret < 0) {
        logError("prepareInput: avcodec_open2 failed: err = " + String(ret));
        return false;
    }

    if (m_captureCodecCtx->pix_fmt < 0 || m_captureCodecCtx->pix_fmt >= AV_PIX_FMT_NB) {
        logError("prepareInput: invalid input pixel format: pix_fmt = " + String(m_captureCodecCtx->pix_fmt));
        return false;
    }

    logln("prepareInput: input pixel format is " << m_captureCodecCtx->pix_fmt);

    m_pxSize = av_image_get_linesize(m_captureCodecCtx->pix_fmt, 10, 0) / 10;

    return true;
}

bool ScreenRecorder::prepareOutput() {
    traceScope();

    if (nullptr == m_captureCodecCtx) {
        logError("prepareOutput: input not ready");
        return false;
    }

    m_capturePacket = av_packet_alloc();
    if (nullptr == m_capturePacket) {
        logError("prepareOutput: unable to allocate AVPacket");
        return false;
    }

    m_captureFrame = av_frame_alloc();
    if (nullptr == m_captureFrame) {
        logError("prepareOutput: unable to allocate AVFrame");
        return false;
    }

    m_outputPacket = av_packet_alloc();
    if (nullptr == m_outputPacket) {
        logError("prepareOutput: unable to allocate AVPacket");
        return false;
    }

    if (nullptr != m_outputCodecCtx && avcodec_is_open(m_outputCodecCtx)) {
        avcodec_close(m_outputCodecCtx);
    } else {
        m_outputCodecCtx = avcodec_alloc_context3(m_outputCodec);
        if (nullptr == m_outputCodecCtx) {
            logError("prepareOutput: unable to allocate codec context");
            return false;
        }
    }

    m_scaledWith = m_downScale ? (int)(m_captureRect.getWidth() / m_scale) : m_captureRect.getWidth();
    m_scaledHeight = m_downScale ? (int)(m_captureRect.getHeight() / m_scale) : m_captureRect.getHeight();

    m_outputCodecCtx->pix_fmt = m_encMode == MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
    m_outputCodecCtx->time_base.num = 1;
    m_outputCodecCtx->time_base.den = 30;
    m_outputCodecCtx->width = m_scaledWith;
    m_outputCodecCtx->height = m_scaledHeight;

    avcodec_align_dimensions(m_outputCodecCtx, &m_outputCodecCtx->width, &m_outputCodecCtx->height);

    logln("prepareOutput: setting output codec context dimensions to "
          << m_outputCodecCtx->width << "x" << m_outputCodecCtx->height << " (unalligned " << m_scaledWith << "x"
          << m_scaledHeight << ")");

    AVDictionary* opts = nullptr;
    switch (m_encMode) {
        case WEBP:
            av_dict_set(&opts, "preset", "none", 0);
            av_dict_set(&opts, "compression_level", "1", 0);
            av_dict_set(&opts, "global_quality", String(m_quality).getCharPointer(), 0);
            break;
        case MJPEG:
            av_dict_set(&opts, "b", String(m_quality).getCharPointer(), 0);
            break;
    }
    int ret = avcodec_open2(m_outputCodecCtx, m_outputCodec, &opts);
    if (ret < 0) {
        logError("prepareOutput: avcodec_open2 failed: err = " + String(ret));
        return false;
    }

    if (nullptr == m_outputFrame) {
        m_outputFrame = av_frame_alloc();
        if (nullptr == m_outputFrame) {
            logError("prepareOutput: unable to allocate AVFrame");
            return false;
        }
    } else {
        av_frame_unref(m_outputFrame);
    }
    m_outputFrame->width = m_outputCodecCtx->width;
    m_outputFrame->height = m_outputCodecCtx->height;
    m_outputFrame->format = m_outputCodecCtx->pix_fmt;

    auto outputFrameBufSize =
        (size_t)av_image_get_buffer_size(m_outputCodecCtx->pix_fmt, m_outputFrame->width, m_outputFrame->height, 32) +
        AV_INPUT_BUFFER_PADDING_SIZE;

    logln("prepareOutput: allocating output frame buffer with " << outputFrameBufSize << " bytes");

    m_outputFrameBuf = (uint8_t*)av_malloc(outputFrameBufSize);

    if (nullptr == m_outputFrameBuf) {
        logError("prepareOutput: unable to allocate output frame buffer");
        return false;
    }

    ret = av_image_fill_arrays(m_outputFrame->data, m_outputFrame->linesize, m_outputFrameBuf,
                               m_outputCodecCtx->pix_fmt, m_outputFrame->width, m_outputFrame->height, 32);
    if (ret < 0) {
        logError("prepareOutput: av_image_fill_arrays failed: err = " + String(ret));
        return false;
    }

    m_swsCtx = sws_getContext(m_captureRect.getWidth(), m_captureRect.getHeight(), m_captureCodecCtx->pix_fmt,
                              m_outputFrame->width, m_outputFrame->height, m_outputCodecCtx->pix_fmt, SWS_BICUBIC,
                              nullptr, nullptr, nullptr);
    if (nullptr == m_swsCtx) {
        logError("prepareOutput: sws_getContext failed");
        return false;
    }

    if (nullptr == m_cropFrame) {
        m_cropFrame = av_frame_alloc();
        if (nullptr == m_cropFrame) {
            logError("prepareOutput: unable to allocate AVFrame");
            return false;
        }
    } else {
        av_frame_unref(m_cropFrame);
    }
    m_cropFrame->width = m_captureRect.getWidth();
    m_cropFrame->height = m_captureRect.getHeight();
    m_cropFrame->format = m_captureCodecCtx->pix_fmt;
    if (av_frame_get_buffer(m_cropFrame, 0) < 0) {
        logError("prepareOutput: unable to allocate AVFrame crop buffers");
        return false;
    }

    return true;
}

void ScreenRecorder::record() {
    traceScope();
    logln("started capturing: rectangle " << m_captureRect.getX() << "," << m_captureRect.getY() << ":"
                                          << m_captureRect.getWidth() << "x" << m_captureRect.getHeight() << " scale *"
                                          << m_scale << " <- input rectange " << m_captureCodecCtx->width << "x"
                                          << m_captureCodecCtx->height << ", codecs: in=" << m_captureCodec->name
                                          << " out=" << m_outputCodec->name);
    auto durationPkt = TimeStatistic::getDuration("screen-pkt");
    auto durationScale = TimeStatistic::getDuration("screen-scale");
    auto durationEnc = TimeStatistic::getDuration("screen-enc");
    int initalFramesToSkip = 3;  // avoid flickering at switching between plugins when an editor is initailly painting
    int retRDF;
    do {
        retRDF = av_read_frame(m_captureFmtCtx, m_capturePacket);
        if (retRDF == 0) {
            if (m_capturePacket->stream_index == m_captureStreamIndex) {
                durationPkt.reset();
                int retSDP = avcodec_send_packet(m_captureCodecCtx, m_capturePacket);
                if (retSDP < 0) {
                    logError("record: avcodec_send_packet failed: err = " + String(retSDP));
                    break;
                }
                int retRCF;
                do {
                    retRCF = avcodec_receive_frame(m_captureCodecCtx, m_captureFrame);
                    if (retRCF == 0) {
                        auto* frame = m_captureFrame;
                        if (m_captureFrame->width != m_cropFrame->width ||
                            m_captureFrame->height != m_cropFrame->height) {
                            // crop the frame to the window dimension
                            for (int y = m_captureRect.getY(); y < m_captureRect.getBottom(); y++) {
                                auto* src = m_captureFrame->data[0] + m_captureFrame->linesize[0] * y +
                                            m_captureRect.getX() * m_pxSize;
                                auto* dst =
                                    m_cropFrame->data[0] + m_cropFrame->linesize[0] * (y - m_captureRect.getY());
                                memcpy(dst, src, (size_t)m_cropFrame->linesize[0]);
                            }
                            frame = m_cropFrame;
                        }

                        // convert pixel format
                        durationScale.reset();
                        sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, m_outputFrame->data,
                                  m_outputFrame->linesize);
                        durationScale.update();

                        // convert to WEBP
                        durationEnc.reset();
                        int retSDF = avcodec_send_frame(m_outputCodecCtx, m_outputFrame);
                        if (retSDF < 0) {
                            logln("record: avcodec_send_frame failed: " + String(retSDF));
                            break;
                        }
                        int retRCP;
                        do {
                            retRCP = avcodec_receive_packet(m_outputCodecCtx, m_outputPacket);
                            durationEnc.update();
                            if (retRCP == 0) {
                                if (initalFramesToSkip == 0) {
                                    double scale = 1.0;
#ifdef JUCE_MAC
                                    // If the user chooses the highest quality, we don't downscale images from retina
                                    // displays. But we have to let the plugin know, that it has to adjust the incomming
                                    // image size.
                                    scale = m_downScale ? 1.0 : m_scale;
#endif
                                    m_captureCallback(m_outputPacket->data, m_outputPacket->size, m_scaledWith,
                                                      m_scaledHeight, m_outputFrame->width, m_outputFrame->height,
                                                      scale);
                                } else {
                                    initalFramesToSkip--;
                                }
                                av_packet_unref(m_outputPacket);
                            }
                        } while (retRCP == AVERROR(EAGAIN));
                        av_frame_unref(m_captureFrame);
                    }
                } while (retRCF == AVERROR(EAGAIN));
                av_packet_unref(m_capturePacket);
                durationPkt.update();
            }
        }
    } while (m_capture && (retRDF == 0 || retRDF == AVERROR(EAGAIN)));
    logln("stopped capturing");
}

}  // namespace e47
