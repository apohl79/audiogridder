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

    void updateParamValue(int paramIdx);

  private:
    PluginProcessor& m_processor;

    const int SCREENTOOLS_HEIGHT = 17;
    const int SCREENTOOLS_MARGIN = 3;
    const int SCREENTOOLS_AB_WIDTH = 12;

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
    bool m_connected = false;

    struct ToolsButton : TextButton {
        void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    };

    // screen tools
    ToolsButton m_stPlus, m_stMinus, m_stFullscreen;
    TextButton m_stA, m_stB;
    int m_currentActiveAB = -1;
    TextButton* m_hilightedStButton = nullptr;

    void createPluginButtons();
    PluginButton* addPluginButton(const String& id, const String& name);
    std::vector<PluginButton*> getPluginButtons(const String& id);
    int getPluginIndex(const String& name);

    void getPresetsMenu(PopupMenu& menu, const File& dir);

    void initStButtons();
    void enableStButton(TextButton* b);
    void disableStButton(TextButton* b);
    void hilightStButton(TextButton* b);
    bool isHilightedStButton(TextButton* b);

    void editPlugin(int idx = -1);
    void hidePlugin(int idx = -1);

    void resetPluginScreen();
    void setPluginScreen(const Image& img, int w, int h);
    bool genericEditorEnabled() const;

    void highlightPluginButton(int idx);
    void unhighlightPluginButton(int idx);

    struct PositionTracker : Timer, LogTagDelegate {
        PluginEditor* e;
        int x, y;

        PositionTracker(PluginEditor* e_) : LogTagDelegate(e_), e(e_), x(e->getScreenX()), y(e->getScreenY()) {
            logln("starting position tracker");
            startTimer(100);
        }

        void timerCallback() override {
            auto active = e->m_processor.getActivePlugin();
            if (active > -1 && (x != e->getScreenX() || y != e->getScreenY())) {
                x = e->getScreenX();
                y = e->getScreenY();
                if (active > -1) {
                    logln("updating editor position to " << x << "x" << y);
                    e->m_processor.editPlugin(active, x + e->getWidth() + 10, y);
                }
            }
        }
    };

    std::unique_ptr<PositionTracker> m_positionTracker;

    ENABLE_ASYNC_FUNCTORS();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

}  // namespace e47
