/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "StartupTab.hpp"

namespace e47 {

StartupTab::StartupTab()
{
}

void StartupTab::paint (Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void StartupTab::resized()
{

}

}  // namespace e47