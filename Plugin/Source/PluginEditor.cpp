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
#include "Version.hpp"
#include "PluginSearchWindow.hpp"

namespace e47 {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), m_processor(p), m_newPluginButton("", "newPlug", false), m_genericEditor(p) {
    setLogTagSource(&m_processor.getClient());
    traceScope();
    initAsyncFunctors();
    logln("creating editor");

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
    m_pluginScreen.setWantsKeyboardFocus(true);
    resetPluginScreen();
    m_pluginScreen.setVisible(false);

    addChildComponent(m_genericEditorView);
    m_genericEditorView.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, 100, 200);
    m_genericEditor.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, 100, 200);
    m_genericEditorView.setViewedComponent(&m_genericEditor, false);
    m_genericEditorView.setVisible(false);

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

    createPluginButtons();
    initStButtons();

    setSize(200, 100);

    if (m_processor.getClient().isServerLocalMode()) {
        m_positionTracker = std::make_unique<PositionTracker>(this);
    }

    logln("setting connected state");
    runOnMsgThreadAsync([this] {
        setConnected(m_processor.getClient().isReadyLockFree());
        setCPULoad(m_processor.getClient().getCPULoad());
    });
    logln("editor created");
}

PluginEditor::~PluginEditor() {
    traceScope();
    stopAsyncFunctors();
    logln("destroying editor");
    m_positionTracker.reset();
    m_wantsScreenUpdates = false;
    if (!m_processor.getKeepEditorOpen()) {
        m_processor.hidePlugin();
    }
    m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
    logln("editor destroyed");
}

void PluginEditor::paint(Graphics& g) {
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

void PluginEditor::ToolsButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted,
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

void PluginEditor::resized() {
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

    if (m_processor.getActivePlugin() > -1) {
        if (!genericEditorEnabled() && !m_pluginScreenEmpty) {
            m_stMinus.setVisible(true);
            m_stPlus.setVisible(true);
            m_stFullscreen.setVisible(true);
        } else {
            m_stMinus.setVisible(false);
            m_stPlus.setVisible(false);
            m_stFullscreen.setVisible(false);
        }
        m_stA.setVisible(true);
        m_stB.setVisible(true);
    } else {
        m_stMinus.setVisible(false);
        m_stPlus.setVisible(false);
        m_stFullscreen.setVisible(false);
        m_stA.setVisible(false);
        m_stB.setVisible(false);
    }
    if (genericEditorEnabled() && m_processor.getActivePlugin() > -1) {
        m_genericEditorView.setVisible(true);
        m_pluginScreen.setVisible(false);
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
    m_stA.setBounds(leftBarWidth + SCREENTOOLS_MARGIN, SCREENTOOLS_MARGIN, SCREENTOOLS_AB_WIDTH, SCREENTOOLS_HEIGHT);
    m_stB.setBounds(leftBarWidth + SCREENTOOLS_MARGIN + SCREENTOOLS_AB_WIDTH, SCREENTOOLS_MARGIN, SCREENTOOLS_AB_WIDTH,
                    SCREENTOOLS_HEIGHT);
    if (m_currentActiveAB != m_processor.getActivePlugin()) {
        initStButtons();
    }
    if (getWidth() != windowWidth || getHeight() != windowHeight) {
        setSize(windowWidth, windowHeight);
    }
    m_logo.setBounds(4, windowHeight - logoHeight - 4, m_logo.getWidth(), m_logo.getHeight());
    m_versionLabel.setBounds(logoHeight + 3, windowHeight - 15, m_versionLabel.getWidth(), m_versionLabel.getHeight());
    m_cpuIcon.setBounds(200 - 45, windowHeight - logoHeight - 3, m_cpuIcon.getWidth(), m_cpuIcon.getHeight());
    m_cpuLabel.setBounds(200 - 45 + logoHeight - 2, windowHeight - 15, m_cpuLabel.getWidth(), m_cpuLabel.getHeight());
}

void PluginEditor::buttonClicked(Button* button, const ModifierKeys& modifiers, PluginButton::AreaType area) {
    traceScope();
    if (!button->getName().compare("newPlug")) {
        auto addFn = [this](const ServerPlugin& plug) {
            traceScope();
            String err;
            bool success = m_processor.loadPlugin(plug, err);
            if (!success) {
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error",
                                                 "Failed to add " + plug.getName() + " plugin!\n\nError: " + err, "OK");
            }
            auto* b = addPluginButton(plug.getId(), plug.getName());
            if (success) {
                editPlugin((int)m_pluginButtons.size() - 1);
            } else {
                b->setEnabled(false);
                b->setTooltip(err);
            }
#if JucePlugin_IsSynth
            m_newPluginButton.setEnabled(false);
#endif
            resized();
        };

        auto bounds = button->getScreenBounds().toFloat();
        auto searchWin = std::make_unique<PluginSearchWindow>(bounds.getX(), bounds.getBottom(), m_processor);
        searchWin->onClick([this, addFn](ServerPlugin plugin) {
            traceScope();
            addFn(plugin);
        });
        searchWin->runModalLoop();
    } else {
        int idx = getPluginIndex(button->getName());
        int active = m_processor.getActivePlugin();
        auto editFn = [this, idx] { editPlugin(idx); };
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
                std::swap(m_pluginButtons[(size_t)idx], m_pluginButtons[(size_t)idx - 1]);
                resized();
            }
        };
        auto moveDownFn = [this, idx] {
            traceScope();
            if ((size_t)(idx) < m_pluginButtons.size() - 1) {
                m_processor.exchangePlugins(idx, idx + 1);
                std::swap(m_pluginButtons[(size_t)idx], m_pluginButtons[(size_t)idx + 1]);
                resized();
            }
        };
        auto deleteFn = [this, idx, active] {
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
                if (idx == active) {
                    int newactive = idx;
                    if (newactive >= (int)m_pluginButtons.size()) {
                        newactive--;
                    }
                    if (newactive > -1) {
                        editPlugin(newactive);
                    }
                }
                if (m_pluginButtons.size() == 0) {
                    m_wantsScreenUpdates = false;
                    m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
                    resetPluginScreen();
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
                    if (idx != active) {
                        editFn();
                    } else if (!m_processor.isEditAlways()) {
                        m_wantsScreenUpdates = false;
                        m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
                        m_processor.hidePlugin();
                        unhighlightPluginButton(active);
                        resetPluginScreen();
                        resized();
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
            params.addItem("Assign all", [this, idx] {
                for (auto& p : m_processor.getLoadedPlugin(idx).params) {
                    if (p.automationSlot == -1) {
                        if (!m_processor.enableParamAutomation(idx, p.idx)) {
                            break;
                        }
                    }
                }
            });
            params.addItem("Unassign all", [this, idx] {
                for (auto& p : m_processor.getLoadedPlugin(idx).params) {
                    if (p.automationSlot > -1) {
                        m_processor.disableParamAutomation(idx, p.idx);
                    }
                }
            });
            params.addSeparator();
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

void PluginEditor::buttonClicked(Button* button) {
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

void PluginEditor::createPluginButtons() {
    traceScope();
    for (auto& b : m_pluginButtons) {
        removeChildComponent(b.get());
    }
    m_pluginButtons.clear();
    for (int idx = 0; idx < m_processor.getNumOfLoadedPlugins(); idx++) {
        auto& plug = m_processor.getLoadedPlugin(idx);
        if (plug.id.isNotEmpty()) {
            auto* b = addPluginButton(plug.id, plug.name);
            if (!plug.ok) {
                b->setEnabled(false);
                b->setTooltip(plug.error);
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
}

PluginButton* PluginEditor::addPluginButton(const String& id, const String& name) {
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

std::vector<PluginButton*> PluginEditor::getPluginButtons(const String& id) {
    traceScope();
    std::vector<PluginButton*> ret;
    for (auto& b : m_pluginButtons) {
        if (b->getPluginId() == id) {
            ret.push_back(b.get());
        }
    }
    return ret;
}

int PluginEditor::getPluginIndex(const String& name) {
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

void PluginEditor::focusOfChildComponentChanged(FocusChangeType cause) {
    traceScope();
    bool focus = hasKeyboardFocus(true);
    if (focus) {
        // reactivate the plugin screen
        int active = m_processor.getActivePlugin();
        if (active > -1) {
            auto p = getLocalModePosition();
            logln("focus change: cause is " << cause);
            m_processor.editPlugin(active, p.x, p.y);
        }
    }
}

void PluginEditor::setConnected(bool connected) {
    traceScope();
    m_connected = connected;
    if (connected) {
        String srvTxt = m_processor.getActiveServerName();
        srvTxt << " (+" << m_processor.getLatencyMillis() << "ms)";
        m_srvLabel.setText(srvTxt, NotificationType::dontSendNotification);
        for (size_t i = 0; i < m_pluginButtons.size(); i++) {
            auto& plug = m_processor.getLoadedPlugin((int)i);
            auto* b = m_pluginButtons[i].get();
            b->setEnabled(plug.ok);
            b->setTooltip(plug.error);
        }
        auto active = m_processor.getActivePlugin();
        if (active > -1) {
            editPlugin();
        } else if (m_processor.isEditAlways()) {
            auto lastActive = m_processor.getLastActivePlugin();
            if (lastActive < 0) {
                lastActive = 0;
            }
            editPlugin(lastActive);
        }
        if (m_processor.getClient().isServerLocalMode() && nullptr == m_positionTracker) {
            m_positionTracker = std::make_unique<PositionTracker>(this);
        }
    } else {
        m_srvLabel.setText("not connected", NotificationType::dontSendNotification);
        setCPULoad(0.0f);
        for (auto& b : m_pluginButtons) {
            b->setEnabled(false);
            b->setTooltip("");
        }
        resetPluginScreen();
        resized();
    }
}

void PluginEditor::setCPULoad(float load) {
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

void PluginEditor::mouseUp(const MouseEvent& event) {
    traceScope();
    if (event.eventComponent == &m_srvIcon) {
        showServerMenu();
    } else if (event.eventComponent == &m_settingsIcon) {
        showSettingsMenu();
    }
}

void PluginEditor::showServerMenu() {
    PopupMenu m;

    if (m_processor.getClient().isReadyLockFree()) {
        m.addItem("Reload", [this] {
            traceScope();
            m_processor.getClient().close();
        });
        m.addSeparator();
    }

    PopupMenu subm;
    int rate = (int)lround(m_processor.getSampleRate());
    int iobuf = m_processor.getBlockSize();
    auto getName = [rate, iobuf](int blocks) -> String {
        String n;
        n << blocks << " Blocks (+" << blocks * iobuf * 1000 / rate << "ms)";
        return n;
    };
    subm.addItem("Disabled (+0ms)", true, m_processor.getNumBuffers() == 0, [this] {
        traceScope();
        m_processor.setNumBuffers(0);
    });
    if (rate > 0.0) {
        subm.addItem(getName(1), true, m_processor.getNumBuffers() == 1, [this] {
            traceScope();
            m_processor.setNumBuffers(1);
        });
        subm.addItem(getName(2), true, m_processor.getNumBuffers() == 2, [this] {
            traceScope();
            m_processor.setNumBuffers(2);
        });
        subm.addItem(getName(4), true, m_processor.getNumBuffers() == 4, [this] {
            traceScope();
            m_processor.setNumBuffers(4);
        });
        subm.addItem(getName(8), true, m_processor.getNumBuffers() == 8, [this] {
            traceScope();
            m_processor.setNumBuffers(8);
        });
        subm.addItem(getName(12), true, m_processor.getNumBuffers() == 12, [this] {
            traceScope();
            m_processor.setNumBuffers(12);
        });
        subm.addItem(getName(16), true, m_processor.getNumBuffers() == 16, [this] {
            traceScope();
            m_processor.setNumBuffers(16);
        });
        subm.addItem(getName(20), true, m_processor.getNumBuffers() == 20, [this] {
            traceScope();
            m_processor.setNumBuffers(20);
        });
        subm.addItem(getName(24), true, m_processor.getNumBuffers() == 24, [this] {
            traceScope();
            m_processor.setNumBuffers(24);
        });
        subm.addItem(getName(28), true, m_processor.getNumBuffers() == 28, [this] {
            traceScope();
            m_processor.setNumBuffers(28);
        });
        subm.addItem(getName(30), true, m_processor.getNumBuffers() == 30, [this] {
            traceScope();
            m_processor.setNumBuffers(30);
        });
    }
    m.addSubMenu("Buffer Size", subm);
    subm.clear();

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
            srvMenu.addItem("Reconnect", [this] {
                traceScope();
                m_processor.getClient().close();
            });
            subm.addSubMenu(s, srvMenu, true, nullptr, true, 0);
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
            subm.addSubMenu(s, srvMenu);
        }
    }
    auto serversMDNS = m_processor.getServersMDNS();
    if (serversMDNS.size() > 0) {
        bool showIp = false;
        std::set<String> names;
        for (auto s : serversMDNS) {
            if (names.find(s.getNameAndID()) != names.end()) {
                showIp = true;
                break;
            } else {
                names.insert(s.getNameAndID());
            }
        }
        for (auto s : serversMDNS) {
            if (servers.contains(s.getHostAndID())) {
                continue;
            }
            String name = s.getNameAndID();
            if (showIp) {
                name << " (" << s.getHost() << ")";
            }
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
                srvMenu.addItem("Reconnect", [this] {
                    traceScope();
                    m_processor.getClient().reconnect();
                });
                subm.addSubMenu(name, srvMenu, true, nullptr, true, 0);
            } else {
                PopupMenu srvMenu;
                srvMenu.addItem("Connect", [this, s] {
                    traceScope();
                    m_processor.setActiveServer(s);
                    m_processor.saveConfig();
                });
                subm.addSubMenu(name, srvMenu);
            }
        }
    }

    subm.addSeparator();

    subm.addItem("Add", [this] {
        traceScope();
        auto w = new NewServerWindow((float)(getScreenX() + 2), (float)(getScreenY() + 30));
        w->onOk([this](String server) {
            traceScope();
            m_processor.addServer(server);
            m_processor.setActiveServer(server);
            m_processor.saveConfig();
        });
        w->setAlwaysOnTop(true);
        w->runModalLoop();
    });

    m.addSubMenu("Servers", subm);
    subm.clear();

    m.showAt(&m_srvIcon);
}

void PluginEditor::showSettingsMenu() {
    PopupMenu m, subm;
#if !JucePlugin_IsSynth && !JucePlugin_IsMidiEffect
    subm.addItem("Make Default", [this] {
        traceScope();
        if (m_processor.hasDefaultPreset() &&
            AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Replace",
                                         "Are you sure you want to replace your existing default preset?", "Yes",
                                         "No")) {
            m_processor.resetPresetDefault();
        }
        m_processor.storePresetDefault();
    });
    subm.addItem("Reset Default", m_processor.hasDefaultPreset(), false, [this] {
        traceScope();
        if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Reset",
                                         "Are you sure you want to delete your default settings?", "Yes", "No")) {
            m_processor.resetPresetDefault();
        }
    });
    subm.addSeparator();
#endif
    subm.addItem("Create New...", [this] {
        traceScope();
        File d(m_processor.getPresetDir());
        if (!d.exists()) {
            d.createDirectory();
        }
        WildcardFileFilter filter("*.preset", String(), "Presets");
        FileBrowserComponent fb(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles, d, &filter,
                                nullptr);
        FileChooserDialogBox fcdialog("Create New Preset", "Enter the name for the new preset.", fb, true,
                                      Colour(Defaults::BG_COLOR));
        fcdialog.setAlwaysOnTop(true);
        if (fcdialog.show(300, 400)) {
            auto file = fb.getSelectedFile(0);
            if (file.getFileExtension() != ".preset") {
                file = file.withFileExtension(".preset");
            }
            if (file.existsAsFile()) {
                file.deleteFile();
            }
            m_processor.storePreset(file);
        }
    });
    subm.addItem("Choose Preset Directory...", [this] {
        traceScope();
        File d(m_processor.getPresetDir());
        if (!d.exists()) {
            d.createDirectory();
        }
        FileChooser fc("Presets Directory", d);
        if (fc.browseForDirectory()) {
            d = fc.getResult();
            logln("setting presets dir to " << d.getFullPathName());
            m_processor.setPresetDir(d.getFullPathName());
            m_processor.saveConfig();
        }
    });
    subm.addItem("Manage...", [this] {
        traceScope();
        StringArray cmd;
#if defined(JUCE_MAC)
        cmd.add("open");
#elif defined(JUCE_WINDOWS)
        cmd.add("explorer.exe");
#elif defined(JUCE_LINUX)
        cmd.add("xdg-open");
#endif
        if (!cmd.isEmpty()) {
            File d(m_processor.getPresetDir());
            if (!d.exists()) {
                d.createDirectory();
            }
            cmd.add(d.getFullPathName());
            logln("spawning child proc: " << cmd[0] << " " << cmd[1]);
            ChildProcess proc;
            if (!proc.start(cmd, 0)) {
                logln("failed to open presets dir");
            }
        }
    });
    subm.addSeparator();
    getPresetsMenu(subm, m_processor.getPresetDir());
    m.addSubMenu("Presets", subm);
    subm.clear();

    m.addSeparator();
    m.addItem("Generic Editor", true, m_processor.getGenericEditor(), [this] {
        traceScope();
        m_processor.setGenericEditor(!m_processor.getGenericEditor());
        m_processor.saveConfig();
        resized();
        editPlugin();
    });

#if !JucePlugin_IsMidiEffect
    m.addSeparator();

    auto addBusChannelItems = [&](AudioProcessor::Bus* bus, size_t& ch) {
        if (bus->isEnabled()) {
            auto& layout = bus->getCurrentLayout();
            bool isInput = bus->isInput();
            for (int i = 0; i < bus->getNumberOfChannels(); i++) {
                auto name = bus->getName() + ": " + layout.getChannelTypeName(layout.getTypeOfChannel(i));
                subm.addItem(name, true, m_processor.getActiveChannels().isActive(ch, isInput), [this, ch, isInput] {
                    m_processor.getActiveChannels().setActive(ch, isInput,
                                                              !m_processor.getActiveChannels().isActive(ch, isInput));
                    m_processor.updateChannelMapping();
                    m_processor.getClient().reconnect();
                });
                ch++;
            }
        }
    };

    size_t ch;

#if JucePlugin_IsSynth
    if (m_processor.getBusCount(false) > 1) {
        subm.addItem("Enable all channels...", [this] {
            m_processor.getActiveChannels().setOutputRangeActive(true);
            m_processor.updateChannelMapping();
            m_processor.getClient().reconnect();
        });
        subm.addItem("Enable Main channels only...", [this] {
            m_processor.getActiveChannels().setOutputRangeActive(false);
            for (int c = 0; c < m_processor.getMainBusNumOutputChannels(); c++) {
                m_processor.getActiveChannels().setOutputActive(c);
            }
            m_processor.updateChannelMapping();
            m_processor.getClient().reconnect();
        });
        subm.addSeparator();
    }

    ch = 0;
    for (int busIdx = 0; busIdx < m_processor.getBusCount(false); busIdx++) {
        addBusChannelItems(m_processor.getBus(false, busIdx), ch);
    }
    m.addSubMenu("Instrument Outputs...", subm);
#else
    subm.addSectionHeader("Inputs");
    ch = 0;
    for (int busIdx = 0; busIdx < m_processor.getBusCount(true); busIdx++) {
        addBusChannelItems(m_processor.getBus(true, busIdx), ch);
    }
    subm.addSectionHeader("Outputs");
    ch = 0;
    for (int busIdx = 0; busIdx < m_processor.getBusCount(false); busIdx++) {
        addBusChannelItems(m_processor.getBus(false, busIdx), ch);
    }
    m.addSubMenu("Active Channels...", subm);
#endif
    subm.clear();
#endif

    m.addSeparator();
    m.addItem("Show Monitor...", [this] { m_processor.showMonitor(); });

    m.addSeparator();
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
    subm.addItem("Disable Recents", true, m_processor.getDisableRecents(), [this] {
        traceScope();
        m_processor.setDisableRecents(!m_processor.getDisableRecents());
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

    subm.addItem("Confirm Delete", true, m_processor.getConfirmDelete(), [this] {
        traceScope();
        m_processor.setConfirmDelete(!m_processor.getConfirmDelete());
        m_processor.saveConfig();
    });
    subm.addItem("Keep Plugin UI Open", true, m_processor.isEditAlways(), [this] {
        traceScope();
        m_processor.setEditAlways(!m_processor.isEditAlways());
        m_processor.saveConfig();
    });
    subm.addItem("Don't close the Plugin Window on the Server", true, m_processor.getKeepEditorOpen(), [this] {
        traceScope();
        m_processor.setKeepEditorOpen(!m_processor.getKeepEditorOpen());
        m_processor.saveConfig();
    });
    subm.addItem("Show Sidechain-Disabled Info", true, m_processor.getShowSidechainDisabledInfo(), [this] {
        traceScope();
        m_processor.setShowSidechainDisabledInfo(!m_processor.getShowSidechainDisabledInfo());
        m_processor.saveConfig();
    });
    subm.addItem("Disable Tray App", true, m_processor.getDisableTray(), [this] {
        traceScope();
        m_processor.setDisableTray(!m_processor.getDisableTray());
        m_processor.saveConfig();
    });
    m.addSubMenu("User Interface", subm);
    subm.clear();

    subm.addItem("Always", true, m_processor.getTransferWhenPlayingOnly() == false, [this] {
        traceScope();
        m_processor.setTransferWhenPlayingOnly(false);
        m_processor.saveConfig();
    });
    subm.addItem("Only when Playing/Recording", true, m_processor.getTransferWhenPlayingOnly() == true, [this] {
        traceScope();
        m_processor.setTransferWhenPlayingOnly(true);
        m_processor.saveConfig();
    });
    m.addSubMenu("Transfer Audio/MIDI", subm);
    subm.clear();

    subm.addItem("Always (every 10s)", true, m_processor.getSyncRemoteMode() == PluginProcessor::SYNC_ALWAYS, [this] {
        m_processor.setSyncRemoteMode(PluginProcessor::SYNC_ALWAYS);
        m_processor.saveConfig();
    });
    subm.addItem("When an editor is active (every 10s)", true,
                 m_processor.getSyncRemoteMode() == PluginProcessor::SYNC_WITH_EDITOR, [this] {
                     m_processor.setSyncRemoteMode(PluginProcessor::SYNC_WITH_EDITOR);
                     m_processor.saveConfig();
                 });
    subm.addItem("When saving the project", true, m_processor.getSyncRemoteMode() == PluginProcessor::SYNC_DISABLED,
                 [this] {
                     m_processor.setSyncRemoteMode(PluginProcessor::SYNC_DISABLED);
                     m_processor.saveConfig();
                 });
    m.addSubMenu("Remote Sync Frequency", subm);
    subm.clear();

    m.addSeparator();

    m.addItem("Bypass when not ready", true, m_processor.getBypassWhenNotConnected(), [this] {
        traceScope();
        m_processor.setBypassWhenNotConnected(!m_processor.getBypassWhenNotConnected());
        m_processor.saveConfig();
    });
    m.addItem("Allow buffer size by plugin", true, m_processor.getBufferSizeByPlugin(), [this] {
        traceScope();
        m_processor.setBufferSizeByPlugin(!m_processor.getBufferSizeByPlugin());
        m_processor.saveConfig();
    });

    m.addSeparator();

    subm.addItem("Logging", true, Logger::isEnabled(), [this] {
        traceScope();
        Logger::setEnabled(!Logger::isEnabled());
        m_processor.saveConfig();
    });
    subm.addItem("Tracing", true, Tracer::isEnabled(), [this] {
        traceScope();
        Tracer::setEnabled(!Tracer::isEnabled());
        m_processor.saveConfig();
    });
    if (m_processor.supportsCrashReporting()) {
        subm.addItem("Send Crash Reports", true, m_processor.getCrashReporting(), [this] {
            traceScope();
            m_processor.setCrashReporting(!m_processor.getCrashReporting());
            m_processor.saveConfig();
        });
    }
    m.addSubMenu("Diagnostics", subm);
    subm.clear();

    m.addItem("Show Statistics...", [this] {
        traceScope();
        StatisticsWindow::show();
    });

    m.showAt(&m_settingsIcon);
}

void PluginEditor::initStButtons() {
    traceScope();
    enableStButton(&m_stA);
    disableStButton(&m_stB);
    m_processor.resetSettingsAB();
    m_hilightedStButton = nullptr;
}

void PluginEditor::enableStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colours::white);
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
}

void PluginEditor::disableStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colours::grey);
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::BUTTON_COLOR));
}

void PluginEditor::hilightStButton(TextButton* b) {
    traceScope();
    b->setColour(PluginButton::textColourOffId, Colour(Defaults::ACTIVE_COLOR));
    b->setColour(ComboBox::outlineColourId, Colour(Defaults::ACTIVE_COLOR));
    m_hilightedStButton = b;
}

bool PluginEditor::isHilightedStButton(TextButton* b) {
    traceScope();
    return b == m_hilightedStButton;
}

void PluginEditor::editPlugin(int idx) {
    traceScope();
    int active = m_processor.getActivePlugin();
    if (idx == -1) {
        idx = active;
    }
    if (idx < 0 || (size_t)idx >= m_pluginButtons.size() || m_processor.isBypassed(idx)) {
        return;
    }
    highlightPluginButton(idx);
    m_stA.setVisible(true);
    m_stB.setVisible(true);
    auto pos = getLocalModePosition();
    m_processor.editPlugin(idx, pos.x, pos.y);
    if (genericEditorEnabled()) {
        m_wantsScreenUpdates = false;
        m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
        resetPluginScreen();
        m_genericEditor.resized();
        resized();
        if (active > -1) {
            m_processor.getClient().hidePlugin();
        }
    } else {
        auto* p_processor = &m_processor;
        m_wantsScreenUpdates = true;
        m_processor.getClient().setPluginScreenUpdateCallback(
            [this, idx, p_processor](std::shared_ptr<Image> img, int width, int height) {
                traceScope();
                if (nullptr != img) {
                    runOnMsgThreadAsync([this, p_processor, img, width, height] {
                        traceScope();
                        auto p = dynamic_cast<PluginEditor*>(p_processor->getActiveEditor());
                        if (this == p && m_wantsScreenUpdates) {  // make sure the editor hasn't been closed
                            setPluginScreen(img->createCopy(), width, height);
                            resized();
                        }
                    });
                } else {
                    runOnMsgThreadAsync([this, idx, p_processor] {
                        traceScope();
                        auto p = dynamic_cast<PluginEditor*>(p_processor->getActiveEditor());
                        if (this == p && m_pluginButtons.size() > (size_t)idx) {
                            m_processor.hidePlugin(false);
                            m_pluginButtons[(size_t)idx]->setActive(false);
                            resetPluginScreen();
                            resized();
                        }
                    });
                }
            });
    }
    if (active > -1 && idx != active && (size_t)active < m_pluginButtons.size()) {
        unhighlightPluginButton(active);
        resized();
    }
}

void PluginEditor::highlightPluginButton(int idx) {
    m_pluginButtons[(size_t)idx]->setActive(true);
    m_pluginButtons[(size_t)idx]->setColour(PluginButton::textColourOffId, Colour(Defaults::ACTIVE_COLOR));
}

void PluginEditor::unhighlightPluginButton(int idx) {
    m_pluginButtons[(size_t)idx]->setActive(false);
    m_pluginButtons[(size_t)idx]->setColour(PluginButton::textColourOffId, Colours::white);
}

Point<int> PluginEditor::getLocalModePosition(juce::Rectangle<int> bounds) {
    if (m_processor.getClient().isServerLocalMode()) {
        if (bounds.isEmpty()) {
            bounds = WindowHelper::getWindowScreenBounds(this);
        }
        if (!bounds.isEmpty()) {
            return {bounds.getRight() + 10, bounds.getY()};
        }
    }
    return {};
}

void PluginEditor::getPresetsMenu(PopupMenu& menu, const File& dir) {
    traceScope();

    if (!dir.exists()) {
        return;
    }

    auto files = dir.findChildFiles(File::findFiles | File::findDirectories, false);
    files.sort();
    for (auto file : files) {
        if (file.isDirectory()) {
            PopupMenu subm;
            getPresetsMenu(subm, file);
            menu.addSubMenu(file.getFileName(), subm);
        } else if (file.getFileExtension() == ".preset") {
            auto j = configParseFile(file.getFullPathName());
            auto mode = jsonGetValue(j, "Mode", String());
            if (mode.isNotEmpty() && mode != m_processor.getMode()) {
                continue;
            }
            menu.addItem(file.getFileNameWithoutExtension(), [this, file] {
                traceScope();
                if (m_processor.loadPreset(file)) {
                    createPluginButtons();
                    resetPluginScreen();
                    resized();
                    m_processor.getClient().reconnect();
                }
            });
        }
    }
}

void PluginEditor::resetPluginScreen() {
    m_pluginScreen.setImage(ImageCache::getFromMemory(Images::pluginlogo_png, Images::pluginlogo_pngSize));
    m_pluginScreen.setBounds(200, SCREENTOOLS_HEIGHT + SCREENTOOLS_MARGIN * 2, PLUGINSCREEN_DEFAULT_W,
                             PLUGINSCREEN_DEFAULT_H);
    m_pluginScreen.removeMouseListener(&m_processor.getClient());
    m_pluginScreen.removeKeyListener(&m_processor.getClient());
    m_pluginScreenEmpty = true;
}

void PluginEditor::setPluginScreen(const Image& img, int w, int h) {
    if (m_pluginScreenEmpty) {
        m_pluginScreenEmpty = false;
        m_pluginScreen.addMouseListener(&m_processor.getClient(), true);
        m_pluginScreen.addKeyListener(&m_processor.getClient());
    }
    m_pluginScreen.setSize(w, h);
    m_pluginScreen.setImage(img);
}

bool PluginEditor::genericEditorEnabled() const {
    bool ret = m_processor.getGenericEditor();
    if (!ret) {
        int active = m_processor.getActivePlugin();
        if (active > -1 && m_processor.getLoadedPlugin(active).ok) {
            ret = !m_processor.getLoadedPlugin(active).hasEditor;
        }
    }
    return ret;
}

void PluginEditor::updateParamValue(int paramIdx) {
    if (genericEditorEnabled()) {
        m_genericEditor.updateParamValue(paramIdx);
    }
}

void PluginEditor::updatePluginStatus(int idx, bool ok, const String& err) {
    if (idx > -1 && (size_t)idx < m_pluginButtons.size()) {
        auto* b = m_pluginButtons[(size_t)idx].get();
        b->setEnabled(ok);
        b->setTooltip(err);
        if (idx == m_processor.getActivePlugin()) {
            resetPluginScreen();
        }
    }
}

}  // namespace e47
