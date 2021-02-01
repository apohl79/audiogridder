/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include <JuceHeader.h>
#include "PluginButton.hpp"
#include "PluginProcessor.hpp"
#include "GenericEditor.hpp"
#include "StatisticsWindow.hpp"
#include "Utils.hpp"

namespace e47 {

class AudioGridderAudioProcessorEditor : public AudioProcessorEditor,
                                         public PluginButton::Listener,
                                         public Button::Listener,
                                         public LogTagDelegate {
  public:
    AudioGridderAudioProcessorEditor(AudioGridderAudioProcessor&);
    ~AudioGridderAudioProcessorEditor() override;

    void paint(Graphics&) override;
    void resized() override;
    void buttonClicked(Button* button, const ModifierKeys& modifiers, PluginButton::AreaType area) override;
    void buttonClicked(Button* button) override;
    void focusOfChildComponentChanged(FocusChangeType cause) override;

    void mouseUp(const MouseEvent& event) override;

    void setConnected(bool connected);
    void setCPULoad(float load);

  private:
    AudioGridderAudioProcessor& m_processor;

    const int SCREENTOOLS_HEIGHT = 17;
    const int SCREENTOOLS_MARGIN = 3;
    const int SCREENTOOLS_AB_WIDTH = 12;

    std::vector<std::unique_ptr<PluginButton>> m_pluginButtons;
    PluginButton m_newPluginButton;
    ImageComponent m_pluginScreen;
    GenericEditor m_genericEditor;
    Viewport m_genericEditorView;
    ImageComponent m_srvIcon, m_settingsIcon, m_cpuIcon;
    Label m_srvLabel, m_versionLabel, m_cpuLabel;
    ImageComponent m_logo;
    bool m_connected = false;

    struct ToolsButton : TextButton {
        void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    };

    // screen tools
    ToolsButton m_stPlus, m_stMinus, m_stFullscreen;
    TextButton m_stA, m_stB;
    int m_currentActiveAB = -1;
    TextButton* m_hilightedStButton = nullptr;

    PluginButton* addPluginButton(const String& id, const String& name);
    std::vector<PluginButton*> getPluginButtons(const String& id);
    int getPluginIndex(const String& name);

    void initStButtons();
    void enableStButton(TextButton* b);
    void disableStButton(TextButton* b);
    void hilightStButton(TextButton* b);
    bool isHilightedStButton(TextButton* b);

    void editPlugin(int idx = -1);

    ENABLE_ASYNC_FUNCTORS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGridderAudioProcessorEditor)
};

}  // namespace e47
