/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "MainTab.hpp"

namespace e47 {

MainTab::MainTab(MainSettings mainSettings)
{
    int row = 0;

    m_nameLabel.setText("Server Name:", NotificationType::dontSendNotification);
    m_nameLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_nameLabel);

    m_nameText.setText(mainSettings.name);
    m_nameText.setBounds(getWideFieldBounds(row));
    addAndMakeVisible(m_nameText);

    row++;

    m_idLabel.setText("Server ID:", NotificationType::dontSendNotification);
    m_idLabel.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_idLabel);

    String idStr(mainSettings.id);
    m_idTextLabel.setText(idStr, NotificationType::dontSendNotification);
    m_idTextLabel.setBounds(getFieldBounds(row));
    m_idTextLabel.setJustificationType(Justification::right);
    addAndMakeVisible(m_idTextLabel);

    row++;

    String tooltip;
    tooltip << "Chain Isolation: Each AG plugin chain created by an AG plugin will run in a dedicated process."
            << newLine << newLine
            << "Plugin Isolation: Each plugin loaded into an AG plugin chain will run in a dedicated process.";
    m_sandboxLabel.setText("Sandbox Mode:", NotificationType::dontSendNotification);
    m_sandboxLabel.setBounds(getLabelBounds(row));
    m_sandboxLabel.setTooltip(tooltip);
    addAndMakeVisible(m_sandboxLabel);

    m_sandboxMode.setBounds(getWideFieldBounds(row));
    m_sandboxMode.addItem("Disabled", 1);
    m_sandboxMode.addItem("Chain Isolation", 2);
    m_sandboxMode.addItem("Plugin Isolation", 3);
    m_sandboxMode.setSelectedItemIndex(mainSettings.mode);
    m_sandboxMode.setTooltip(tooltip);
    addAndMakeVisible(m_sandboxMode);
}

void MainTab::paint(Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void MainTab::resized() {}

}  // namespace e47