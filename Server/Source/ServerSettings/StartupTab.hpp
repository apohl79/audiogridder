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

class StartupTab : public juce::Component
{
  public:
    StartupTab(bool scanForPlugins);
    void paint (Graphics& g) override;
    void resized() override;
    bool getScanForPlugins() { return m_scanForPlugins.getToggleState(); }
  private:
    ToggleButton m_scanForPlugins;
    TextEditor m_idText;
    Label m_autoStartLbl, m_scanForPluginsLbl;
};

}  // namespace e47
