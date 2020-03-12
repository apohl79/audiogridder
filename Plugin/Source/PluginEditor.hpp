/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginButton.hpp"
#include "PluginProcessor.hpp"

class AudioGridderAudioProcessorEditor : public AudioProcessorEditor, public PluginButton::Listener {
  public:
    AudioGridderAudioProcessorEditor(AudioGridderAudioProcessor&);
    ~AudioGridderAudioProcessorEditor();

    void paint(Graphics&) override;
    void resized() override;
    virtual void buttonClicked(Button* button, const ModifierKeys& modifiers) override;
    virtual void focusOfChildComponentChanged(FocusChangeType cause) override;

    virtual void mouseUp(const MouseEvent& event) override;  // server icon

    void setConnected(bool connected);

  private:
    AudioGridderAudioProcessor& m_processor;

    std::vector<std::unique_ptr<PluginButton>> m_pluginButtons;
    PluginButton m_newPluginButton;
    ImageComponent m_pluginScreen;
    ImageComponent m_srvIcon;
    Label m_srvLabel;
    bool m_connected = false;

    Button* addPluginButton(const String& id, const String& name);
    std::vector<Button*> getPluginButtons(const String& id);
    int getPluginIndex(const String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGridderAudioProcessorEditor)
};
