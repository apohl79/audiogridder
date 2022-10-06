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
#include "WindowHelper.hpp"

namespace e47 {

class PluginEditor : public AudioProcessorEditor,
                     public PluginButton::Listener,
                     public Button::Listener,
                     public LogTagDelegate {
  public:
    PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    void paint(Graphics&) override;
    void resized() override;
    void buttonClicked(Button* button, const ModifierKeys& modifiers, PluginButton::AreaType area) override;
    void buttonClicked(Button* button) override;
    void focusOfChildComponentChanged(FocusChangeType cause) override;

    void mouseUp(const MouseEvent& event) override;

    void setConnected(bool connected);
    void setCPULoad(float load);
    void updateState();

    void updateParamValue(int paramIdx);
    void updatePluginStatus(int idx, bool ok, const String& err);
    void hidePluginFromServer(int idx);

  private:
    PluginProcessor& m_processor;

    const int SCREENTOOLS_HEIGHT = 17;
    const int SCREENTOOLS_MARGIN = 3;
    const int SCREENTOOLS_AB_WIDTH = 12;
    const int SCREENTOOLS_CHANNEL_WIDTH = 35;

    const int PLUGINSCREEN_DEFAULT_W = 250;
    const int PLUGINSCREEN_DEFAULT_H = 100;

    std::vector<std::unique_ptr<PluginButton>> m_pluginButtons;
    PluginButton m_newPluginButton;
    ImageComponent m_pluginScreen;
    bool m_pluginScreenEmpty = true;
    std::atomic_bool m_wantsScreenUpdates{false};
    GenericEditor m_genericEditor;
    Viewport m_genericEditorView;
    ImageComponent m_srvIcon, m_settingsIcon, m_cpuIcon;
    Label m_srvLabel, m_versionLabel, m_cpuLabel;
    ImageComponent m_logo;
    TooltipWindow m_tooltipWindow;
    bool m_connected = false;

    struct ToolsButton : TextButton {
        void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    };

    // tools
    ToolsButton m_toolsButtonPlus, m_toolsButtonMinus, m_toolsButtonFullscreen, m_toolsButtonOnOff;
    TextButton m_toolsButtonA, m_toolsButtonB, m_toolsButtonChannel;
    int m_currentActiveAB = -1;
    std::set<TextButton*> m_hilightedToolsButtons;

    void createPluginButtons();
    PluginButton* addPluginButton(const String& id, const String& name);
    std::vector<PluginButton*> getPluginButtons(const String& id);
    int getPluginIndex(const String& name);

    void getPresetsMenu(PopupMenu& menu, const File& dir);

    void initToolsButtons();
    void enableToolsButton(TextButton* b);
    void disableToolsButton(TextButton* b);
    void hilightToolsButton(TextButton* b);
    void unhilightToolsButton(TextButton* b);
    bool isHilightedToolsButton(TextButton* b);
    void updateToolsOnOffButton();

    void editPlugin(int idx = -1, int channel = -1);
    void hidePlugin(int idx = -1);

    void resetPluginScreen();
    void setPluginScreen(const Image& img, int w, int h);
    bool genericEditorEnabled() const;

    void highlightPluginButton(int idx);
    void unhighlightPluginButton(int idx);

    void showServerMenu();
    void showSettingsMenu();

    Point<int> getLocalModePosition(juce::Rectangle<int> bounds = {});

    struct PositionTracker : Timer, LogTagDelegate {
        PluginEditor* e;
        juce::Rectangle<int> r;

        PositionTracker(PluginEditor* e_) : LogTagDelegate(e_), e(e_), r(WindowHelper::getWindowScreenBounds(e)) {
            logln("starting position tracker");
            startTimer(100);
        }

        void timerCallback() override {
            auto active = e->m_processor.getActivePlugin();
            auto bounds = WindowHelper::getWindowScreenBounds(e);
            if (bounds.isEmpty()) {
                bounds = e->getScreenBounds();
            }
            if (active > -1 && r != bounds) {
                r = bounds;
                auto p = e->getLocalModePosition(r);
                logln("updating editor position to " << p.x << "x" << p.y);
                e->m_processor.editPlugin(active, e->m_processor.getActivePluginChannel(), p.x, p.y);
            }
        }
    };

    std::unique_ptr<PositionTracker> m_positionTracker;

    ENABLE_ASYNC_FUNCTORS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

}  // namespace e47
