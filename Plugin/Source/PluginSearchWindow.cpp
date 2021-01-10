/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginSearchWindow.hpp"

namespace e47 {

PluginSearchWindow::PluginSearchWindow(float x, float y, AudioGridderAudioProcessor& p)
    : TopLevelWindow("New Server", true), LogTagDelegate(&p.getClient()), m_processor(p) {

    setWantsKeyboardFocus(false);

    int totalWidth = 250;
    int totalHeight = 35;

    setBounds((int)lroundf(x), (int)lroundf(y), totalWidth, totalHeight);

    m_search.setBounds(5, 5, totalWidth - 10, 25);
    m_search.setWantsKeyboardFocus(true);
    m_search.setExplicitFocusOrder(1);
    m_search.addKeyListener(this);
    m_search.onTextChange = [this] {
        updateTree(m_search.getText());
        updateHeight();
    };
    addAndMakeVisible(&m_search);

    m_tree.setExplicitFocusOrder(2);
    m_tree.addKeyListener(this);
    m_tree.addMouseListener(this, true);
    m_tree.setIndentSize(10);
    m_tree.setRootItem(new TreeRoot());
    m_tree.setRootItemVisible(false);
    m_tree.setColour(TreeView::backgroundColourId, Colour(Defaults::BG_COLOR));
    m_tree.setColour(TreeView::evenItemsColourId, Colour(Defaults::BG_COLOR));
    m_tree.setColour(TreeView::oddItemsColourId, Colour(Defaults::BG_COLOR));
    addAndMakeVisible(&m_tree);

    m_recents = m_processor.getClient().getRecents();
    updateTree();
    updateHeight();

    setVisible(true);
}

PluginSearchWindow::~PluginSearchWindow() {
    m_search.removeKeyListener(this);
    m_tree.removeKeyListener(this);
    m_tree.removeMouseListener(this);
}

void PluginSearchWindow::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
}

bool PluginSearchWindow::keyPressed(const KeyPress& kp, Component*) {
    if (kp.isKeyCurrentlyDown(KeyPress::escapeKey)) {
        delete this;
        return true;
    }
    return false;
}

void PluginSearchWindow::mouseMove(const MouseEvent& e){
    if (m_tree.isMouseOver(true)) {
        auto* item = m_tree.getItemAt(e.getPosition().getY());
        if (nullptr != item && !item->isSelected()) {
            item->setSelected(true, true);
        }
    }
}

void PluginSearchWindow::mouseExit(const MouseEvent&) {
    m_tree.clearSelectedItems();
}

void PluginSearchWindow::updateHeight() {
    int items = m_tree.getNumRowsInTree();
    if (items > MAX_ITEMS_VISIBLE) {
        items = MAX_ITEMS_VISIBLE;
    }
    int totalHeight = 40 + ITEM_HEIGHT * items;
    if (m_search.isEmpty() && !m_recents.empty()) {
        // remove some pixels, as the separator is not using the full height
        totalHeight += SEPARATOR_HEIGHT - ITEM_HEIGHT;
    }
    if (totalHeight != getHeight()) {
        m_tree.setBounds(5, 35, getWidth() - 10, totalHeight - 40);
        setBounds(getX(), getY(), getWidth(), totalHeight);
    }
}

void PluginSearchWindow::activeWindowStatusChanged() {
    TopLevelWindow::activeWindowStatusChanged();
    if (!isActiveWindow()) {
        delete this;
    }
}

void PluginSearchWindow::updateTree(const String& filter) {
    traceScope();
    logln("filter = " << filter);

    auto addFn = [this](const ServerPlugin& p) {
        traceScope();
        if (m_onClick) {
            m_onClick(p);
        }
        delete this;
    };

    auto* root = m_tree.getRootItem();
    root->clearSubItems();

    m_tree.setDefaultOpenness(filter.isNotEmpty());

    if (filter.isEmpty() && !m_recents.empty()) {
        for (const auto& plug : m_recents) {
            root->addSubItem(new TreePlugin(plug, addFn));
        }
        root->addSubItem(new TreeSeparator());
    }

    // ceate menu structure: type -> [category] -> [company] -> plugin
    std::map<String, MenuLevel> menuMap;
    for (const auto& type : m_processor.getPluginTypes()) {
        for (const auto& plug : m_processor.getPlugins(type)) {
            if (filter.isNotEmpty() && !plug.getName().containsIgnoreCase(filter)) {
                continue;
            }
            auto& typeEntry = menuMap[type];
            if (nullptr == typeEntry.subMap) {
                typeEntry.subMap = std::make_unique<std::map<String, MenuLevel>>();
            }
            auto* level = &typeEntry;
            if (m_processor.getMenuShowCategory()) {
                if (nullptr == level->subMap) {
                    level->subMap = std::make_unique<std::map<String, MenuLevel>>();
                }
                auto& entry = (*level->subMap)[plug.getCategory()];
                level = &entry;
            }
            if (m_processor.getMenuShowCompany()) {
                if (nullptr == level->subMap) {
                    level->subMap = std::make_unique<std::map<String, MenuLevel>>();
                }
                auto& entry = (*level->subMap)[plug.getCompany()];
                level = &entry;
            }
            if (nullptr == level->entryMap) {
                level->entryMap = std::make_unique<std::map<String, ServerPlugin>>();
            }
            (*level->entryMap)[plug.getName()] = plug;
        }
    }
    for (auto& type : menuMap) {
        root->addSubItem(createPluginMenu(type.first, type.second, addFn));
    }
}

TreeViewItem* PluginSearchWindow::createPluginMenu(const String& name, MenuLevel& level,
                                                   std::function<void(const ServerPlugin& plug)> addFn) {
    traceScope();
    auto* m = new TreeFolder(name, [this] { updateHeight(); });
    if (nullptr != level.entryMap) {
        for (auto& pair : *level.entryMap) {
            auto& plug = pair.second;
            m->addSubItem(new TreePlugin(plug, addFn));
        }
    }
    if (nullptr != level.subMap) {
        for (auto& pair : *level.subMap) {
            m->addSubItem(createPluginMenu(pair.first, pair.second, addFn));
        }
    }
    return m;
}

}  // namespace e47
