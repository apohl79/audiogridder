/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginSearchWindow.hpp"
#include "PluginEditor.hpp"

namespace e47 {

PluginSearchWindow::PluginSearchWindow(float x, float y, AudioGridderAudioProcessor& p)
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

    m_recents = m_processor.getClient().getRecents();
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
    traceScope();
    delete this;
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
            auto item = dynamic_cast<TreePlugin*>(m_tree.getSelectedItem(0));
            if (nullptr != item) {
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

void PluginSearchWindow::updateHeight() {
    traceScope();
    int items = m_tree.getNumRowsInTree();
    if (items > MAX_ITEMS_VISIBLE) {
        items = MAX_ITEMS_VISIBLE;
    }
    int totalHeight = 40 + ITEM_HEIGHT * items;
    int minHeight = 40 + ITEM_HEIGHT * MIN_ITEMS_VISIBLE;
    if (m_search.isEmpty() && !m_recents.empty()) {
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

    auto addFn = [this](const ServerPlugin& p) {
        traceScope();
        if (m_onClick) {
            m_onClick(p);
        }
        hide();
    };

    auto* root = m_tree.getRootItem();
    root->clearSubItems();

    m_tree.setDefaultOpenness(filter.isNotEmpty());

    if (filter.isEmpty() && !m_recents.empty()) {
        for (const auto& plug : m_recents) {
            root->addSubItem(new TreePlugin(plug, addFn, m_showType));
        }
        root->addSubItem(new TreeSeparator());
    }

    auto filterParts = StringArray::fromTokens(filter, " ", "");

    // ceate menu structure: type -> [category] -> [company] -> plugin
    std::map<String, MenuLevel> menuMap;
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
            m->addSubItem(new TreePlugin(plug, addFn, m_showType));
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
