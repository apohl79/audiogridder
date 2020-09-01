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
#include "Utils.hpp"

class AudioGridderAudioProcessorEditor : public AudioProcessorEditor,
                                         public PluginButton::Listener,
                                         public Button::Listener {
  public:
    AudioGridderAudioProcessorEditor(AudioGridderAudioProcessor&);
    ~AudioGridderAudioProcessorEditor() override;

    void paint(Graphics&) override;
    void resized() override;
    void buttonClicked(Button* button, const ModifierKeys& modifiers) override;
    void buttonClicked(Button* button) override;
    void focusOfChildComponentChanged(FocusChangeType cause) override;

    void mouseUp(const MouseEvent& event) override;  // server icon

    void setConnected(bool connected);

  private:
    AudioGridderAudioProcessor& m_processor;

    const int SCREENTOOLS_HEIGHT = 17;
    const int SCREENTOOLS_MARGIN = 3;

    std::vector<std::unique_ptr<PluginButton>> m_pluginButtons;
    PluginButton m_newPluginButton;
    ImageComponent m_pluginScreen;
    ImageComponent m_srvIcon;
    Label m_srvLabel, m_versionLabel;
    bool m_connected = false;

    // screen tools
    TextButton m_stPlus, m_stMinus;

    Button* addPluginButton(const String& id, const String& name);
    std::vector<Button*> getPluginButtons(const String& id);
    int getPluginIndex(const String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGridderAudioProcessorEditor)
};
