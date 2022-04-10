/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ScreenWorker.hpp"
#include "Message.hpp"
#include "ImageDiff.hpp"
#include "App.hpp"
#include "Server.hpp"
#include "Processor.hpp"

namespace e47 {

ScreenWorker::ScreenWorker(LogTag* tag) : Thread("ScreenWorker"), LogTagDelegate(tag) { initAsyncFunctors(); }

ScreenWorker::~ScreenWorker() {
    traceScope();
    stopAsyncFunctors();
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
    waitForThreadAndLog(getLogTagSource(), this);
}

void ScreenWorker::init(std::unique_ptr<StreamingSocket> s) {
    traceScope();
    m_socket = std::move(s);
}

void ScreenWorker::run() {
    traceScope();
    logln("screen processor started");

    if (getApp()->getServer()->getScreenCapturingFFmpeg()) {
        runFFmpeg();
    } else if (!getApp()->getServer()->getScreenCapturingOff()) {
        runNative();
    } else {
        while (!threadShouldExit() && nullptr != m_socket && m_socket->isConnected()) {
            sleepExitAware(100);
        }
    }

    logln("screen processor terminated");
}

void ScreenWorker::runFFmpeg() {
    traceScope();
    Message<ScreenCapture> msg;
    while (isOk()) {
        std::unique_lock<std::mutex> lock(m_currentImageLock);
        if (m_currentImageCv.wait_for(lock, 50ms, [this] { return m_updated; })) {
            m_updated = false;
            if (m_imageBuf.size() > 0) {
                if (m_imageBuf.size() <= Message<ScreenCapture>::MAX_SIZE) {
                    msg.payload.setImage(m_width, m_height, m_scale, m_imageBuf.data(), m_imageBuf.size());
                    lock.unlock();
                    std::lock_guard<std::mutex> socklock(m_mtx);
                    msg.send(m_socket.get());
                } else {
                    logln(
                        "plugin screen image data exceeds max message size, Message::MAX_SIZE has to be "
                        "increased.");
                }
            }
        }
    }
}

void ScreenWorker::runNative() {
    traceScope();
    Message<ScreenCapture> msg;
    float qual = getApp()->getServer()->getScreenQuality();
    PNGImageFormat png;
    JPEGImageFormat jpg;
    bool diffDetect = getApp()->getServer()->getScreenDiffDetection();
    uint32_t captureCount = 0;
    while (isOk()) {
        std::unique_lock<std::mutex> lock(m_currentImageLock);
        m_currentImageCv.wait(lock, [this] { return m_updated; });
        m_updated = false;

        if (nullptr != m_currentImage) {
            std::shared_ptr<Image> imgToSend = m_currentImage;
            bool needsBrightnessCheckOrRefresh = (captureCount++ % 20) == 0;
            bool forceFullImg = !diffDetect || needsBrightnessCheckOrRefresh;  // send a full image once per second

            // For some reason the plugin window turns white or black sometimes, this should be investigated..
            // For now as a hack: Check if the image is mostly white, and reset the plugin window in this case.
            float mostlyWhite = m_width * m_height * 0.99f;
            float mostlyBlack = 0.1f;
            float brightness = mostlyWhite / 2;

            // Calculate the difference between the current and the last image
            auto diffPxCount = (uint64_t)(m_width * m_height);
            if (!forceFullImg && m_lastImage != nullptr && m_currentImage->getBounds() == m_lastImage->getBounds() &&
                m_diffImage != nullptr) {
                brightness = 0;
                diffPxCount = ImageDiff::getDelta(
                    *m_lastImage, *m_currentImage, *m_diffImage,
                    [&brightness](const PixelARGB& px) { brightness += ImageDiff::getBrightness(px); });
                imgToSend = m_diffImage;
            } else if (needsBrightnessCheckOrRefresh && !diffDetect) {
                brightness = ImageDiff::getBrightness(*imgToSend);
            }

            if (brightness >= mostlyWhite || brightness <= mostlyBlack) {
                logln("resetting editor window");
                runOnMsgThreadAsync([this] {
                    traceScope();
                    getApp()->resetEditor(m_currentTid);
                });
                runOnMsgThreadAsync([this] {
                    traceScope();
                    getApp()->restartEditor(m_currentTid);
                });
            } else {
                if (diffPxCount > 0) {
                    MemoryOutputStream mos;
                    if (diffDetect) {
                        png.writeImageToStream(*imgToSend, mos);
                    } else {
                        jpg.setQuality(qual);
                        jpg.writeImageToStream(*imgToSend, mos);
                    }

                    lock.unlock();

                    if (mos.getDataSize() > Message<ScreenCapture>::MAX_SIZE) {
                        if (!diffDetect && qual > 0.1) {
                            qual -= 0.1f;
                        } else {
                            logln(
                                "plugin screen image data exceeds max message size, Message::MAX_SIZE has to be "
                                "increased.");
                        }
                    } else {
                        msg.payload.setImage(m_width, m_height, 1, mos.getData(), mos.getDataSize());
                        std::lock_guard<std::mutex> socklock(m_mtx);
                        msg.send(m_socket.get());
                    }
                }
            }
        } else {
            // another client took over, notify this one
            msg.payload.setImage(0, 0, 0, nullptr, 0);
            std::lock_guard<std::mutex> socklock(m_mtx);
            msg.send(m_socket.get());
        }
    }
}

void ScreenWorker::shutdown() {
    traceScope();
    signalThreadShouldExit();
    if (m_visible) {
        hideEditor();
    }
    std::lock_guard<std::mutex> lock(m_currentImageLock);
    m_currentImage = nullptr;
    m_updated = true;
    m_currentImageCv.notify_one();
}

void ScreenWorker::showEditor(Thread::ThreadID tid, std::shared_ptr<Processor> proc, int x, int y) {
    traceScope();
    logln("showing editor for " << proc->getName() << " at " << x << "x" << y);

    m_currentTid = tid;

    if (m_visible && proc.get() == m_currentProc && proc == getApp()->getCurrentWindowProc(m_currentTid)) {
        logln("already showing editor");
        runOnMsgThreadAsync([this, x, y] {
            traceScope();
            getApp()->moveEditor(m_currentTid, x, y);
            getApp()->bringEditorToFront(m_currentTid);
        });
        return;
    }

    runOnMsgThreadAsync([this] {
        traceScope();
        if (getApp()->getServer()->getScreenCapturingOff()) {
            getApp()->hideEditor(m_currentTid, false);
        } else {
            // we allow only one plugin UI at a time when capturing the screen, so we hide all
            getApp()->hideEditor(nullptr, false);
        }
    });

    if (getApp()->getServer()->getScreenCapturingFFmpeg()) {
        runOnMsgThreadAsync([this, proc] {
            traceScope();
            getApp()->showEditor(proc, m_currentTid, [this](const uint8_t* data, int size, int w, int h, double scale) {
                traceScope();
                if (threadShouldExit()) {
                    return;
                }
                std::lock_guard<std::mutex> lock(m_currentImageLock);
                if (m_imageBuf.size() < (size_t)size) {
                    m_imageBuf.resize((size_t)size);
                }
                memcpy(m_imageBuf.data(), data, (size_t)size);
                m_width = w;
                m_height = h;
                m_scale = scale;
                m_updated = true;
                m_currentImageCv.notify_one();
            });
        });
    } else {
        runOnMsgThreadAsync([this, proc] {
            traceScope();
            m_currentImageLock.lock();
            m_currentImage.reset();
            m_lastImage.reset();
            m_currentImageLock.unlock();

            getApp()->showEditor(proc, m_currentTid, [this](std::shared_ptr<Image> i, int w, int h) {
                traceScope();
                if (nullptr != i) {
                    if (threadShouldExit()) {
                        return;
                    }
                    std::lock_guard<std::mutex> lock(m_currentImageLock);
                    m_lastImage = m_currentImage;
                    m_currentImage = i;
                    if (m_lastImage == nullptr || m_lastImage->getBounds() != m_currentImage->getBounds() ||
                        m_diffImage == nullptr) {
                        m_diffImage = std::make_shared<Image>(Image::ARGB, w, h, false);
                    }
                    m_width = w;
                    m_height = h;
                    m_updated = true;
                    m_currentImageCv.notify_one();
                }
            });
        });
    }

    runOnMsgThreadAsync([this, x, y] {
        traceScope();
        getApp()->moveEditor(m_currentTid, x, y);
        getApp()->bringEditorToFront(m_currentTid);
    });

    m_visible = true;
    m_currentProc = proc.get();
}

void ScreenWorker::hideEditor() {
    logln("hiding editor");

    runOnMsgThreadAsync([this, tid = m_currentTid] {
        logln("hiding editor (msg thread)");
        getApp()->hideEditor(tid);

        std::lock_guard<std::mutex> lock(m_currentImageLock);
        m_currentImage.reset();
        m_lastImage.reset();
    });

    m_visible = false;
    m_currentProc = nullptr;
    m_currentTid = nullptr;
}

}  // namespace e47
