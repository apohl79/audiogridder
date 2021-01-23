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

namespace e47 {

class App;

class ServerSettingsWindow : public DocumentWindow, public LogTag {
  public:
    ServerSettingsWindow(App* app);
    ~ServerSettingsWindow() override;

    void closeButtonPressed() override;

  private:
    App* m_app;
    std::vector<std::unique_ptr<Component>> m_components;
    TextEditor m_idText, m_nameText, m_screenJpgQuality, m_vst2Folders, m_vst3Folders;
    ToggleButton m_auSupport, m_vst3Support, m_vst2Support, m_screenDiffDetection, m_scanForPlugins, m_tracer, m_logger,
        m_vstNoStandardFolders, m_parallelPluginLoad;
    TextButton m_saveButton;
    Label m_screenJpgQualityLbl, m_screenDiffDetectionLbl, m_screenCapturingQualityLbl;
    ComboBox m_screenCapturingMode, m_screenCapturingQuality;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServerSettingsWindow)
};

}  // namespace e47

#endif /* ServerSettingsWindow_hpp */
