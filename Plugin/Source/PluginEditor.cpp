/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginEditor.hpp"
#include "Images.hpp"
#include "NewServerWindow.hpp"
#include "PluginProcessor.hpp"
#include "NumberConversion.hpp"
#include "Version.hpp"
#include "PluginMonitor.hpp"
#include "PluginSearchWindow.hpp"

namespace e47 {

AudioGridderAudioProcessorEditor::AudioGridderAudioProcessorEditor(AudioGridderAudioProcessor& p)
    : AudioProcessorEditor(&p), m_processor(p), m_newPluginButton("", "newPlug", false), m_genericEditor(p) {
    setLogTagSource(&m_processor.getClient());
    traceScope();
    initAsyncFunctors();
    logln("creating editor");

    auto& lf = getLookAndFeel();
    lf.setUsingNativeAlertWindows(true);
    lf.setColour(ResizableWindow::backgroundColourId, Colour(Defaults::BG_COLOR));
    lf.setColour(PopupMenu::backgroundColourId, Colour(Defaults::BG_COLOR));
    lf.setColour(TextEditor::backgroundColourId, Colour(Defaults::BUTTON_COLOR));
    lf.setColour(TextButton::buttonColourId, Colour(Defaults::BUTTON_COLOR));
    lf.setColour(ComboBox::backgroundColourId, Colour(Defaults::BUTTON_COLOR));
    lf.setColour(PopupMenu::highlightedBackgroundColourId, Colour(Defaults::ACTIVE_COLOR).withAlpha(0.05f));
    lf.setColour(Slider::thumbColourId, Colour(Defaults::SLIDERTHUMB_COLOR));
    lf.setColour(Slider::trackColourId, Colour(Defaults::SLIDERTRACK_COLOR));
    lf.setColour(Slider::backgroundColourId, Colour(Defaults::SLIDERBG_COLOR));
    if (auto lfv4 = dynamic_cast<LookAndFeel_V4*>(&lf)) {
        lfv4->getCurrentColourScheme().setUIColour(LookAndFeel_V4::ColourScheme::widgetBackground,
                                                   Colour(Defaults::BG_COLOR));
    }

    addAndMakeVisible(m_srvIcon);
    m_srvIcon.setImage(ImageCache::getFromMemory(Images::server_png, Images::server_pngSize));
    m_srvIcon.setAlpha(0.5);
    m_srvIcon.setBounds(5, 5, 20, 20);
    m_srvIcon.addMouseListener(this, true);

    addAndMakeVisible(m_settingsIcon);
    m_settingsIcon.setImage(ImageCache::getFromMemory(Images::settings_png, Images::settings_pngSize));
    m_settingsIcon.setAlpha(0.5);
    m_settingsIcon.setBounds(175, 5, 20, 20);
    m_settingsIcon.addMouseListener(this, true);

    addAndMakeVisible(m_srvLabel);
    m_srvLabel.setText("not connected", NotificationType::dontSendNotification);

    m_srvLabel.setBounds(30, 5, 140, 20);
    auto font = m_srvLabel.getFont();
    font.setHeight(font.getHeight() - 2);
    m_srvLabel.setFont(font);

    addAndMakeVisible(m_logo);
    m_logo.setImage(ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
    m_logo.setBounds(0, 89, 16, 16);
    m_logo.setAlpha(0.3f);

    addAndMakeVisible(m_versionLabel);
    String v = "";
    v << AUDIOGRIDDER_VERSION;
#if JucePlugin_IsSynth
    v << " (inst)";
#elif JucePlugin_IsMidiEffect
    v << " (midi)";
#else
    v << " (fx)";
#endif
    m_versionLabel.setText(v, NotificationType::dontSendNotification);
    m_versionLabel.setBounds(16, 89, 190, 10);
    m_versionLabel.setFont(Font(10, Font::plain));
    m_versionLabel.setAlpha(0.4f);

    addAndMakeVisible(m_cpuIcon);
    m_cpuIcon.setImage(ImageCache::getFromMemory(Images::cpu_png, Images::cpu_pngSize));
    m_cpuIcon.setBounds(200 - 45, 89, 16, 16);
    m_cpuIcon.setAlpha(0.6f);

    addAndMakeVisible(m_newPluginButton);
    m_newPluginButton.setButtonText("+");
    m_newPluginButton.setOnClickWithModListener(this);

    addAndMakeVisible(m_cpuLabel);
    m_cpuLabel.setBounds(200 - 45 + 16 - 2, 89, 50, 10);
    m_cpuLabel.setFont(Font(10, Font::plain));
    m_cpuLabel.setAlpha(0.6f);

    addChildComponent(m_pluginScreen);
    m_pluginScreen.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, 1, 1);
    m_pluginScreen.setWantsKeyboardFocus(true);
    m_pluginScreen.addMouseListener(&m_processor.getClient(), true);
    m_pluginScreen.addKeyListener(&m_processor.getClient());
    m_pluginScreen.setVisible(false);

    addChildComponent(m_genericEditorView);
    m_genericEditorView.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, 100, 200);
    m_genericEditor.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, 100, 200);
    m_genericEditorView.setViewedComponent(&m_genericEditor, false);
    m_genericEditorView.setVisible(false);

    ;
    for (int idx = 0; idx < m_processor.getNumOfLoadedPlugins(); idx++) {
        auto& plug = m_processor.getLoadedPlugin(idx);
        if (plug.id.isNotEmpty()) {
            auto* b = addPluginButton(plug.id, plug.name);
            if (!plug.ok) {
                b->setEnabled(false);
            }
            if (plug.bypassed) {
                b->setButtonText("( " + m_processor.getLoadedPlugin(idx).name + " )");
                b->setColour(PluginButton::textColourOffId, Colours::grey);
            }
#if JucePlugin_IsSynth
            m_newPluginButton.setEnabled(false);
#endif
        }
    }

    auto active = m_processor.getActivePlugin();
    if (active > -1) {
        m_pluginButtons[as<size_t>(active)]->setActive(true);
    }

    m_stFullscreen.setButtonText("fs");
    m_stFullscreen.setBounds(201, 1, 1, 1);
    m_stFullscreen.setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
    m_stFullscreen.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight | Button::ConnectedOnTop |
                                     Button::ConnectedOnBottom);
    m_stFullscreen.addListener(this);
    addAndMakeVisible(&m_stFullscreen);

    m_stPlus.setButtonText("+");
    m_stPlus.setBounds(201, 1, 1, 1);
    m_stPlus.setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
    m_stPlus.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight | Button::ConnectedOnTop |
                               Button::ConnectedOnBottom);
    m_stPlus.addListener(this);
    addAndMakeVisible(&m_stPlus);

    m_stMinus.setButtonText("-");
    m_stMinus.setBounds(201, 1, 1, 1);
    m_stMinus.setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
    m_stMinus.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight | Button::ConnectedOnTop |
                                Button::ConnectedOnBottom);
    m_stMinus.addListener(this);
    addAndMakeVisible(&m_stMinus);

    m_stA.setButtonText("A");
    m_stA.setBounds(201, 1, 1, 1);
    m_stA.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight | Button::ConnectedOnTop |
                            Button::ConnectedOnBottom);
    m_stA.addListener(this);
    addAndMakeVisible(&m_stA);

    m_stB.setButtonText("B");
    m_stB.setBounds(201, 1, 1, 1);
    m_stB.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight | Button::ConnectedOnTop |
                            Button::ConnectedOnBottom);
    m_stB.addListener(this);
    addAndMakeVisible(&m_stB);

    initStButtons();

    setSize(200, 100);

    logln("  setting connected state");
    setConnected(m_processor.getClient().isReadyLockFree());
    setCPULoad(m_processor.getClient().getCPULoad());
    logln("editor created");
}

AudioGridderAudioProcessorEditor::~AudioGridderAudioProcessorEditor() {
    traceScope();
    stopAsyncFunctors();
    logln("destroying editor");
    m_processor.hidePlugin();
    m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
    logln("editor destroyed");
}

void AudioGridderAudioProcessorEditor::paint(Graphics& g) {
    traceScope();
    FillType ft;
    auto colBG = getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    auto tp = m_processor.getTrackProperties();
    if (!tp.colour.isTransparent()) {
        auto gradient = ColourGradient::horizontal(colBG.interpolatedWith(tp.colour, 0.05f), 0, colBG, 100);
        g.setGradientFill(gradient);
        g.fillAll();
        g.setColour(tp.colour);
        g.fillRect(0, 0, 2, getHeight());
    } else {
        g.fillAll(colBG);
    }
}

void AudioGridderAudioProcessorEditor::ToolsButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted,
                                                                bool shouldDrawButtonAsDown) {
    auto& lf = getLookAndFeel();
    lf.drawButtonBackground(g, *this, findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                            shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    Path p;
    if (getButtonText() == "+") {
        p.addLineSegment(Line<int>(3, getHeight() / 2 + 1, getWidth() - 2, getHeight() / 2 + 1).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(getWidth() / 2 + 1, 3, getWidth() / 2 + 1, getHeight() - 2).toFloat(), 1.5f);
    } else if (getButtonText() == "-") {
        p.addLineSegment(Line<int>(2, getHeight() / 2 + 1, getWidth() - 2, getHeight() / 2 + 1).toFloat(), 1.5f);
    } else if (getButtonText() == "fs") {
        p.addLineSegment(Line<int>(2, 2, 6, 2).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(2, 2, 2, 6).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(getWidth() - 2, 2, getWidth() - 6, 2).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(getWidth() - 2, 2, getWidth() - 2, 6).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(2, getHeight() - 2, 6, getHeight() - 2).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(2, getHeight() - 2, 2, getHeight() - 6).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(getWidth() - 2, getHeight() - 2, getWidth() - 6, getHeight() - 2).toFloat(), 1.5f);
        p.addLineSegment(Line<int>(getWidth() - 2, getHeight() - 2, getWidth() - 2, getHeight() - 6).toFloat(), 1.5f);
    }
    g.setColour(Colours::white.withAlpha(0.8f));
    g.fillPath(p);
}

void AudioGridderAudioProcessorEditor::resized() {
    traceScope();
    int buttonWidth = 196;
    int buttonHeight = 20;
    int logoHeight = m_logo.getHeight();
    int num = 0;
    int top = 30;
    for (auto& b : m_pluginButtons) {
        b->setBounds(2, top, buttonWidth, buttonHeight);
        top += buttonHeight + 2;
        num++;
    }
    m_newPluginButton.setBounds(2, top, buttonWidth, buttonHeight);
    top += buttonHeight + logoHeight + 6;
    int windowHeight = jmax(100, top);
    int leftBarWidth = 200;
    int windowWidth = leftBarWidth;
    if (m_processor.getActivePlugin() != -1) {
        if (m_processor.getGenericEditor()) {
            m_genericEditorView.setVisible(true);
            m_pluginScreen.setVisible(false);
            m_stMinus.setVisible(false);
            m_stPlus.setVisible(false);
            int screenHeight = m_genericEditor.getHeight() + SCREENTOOLS_HEIGHT;
            bool showScrollBar = false;
            if (screenHeight > 600) {
                screenHeight = 600;
                showScrollBar = true;
            }
            m_genericEditorView.setSize(m_genericEditor.getWidth(), screenHeight - SCREENTOOLS_HEIGHT);
            m_genericEditorView.setScrollBarsShown(showScrollBar, false);
            windowHeight = jmax(windowHeight, screenHeight);
            windowWidth += m_genericEditor.getWidth();
        } else {
            m_genericEditorView.setVisible(false);
            m_pluginScreen.setVisible(true);
            m_stMinus.setVisible(true);
            m_stPlus.setVisible(true);
            int screenHeight = m_pluginScreen.getHeight() + SCREENTOOLS_HEIGHT + 5;
            windowHeight = jmax(windowHeight, screenHeight);
            windowWidth += m_pluginScreen.getWidth();
            m_stMinus.setBounds(windowWidth - SCREENTOOLS_HEIGHT - SCREENTOOLS_MARGIN * 2, SCREENTOOLS_MARGIN,
                                SCREENTOOLS_HEIGHT, SCREENTOOLS_HEIGHT);
            m_stPlus.setBounds(windowWidth - SCREENTOOLS_HEIGHT * 2 - SCREENTOOLS_MARGIN * 3, SCREENTOOLS_MARGIN,
                               SCREENTOOLS_HEIGHT, SCREENTOOLS_HEIGHT);
            m_stFullscreen.setBounds(windowWidth - SCREENTOOLS_HEIGHT * 3 - SCREENTOOLS_MARGIN * 4, SCREENTOOLS_MARGIN,
                                     SCREENTOOLS_HEIGHT, SCREENTOOLS_HEIGHT);
        }
        m_stA.setBounds(leftBarWidth + SCREENTOOLS_MARGIN, SCREENTOOLS_MARGIN, SCREENTOOLS_AB_WIDTH,
                        SCREENTOOLS_HEIGHT);
        m_stB.setBounds(leftBarWidth + SCREENTOOLS_MARGIN + SCREENTOOLS_AB_WIDTH, SCREENTOOLS_MARGIN,
                        SCREENTOOLS_AB_WIDTH, SCREENTOOLS_HEIGHT);
        if (m_currentActiveAB != m_processor.getActivePlugin()) {
            initStButtons();
        }
    }
    if (getWidth() != windowWidth || getHeight() != windowHeight) {
        setSize(windowWidth, windowHeight);
    }
    m_logo.setBounds(4, windowHeight - logoHeight - 4, m_logo.getWidth(), m_logo.getHeight());
    m_versionLabel.setBounds(logoHeight + 3, windowHeight - 15, m_versionLabel.getWidth(), m_versionLabel.getHeight());
    m_cpuIcon.setBounds(200 - 45, windowHeight - logoHeight - 3, m_cpuIcon.getWidth(), m_cpuIcon.getHeight());
    m_cpuLabel.setBounds(200 - 45 + logoHeight - 2, windowHeight - 15, m_cpuLabel.getWidth(), m_cpuLabel.getHeight());
}

void AudioGridderAudioProcessorEditor::buttonClicked(Button* button, const ModifierKeys& modifiers,
                                                     PluginButton::AreaType area) {
    traceScope();
    if (!button->getName().compare("newPlug")) {
        auto addFn = [this](const ServerPlugin& plug) {
            traceScope();
            String err;
            if (m_processor.loadPlugin(plug.getId(), plug.getName(), err)) {
                addPluginButton(plug.getId(), plug.getName());
#if JucePlugin_IsSynth
                m_newPluginButton.setEnabled(false);
#endif
                resized();
            } else {
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error",
                                                 "Failed to add " + plug.getName() + " plugin!\n\nError: " + err, "OK");
            }
        };

        auto bounds = button->getScreenBounds().toFloat();
        auto searchWin = new PluginSearchWindow(bounds.getX(), bounds.getBottom(), m_processor);
        searchWin->onClick([this, addFn](ServerPlugin plugin) {
            traceScope();
            addFn(plugin);
        });
        searchWin->runModalLoop();
    } else {
        int idx = getPluginIndex(button->getName());
        int active = m_processor.getActivePlugin();
        auto editFn = [this, idx] { editPlugin(idx); };
        auto hideFn = [this, idx](int i = -1) {
            traceScope();
            m_processor.hidePlugin();
            size_t index = i > -1 ? (size_t)i : (size_t)idx;
            m_pluginButtons[index]->setActive(false);
            if (m_processor.isBypassed((int)index)) {
                m_pluginButtons[index]->setColour(PluginButton::textColourOffId, Colours::grey);
            } else {
                m_pluginButtons[index]->setColour(PluginButton::textColourOffId, Colours::white);
            }
            resized();
        };
        auto bypassFn = [this, idx, button] {
            traceScope();
            m_processor.bypassPlugin(idx);
            button->setButtonText("( " + m_processor.getLoadedPlugin(idx).name + " )");
            button->setColour(PluginButton::textColourOffId, Colours::grey);
        };
        auto unBypassFn = [this, idx, active, button] {
            traceScope();
            m_processor.unbypassPlugin(idx);
            button->setButtonText(m_processor.getLoadedPlugin(idx).name);
            if (idx == active) {
                button->setColour(PluginButton::textColourOffId, Colour(Defaults::ACTIVE_COLOR));
            } else {
                button->setColour(PluginButton::textColourOffId, Colours::white);
            }
        };
        auto moveUpFn = [this, idx] {
            traceScope();
            if (idx > 0) {
                m_processor.exchangePlugins(idx, idx - 1);
                std::swap(m_pluginButtons[as<size_t>(idx)], m_pluginButtons[as<size_t>(idx) - 1]);
                resized();
            }
        };
        auto moveDownFn = [this, idx] {
            traceScope();
            if (as<size_t>(idx) < m_pluginButtons.size() - 1) {
                m_processor.exchangePlugins(idx, idx + 1);
                std::swap(m_pluginButtons[as<size_t>(idx)], m_pluginButtons[as<size_t>(idx) + 1]);
                resized();
            }
        };
        auto deleteFn = [this, idx] {
            traceScope();
            if (!m_processor.getConfirmDelete() ||
                AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Delete",
                                             "Are you sure to delete >" + m_processor.getLoadedPlugin(idx).name + "< ?",
                                             "Yes", "No")) {
                m_processor.unloadPlugin(idx);
                int i = 0;
                for (auto it = m_pluginButtons.begin(); it < m_pluginButtons.end(); it++) {
                    if (i++ == idx) {
                        m_pluginButtons.erase(it);
                        break;
                    }
                }
#if JucePlugin_IsSynth
                m_newPluginButton.setEnabled(true);
#endif
                resized();
            }
        };
        if (modifiers.isLeftButtonDown()) {
            switch (area) {
                case PluginButton::MAIN: {
                    bool edit = idx != active;
                    if (active > -1) {
                        hideFn(active);
                    }
                    if (edit) {
                        editFn();
                    }
                    break;
                }
                case PluginButton::BYPASS:
                    if (m_processor.isBypassed(idx)) {
                        unBypassFn();
                    } else {
                        bypassFn();
                    }
                    break;
                case PluginButton::MOVE_DOWN:
                    moveDownFn();
                    break;
                case PluginButton::MOVE_UP:
                    moveUpFn();
                    break;
                case PluginButton::DELETE:
                    deleteFn();
                    break;
                default:
                    break;
            }
        } else {
            PopupMenu m;
            if (m_processor.isBypassed(idx)) {
                m.addItem("Unbypass", unBypassFn);
            } else {
                m.addItem("Bypass", bypassFn);
            }
            m.addItem("Edit", editFn);
            m.addItem("Hide", idx == m_processor.getActivePlugin(), false, hideFn);
            m.addSeparator();
            m.addItem("Move Up", idx > 0, false, moveUpFn);
            m.addItem("Move Down", as<size_t>(idx) < m_pluginButtons.size() - 1, false, moveDownFn);
            m.addSeparator();
            m.addItem("Delete", deleteFn);
            m.addSeparator();
            PopupMenu presets;
            int preset = 0;
            for (auto& p : m_processor.getLoadedPlugin(idx).presets) {
                presets.addItem(p, [this, idx, preset] {
                    traceScope();
                    m_processor.getClient().setPreset(idx, preset);
                });
                preset++;
            }
            m.addSubMenu("Presets", presets);
            m.addSeparator();
            PopupMenu params;
            for (auto& p : m_processor.getLoadedPlugin(idx).params) {
                int paramIdx = p.idx;
                String name = p.name;
                bool enabled = false;
                if (p.automationSlot > -1) {
                    name << " -> [" << p.automationSlot << "]";
                    enabled = true;
                }
                params.addItem(name, true, enabled, [this, idx, paramIdx, enabled] {
                    traceScope();
                    if (enabled) {
                        m_processor.disableParamAutomation(idx, paramIdx);
                    } else {
                        m_processor.enableParamAutomation(idx, paramIdx);
                    }
                });
            }
            m.addSubMenu("Automation", params);
            m.showAt(button);
        }
    }
}

void AudioGridderAudioProcessorEditor::buttonClicked(Button* button) {
    traceScope();
    TextButton* tb = reinterpret_cast<TextButton*>(button);
    if (tb == &m_stPlus) {
        m_processor.increaseSCArea();
    } else if (tb == &m_stMinus) {
        m_processor.decreaseSCArea();
    } else if (tb == &m_stFullscreen) {
        m_processor.toggleFullscreenSCArea();
    } else if (tb == &m_stA || tb == &m_stB) {
        m_currentActiveAB = m_processor.getActivePlugin();
        if (isHilightedStButton(&m_stB)) {
            m_processor.storeSettingsB();
            m_processor.restoreSettingsA();
            hilightStButton(&m_stA);
            enableStButton(&m_stB);
        } else {
            m_processor.storeSettingsA();
            m_processor.restoreSettingsB();
            hilightStButton(&m_stB);
            enableStButton(&m_stA);
        }
    }
}

PluginButton* AudioGridderAudioProcessorEditor::addPluginButton(const String& id, const String& name) {
    traceScope();
    int num = 0;
    for (auto& plug : m_pluginButtons) {
        if (!id.compare(plug->getPluginId()) || !name.compare(plug->getButtonText())) {
            num++;
        }
    }
    String suffix;
    if (num > 0) {
        suffix << " (" << num + 1 << ")";
    }
    auto but = std::make_unique<PluginButton>(id, name + suffix);
    auto* ret = but.get();
    but->setOnClickWithModListener(this);
    addAndMakeVisible(*but);
    m_pluginButtons.push_back(std::move(but));
    return ret;
}

std::vector<PluginButton*> AudioGridderAudioProcessorEditor::getPluginButtons(const String& id) {
    traceScope();
    std::vector<PluginButton*> ret;
    for (auto& b : m_pluginButtons) {
        if (b->getPluginId() == id) {
            ret.push_back(b.get());
        }
    }
    return ret;
}

int AudioGridderAudioProcessorEditor::getPluginIndex(const String& name) {
    traceScope();
    int idx = 0;
    for (auto& plug : m_pluginButtons) {
        if (!name.compare(plug->getName())) {
            return idx;
        }
        idx++;
    }
    return -1;
}

void AudioGridderAudioProcessorEditor::focusOfChildComponentChanged(FocusChangeType /* cause */) {
    traceScope();
    if (hasKeyboardFocus(true)) {
        // reactivate the plugin screen
        int active = m_processor.getActivePlugin();
        if (active > -1) {
            m_processor.editPlugin(active);
        }
    }
}

void AudioGridderAudioProcessorEditor::setConnected(bool connected) {
    traceScope();
    m_connected = connected;
    if (connected) {
        String srvTxt = m_processor.getActiveServerName();
        srvTxt << " (+" << m_processor.getLatencyMillis() << "ms)";
        m_srvLabel.setText(srvTxt, NotificationType::dontSendNotification);
        for (size_t i = 0; i < m_pluginButtons.size(); i++) {
            m_pluginButtons[i]->setEnabled(m_processor.getLoadedPlugin((int)i).ok);
        }
    } else {
        m_srvLabel.setText("not connected", NotificationType::dontSendNotification);
        setCPULoad(0.0f);
        for (auto& but : m_pluginButtons) {
            but->setEnabled(false);
        }
    }
}

void AudioGridderAudioProcessorEditor::setCPULoad(float load) {
    traceScope();
    m_cpuLabel.setText(String(lround(load)) + "%", NotificationType::dontSendNotification);
    uint32 col;
    if (!m_connected) {
        col = Colours::white.getARGB();
    } else if (load < 50.0f) {
        col = Defaults::CPU_LOW_COLOR;
    } else if (load < 90.0f) {
        col = Defaults::CPU_MEDIUM_COLOR;
    } else {
        col = Defaults::CPU_HIGH_COLOR;
    }
    m_cpuLabel.setColour(Label::textColourId, Colour(col));
}

void AudioGridderAudioProcessorEditor::mouseUp(const MouseEvent& event) {
    traceScope();
    if (event.eventComponent == &m_srvIcon) {
        PopupMenu m;
        m.addSectionHeader("Buffering");
        PopupMenu bufMenu;
        int rate = as<int>(lround(m_processor.getSampleRate()));
        int iobuf = m_processor.getBlockSize();
        auto getName = [rate, iobuf](int blocks) -> String {
            String n;
            n << blocks << " Blocks (+" << blocks * iobuf * 1000 / rate << "ms)";
            return n;
        };
        bufMenu.addItem("Disabled (+0ms)", true, m_processor.getClient().NUM_OF_BUFFERS == 0, [this] {
            traceScope();
            m_processor.saveConfig(0);
        });
        bufMenu.addItem(getName(1), true, m_processor.getClient().NUM_OF_BUFFERS == 1, [this] {
            traceScope();
            m_processor.saveConfig(1);
        });
        bufMenu.addItem(getName(2), true, m_processor.getClient().NUM_OF_BUFFERS == 2, [this] {
            traceScope();
            m_processor.saveConfig(2);
        });
        bufMenu.addItem(getName(4), true, m_processor.getClient().NUM_OF_BUFFERS == 4, [this] {
            traceScope();
            m_processor.saveConfig(4);
        });
        bufMenu.addItem(getName(8), true, m_processor.getClient().NUM_OF_BUFFERS == 8, [this] {
            traceScope();
            m_processor.saveConfig(8);
        });
        bufMenu.addItem(getName(12), true, m_processor.getClient().NUM_OF_BUFFERS == 12, [this] {
            traceScope();
            m_processor.saveConfig(12);
        });
        bufMenu.addItem(getName(16), true, m_processor.getClient().NUM_OF_BUFFERS == 16, [this] {
            traceScope();
            m_processor.saveConfig(16);
        });
        bufMenu.addItem(getName(20), true, m_processor.getClient().NUM_OF_BUFFERS == 20, [this] {
            traceScope();
            m_processor.saveConfig(20);
        });
        bufMenu.addItem(getName(24), true, m_processor.getClient().NUM_OF_BUFFERS == 24, [this] {
            traceScope();
            m_processor.saveConfig(24);
        });
        bufMenu.addItem(getName(28), true, m_processor.getClient().NUM_OF_BUFFERS == 28, [this] {
            traceScope();
            m_processor.saveConfig(28);
        });
        bufMenu.addItem(getName(30), true, m_processor.getClient().NUM_OF_BUFFERS == 30, [this] {
            traceScope();
            m_processor.saveConfig(30);
        });
        m.addSubMenu("Buffer Size", bufMenu);
        m.addSectionHeader("Servers");
        auto& servers = m_processor.getServers();
        auto active = m_processor.getActiveServerHost();
        for (auto s : servers) {
            if (s == active) {
                PopupMenu srvMenu;
                srvMenu.addItem("Rescan", [this] {
                    traceScope();
                    m_processor.getClient().rescan();
                });
                srvMenu.addItem("Wipe Cache & Rescan", [this] {
                    traceScope();
                    m_processor.getClient().rescan(true);
                });
                srvMenu.addItem("Restart server", [this] {
                    traceScope();
                    m_processor.getClient().restart();
                });
                srvMenu.addItem("Reconnect", [this] {
                    traceScope();
                    m_processor.getClient().reconnect();
                });
                m.addSubMenu(s, srvMenu, true, nullptr, true, 0);
            } else {
                PopupMenu srvMenu;
                srvMenu.addItem("Connect", [this, s] {
                    traceScope();
                    m_processor.setActiveServer(s);
                    m_processor.saveConfig();
                });
                srvMenu.addItem("Remove", [this, s] {
                    traceScope();
                    m_processor.delServer(s);
                    m_processor.saveConfig();
                });
                m.addSubMenu(s, srvMenu);
            }
        }
        auto serversMDNS = m_processor.getServersMDNS();
        if (serversMDNS.size() > 0) {
            for (auto s : serversMDNS) {
                if (servers.contains(s.getHostAndID())) {
                    continue;
                }
                String name = s.getNameAndID();
                name << " [load: " << lround(s.getLoad()) << "%]";
                if (s.getHostAndID() == active) {
                    PopupMenu srvMenu;
                    srvMenu.addItem("Rescan", [this] {
                        traceScope();
                        m_processor.getClient().rescan();
                    });
                    srvMenu.addItem("Wipe Cache & Rescan", [this] {
                        traceScope();
                        m_processor.getClient().rescan(true);
                    });
                    srvMenu.addItem("Restart server", [this] {
                        traceScope();
                        m_processor.getClient().restart();
                    });
                    srvMenu.addItem("Reconnect", [this] {
                        traceScope();
                        m_processor.getClient().reconnect();
                    });
                    m.addSubMenu(name, srvMenu, true, nullptr, true, 0);
                } else {
                    PopupMenu srvMenu;
                    srvMenu.addItem("Connect", [this, s] {
                        traceScope();
                        m_processor.setActiveServer(s);
                        m_processor.saveConfig();
                    });
                    m.addSubMenu(name, srvMenu);
                }
            }
        }
        m.addSeparator();
        m.addItem("Add", [this] {
            traceScope();
            auto w = new NewServerWindow(as<float>(getScreenX() + 2), as<float>(getScreenY() + 30));
            w->onOk([this](String server) {
                traceScope();
                m_processor.addServer(server);
                m_processor.setActiveServer(server);
                m_processor.saveConfig();
            });
            w->setAlwaysOnTop(true);
            w->runModalLoop();
        });
        m.showAt(&m_srvIcon);
    } else if (event.eventComponent == &m_settingsIcon) {
        PopupMenu m;
        m.addItem("Generic Editor", true, m_processor.getGenericEditor(), [this] {
            traceScope();
            m_processor.setGenericEditor(!m_processor.getGenericEditor());
            m_processor.saveConfig();
            editPlugin();
        });

        m.addSeparator();

        PopupMenu subm;
        subm.addItem("Show Category", true, m_processor.getMenuShowCategory(), [this] {
            traceScope();
            m_processor.setMenuShowCategory(!m_processor.getMenuShowCategory());
            m_processor.saveConfig();
        });
        subm.addItem("Show Company", true, m_processor.getMenuShowCompany(), [this] {
            traceScope();
            m_processor.setMenuShowCompany(!m_processor.getMenuShowCompany());
            m_processor.saveConfig();
        });
        subm.addItem("Disable Server Filter", true, m_processor.getNoSrvPluginListFilter(), [this] {
            traceScope();
            m_processor.setNoSrvPluginListFilter(!m_processor.getNoSrvPluginListFilter());
            m_processor.saveConfig();
            m_processor.getClient().reconnect();
        });

        m.addSubMenu("Plugin Menu", subm);
        subm.clear();

        subm.addItem("Show...", true, false, [this] {
            traceScope();
            PluginMonitor::setAlwaysShow(true);
        });
        subm.addItem("Automatic", true, PluginMonitor::getAutoShow(), [this] {
            traceScope();
            PluginMonitor::setAutoShow(!PluginMonitor::getAutoShow());
            m_processor.saveConfig();
        });
        m.addSubMenu("Plugin Monitor", subm);
        subm.clear();

        float sf = Desktop::getInstance().getGlobalScaleFactor();
        auto updateZoom = [this, sf](float f) -> std::function<void()> {
            traceScope();
            return [this, sf, f] {
                if (f != sf) {
                    logln("updating scale factor to " << f);
                    Desktop::getInstance().setGlobalScaleFactor(f);
                    m_processor.setScaleFactor(f);
                    m_processor.saveConfig();
                }
            };
        };
        subm.addItem("50%", true, sf == 0.5f, updateZoom(0.5f));
        subm.addItem("75%", true, sf == 0.75f, updateZoom(0.75f));
        subm.addItem("100%", true, sf == 1.0f, updateZoom(1.0f));
        subm.addItem("125%", true, sf == 1.25f, updateZoom(1.25f));
        subm.addItem("150%", true, sf == 1.5f, updateZoom(1.5f));
        subm.addItem("175%", true, sf == 1.75f, updateZoom(1.75f));
        subm.addItem("200%", true, sf == 2.0f, updateZoom(2.0f));
        m.addSubMenu("Zoom", subm);
        subm.clear();

        m.addSeparator();

        m.addItem("Confirm Delete", true, m_processor.getConfirmDelete(), [this] {
            traceScope();
            m_processor.setConfirmDelete(!m_processor.getConfirmDelete());
            m_processor.saveConfig();
        });

        subm.addItem("Always (every 10s)", true,
                     m_processor.getSyncRemoteMode() == AudioGridderAudioProcessor::SYNC_ALWAYS, [this] {
                         m_processor.setSyncRemoteMode(AudioGridderAudioProcessor::SYNC_ALWAYS);
                         m_processor.saveConfig();
                     });
        subm.addItem("When an editor is active (every 10s)", true,
                     m_processor.getSyncRemoteMode() == AudioGridderAudioProcessor::SYNC_WITH_EDITOR, [this] {
                         m_processor.setSyncRemoteMode(AudioGridderAudioProcessor::SYNC_WITH_EDITOR);
                         m_processor.saveConfig();
                     });
        subm.addItem("When saving the project", true,
                     m_processor.getSyncRemoteMode() == AudioGridderAudioProcessor::SYNC_DISABLED, [this] {
                         m_processor.setSyncRemoteMode(AudioGridderAudioProcessor::SYNC_DISABLED);
                         m_processor.saveConfig();
                     });
        m.addSubMenu("Remote Sync Frequency", subm);
        subm.clear();

        m.addSeparator();

        subm.addItem("Logging", true, AGLogger::isEnabled(), [this] {
            traceScope();
            AGLogger::setEnabled(!AGLogger::isEnabled());
            m_processor.saveConfig();
        });
        subm.addItem("Tracing", true, Tracer::isEnabled(), [this] {
            traceScope();
            Tracer::setEnabled(!Tracer::isEnabled());
            m_processor.saveConfig();
        });
        m.addSubMenu("Diagnostics", subm);
        subm.clear();

        m.addItem("Show Statistics...", [this] {
            traceScope();
            StatisticsWindow::show();
        });
        m.showAt(&m_settingsIcon);
    }
}

void AudioGridderAudioProcessorEditor::initStButtons() {
    traceScope();
    enableStButton(&m_stA);
    disableStButton(&m_stB);
    m_processor.resetSettingsAB();
    m_hilightedStButton = nullptr;
}

void AudioGridderAudioProcessorEditor::enableStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colours::white);
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
}

void AudioGridderAudioProcessorEditor::disableStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colours::grey);
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
}

void AudioGridderAudioProcessorEditor::hilightStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colour(Defaults::ACTIVE_COLOR));
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::ACTIVE_COLOR));
    m_hilightedStButton = b;
}

bool AudioGridderAudioProcessorEditor::isHilightedStButton(TextButton* b) {
    traceScope();
    return b == m_hilightedStButton;
}

void AudioGridderAudioProcessorEditor::editPlugin(int idx) {
    traceScope();
    int active = m_processor.getActivePlugin();
    if (idx == -1) {
        idx = active;
    }
    if (idx < 0 || m_processor.isBypassed(idx)) {
        return;
    }
    m_pluginButtons[as<size_t>(idx)]->setActive(true);
    m_pluginButtons[as<size_t>(idx)]->setColour(PluginButton::textColourOffId, Colour(Defaults::ACTIVE_COLOR));
    m_processor.editPlugin(idx);
    if (m_processor.getGenericEditor()) {
        m_genericEditor.resized();
        resized();
        if (active > -1) {
            m_processor.getClient().hidePlugin();
        }
    } else {
        auto* p_processor = &m_processor;
        m_processor.getClient().setPluginScreenUpdateCallback(
            [this, idx, p_processor](std::shared_ptr<Image> img, int width, int height) {
                traceScope();
                if (nullptr != img) {
                    runOnMsgThreadAsync([this, p_processor, img, width, height] {
                        traceScope();
                        auto p = dynamic_cast<AudioGridderAudioProcessorEditor*>(p_processor->getActiveEditor());
                        if (this == p) {  // make sure the editor hasn't been closed
                            m_pluginScreen.setSize(width, height);
                            m_pluginScreen.setImage(img->createCopy());
                            resized();
                        }
                    });
                } else {
                    runOnMsgThreadAsync([this, idx, p_processor] {
                        traceScope();
                        auto p = dynamic_cast<AudioGridderAudioProcessorEditor*>(p_processor->getActiveEditor());
                        if (this == p && m_pluginButtons.size() > as<size_t>(idx)) {
                            m_processor.hidePlugin(false);
                            m_pluginButtons[as<size_t>(idx)]->setActive(false);
                            resized();
                        }
                    });
                }
            });
    }
    if (active > -1 && idx != active && as<size_t>(active) < m_pluginButtons.size()) {
        m_pluginButtons[as<size_t>(active)]->setActive(false);
        m_pluginButtons[as<size_t>(active)]->setColour(PluginButton::textColourOffId, Colours::white);
        resized();
    }
}

}  // namespace e47
