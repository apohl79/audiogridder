/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
 */

#include "IOTab.hpp"

namespace e47 {

IOTab::IOTab(IOSettings ioSettings) : m_deviceSelector(ioSettings)
{
    addAndMakeVisible(m_deviceSelector);
}

void IOTab::paint(Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void IOTab::resized()
{
    auto bounds = getLocalBounds();
    m_deviceSelector.setBounds(bounds);
}

}  // namespace e47