/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#pragma once

#include <JuceHeader.h>
#include "TabCommon.h"
#include "Utils.hpp"

namespace e47 {

class MainTab : public juce::Component {
  public:
    MainTab(MainSettings mainSettings);
    void paint(Graphics& g) override;
    void resized() override;
    String getNameText() { return m_nameText.getText(); }
    String getIdText() { return m_idTextLabel.getText(); }
    int getSandboxSelectedIndex() { return m_sandboxMode.getSelectedItemIndex(); }

  private:
    Label m_nameLabel, m_idLabel, m_sandboxLabel, m_idTextLabel;
    TextEditor m_nameText;
    ComboBox m_sandboxMode;
};

}  // namespace e47
