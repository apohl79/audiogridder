/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "PluginListWindow.hpp"
#include "Server.hpp"
#include "WindowPositions.hpp"

namespace e47 {

PluginListWindow::PluginListWindow(App* app, KnownPluginList& list, const String& deadMansPedalFile)
    : DocumentWindow("Available Plugins",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      m_app(app),
      m_pluginlist(list),
      m_deadMansPedalFile(deadMansPedalFile) {
    setUsingNativeTitleBar(true);
    m_plugmgr.addDefaultFormats();
    setContentOwned(new AudioGridderPluginListComponent(m_plugmgr, m_pluginlist, m_app->getServer().getExcludeList(),
                                                        m_deadMansPedalFile),
                    true);

    setResizable(true, false);
    centreWithSize(700, 600);
    setBounds(WindowPositions::get(WindowPositions::ServerPlugins, getBounds()));

    setVisible(true);
    windowToFront(this);
}

PluginListWindow::~PluginListWindow() {
    WindowPositions::set(WindowPositions::ServerPlugins, getBounds());
    clearContentComponent();
}

void PluginListWindow::closeButtonPressed() { m_app->hidePluginList(); }

}  // namespace e47
