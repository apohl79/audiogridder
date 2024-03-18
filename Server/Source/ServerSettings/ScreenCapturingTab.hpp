/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#pragma once

#include <JuceHeader.h>
#include "TabCommon.h"

namespace e47 {

class ScreenCapturingTab : public juce::Component
{
  public:
    ScreenCapturingTab();
    void paint (Graphics& g) override;
    void resized() override;
    int getModeSelectedId() { return m_screenCapturingMode.getSelectedId(); }
    int getQualitySelectedId() { return m_screenCapturingQuality.getSelectedId(); }
    bool getWindowsOnTopEnabled() { return m_pluginWindowsOnTop.getToggleState(); }
    bool getDiffDetectionEnabled() { return m_screenDiffDetection.getToggleState(); }
    String getJpgQualityText() { return m_screenJpgQuality.getText(); }
    String getMouseOffsetXYText() { return m_screenMouseOffsetXY.getText(); }
  private:
    ComboBox m_screenCapturingMode, m_screenCapturingQuality;
    ToggleButton m_pluginWindowsOnTop, m_screenDiffDetection;
    TextEditor m_screenJpgQuality, m_screenMouseOffsetXY;
};

}  // namespace e47
