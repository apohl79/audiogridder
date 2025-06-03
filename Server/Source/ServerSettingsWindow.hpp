/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ServerSettingsWindow_hpp
#define ServerSettingsWindow_hpp

#include <JuceHeader.h>

#include "Utils.hpp"
#include "ServerSettings/MainTab.hpp"
#include "ServerSettings/PluginFormatsTab.hpp"
#include "ServerSettings/ScreenCapturingTab.hpp"
#include "ServerSettings/StartupTab.hpp"
#include "ServerSettings/DiagnosticsTab.hpp"
#include "ServerSettings/IOTab.hpp"

namespace e47 {

class App;

class ServerSettingsWindow : public DocumentWindow, public LogTag {
  public:
    explicit ServerSettingsWindow(App* app);
    ~ServerSettingsWindow() override;

    void closeButtonPressed() override;

  private:
    App* m_app;

    TextButton m_saveButton;

    TabbedComponent m_tabbedComponent;
    MainTab m_mainTab;
    PluginFormatsTab m_pluginFormatsTab;
    ScreenCapturingTab m_screenCapturingTab;
    StartupTab m_startupTab;
    DiagnosticsTab m_diagnosticsTab;
    IOTab m_ioTab;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServerSettingsWindow)
};

}  // namespace e47

#endif /* ServerSettingsWindow_hpp */
