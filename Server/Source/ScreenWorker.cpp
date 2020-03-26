/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ScreenWorker.hpp"
#include "Message.hpp"
#include "Utils.hpp"

namespace e47 {

ScreenWorker::~ScreenWorker() {
    if (nullptr != m_socket && m_socket->isConnected()) {
        m_socket->close();
    }
}

void ScreenWorker::init(std::unique_ptr<StreamingSocket> s) { m_socket = std::move(s); }

void ScreenWorker::run() {
    Message<ScreenCapture> msg;
    float qual = 0.9;
    while (!currentThreadShouldExit() && nullptr != m_socket && m_socket->isConnected()) {
        std::unique_lock<std::mutex> lock(m_currentImageLock);
        m_currentImageCv.wait(lock, [this] { return m_updated; });
        m_updated = false;

        if (nullptr != m_currentImage) {
            MemoryOutputStream mos;
            JPEGImageFormat jpg;
            jpg.setQuality(qual);
            jpg.writeImageToStream(*m_currentImage, mos);

            lock.unlock();

            if (mos.getDataSize() > Message<ScreenCapture>::MAX_SIZE) {
                if (qual > 0.1) {
                    qual -= 0.1;
                } else {
                    logln("plugin screen image data exceeds max message size, Message::MAX_SIZE has to be increased.");
                }
            } else {
                msg.payload.setImage(m_width, m_height, mos.getData(), mos.getDataSize());
                msg.send(m_socket.get());
            }
        } else {
            // another client took over, notify this one
            msg.payload.setImage(0, 0, nullptr, 0);
            msg.send(m_socket.get());
        }
    }
    hideEditor();
    dbgln("screen processor terminated");
}

void ScreenWorker::shutdown() {
    signalThreadShouldExit();
    std::lock_guard<std::mutex> lock(m_currentImageLock);
    m_currentImage = nullptr;
    m_updated = true;
    m_currentImageCv.notify_one();
}

void ScreenWorker::showEditor(std::shared_ptr<AudioProcessor> proc) {
    auto tid = getThreadId();
    MessageManager::callAsync([this, proc, tid] {
        getApp().showEditor(proc, tid, [this](std::shared_ptr<Image> i, int w, int h) {
            if (nullptr != i) {
                std::lock_guard<std::mutex> lock(m_currentImageLock);
                m_currentImage = i;
                m_width = w;
                m_height = h;
                m_updated = true;
                m_currentImageCv.notify_one();
            }
        });
    });
}

void ScreenWorker::hideEditor() {
    auto tid = getThreadId();
    MessageManager::callAsync([tid] { getApp().hideEditor(tid); });
}

}  // namespace e47
