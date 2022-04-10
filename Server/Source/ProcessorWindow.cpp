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

ProcessorWindow::ProcessorWindow(std::shared_ptr<Processor> proc, CaptureCallbackNative func)
    : DocumentWindow(proc->getName(), Colours::lightgrey, DocumentWindow::closeButton),
      LogTag("procwindow"),
      m_processor(proc),
      m_callbackNative(func),
      m_callbackFFmpeg(nullptr) {
    traceScope();
    if (m_processor->hasEditor()) {
        createEditor();
    }
}

ProcessorWindow::ProcessorWindow(std::shared_ptr<Processor> proc, CaptureCallbackFFmpeg func)
    : DocumentWindow(proc->getName(), Colours::lightgrey, DocumentWindow::closeButton),
      LogTag("procwindow"),
      m_processor(proc),
      m_callbackNative(nullptr),
      m_callbackFFmpeg(func) {
    traceScope();
    logln("creating processor window: " << m_processor->getName());
    if (m_processor->hasEditor()) {
        createEditor();
    }
}

ProcessorWindow::~ProcessorWindow() {
    traceScope();
    logln("destroying processor window: " << m_processor->getName());
    stopCapturing();
    if (m_editor != nullptr) {
        delete m_editor;
        m_editor = nullptr;
    } else if (m_processor->isClient()) {
        m_processor->hideEditor();
    }
    m_processor->setLastPosition(getPosition());
}

void ProcessorWindow::closeButtonPressed() { getApp()->hideEditor(); }

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
        if (!fs) {
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
    }
    traceln("m_editor=" << (uint64)m_editor << " m_processor=" << String::toHexString((uint64)m_processor.get()));
    return m_screenCaptureRect;
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
                rec->resume(m_screenCaptureRect);
            }
        }
    } else {
        logln("error: can't update screen capture area with empty rect");
    }
}

void ProcessorWindow::startCapturing() {
    traceScope();
    if (!getApp()->getServer()->getScreenCapturingOff()) {
        if (m_callbackNative) {
            startTimer(50);
        } else {
            m_screenCaptureRect = getScreenCaptureRect();
            if (!m_screenCaptureRect.isEmpty()) {
                if (auto rec = ScreenRecorder::getInstance()) {
                    if (rec->isRecording()) {
                        rec->stop();
                    }
                    rec->start(m_screenCaptureRect, m_callbackFFmpeg);
                } else {
                    logln("error: no screen recorder");
                }
            } else {
                logln("error: can't start screen capturing with empty rect");
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
    }
    if (!m_processor->isClient()) {
        Component::setVisible(b);
    }
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
    auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (nullptr != disp) {
        m_totalRect = disp->totalArea;
    }

    bool success = true;

    if (m_processor->isClient()) {
        auto p = m_processor->getLastPosition();
        m_processor->showEditor(p.x, p.y);
    } else {
        m_editor = m_processor->createEditorIfNeeded();
        if (nullptr != m_editor) {
            setContentNonOwned(m_editor, true);
            if (getApp()->getServer()->getScreenCapturingOff()) {
                setTopLeftPosition(m_processor->getLastPosition());
            }
            Component::setVisible(true);
            if (getApp()->getServer()->getPluginWindowsOnTop()) {
                setAlwaysOnTop(true);
            } else {
                windowToFront(this);
            }
        } else {
            logln("failed to create editor");
            success = false;
        }
    }

    if (success) {
        startCapturing();
    }
}

}  // namespace e47
