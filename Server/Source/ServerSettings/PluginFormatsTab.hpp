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

class PluginFormatsTab : public juce::Component
{
  public:
    PluginFormatsTab();
    void paint (Graphics& g) override;
    void resized() override;
    bool getAuSupport() { return m_auSupport.getToggleState(); }
    bool getVst3Support() { return m_vst3Support.getToggleState(); }
    bool getVst2Support() { return m_vst2Support.getToggleState(); }
    bool getLv2Support() { return m_lv2Support.getToggleState(); }
    bool getVstNoStandardFolders() { return m_vstNoStandardFolders.getToggleState(); }
    String getVst2FoldersText() { return m_vst2Folders.getText(); }
    String getVst3FoldersText() { return m_vst3Folders.getText(); }
    String getLv2FoldersText() { return m_lv2Folders.getText(); }
  private:
    ToggleButton m_auSupport, m_vst2Support, m_vst3Support, m_lv2Support, m_vstNoStandardFolders;
    TextEditor m_vst2Folders, m_vst3Folders, m_lv2Folders;
};

}  // namespace e47