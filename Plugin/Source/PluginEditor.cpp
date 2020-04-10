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

AudioGridderAudioProcessorEditor::AudioGridderAudioProcessorEditor(AudioGridderAudioProcessor& p)
    : AudioProcessorEditor(&p), m_processor(p), m_newPluginButton("", "newPlug") {
    addAndMakeVisible(m_srvIcon);
    m_srvIcon.setImage(ImageCache::getFromMemory(Images::server_png, Images::server_pngSize));
    m_srvIcon.setAlpha(0.5);
    m_srvIcon.setBounds(5, 5, 20, 20);
    m_srvIcon.addMouseListener(this, true);

    addAndMakeVisible(m_srvLabel);
    m_srvLabel.setText("not connected", NotificationType::dontSendNotification);

    setConnected(m_processor.getClient().isReadyLockFree());

    m_srvLabel.setBounds(30, 5, 170, 20);
    auto font = m_srvLabel.getFont();
    font.setHeight(font.getHeight() - 2);
    m_srvLabel.setFont(font);

    addAndMakeVisible(m_newPluginButton);
    m_newPluginButton.setButtonText("+");
    m_newPluginButton.setOnClickWithModListener(this);
    addAndMakeVisible(m_pluginScreen);
    m_pluginScreen.setBounds(200, 1, 100, 100);
    m_pluginScreen.setWantsKeyboardFocus(true);
    m_pluginScreen.addMouseListener(&m_processor.getClient(), true);
    m_pluginScreen.addKeyListener(&m_processor.getClient());

    int idx = 0;
    for (auto& plug : m_processor.getLoadedPlugins()) {
        auto* b = addPluginButton(plug.id, plug.name);
        if (!plug.ok) {
            b->setEnabled(false);
        }
        if (plug.bypassed) {
            b->setButtonText("( " + m_processor.getLoadedPlugin(idx).name + " )");
            b->setColour(PluginButton::textColourOffId, Colours::grey);
        }
        idx++;
    }

    auto active = m_processor.getActivePlugin();
    if (active > -1) {
        m_pluginButtons[active]->setActive(true);
    }

    setSize(200, 100);
}

AudioGridderAudioProcessorEditor::~AudioGridderAudioProcessorEditor() {
    m_processor.hidePlugin();
    m_processor.getClient().setPluginScreenUpdateCallback(nullptr);
}

void AudioGridderAudioProcessorEditor::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void AudioGridderAudioProcessorEditor::resized() {
    int buttonWidth = 197;
    int buttonHeight = 20;
    int num = 0;
    int top = 30;
    for (auto& b : m_pluginButtons) {
        b->setBounds(1, top, buttonWidth, buttonHeight);
        top += buttonHeight + 2;
        num++;
    }
    m_newPluginButton.setBounds(1, top, buttonWidth, buttonHeight);
    top += buttonHeight + 2;
    int windowHeight = jmax(100, top);
    int windowWidth = 200;
    if (m_processor.getActivePlugin() != -1) {
        windowHeight = jmax(windowHeight, m_pluginScreen.getHeight());
        windowWidth += m_pluginScreen.getWidth();
    }
    if (getWidth() != windowWidth || getHeight() != windowHeight) {
        setSize(windowWidth, windowHeight);
    }
}

void AudioGridderAudioProcessorEditor::buttonClicked(Button* button, const ModifierKeys& modifiers) {
    if (!button->getName().compare("newPlug")) {
        PopupMenu m;
        auto recents = m_processor.getClient().getRecents();
        for (const auto& plug : recents) {
            m.addItem(plug.getName(), [this, plug] {
                if (m_processor.loadPlugin(plug.getId(), plug.getName())) {
                    addPluginButton(plug.getId(), plug.getName());
                    resized();
                }
            });
        }
        if (recents.size() > 0) {
            m.addSeparator();
        }
        // create type menu
        std::map<String, PopupMenu> typeMenus;
        for (const auto& type : m_processor.getPluginTypes()) {
            PopupMenu& sub = typeMenus[type];
            std::map<String, PopupMenu> companyMenus;
            for (const auto& plug : m_processor.getPlugins(type)) {
                auto& menu = companyMenus[plug.getCompany()];
                menu.addItem(plug.getName(), [this, plug] {
                    if (m_processor.loadPlugin(plug.getId(), plug.getName())) {
                        addPluginButton(plug.getId(), plug.getName());
                        resized();
                    }
                });
            }
            for (auto& company : companyMenus) {
                sub.addSubMenu(company.first, company.second);
            }
        }
        if (typeMenus.size() > 1) {
            for (auto& sub : typeMenus) {
                m.addSubMenu(sub.first, sub.second);
            }
        } else if (typeMenus.size() > 0) {
            // skip the type menu if there is only one type
            for (auto& tm : typeMenus) {
                PopupMenu::MenuItemIterator it(tm.second);
                while (it.next()) {
                    auto& item = it.getItem();
                    m.addSubMenu(item.text, *item.subMenu);
                }
            }
        }
        m.showAt(button);
    } else {
        int idx = getPluginIndex(button->getName());
        int active = m_processor.getActivePlugin();
        auto editFn = [this, idx, active] {
            m_processor.editPlugin(idx);
            m_pluginButtons[idx]->setActive(true);
            m_pluginButtons[idx]->setColour(PluginButton::textColourOffId, Colours::yellow);
            auto* p_processor = &m_processor;
            m_processor.getClient().setPluginScreenUpdateCallback(
                [this, idx, p_processor](std::shared_ptr<Image> img, int width, int height) {
                    if (nullptr != img) {
                        MessageManager::callAsync([this, p_processor, img, width, height] {
                            auto p = dynamic_cast<AudioGridderAudioProcessorEditor*>(p_processor->getActiveEditor());
                            if (this == p) {  // make sure the editor hasn't been closed
                                m_pluginScreen.setSize(width, height);
                                m_pluginScreen.setImage(*img);
                                resized();
                            }
                        });
                    } else {
                        MessageManager::callAsync([this, idx, p_processor] {
                            auto p = dynamic_cast<AudioGridderAudioProcessorEditor*>(p_processor->getActiveEditor());
                            if (this == p && m_pluginButtons.size() > idx) {
                                m_processor.hidePlugin(false);
                                m_pluginButtons[idx]->setActive(false);
                                resized();
                            }
                        });
                    }
                });
            if (active > -1) {
                m_pluginButtons[active]->setActive(false);
                m_pluginButtons[active]->setColour(PluginButton::textColourOffId, Colours::white);
            }
        };
        auto hideFn = [this, idx] {
            m_processor.hidePlugin();
            m_pluginButtons[idx]->setActive(false);
            m_pluginButtons[idx]->setColour(PluginButton::textColourOffId, Colours::white);
            resized();
        };
        if (modifiers.isLeftButtonDown()) {
            if (idx != active) {
                editFn();
            } else {
                hideFn();
            }
        } else {
            PopupMenu m;
            bool bypassed = m_processor.isBypassed(idx);
            if (bypassed) {
                m.addItem("Unbypass", [this, idx, button] {
                    m_processor.unbypassPlugin(idx);
                    button->setButtonText(m_processor.getLoadedPlugin(idx).name);
                    button->setColour(PluginButton::textColourOffId, Colours::white);
                });
            } else {
                m.addItem("Bypass", [this, idx, button] {
                    m_processor.bypassPlugin(idx);
                    button->setButtonText("( " + m_processor.getLoadedPlugin(idx).name + " )");
                    button->setColour(PluginButton::textColourOffId, Colours::grey);
                });
            }
            m.addItem("Edit", editFn);
            m.addItem("Hide", idx == m_processor.getActivePlugin(), false, hideFn);
            m.addSeparator();
            m.addItem("Move Up", idx > 0, false, [this, idx] {
                m_processor.exchangePlugins(idx, idx - 1);
                std::swap(m_pluginButtons[idx], m_pluginButtons[idx - 1]);
                resized();
            });
            m.addItem("Move Down", idx < m_pluginButtons.size() - 1, false, [this, idx] {
                m_processor.exchangePlugins(idx, idx + 1);
                std::swap(m_pluginButtons[idx], m_pluginButtons[idx + 1]);
                resized();
            });
            m.addSeparator();
            m.addItem("Delete", [this, idx] {
                m_processor.unloadPlugin(idx);
                int i = 0;
                for (auto it = m_pluginButtons.begin(); it < m_pluginButtons.end(); it++) {
                    if (i++ == idx) {
                        m_pluginButtons.erase(it);
                        break;
                    }
                }
                resized();
            });
            m.addSeparator();
            PopupMenu presets;
            int preset = 0;
            for (auto& p : m_processor.getLoadedPlugin(idx).presets) {
                presets.addItem(p, [this, idx, preset] { m_processor.getClient().setPreset(idx, preset); });
                preset++;
            }
            m.addSubMenu("Presets", presets);
            m.showAt(button);
        }
    }
}

Button* AudioGridderAudioProcessorEditor::addPluginButton(const String& id, const String& name) {
    int num = 0;
    for (auto& plug : m_pluginButtons) {
        if (!id.compare(plug->getPluginId())) {
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

std::vector<Button*> AudioGridderAudioProcessorEditor::getPluginButtons(const String& id) {
    std::vector<Button*> ret;
    for (auto& b : m_pluginButtons) {
        if (b->getPluginId() == id) {
            ret.push_back(b.get());
        }
    }
    return ret;
}

int AudioGridderAudioProcessorEditor::getPluginIndex(const String& name) {
    int idx = 0;
    for (auto& plug : m_pluginButtons) {
        if (!name.compare(plug->getName())) {
            return idx;
        }
        idx++;
    }
    return -1;
}

void AudioGridderAudioProcessorEditor::focusOfChildComponentChanged(FocusChangeType cause) {
    if (hasKeyboardFocus(true)) {
        // reactivate the plugin screen
        int active = m_processor.getActivePlugin();
        if (active > -1) {
            m_processor.editPlugin(active);
        }
    }
}

void AudioGridderAudioProcessorEditor::setConnected(bool connected) {
    if (connected != m_connected) {
        m_connected = connected;
        if (connected) {
            m_srvLabel.setText(m_processor.getClient().getServerHostAndID(), NotificationType::dontSendNotification);
            auto& plugins = m_processor.getLoadedPlugins();
            for (int i = 0; i < m_pluginButtons.size(); i++) {
                m_pluginButtons[i]->setEnabled(plugins[i].ok);
            }
        } else {
            m_srvLabel.setText("not connected", NotificationType::dontSendNotification);
            for (auto& but : m_pluginButtons) {
                but->setEnabled(false);
            }
        }
    }
}

void AudioGridderAudioProcessorEditor::mouseUp(const MouseEvent& event) {
    if (!m_srvIcon.contains(event.getMouseDownPosition())) {
        return;
    }
    PopupMenu m;
    auto& servers = m_processor.getServers();
    for (int i = 0; i < servers.size(); i++) {
        if (i == m_processor.getActiveServer()) {
            m.addItem(servers[i], true, true, [] {});
        } else {
            PopupMenu srvMenu;
            srvMenu.addItem("Connect", [this, i] { m_processor.setActiveServer(i); });
            srvMenu.addItem("Remove", [this, i] { m_processor.delServer(i); });
            m.addSubMenu(servers[i], srvMenu);
        }
    }
    m.addSeparator();
    m.addItem("Add", [this] {
        auto w = new NewServerWindow(getScreenX() + 2, getScreenY() + 30);
        w->onOk([this](String server) { m_processor.addServer(server); });
        w->setAlwaysOnTop(true);
        w->runModalLoop();
    });
    m.showAt(&m_srvIcon);
}
