/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ProcessorWindow.hpp"
#include "App.hpp"
#include "Server.hpp"
#include "Processor.hpp"
#include "Screen.h"

namespace e47 {

ProcessorWindow::ProcessorWindow(std::shared_ptr<Processor> proc, Thread::ThreadID tid, CaptureCallbackNative func,
                                 std::function<void()> onHide, int x, int y)
    : DocumentWindow(proc->getName(), Colours::lightgrey, DocumentWindow::closeButton),
      LogTag("procwindow"),
      m_processor(proc),
      m_tid(tid),
      m_callbackNative(func),
      m_callbackFFmpeg(nullptr),
      m_onHide(onHide) {
    traceScope();
    initAsyncFunctors();
    setBounds(x, y, 100, 100);
    logln("creating processor window for " << m_processor->getName() << "(channel=" << proc->getActiveWindowChannel()
                                           << ") at " << x << "x" << y);
    if (m_processor->hasEditor()) {
        createEditor();
    }
}

ProcessorWindow::ProcessorWindow(std::shared_ptr<Processor> proc, Thread::ThreadID tid, CaptureCallbackFFmpeg func,
                                 std::function<void()> onHide, int x, int y)
    : DocumentWindow(proc->getName(), Colours::lightgrey, DocumentWindow::closeButton),
      LogTag("procwindow"),
      m_processor(proc),
      m_tid(tid),
      m_callbackNative(nullptr),
      m_callbackFFmpeg(func),
      m_onHide(onHide) {
    traceScope();
    initAsyncFunctors();
    setBounds(x, y, 100, 100);
    logln("creating processor window for " << m_processor->getName() << " (channel=" << proc->getActiveWindowChannel()
                                           << ") at " << x << "x" << y);
    if (m_processor->hasEditor()) {
        createEditor();
    }
}

ProcessorWindow::~ProcessorWindow() {
    traceScope();
    logln("destroying processor window for " << m_processor->getName()
                                             << " (channel=" << m_processor->getActiveWindowChannel() << ")");
    stopAsyncFunctors();
    stopCapturing();
    if (m_editor != nullptr) {
        delete m_editor;
        m_editor = nullptr;
    } else if (m_processor->isClient()) {
        m_processor->hideEditor();
    }
    m_processor->setLastPosition(getPosition());
}

void ProcessorWindow::closeButtonPressed() {
    getApp()->hideEditor(m_tid);
    if (nullptr != m_onHide) {
        m_onHide();
    }
}

void ProcessorWindow::forgetEditor() {
    traceScope();
    // Allow a processor to delete his editor, so we should not delete it again
    m_editor = nullptr;
    stopCapturing();
}

juce::Rectangle<int> ProcessorWindow::getScreenCaptureRect() {
    traceScope();
    if (nullptr != m_processor) {
        bool fs = m_processor->isFullscreen();
        auto rect = fs ? m_totalRect : m_processor->getScreenBounds();
        if (!fs && m_processor->getAdditionalScreenCapturingSpace() > 0) {
            rect.setSize(rect.getWidth() + m_processor->getAdditionalScreenCapturingSpace(),
                         rect.getHeight() + m_processor->getAdditionalScreenCapturingSpace());
            if (rect.getRight() > m_totalRect.getRight()) {
                rect.setRight(m_totalRect.getRight());
            }
            if (rect.getBottom() > m_totalRect.getBottom()) {
                rect.setBottom(m_totalRect.getBottom());
            }
        }
        return rect;
    } else {
        logln("getScreenCaptureRect failed: no processor");
    }
    traceln("m_editor=" << (uint64)m_editor << " m_processor=" << String::toHexString((uint64)m_processor.get()));
    return m_screenCaptureRect;
}

bool ProcessorWindow::isFullyVisible() const {
    return m_screenCaptureRect.getX() >= 0 && m_screenCaptureRect.getY() >= 0 &&
           m_screenCaptureRect.getRight() <= m_totalRect.getRight() &&
           m_screenCaptureRect.getBottom() <= m_totalRect.getBottom();
}

void ProcessorWindow::updateScreenCaptureArea() {
    traceScope();
    auto rect = getScreenCaptureRect();
    if (!rect.isEmpty()) {
        if (auto rec = ScreenRecorder::getInstance()) {
            if (rec->isRecording() && m_screenCaptureRect != rect &&
                (m_processor->isClient() || (m_processor->hasEditor() && nullptr != m_editor))) {
                traceln("updating area");
                m_screenCaptureRect = rect;
                rec->stop();

                if (isFullyVisible()) {
                    rec->resume(m_screenCaptureRect);
                } else {
                    if (auto onErr = getApp()->getWorkerErrorCallback(m_tid)) {
                        onErr("Screen capturing failed: The plugin window must be fully visible to be captured!");
                    }
                    logln("error: can't resume capturing when plugin window not fully visible");
                }
            }
        }
    } else {
        logln("error: can't update screen capture area with empty rect");
    }
}

void ProcessorWindow::startCapturing() {
    traceScope();
    if (auto srv = getApp()->getServer()) {
        if (!srv->getScreenCapturingOff()) {
            if (m_callbackNative) {
                startTimer(50);
            } else {
                m_screenCaptureRect = getScreenCaptureRect();
                if (!m_screenCaptureRect.isEmpty()) {
                    if (isFullyVisible()) {
                        if (auto rec = ScreenRecorder::getInstance()) {
                            if (rec->isRecording()) {
                                rec->stop();
                            }
                            rec->start(m_screenCaptureRect, m_callbackFFmpeg, [tid = m_tid](const String& err) {
                                if (auto onErr = getApp()->getWorkerErrorCallback(tid)) {
                                    onErr("Screen capturing failed: " + err);
                                }
                            });
                        } else {
                            logln("error: no screen recorder");
                        }
                    } else {
                        if (auto onErr = getApp()->getWorkerErrorCallback(m_tid)) {
                            onErr("Screen capturing failed: The plugin window must be fully visible to be captured!");
                        }
                        logln("error: can't start capturing when plugin window not fully visible");
                    }
                } else {
                    // when launching a plugin sandbox, it might take a little bit to ramp up the plugin editor, so we
                    // retry
                    bool retry = ++m_startCapturingRetry < 100;
                    logln("error: can't start screen capturing with empty rect ("
                          << (retry ? "retrying in 100ms" : "giving up") << ")");
                    if (retry) {
                        Timer::callAfterDelay(100, safeLambda([this] { startCapturing(); }));
                    }
                }
            }
        }
    }
}

void ProcessorWindow::stopCapturing() {
    traceScope();
    if (m_callbackNative) {
        stopTimer();
    } else {
        if (auto rec = ScreenRecorder::getInstance()) {
            rec->stop();
        }
    }
}

void ProcessorWindow::resized() {
    traceScope();
    DocumentWindow::resized();
    updateScreenCaptureArea();
}

void ProcessorWindow::setVisible(bool b) {
    traceScope();
    if (!b) {
        stopCapturing();
        if (m_processor->isClient()) {
            m_processor->hideEditor();
        }
    }
    bool wasVisible = false;
    if (!m_processor->isClient()) {
        wasVisible = isVisible();
        Component::setVisible(b);
    }
    if (b && !wasVisible) {
        if (m_processor->isClient()) {
            m_processor->showEditor(getX(), getY());
        } else {
            windowToFront(this);
        }
        m_startCapturingRetry = 0;
        logln("starting to capture from set visible");
        startCapturing();
    }
    m_isShowing = b;
}

void ProcessorWindow::move(int x, int y) {
    setBounds(x, y, getWidth(), getHeight());
    if (m_processor->isClient()) {
        m_processor->showEditor(x, y);
    }
}

void ProcessorWindow::toTop() {
    if (m_processor->isClient()) {
        m_processor->showEditor(getX(), getY());
    } else {
        windowToFront(this);
    }
}

void ProcessorWindow::captureWindow() {
    traceScope();
    if (m_editor == nullptr || m_processor->isClient()) {
        traceln("no editor");
        return;
    }
    if (m_callbackNative) {
        m_screenCaptureRect = getScreenCaptureRect();
        m_callbackNative(captureScreenNative(m_screenCaptureRect), m_screenCaptureRect.getWidth(),
                         m_screenCaptureRect.getHeight());
    } else {
        traceln("no callback");
    }
}

void ProcessorWindow::createEditor() {
    traceScope();

    juce::Rectangle<int> userRect;

    if (auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
        m_totalRect = disp->totalArea;
        userRect = disp->userArea;
    }

    if (m_processor->isClient()) {
        m_processor->showEditor(getX(), getY());
        m_startCapturingRetry = 0;
        startCapturing();
    } else {
        m_editor = m_processor->createEditorIfNeeded();
        if (nullptr != m_editor) {
            setContentNonOwned(m_editor, true);
            bool slm = false;
            bool wot = false;
            if (auto srv = getApp()->getServer()) {
                slm = srv->getScreenLocalMode();
                wot = srv->getPluginWindowsOnTop();
            }
            if (slm) {
                setTopLeftPosition({getX(), getY()});
            } else {
                setTopLeftPosition(userRect.getTopLeft());
            }
            if (wot) {
                setAlwaysOnTop(true);
            }
        } else {
            logln("failed to create editor");
        }
    }
}

}  // namespace e47
