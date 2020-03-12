/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginListWindow.hpp"
#include "App.hpp"
#include "Server.hpp"

namespace e47 {

PluginListWindow::PluginListWindow(App* app, KnownPluginList& list, const String& deadMansPedalFile)
    : DocumentWindow("Available Plugins",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      m_app(app),
      m_pluginlist(list),
      m_deadMansPedalFile(deadMansPedalFile) {
    m_plugmgr.addDefaultFormats();
    setContentOwned(new AudioGridderPluginListComponent(m_plugmgr, m_pluginlist, m_app->getServer().getExcludeList(),
                                                        m_deadMansPedalFile, nullptr, false),
                    true);

    setResizable(true, false);
    centreWithSize(700, 600);

    setVisible(true);
}

void PluginListWindow::closeButtonPressed() { m_app->hidePluginList(); }

}  // namespace e47
