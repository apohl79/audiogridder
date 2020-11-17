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
    static std::atomic_uint32_t count;
    static std::atomic_uint32_t runCount;

    ScreenWorker(LogTag* tag);
    virtual ~ScreenWorker();

    void init(std::unique_ptr<StreamingSocket> s);

    void run();
    void runNative();
    void runFFmpeg();
    void shutdown();

    void showEditor(std::shared_ptr<AGProcessor> proc);
    void hideEditor();

  private:
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

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* ScreenWorker_hpp */
