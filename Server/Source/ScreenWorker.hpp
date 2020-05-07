/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ScreenWorker_hpp
#define ScreenWorker_hpp

#include "../JuceLibraryCode/JuceHeader.h"

#include <thread>

namespace e47 {

class ScreenWorker : public Thread {
  public:
    ScreenWorker() : Thread("ScreenWorker") {}
    virtual ~ScreenWorker();

    void init(std::unique_ptr<StreamingSocket> s);
    void run();
    void shutdown();

    void showEditor(std::shared_ptr<AudioProcessor> proc);
    void hideEditor();

  private:
    std::unique_ptr<StreamingSocket> m_socket;
    std::shared_ptr<Image> m_currentImage, m_lastImage, m_diffImage;
    int m_width;
    int m_height;
    bool m_updated = false;
    std::mutex m_currentImageLock;
    std::condition_variable m_currentImageCv;
};

}  // namespace e47

#endif /* ScreenWorker_hpp */
