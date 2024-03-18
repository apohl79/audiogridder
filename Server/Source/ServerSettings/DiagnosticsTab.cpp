/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "DiagnosticsTab.hpp"

namespace e47 {

DiagnosticsTab::DiagnosticsTab()
{
}

void DiagnosticsTab::paint (Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void DiagnosticsTab::resized()
{

}

}  // namespace e47