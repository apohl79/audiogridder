/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ServerSettingsWindow_hpp
#define ServerSettingsWindow_hpp

#include "../JuceLibraryCode/JuceHeader.h"

namespace e47 {

class App;

class ServerSettingsWindow : public DocumentWindow {
  public:
    ServerSettingsWindow(App* app);
    ~ServerSettingsWindow() override { clearContentComponent(); }

    void closeButtonPressed() override;

  private:
    App* m_app;
    std::vector<std::unique_ptr<Component>> m_components;
    TextEditor m_idText, m_screenJpgQuality;
    ToggleButton m_auSupport, m_vstSupport, m_vst2Support, m_screenDiffDetection;
    TextButton m_saveButton;
    Label m_screenJpgQualityLbl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServerSettingsWindow)
};

}  // namespace e47

#endif /* ServerSettingsWindow_hpp */
