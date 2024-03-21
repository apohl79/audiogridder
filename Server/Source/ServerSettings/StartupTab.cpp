/*
 * Copyright (c) 2024 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Kieran Coulter
 */

#include "StartupTab.hpp"
#include "Defaults.hpp"

namespace e47 {

StartupTab::StartupTab(bool scanForPlugins) {
    String tooltip;
    int row = 0;

    tooltip << "Enter the IDs of servers that you want to start automatically. An ID must be a number in the range of "
               "0-31. Example: 0,1,4-8"
            << newLine << newLine << "Note: You have to restart manually for taking changes into effect.";

    m_autoStartLbl.setText("Autostart servers with IDs:", NotificationType::dontSendNotification);
    m_autoStartLbl.setTooltip(tooltip);
    m_autoStartLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_autoStartLbl);

    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigServerStartup));
    if (jsonHasValue(cfg, "IDs")) {
        m_idText.setText(jsonGetValue(cfg, "IDs", String()));
    }
    m_idText.setInputFilter(new TextEditor::LengthAndCharacterRestriction(103, "0123456789-,"), true);
    m_idText.setBounds(getWideFieldBounds(row));
    m_idText.setTooltip(tooltip);
    addAndMakeVisible(m_idText);

    row++;

    m_scanForPluginsLbl.setText("Scan for Plugins at Startup:", NotificationType::dontSendNotification);
    m_scanForPluginsLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_scanForPluginsLbl);

    m_scanForPlugins.setBounds(getCheckBoxBounds(row));
    m_scanForPlugins.setToggleState(scanForPlugins, NotificationType::dontSendNotification);
    addAndMakeVisible(m_scanForPlugins);
}

void StartupTab::paint(Graphics& g) {
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void StartupTab::resized() {}

}  // namespace e47