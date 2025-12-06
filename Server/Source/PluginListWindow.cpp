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
#include <juce_audio_processors/format_types/juce_VSTPluginFormat.h>

namespace e47 {

PluginListWindow::PluginListWindow(App* app, KnownPluginList& list, const String& deadMansPedalFile)
    : DocumentWindow("Available Plugins",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      m_app(app),
      m_pluginlist(list),
      m_deadMansPedalFile(deadMansPedalFile) {
    if (auto srv = m_app->getServer()) {
        setUsingNativeTitleBar(true);

        // See <JUCE_module_dir>/modules/juce_audio_processors/format_types
        // m_plugmgr.addFormat (new VSTPluginFormat()); // This fails/not found
        m_plugmgr.addFormat (new VST3PluginFormat());
        m_plugmgr.addFormat (new AudioUnitPluginFormat());
        // m_plugmgr.addFormat (new AAXPluginFormat()); // This fails/not found

        setContentOwned(new PluginListComponent(m_plugmgr, m_pluginlist, srv->getExcludeList(), m_deadMansPedalFile),
                        true);

        setResizable(true, false);
        centreWithSize(700, 600);
        setBounds(WindowPositions::get(WindowPositions::ServerPlugins, getBounds()));

        setVisible(true);
        windowToFront(this);
    }
}

PluginListWindow::~PluginListWindow() {
    WindowPositions::set(WindowPositions::ServerPlugins, getBounds());
    clearContentComponent();
}

void PluginListWindow::closeButtonPressed() { m_app->hidePluginList(); }

}  // namespace e47
