/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#include "ImageReader.hpp"
#include "ImageDiff.hpp"

namespace e47 {

ImageReader::ImageReader() {}

std::shared_ptr<Image> ImageReader::read(const char* data, size_t size, int width, int height, double scale) {
    traceScope();
    if (nullptr != data) {
        if (size > 4 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
            if ((m_width != width || m_height != height) && nullptr != m_inputCodecCtx) {
                closeCodec();
            }
            m_width = width;
            m_height = height;
            m_scale = scale;
            if (nullptr == m_inputCodecCtx) {
                if (!initCodec()) {
                    logln("failed to initialize codec");
                    return nullptr;
                }
            }
            int ret;
            if (nullptr == m_inputPacket->buf || (size_t)m_inputPacket->size < size) {
                ret = av_new_packet(m_inputPacket, (int)size);
                if (ret != 0) {
                    logln("av_new_packet failed: " << ret);
                    return nullptr;
                }
            }
            memcpy(m_inputPacket->data, data, size);
            do {
                ret = avcodec_send_packet(m_inputCodecCtx, m_inputPacket);
                if (ret < 0 && ret != AVERROR(EAGAIN)) {
                    if (ret == AVERROR_EOF) {
                        logln("avcodec_send_packet failed: EOF");
                    } else if (ret == AVERROR(EINVAL)) {
                        logln("avcodec_send_packet failed: EINVAL");
                    } else if (ret == AVERROR(ENOMEM)) {
                        logln("avcodec_send_packet failed: ENOMEM");
                    } else if (ret == AVERROR_INVALIDDATA) {
                        logln("avcodec_send_packet failed: AVERROR_INVALIDDATA");
                    } else if (ret == AVERROR_PATCHWELCOME) {
                        logln("avcodec_send_packet failed: AVERROR_PATCHWELCOME");
                    } else if (ret == AVERROR_BUG) {
                        logln("avcodec_send_packet failed: AVERROR_BUG");
                    } else {
                        logln("avcodec_send_packet failed: unknown code " << ret);
                    }
                    closeCodec();
                    return nullptr;
                }
            } while (ret == AVERROR(EAGAIN));
            do {
                ret = avcodec_receive_frame(m_inputCodecCtx, m_inputFrame);
                if (ret >= 0) {
                    // put decoded frame into a juce image
                    sws_scale(m_swsCtx, m_inputFrame->data, m_inputFrame->linesize, 0, m_inputFrame->height,
                              m_outputFrame->data, m_outputFrame->linesize);
                    if (nullptr == m_image || m_image->getWidth() != m_inputFrame->width ||
                        m_image->getHeight() != m_inputFrame->height) {
                        m_image =
                            std::make_shared<Image>(Image::ARGB, m_inputFrame->width, m_inputFrame->height, false);
                    }
                    Image::BitmapData bd(*m_image, 0, 0, m_image->getWidth(), m_image->getHeight());
                    memcpy(bd.data, m_outputFrame->data[0],
                           (size_t)(m_outputFrame->linesize[0] * m_outputFrame->height));
                }
            } while (ret == AVERROR(EAGAIN));
        } else if (size > 3 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
            auto img = std::make_shared<Image>(PNGImageFormat::loadFrom(data, size));
            if (m_image == nullptr || m_image->getBounds() != img->getBounds()) {
                m_image = img;
            } else {
                ImageDiff::applyDelta(*m_image, *img);
            }

        } else {
            m_image = std::make_shared<Image>(JPEGImageFormat::loadFrom(data, size));
        }
    }
    return m_image;
}

bool ImageReader::initCodec() {
    traceScope();

    av_log_set_level(AV_LOG_QUIET);

    m_inputCodec = avcodec_find_decoder_by_name("webp");
    if (nullptr == m_inputCodec) {
        logln("unable to find webp codec");
        return false;
    }

    m_inputPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
    if (nullptr == m_inputPacket) {
        logln("unable to allocate AVPacket");
        return false;
    }
    av_init_packet(m_inputPacket);

    m_inputFrame = av_frame_alloc();
    if (nullptr == m_inputFrame) {
        logln("unable to allocate AVFrame");
        return false;
    }

    m_inputCodecCtx = avcodec_alloc_context3(nullptr);
    if (nullptr == m_inputCodecCtx) {
        logln("unable to allocate codec context");
        return false;
    }
    m_inputCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_inputCodecCtx->time_base.num = 1;
    m_inputCodecCtx->time_base.den = 20;
    m_inputCodecCtx->width = m_width;
    m_inputCodecCtx->height = m_height;
    int ret = avcodec_open2(m_inputCodecCtx, m_inputCodec, nullptr);
    if (ret < 0) {
        logln("avcodec_open2 failed: " << ret);
        return false;
    }

    m_outputFrame = av_frame_alloc();
    if (nullptr == m_outputFrame) {
        logln("unable to allocate AVFrame");
        return false;
    }
    auto fmt = AV_PIX_FMT_RGB32;
    m_outputFrame->width = m_width;
    m_outputFrame->height = m_height;
    m_outputFrame->format = fmt;
    m_outputFrameBuf =
        (uint8_t*)av_malloc((size_t)av_image_get_buffer_size(fmt, m_width, m_height, 1) + AV_INPUT_BUFFER_PADDING_SIZE);
    av_image_fill_arrays(m_outputFrame->data, m_outputFrame->linesize, m_outputFrameBuf, fmt, m_width, m_height, 1);

    m_swsCtx = sws_getContext(m_width, m_height, m_inputCodecCtx->pix_fmt, m_width, m_height, fmt, SWS_FAST_BILINEAR,
                              nullptr, nullptr, nullptr);

    logln("ready to process image stream with resolution: " << m_width << "x" << m_height << " *" << m_scale);

    return true;
}

void ImageReader::closeCodec() {
    traceScope();
    if (nullptr != m_inputPacket) {
        av_packet_unref(m_inputPacket);
        av_free(m_inputPacket);
        m_inputPacket = nullptr;
    }

    if (nullptr != m_inputFrame) {
        av_frame_unref(m_inputFrame);
        av_frame_free(&m_inputFrame);
        m_inputFrame = nullptr;
    }

    if (nullptr != m_inputCodecCtx) {
        avcodec_close(m_inputCodecCtx);
        avcodec_free_context(&m_inputCodecCtx);
        m_inputCodecCtx = nullptr;
    }

    if (nullptr != m_outputFrameBuf) {
        av_free(m_outputFrameBuf);
        m_outputFrameBuf = nullptr;
    }

    if (nullptr != m_outputFrame) {
        av_frame_unref(m_outputFrame);
        av_frame_free(&m_outputFrame);
        m_outputFrame = nullptr;
    }

    if (nullptr != m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

}  // namespace e47
