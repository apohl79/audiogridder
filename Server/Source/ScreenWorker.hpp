/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ScreenWorker_hpp
#define ScreenWorker_hpp

#include <JuceHeader.h>
#include <thread>

#include "Utils.hpp"
#include "ProcessorChain.hpp"

namespace e47 {

class ScreenWorker : public Thread, public LogTagDelegate {
  public:
    ScreenWorker(LogTag* tag);
    virtual ~ScreenWorker();

    void init(std::unique_ptr<StreamingSocket> s);

    bool isOk() {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_wasOk = !currentThreadShouldExit() && nullptr != m_socket && m_socket->isConnected();
        return m_wasOk;
    }

    bool isOkNoLock() const { return m_wasOk; }

    void run();
    void runNative();
    void runFFmpeg();
    void shutdown();

    void showEditor(std::shared_ptr<AGProcessor> proc, int x, int y);
    void hideEditor();

  private:
    std::mutex m_mtx;
    std::atomic_bool m_wasOk{true};
    std::unique_ptr<StreamingSocket> m_socket;

    // Native capturing
    std::shared_ptr<Image> m_currentImage, m_lastImage, m_diffImage;
    // FFmpeg capturing
    std::vector<char> m_imageBuf;

    int m_width;
    int m_height;
    double m_scale;
    bool m_updated = false;
    std::mutex m_currentImageLock;
    std::condition_variable m_currentImageCv;

    std::atomic_bool m_visible{false};
    AGProcessor* m_currentProc;

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* ScreenWorker_hpp */
