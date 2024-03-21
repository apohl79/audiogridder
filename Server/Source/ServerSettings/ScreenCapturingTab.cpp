/*
 * Copyright (c) 2024 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Kieran Coulter
 */

#include "ScreenCapturingTab.hpp"
#include "ScreenRecorder.hpp"

namespace e47 {

ScreenCapturingTab::ScreenCapturingTab(CaptureSettings captureSettings) {
    String tooltip;
    int row = 0;

    tooltip << "FFmpeg: Use FFmpeg for screen capturing. This is recommended as it gives best quality at lowest "
               "bandwidth costs."
            << newLine << newLine;
    tooltip << "Legacy: This mode takes screenshots every 50ms. Use this only if FFmpeg does not work for you."
            << newLine << newLine;
    tooltip << "Disabled (Local Mode): If you run AG server and your DAW on the same computer you should enable this "
               "mode. It positions the plugin windows next to the AG plugin window and allows you to open multiple "
               "plugin windows at the same time."
            << newLine << newLine;
    tooltip << "Disabled: No screen capturing.";
    m_screenCapturingModeLbl.setText("Screen Capturing Mode:", NotificationType::dontSendNotification);
    m_screenCapturingModeLbl.setBounds(getLabelBounds(row));
    m_screenCapturingModeLbl.setTooltip(tooltip);
    addAndMakeVisible(m_screenCapturingModeLbl);

    m_screenCapturingMode.setBounds(getWideFieldBounds(row));
    m_screenCapturingMode.setTooltip(tooltip);
    m_screenCapturingMode.addItem("FFmpeg", 1);
    // m_screenCapturingMode.addItem("FFmpeg (mjpeg)", 2);
    m_screenCapturingMode.addItem("Legacy", 3);
    m_screenCapturingMode.addItem("Disabled (Local Mode)", 4);
    m_screenCapturingMode.addItem("Disabled", 5);
    int mode = 1;
    if (captureSettings.capOff) {
        if (captureSettings.localMode) {
            mode = 4;
        } else {
            mode = 5;
        }
    } else if (!captureSettings.capFFmpeg) {
        mode = 3;
    }
    // else {
    //    switch (srv->getScreenCapturingFFmpegEncoder()) {
    //        case ScreenRecorder::WEBP:
    //            mode = 1;
    //            break;
    //        case ScreenRecorder::MJPEG:
    //            mode = 2;
    //            break;
    //    }
    //}
    m_screenCapturingMode.setSelectedId(mode, NotificationType::dontSendNotification);
    m_screenCapturingMode.onChange = [this] {
        switch (m_screenCapturingMode.getSelectedId()) {
            case 1:
            case 2:
                m_screenCapturingQualityLbl.setAlpha(1);
                m_screenCapturingQuality.setAlpha(1);
                m_screenCapturingQuality.setEnabled(true);

                m_pluginWindowsOnTopLbl.setAlpha(0.5);
                m_pluginWindowsOnTop.setEnabled(false);
                m_pluginWindowsOnTop.setAlpha(0.5);

                m_screenDiffDetectionLbl.setAlpha(0.5);
                m_screenDiffDetection.setEnabled(false);
                m_screenDiffDetection.setAlpha(0.5);

                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
                break;
            case 3:
                m_screenDiffDetectionLbl.setAlpha(1);
                m_screenDiffDetection.setEnabled(true);
                m_screenDiffDetection.setAlpha(1);

                m_screenJpgQualityLbl.setAlpha(1);
                m_screenJpgQuality.setEnabled(true);
                m_screenJpgQuality.setAlpha(1);

                m_screenCapturingQualityLbl.setAlpha(0.5);
                m_screenCapturingQuality.setAlpha(0.5);
                m_screenCapturingQuality.setEnabled(false);

                m_pluginWindowsOnTopLbl.setAlpha(0.5);
                m_pluginWindowsOnTop.setEnabled(false);
                m_pluginWindowsOnTop.setAlpha(0.5);

                if (nullptr != m_screenDiffDetection.onClick) {
                    m_screenDiffDetection.onClick();
                }
                break;
            case 4:
            case 5:
                m_screenCapturingQualityLbl.setAlpha(0.5);
                m_screenCapturingQuality.setAlpha(0.5);
                m_screenCapturingQuality.setEnabled(false);

                m_pluginWindowsOnTopLbl.setAlpha(1);
                m_pluginWindowsOnTop.setEnabled(true);
                m_pluginWindowsOnTop.setAlpha(1);

                m_screenDiffDetectionLbl.setAlpha(0.5);
                m_screenDiffDetection.setEnabled(false);
                m_screenDiffDetection.setAlpha(0.5);

                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
                break;
        }
    };
    m_screenCapturingMode.onChange();
    addAndMakeVisible(m_screenCapturingMode);

    tooltip.clear();
    row++;

    m_screenCapturingQualityLbl.setText("Screen Capturing Quality:", NotificationType::dontSendNotification);
    m_screenCapturingQualityLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_screenCapturingQualityLbl);

    m_screenCapturingQuality.setBounds(getWideFieldBounds(row));
    m_screenCapturingQuality.addItem("High", ScreenRecorder::ENC_QUALITY_HIGH + 1);
    m_screenCapturingQuality.addItem("Medium", ScreenRecorder::ENC_QUALITY_MEDIUM + 1);
    m_screenCapturingQuality.addItem("Low", ScreenRecorder::ENC_QUALITY_LOW + 1);
    m_screenCapturingQuality.setSelectedId(captureSettings.FFmpegQuality + 1);
    addAndMakeVisible(m_screenCapturingQuality);

    row++;

    m_screenDiffDetectionLbl.setText("Legacy Diff Detection:", NotificationType::dontSendNotification);
    m_screenDiffDetectionLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_screenDiffDetectionLbl);

    m_screenDiffDetection.setBounds(getCheckBoxBounds(row));
    m_screenDiffDetection.setToggleState(captureSettings.diffDetect, NotificationType::dontSendNotification);
    m_screenDiffDetection.onClick = [this] {
        if (m_screenCapturingMode.getSelectedId() == 2) {
            if (m_screenDiffDetection.getToggleState()) {
                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
            } else {
                m_screenJpgQualityLbl.setAlpha(1);
                m_screenJpgQuality.setEnabled(true);
                m_screenJpgQuality.setAlpha(1);
            }
        }
    };
    m_screenDiffDetection.onClick();
    addAndMakeVisible(m_screenDiffDetection);

    row++;

    m_screenJpgQualityLbl.setText("Legacy Quality (0.1-1.0):", NotificationType::dontSendNotification);
    m_screenJpgQualityLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_screenJpgQualityLbl);

    String q;
    q << captureSettings.screenQuality;
    m_screenJpgQuality.setText(q);
    m_screenJpgQuality.setBounds(getFieldBounds(row));
    addAndMakeVisible(m_screenJpgQuality);

    row++;

    m_pluginWindowsOnTopLbl.setText("Keep Plugin Windows on Top:", NotificationType::dontSendNotification);
    m_pluginWindowsOnTopLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_pluginWindowsOnTopLbl);

    m_pluginWindowsOnTop.setToggleState(captureSettings.winOnTop, NotificationType::dontSendNotification);
    m_pluginWindowsOnTop.setBounds(getCheckBoxBounds(row));
    addAndMakeVisible(m_pluginWindowsOnTop);

    row++;

    m_screenMouseOffsetXYLbl.setText("Mouse Offset Correction:", NotificationType::dontSendNotification);
    m_screenMouseOffsetXYLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_screenMouseOffsetXYLbl);

    m_screenMouseOffsetXY.setBounds(getWideFieldBounds(row));
    m_screenMouseOffsetXY.setText(String(captureSettings.offsetX) + "x" + String(captureSettings.offsetY));
    m_screenMouseOffsetXY.setInputFilter(new TextEditor::LengthAndCharacterRestriction(11, "0123456789x-,"), true);
    addAndMakeVisible(m_screenMouseOffsetXY);
}

void ScreenCapturingTab::paint(Graphics& g) {
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void ScreenCapturingTab::resized() {}

}  // namespace e47