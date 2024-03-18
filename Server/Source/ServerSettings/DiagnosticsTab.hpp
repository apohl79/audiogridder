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

class DiagnosticsTab : public juce::Component
{
  public:
    DiagnosticsTab();
    void paint (Graphics& g) override;
    void resized() override;
    bool getTracerEnabled() { return m_tracer.getToggleState(); }
    bool getLoggerEnabled() { return m_logger.getToggleState(); }
    bool getCrashReportingEnabled() { return m_crashReporting.getToggleState(); }
  private:
    ToggleButton m_tracer, m_logger, m_crashReporting;
};

}  // namespace e47
