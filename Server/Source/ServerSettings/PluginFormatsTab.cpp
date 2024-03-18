/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "PluginFormatsTab.hpp"

namespace e47 {

PluginFormatsTab::PluginFormatsTab()
{
}

void PluginFormatsTab::paint (Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void PluginFormatsTab::resized()
{

}

}  // namespace e47