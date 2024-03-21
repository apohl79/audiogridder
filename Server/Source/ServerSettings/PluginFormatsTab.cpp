/*
 * Copyright (c) 2024 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Kieran Coulter
 */

#include "PluginFormatsTab.hpp"

namespace e47 {

PluginFormatsTab::PluginFormatsTab(FormatSettings formatSettings) {
    int row = 0;
    String tmpStr;

#ifdef JUCE_MAC
    m_auLabel.setText("AudioUnit Support:", NotificationType::dontSendNotification);
    m_auLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_auLabel);

    m_auSupport.setBounds(getCheckBoxBounds(row));
    m_auSupport.setToggleState(formatSettings.au, NotificationType::dontSendNotification);
    addAndMakeVisible(m_auSupport);

    row++;
#endif

    m_vst3Label.setText("VST3 Support:", NotificationType::dontSendNotification);
    m_vst3Label.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_vst3Label);

    m_vst3Support.setBounds(getCheckBoxBounds(row));
    m_vst3Support.setToggleState(formatSettings.vst3, NotificationType::dontSendNotification);
    addAndMakeVisible(m_vst3Support);

    row++;

    tmpStr = "VST3 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    m_vst3CustomLabel.setText(tmpStr, NotificationType::dontSendNotification);
    m_vst3CustomLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_vst3CustomLabel);

    m_vst3Folders.setBounds(getLargeFieldBounds(row));
    m_vst3Folders.setMultiLine(true, false);
    m_vst3Folders.setReturnKeyStartsNewLine(true);
    addAndMakeVisible(m_vst3Folders);

    tmpStr = "";
    for (auto& folder : formatSettings.vst3Folders) {
        tmpStr << folder << newLine;
    }
    m_vst3Folders.setText(tmpStr);

    row += largeFieldRows;

    m_vst2Label.setText("VST2 Support:", NotificationType::dontSendNotification);
    m_vst2Label.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_vst2Label);

    m_vst2Support.setBounds(getCheckBoxBounds(row));
    m_vst2Support.setToggleState(formatSettings.vst2, NotificationType::dontSendNotification);
    addAndMakeVisible(m_vst2Support);

    row++;

    tmpStr = "VST2 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    m_vst2CustomLabel.setText(tmpStr, NotificationType::dontSendNotification);
    m_vst2CustomLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_vst2CustomLabel);

    m_vst2Folders.setBounds(getLargeFieldBounds(row));
    m_vst2Folders.setMultiLine(true, false);
    m_vst2Folders.setReturnKeyStartsNewLine(true);
    addChildAndSetID(&m_vst2Folders, "vst2fold");

    tmpStr = "";
    for (auto& folder : formatSettings.vst2Folders) {
        tmpStr << folder << newLine;
    }
    m_vst2Folders.setText(tmpStr);

    row += largeFieldRows;

    String tooltip = "If you select this, only custom folders will be scanned.";
    m_vst2CustomOnlyLabel.setText("Do not include VST standard folders:", NotificationType::dontSendNotification);
    m_vst2CustomOnlyLabel.setBounds(getLabelBounds(row));
    m_vst2CustomOnlyLabel.setTooltip(tooltip);
    addAndMakeVisible(m_vst2CustomOnlyLabel);

    m_vstNoStandardFolders.setBounds(getCheckBoxBounds(row));
    m_vstNoStandardFolders.setToggleState(formatSettings.vst2NoStandard, NotificationType::dontSendNotification);
    m_vstNoStandardFolders.setTooltip(tooltip);
    addChildAndSetID(&m_vstNoStandardFolders, "vstnostandarddirs");

    row++;

    m_lv2Label.setText("LV2 Support:", NotificationType::dontSendNotification);
    m_lv2Label.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_lv2Label);

    m_lv2Support.setBounds(getCheckBoxBounds(row));
    m_lv2Support.setToggleState(formatSettings.lv2, NotificationType::dontSendNotification);
    addAndMakeVisible(m_lv2Support);

    row++;

    tmpStr = "LV2 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    m_lv2CustomLabel.setText(tmpStr, NotificationType::dontSendNotification);
    m_lv2CustomLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_lv2CustomLabel);

    m_lv2Folders.setBounds(getLargeFieldBounds(row));
    m_lv2Folders.setMultiLine(true, false);
    m_lv2Folders.setReturnKeyStartsNewLine(true);
    addChildAndSetID(&m_lv2Folders, "lv2fold");

    tmpStr = "";
    for (auto& folder : formatSettings.lv2Folders) {
        tmpStr << folder << newLine;
    }
    m_lv2Folders.setText(tmpStr);
}

void PluginFormatsTab::paint(Graphics& g) {
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void PluginFormatsTab::resized() {}

}  // namespace e47