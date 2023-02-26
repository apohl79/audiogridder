/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _PROCESSORWINDOW_HPP_
#define _PROCESSORWINDOW_HPP_

#include <JuceHeader.h>

#include "ScreenRecorder.hpp"
#include "Utils.hpp"

namespace e47 {

class Processor;

class ProcessorWindow : public DocumentWindow, private Timer, public LogTag {
  public:
    using CaptureCallbackNative = std::function<void(std::shared_ptr<Image> image, int width, int height)>;
    using CaptureCallbackFFmpeg = ScreenRecorder::CaptureCallback;

    ProcessorWindow(std::shared_ptr<Processor> proc, Thread::ThreadID tid, CaptureCallbackNative func,
                    std::function<void()> onHide, int x, int y);
    ProcessorWindow(std::shared_ptr<Processor> proc, Thread::ThreadID tid, CaptureCallbackFFmpeg func,
                    std::function<void()> onHide, int x, int y);
    ~ProcessorWindow() override;

    void closeButtonPressed() override;
    BorderSize<int> getBorderThickness() override { return {}; }

    void forgetEditor();
    juce::Rectangle<int> getScreenCaptureRect();
    void updateScreenCaptureArea();
    void startCapturing();
    void stopCapturing();
    void resized() override;
    void setVisible(bool b) override;
    bool isShowingPlugin() const { return m_isShowing; }
    bool hasEditor() const { return nullptr != m_editor; }
    void move(int x, int y);
    void toTop();

    CaptureCallbackFFmpeg getCaptureCallbackFFmpeg() const { return m_callbackFFmpeg; }
    CaptureCallbackNative getCaptureCallbackNative() const { return m_callbackNative; }
    std::function<void()> getOnHide() const { return m_onHide; }

    Thread::ThreadID getTid() const { return m_tid; }

  private:
    std::shared_ptr<Processor> m_processor;
    Thread::ThreadID m_tid;
    AudioProcessorEditor* m_editor = nullptr;
    CaptureCallbackNative m_callbackNative;
    CaptureCallbackFFmpeg m_callbackFFmpeg;
    std::function<void()> m_onHide;
    juce::Rectangle<int> m_screenCaptureRect, m_totalRect;
    int m_startCapturingRetry;
    bool m_isShowing = false;

    void createEditor();
    void captureWindow();

    void timerCallback() override { captureWindow(); }

    bool isFullyVisible() const;

    ENABLE_ASYNC_FUNCTORS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorWindow)
};

}  // namespace e47

#endif  // _PROCESSORWINDOW_HPP_
