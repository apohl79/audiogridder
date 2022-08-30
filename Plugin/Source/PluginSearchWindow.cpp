/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginSearchWindow.hpp"
#include "PluginEditor.hpp"

namespace e47 {

PluginSearchWindow::PluginSearchWindow(float x, float y, PluginProcessor& p)
    : TopLevelWindow("Search", true),
      LogTagDelegate(&p.getClient()),
      m_processor(p),
      m_showType(m_processor.getNoSrvPluginListFilter()) {
    traceScope();

    setWantsKeyboardFocus(false);
    setAlwaysOnTop(true);

    int totalWidth = m_showType ? 300 : 270;
    int totalHeight = 35;

    setBounds((int)lroundf(x), (int)lroundf(y), totalWidth, totalHeight);

    m_search.setBounds(5, 5, totalWidth - 10, 25);
    m_search.setWantsKeyboardFocus(true);
    m_search.addKeyListener(this);
    m_search.onTextChange = [this] {
        updateTree(m_search.getText());
        updateHeight();
    };
    addAndMakeVisible(&m_search);

    m_tree.addKeyListener(this);
    m_tree.addMouseListener(this, true);
    m_tree.setIndentSize(10);
    m_tree.setRootItem(new TreeRoot());
    m_tree.setRootItemVisible(false);
    m_tree.setColour(TreeView::backgroundColourId, Colour(Defaults::BG_COLOR));
    m_tree.setColour(TreeView::evenItemsColourId, Colour(Defaults::BG_COLOR));
    m_tree.setColour(TreeView::oddItemsColourId, Colour(Defaults::BG_COLOR));
    addAndMakeVisible(&m_tree);

    m_recents = m_processor.getRecents();

    for (auto& plug : m_processor.getPlugins()) {
        m_pluginsByName[plug.getType() + plug.getName()] = plug;
    }

    updateTree();
    updateHeight();

    setVisible(true);
}

PluginSearchWindow::~PluginSearchWindow() {
    traceScope();
    m_search.removeKeyListener(this);
    m_tree.removeKeyListener(this);
    m_tree.removeMouseListener(this);
}

void PluginSearchWindow::hide() {
    if (isVisible()) {
        logln("hiding serach window 0x" << String::toHexString((uint64)this));
        exitModalState(0);
        setVisible(false);
    }
}

void PluginSearchWindow::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
}

bool PluginSearchWindow::keyPressed(const KeyPress& kp, Component*) {
    traceScope();
    if (kp.isKeyCurrentlyDown(KeyPress::escapeKey)) {
        hide();
        return true;
    } else if (kp.isKeyCurrentlyDown(KeyPress::returnKey)) {
        int num = m_tree.getNumSelectedItems();
        if (num > 0) {
            if (auto item = dynamic_cast<TreeLayout*>(m_tree.getSelectedItem(0))) {
                item->itemClicked();
            }
        }
        return true;
    } else if (kp.isKeyCurrentlyDown(KeyPress::tabKey)) {
        if (m_tree.hasKeyboardFocus(true)) {
            m_tree.clearSelectedItems();
            m_search.grabKeyboardFocus();
        } else {
            auto* root = m_tree.getRootItem();
            if (nullptr != root) {
                for (int i = 0; i < root->getNumSubItems(); i++) {
                    auto* item = root->getSubItem(i);
                    if (nullptr != item && item->canBeSelected()) {
                        item->setSelected(true, true);
                        break;
                    }
                }
            }
            m_tree.grabKeyboardFocus();
        }
        return true;
    }
    return false;
}

void PluginSearchWindow::mouseMove(const MouseEvent&) {
    traceScope();
    if (m_tree.isMouseOver(true)) {
        auto vp = m_tree.getViewport();
        auto* item = m_tree.getItemAt(vp->getMouseXYRelative().y);
        if (nullptr != item && !item->isSelected()) {
            item->setSelected(true, true);
        }
    }
}

void PluginSearchWindow::mouseExit(const MouseEvent&) {
    traceScope();
    m_tree.clearSelectedItems();
}

const ServerPlugin* PluginSearchWindow::getPlugin(const String& key) {
    auto it = m_pluginsByName.find(key);
    if (it != m_pluginsByName.end()) {
        return &it->second;
    }
    return nullptr;
}

void PluginSearchWindow::updateHeight() {
    traceScope();
    int items = m_tree.getNumRowsInTree();
    if (items > MAX_ITEMS_VISIBLE) {
        items = MAX_ITEMS_VISIBLE;
    }
    int totalHeight = 40 + ITEM_HEIGHT * items;
    int minHeight = 40 + ITEM_HEIGHT * MIN_ITEMS_VISIBLE;
    if (m_search.isEmpty() && !m_recents.isEmpty()) {
        // remove some pixels, as the separator is not using the full height
        totalHeight += SEPARATOR_HEIGHT - ITEM_HEIGHT;
        minHeight += SEPARATOR_HEIGHT - ITEM_HEIGHT;
    }
    int distMoveUp = 0;
    auto disp = Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
    if (nullptr != disp) {
        auto totalArea = disp->totalArea;
        int screenBottom = getScreenY() + totalHeight;
        if (screenBottom > totalArea.getBottom()) {
            totalHeight -= screenBottom - totalArea.getBottom();
            if (totalHeight < minHeight) {
                distMoveUp = minHeight - totalHeight;
                totalHeight = minHeight;
            }
        }
    }
    if (totalHeight != getHeight()) {
        m_tree.setBounds(5, 35, getWidth() - 10, totalHeight - 40);
        setBounds(getX(), getY() - distMoveUp, getWidth(), totalHeight);
    }
}

void PluginSearchWindow::updateTree(const String& filter) {
    traceScope();

    auto addFn = [this](const ServerPlugin& p, const String& l) {
        traceScope();
        if (m_onClick) {
            m_onClick(p, l);
        }
        hide();
    };

    auto* root = m_tree.getRootItem();
    root->clearSubItems();

    m_tree.setDefaultOpenness(filter.isNotEmpty());

    if (filter.isEmpty() && !m_recents.isEmpty()) {
        int countAdded = 0;
        for (const auto& p : m_recents) {
            if (auto* plug = getPlugin(p.getType() + p.getName())) {
                auto* plugEntry = new TreePlugin(
                    *plug, [this] { updateHeight(); }, false, true);
                root->addSubItem(plugEntry);
                if (plug->getLayouts().isEmpty()) {
                    plugEntry->addSubItem(new TreeLayout(*plug, "Default", addFn));
                } else {
                    for (auto& l : plug->getLayouts()) {
                        plugEntry->addSubItem(new TreeLayout(*plug, l, addFn));
                    }
                }
                plugEntry->setOpenness(TreeViewItem::Openness::opennessClosed);
                countAdded++;
            }
        }
        if (countAdded > 0) {
            root->addSubItem(new TreeSeparator());
        }
    }

    auto filterParts = StringArray::fromTokens(filter, " ", "");

    // ceate menu structure: [type] -> [category] -> [company] -> plugin
    MenuLevel menuRoot;
    for (const auto& type : m_processor.getPluginTypes()) {
        for (const auto& plug : m_processor.getPlugins(type)) {
            if (filterParts.size() > 0) {
                bool match = true;
                for (auto& f : filterParts) {
                    if (f.isNotEmpty() && !plug.getName().containsIgnoreCase(f) &&
                        !plug.getCompany().containsIgnoreCase(f) && !plug.getCategory().containsIgnoreCase(f)) {
                        match = false;
                    }
                }
                if (!match) {
                    continue;
                }
            }

            auto* level = &menuRoot;

            if (m_processor.getMenuShowType()) {
                level = addTypeMenu(level, type);
            }

            if (m_processor.getMenuShowCategory()) {
                level = addCategoryMenu(level, plug.getCategory());
            }

            if (m_processor.getMenuShowCompany()) {
                level = addCompanyMenu(level, plug.getCompany());
            }

            level = findPluginLevel(level);

            if (nullptr == level->entryMap) {
                level->entryMap = std::make_unique<std::map<String, ServerPlugin>>();
            }

            int num = 0;
            String name;

            do {
                name = plug.getName();
                if (num > 0) {
                    name << " (" << num << ")";
                }
            } while (level->entryMap->find(name) != level->entryMap->end() && ++num > 0);

            (*level->entryMap)[name] = plug;
        }
    }

    if (nullptr != menuRoot.subMap) {
        for (auto& type : *menuRoot.subMap) {
            root->addSubItem(createPluginMenu(type.first, type.second, addFn));
        }
    }
}

MenuLevel* PluginSearchWindow::addTypeMenu(MenuLevel* level, const String& type) {
    if (nullptr == level->subMap) {
        level->subMap = std::make_unique<std::map<String, MenuLevel>>();
    }

    if (level->type == MenuLevel::NONE) {
        level->type = MenuLevel::FORMAT;
    }

    auto& entry = (*level->subMap)[type];

    return &entry;
}

MenuLevel* PluginSearchWindow::addCategoryMenu(MenuLevel* level, const String& category) {
    for (auto subCategory : StringArray::fromTokens(category, "|", "")) {
        auto normalizedCategory = normalizeCategory(subCategory);
        if (normalizedCategory.isNotEmpty()) {
            if (nullptr == level->subMap) {
                level->subMap = std::make_unique<std::map<String, MenuLevel>>();
            }

            if (level->type == MenuLevel::NONE) {
                level->type = MenuLevel::CATEGORY;
            } else if (level->type != MenuLevel::CATEGORY) {
                // Plugin or company entries have already been added, but we have another sub category, so we
                // migrate them to "Other"
                auto& otherEntry = (*level->subMap)["Other"];

                if (nullptr == otherEntry.subMap) {
                    otherEntry.subMap = std::make_unique<std::map<String, MenuLevel>>();
                }

                for (auto it = level->subMap->begin(); it != level->subMap->end();) {
                    if (it->first != "Other") {
                        (*otherEntry.subMap)[it->first] = std::move(it->second);
                        it = level->subMap->erase(it);
                    } else {
                        it++;
                    }
                }

                if (nullptr != level->entryMap) {
                    if (nullptr == otherEntry.entryMap) {
                        otherEntry.entryMap = std::make_unique<std::map<String, ServerPlugin>>();
                    }

                    for (auto& kv : *level->entryMap) {
                        (*otherEntry.entryMap)[kv.first] = std::move(kv.second);
                    }

                    level->entryMap->clear();
                }

                level->type = MenuLevel::CATEGORY;
            }

            auto& entry = (*level->subMap)[normalizedCategory];
            level = &entry;
        }
    }
    return level;
}

MenuLevel* PluginSearchWindow::addCompanyMenu(MenuLevel* level, const String& company) {
    if (nullptr == level->subMap) {
        level->subMap = std::make_unique<std::map<String, MenuLevel>>();
    }

    if (level->type == MenuLevel::NONE) {
        level->type = MenuLevel::COMPANY;
    } else if (level->type == MenuLevel::CATEGORY) {
        // There are sub catories, but this plugin does not have one, so we move it to "Other"
        auto& otherEntry = (*level->subMap)["Other"];
        return addCompanyMenu(&otherEntry, company);
    }

    auto& entry = (*level->subMap)[normalizeCompany(company)];
    return &entry;
}

MenuLevel* PluginSearchWindow::findPluginLevel(MenuLevel* level) {
    if (level->type == MenuLevel::NONE) {
        level->type = MenuLevel::PLUGIN;
    }

    if (level->type != MenuLevel::PLUGIN) {
        if (nullptr == level->subMap) {
            level->subMap = std::make_unique<std::map<String, MenuLevel>>();
        }
        auto& otherEntry = (*level->subMap)["Other"];
        return findPluginLevel(&otherEntry);
    }

    return level;
}

TreeViewItem* PluginSearchWindow::createPluginMenu(const String& name, MenuLevel& level,
                                                   PluginSearchWindow::ClickFunction addFn) {
    traceScope();
    auto onOpenClose = [this] { updateHeight(); };
    auto* m = new TreeFolder(name, onOpenClose);
    if (nullptr != level.entryMap) {
        for (auto& pair : *level.entryMap) {
            auto& plug = pair.second;
            auto* plugEntry = new TreePlugin(plug, onOpenClose, /*m_showType*/ false, true);
            m->addSubItem(plugEntry);
            if (plug.getLayouts().isEmpty()) {
                plugEntry->addSubItem(new TreeLayout(plug, "Default", addFn));
            } else {
                for (auto& l : plug.getLayouts()) {
                    plugEntry->addSubItem(new TreeLayout(plug, l, addFn));
                }
            }
            plugEntry->setOpenness(TreeViewItem::Openness::opennessClosed);
        }
    }
    if (nullptr != level.subMap) {
        for (auto& pair : *level.subMap) {
            m->addSubItem(createPluginMenu(pair.first, pair.second, addFn));
        }
    }
    return m;
}

const String& PluginSearchWindow::normalizeCategory(const String& category) {
    static std::unordered_map<String, String> map = {{"Fx", "Effect"}, {"Synth", "Instrument"}, {"Waves", ""}};

    auto it = map.find(category);
    if (it != map.end()) {
        return it->second;
    }

    return category;
}

const String& PluginSearchWindow::normalizeCompany(const String& company) {
    static std::unordered_map<String, String> map = {{"iZotope, Inc.", "iZotope"},
                                                     {"Native Instruments GmbH", "Native Instruments"}};

    auto it = map.find(company);
    if (it != map.end()) {
        return it->second;
    }

    return company;
}

}  // namespace e47
