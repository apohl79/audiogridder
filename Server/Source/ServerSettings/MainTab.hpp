/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#pragma once

#include <JuceHeader.h>
#include "TabCommon.h"

class MainTab : public juce::Component
{
  public:
    MainTab();
    void paint (Graphics& g) override;
    void resized() override;
    String getNameText() { return m_nameText.getText(); }
    String getIdText() { return m_idText.getText(); }
    int getSandboxSelectedIndex() { return m_sandboxMode.getSelectedItemIndex(); }
  private:
    TextEditor m_nameText, m_idText;
    ComboBox m_sandboxMode;
};


