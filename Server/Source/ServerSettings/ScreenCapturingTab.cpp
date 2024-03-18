/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "ScreenCapturingTab.hpp"

namespace e47 {

ScreenCapturingTab::ScreenCapturingTab()
{
}

void ScreenCapturingTab::paint (Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void ScreenCapturingTab::resized()
{

}

}  // namespace e47