/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Heavily based on juce_audio_processors/scanning/juce_PluginListComponent.cpp
 */

#include "PluginListComponent.hpp"
#include "App.hpp"
#include "Server.hpp"
#include "Utils.hpp"

namespace e47 {

class PluginListComponent::TableModel : public TableListBoxModel {
  public:
    TableModel(PluginListComponent& c, KnownPluginList& l, std::set<String>& e) : owner(c), list(l), exlist(e) {}

    int getNumRows() override {
        return list.getNumTypes() + list.getBlacklistedFiles().size() + static_cast<int>(exlist.size());
    }

    void paintRowBackground(Graphics& g, int /*rowNumber*/, int /*width*/, int /*height*/,
                            bool rowIsSelected) override {
        const auto defaultColour = owner.findColour(ListBox::backgroundColourId);
        const auto c = rowIsSelected ? defaultColour.interpolatedWith(owner.findColour(ListBox::textColourId), 0.1f)
                                     : defaultColour;

        g.fillAll(c);
    }

    enum { nameCol = 1, typeCol = 2, categoryCol = 3, manufacturerCol = 4, descCol = 5 };

    bool isBlacklisted(int row) const { return row >= list.getNumTypes(); }
    bool isExcluded(int row) const { return row >= list.getNumTypes() + list.getBlacklistedFiles().size(); }

    void paintCell(Graphics& g, int row, int columnId, int width, int height, bool /*rowIsSelected*/) override {
        String text;
        bool blacklisted = isBlacklisted(row);
        bool excluded = isExcluded(row);

        if (blacklisted) {
            String name;
            int idx = row - list.getNumTypes();
            if (excluded) {
                idx -= list.getBlacklistedFiles().size();
                auto it = exlist.begin();
                while (idx-- > 0 && it != exlist.end()) it++;
                if (it != exlist.end()) {
                    name = *it;
                } else {
                    name = "out of range";
                }
            } else {
                name = list.getBlacklistedFiles()[idx];
            }

            File f(name);
            String type;
            if (f.exists()) {
                name = f.getFileNameWithoutExtension();
                type = f.getFileExtension().toUpperCase().substring(1);
#if JUCE_MAC

            } else if (name.startsWith("AudioUnit")) {
                AudioUnitPluginFormat fmt;
                name = fmt.getNameOfPluginFromIdentifier(name);
                type = "AudioUnit";
#endif
            }

            if (columnId == nameCol) {
                text = name;
            } else if (columnId == typeCol) {
                text = type;
            } else if (columnId == descCol) {
                text = excluded ? "Deactivated" : "Failed";
            }
        } else {
            auto desc = list.getTypes()[row];

            switch (columnId) {
                case nameCol:
                    text = desc.name;
                    break;
                case typeCol:
                    text = desc.pluginFormatName;
                    break;
                case categoryCol:
                    text = desc.category.isNotEmpty() ? desc.category : "-";
                    break;
                case manufacturerCol:
                    text = desc.manufacturerName;
                    break;
                case descCol:
                    text = getPluginDescription(desc);
                    break;

                default:
                    jassertfalse;
                    break;
            }
        }

        if (text.isNotEmpty()) {
            auto col = owner.findColour(ListBox::textColourId);
            if (blacklisted) {
                col = excluded ? Colours::grey : Colours::red;
            } else {
                col = columnId == nameCol ? col : col.interpolatedWith(Colours::transparentBlack, 0.3f);
            }
            g.setColour(col);
            g.setFont(Font(height * 0.7f, Font::bold));
            g.drawFittedText(text, 4, 0, width - 6, height, Justification::centredLeft, 1, 0.9f);
        }
    }

    void selectedRowsChanged(int) override {
        selectedRows.clear();

        auto rowsSet = owner.m_table.getSelectedRows();

        for (auto& range : rowsSet.getRanges()) {
            for (int row = range.getStart(); row < range.getEnd(); row++) {
                selectedRows.push_back(row);
            }
        }
    }

    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override {
        TableListBoxModel::cellClicked(rowNumber, columnId, e);

        if (rowNumber >= 0 && rowNumber < getNumRows() && e.mods.isPopupMenu()) {
            owner.createMenuForRow(rowNumber).showMenuAsync(PopupMenu::Options().withDeletionCheck(owner));
        }
    }

    void deleteKeyPressed(int) override { owner.removePluginItems(selectedRows); }

    void sortOrderChanged(int newSortColumnId, bool isForwards) override {
        switch (newSortColumnId) {
            case nameCol:
                list.sort(KnownPluginList::sortAlphabetically, isForwards);
                break;
            case typeCol:
                list.sort(KnownPluginList::sortByFormat, isForwards);
                break;
            case categoryCol:
                list.sort(KnownPluginList::sortByCategory, isForwards);
                break;
            case manufacturerCol:
                list.sort(KnownPluginList::sortByManufacturer, isForwards);
                break;
            case descCol:
                break;

            default:
                jassertfalse;
                break;
        }
    }

    static String getPluginDescription(const PluginDescription& desc) {
        StringArray items;

        if (desc.descriptiveName != desc.name) items.add(desc.descriptiveName);

        items.add(desc.version);

        items.removeEmptyStrings();
        return items.joinIntoString(" - ");
    }

    PluginListComponent& owner;
    KnownPluginList& list;
    std::set<String>& exlist;
    std::vector<int> selectedRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TableModel)
};

PluginListComponent::PluginListComponent(AudioPluginFormatManager& manager, KnownPluginList& listToEdit,
                                         std::set<String>& exList, const File& deadMansPedal)
    : m_formatManager(manager), m_list(listToEdit), m_excludeList(exList), m_deadMansPedalFile(deadMansPedal) {
    m_tableModel.reset(new TableModel(*this, listToEdit, exList));

    TableHeaderComponent& header = m_table.getHeader();

    header.addColumn(TRANS("Name"), TableModel::nameCol, 200, 100, 700,
                     TableHeaderComponent::defaultFlags | TableHeaderComponent::sortedForwards);
    header.addColumn(TRANS("Format"), TableModel::typeCol, 80, 80, 80, TableHeaderComponent::notResizable);
    header.addColumn(TRANS("Category"), TableModel::categoryCol, 100, 100, 200);
    header.addColumn(TRANS("Manufacturer"), TableModel::manufacturerCol, 200, 100, 300);
    header.addColumn(TRANS("Description"), TableModel::descCol, 100, 100, 500, TableHeaderComponent::notSortable);

    m_table.setHeaderHeight(22);
    m_table.setRowHeight(20);
    m_table.setModel(m_tableModel.get());
    m_table.setMultipleSelectionEnabled(true);
    addAndMakeVisible(m_table);

    setSize(400, 600);
    m_list.addChangeListener(this);
    updateList();
    m_table.getHeader().reSortTable();

    PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal(m_list, m_deadMansPedalFile);
    m_deadMansPedalFile.deleteFile();
}

PluginListComponent::~PluginListComponent() { m_list.removeChangeListener(this); }

void PluginListComponent::resized() {
    auto r = getLocalBounds().reduced(2);
    m_table.setBounds(r);
}

void PluginListComponent::changeListenerCallback(ChangeBroadcaster*) {
    m_table.getHeader().reSortTable();
    updateList();
}

void PluginListComponent::updateList() {
    m_table.updateContent();
    m_table.repaint();
}

void PluginListComponent::removeMissingPlugins() {
    auto types = m_list.getTypes();

    for (int i = types.size(); --i >= 0;) {
        auto type = types.getUnchecked(i);

        if (!m_formatManager.doesPluginStillExist(type)) {
            m_list.removeType(type);
        }
    }
}

void PluginListComponent::removePluginItems(const std::vector<int> indexes) {
    auto types = m_list.getTypes();

    for (int index : indexes) {
        if (index < types.size()) {
            auto p = types[index];
            if (!p.pluginFormatName.compare("AudioUnit")) {
                m_excludeList.insert(p.descriptiveName);
            } else {
                m_excludeList.insert(p.fileOrIdentifier);
            }
            m_list.removeType(p);
        }
    }

    getApp()->getServer()->saveConfig();
}

void PluginListComponent::addPluginItems(const std::vector<int> indexes) {
    auto types = m_list.getTypes();
    int numTypes = types.size();
    int numBlacklistedFiles = m_list.getBlacklistedFiles().size();
    std::vector<String> names;

    for (int index : indexes) {
        if (index >= (numTypes + numBlacklistedFiles)) {
            index -= numTypes;
            index -= numBlacklistedFiles;
            auto it = m_excludeList.begin();
            while (index-- > 0 && it != m_excludeList.end()) it++;
            if (it != m_excludeList.end()) {
                auto name = *it;
                names.push_back(name);
            }
        }
    }

    for (auto& name : names) {
        m_excludeList.erase(name);
    }

    getApp()->getServer()->saveConfig();
    getApp()->getServer()->addPlugins(names, [this, names](bool success) {
        if (!success) {
            for (auto& name : names) {
                m_excludeList.insert(name);
            }
        }
    });
}

void PluginListComponent::rescanPluginItems(const std::vector<int> indexes) {
    auto types = m_list.getTypes();
    int numTypes = types.size();
    int numBlacklistedFiles = m_list.getBlacklistedFiles().size();
    std::vector<String> ids;

    for (int index : indexes) {
        if (index >= numTypes && index < (numTypes + numBlacklistedFiles)) {
            index -= numTypes;
            auto id = m_list.getBlacklistedFiles()[index];
            ids.push_back(id);
        }
    }

    for (auto& id : ids) {
        m_list.removeFromBlacklist(id);
        getApp()->getServer()->saveKnownPluginList();
    }
}

PopupMenu PluginListComponent::createMenuForRow(int rowNumber) {
    PopupMenu menu;

    if (rowNumber >= 0 && rowNumber < m_tableModel->getNumRows()) {
        if (!m_tableModel->isBlacklisted(rowNumber)) {
            menu.addItem("Deactivate", [this] {
                removePluginItems(m_tableModel->selectedRows);
                m_table.deselectAllRows();
            });
        } else if (m_tableModel->isExcluded(rowNumber)) {
            menu.addItem("Activate", [this] {
                addPluginItems(m_tableModel->selectedRows);
                m_table.deselectAllRows();
            });
        } else {
            menu.addItem("Remove from blacklist", [this] {
                rescanPluginItems(m_tableModel->selectedRows);
                m_table.deselectAllRows();
            });
        }
    }

    return menu;
}

}  // namespace e47
