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

namespace e47 {

class App;

#define OLD_LAYOUT 0

class ServerSettingsWindow : public DocumentWindow, public LogTag {
  public:
    explicit ServerSettingsWindow(App* app);
    ~ServerSettingsWindow() override;

    void closeButtonPressed() override;

  private:
    App* m_app;

    TextButton m_saveButton;

#if OLD_LAYOUT
    std::vector<std::unique_ptr<Component>> m_components;
    TextEditor m_idText, m_nameText, m_screenJpgQuality, m_vst2Folders, m_vst3Folders, m_lv2Folders,
        m_screenMouseOffsetXY;
    ToggleButton m_auSupport, m_vst3Support, m_vst2Support, m_lv2Support, m_screenDiffDetection, m_scanForPlugins,
        m_tracer, m_logger, m_vstNoStandardFolders, m_pluginWindowsOnTop, m_crashReporting;
    Label m_screenJpgQualityLbl, m_screenDiffDetectionLbl, m_screenCapturingQualityLbl, m_pluginWindowsOnTopLbl;
    ComboBox m_screenCapturingMode, m_screenCapturingQuality, m_sandboxMode;
    TooltipWindow m_tooltipWindow;
#else
    TabbedComponent m_tabbedComponent;
    MainTab m_mainTab;
    PluginFormatsTab m_pluginFormatsTab;
    ScreenCapturingTab m_screenCapturingTab;
    StartupTab m_startupTab;
    DiagnosticsTab m_diagnosticsTab;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServerSettingsWindow)
};

}  // namespace e47

#endif /* ServerSettingsWindow_hpp */
