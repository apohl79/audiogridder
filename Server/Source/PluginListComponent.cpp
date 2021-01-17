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
#include "Utils.hpp"

namespace e47 {

class AudioGridderPluginListComponent::TableModel : public TableListBoxModel {
  public:
    TableModel(AudioGridderPluginListComponent& c, KnownPluginList& l, std::set<String>& e)
        : owner(c), list(l), exlist(e) {}

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
            if (columnId == nameCol) {
                text = name;
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

    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override {
        TableListBoxModel::cellClicked(rowNumber, columnId, e);

        if (rowNumber >= 0 && rowNumber < getNumRows() && e.mods.isPopupMenu())
            owner.createMenuForRow(rowNumber).showMenuAsync(PopupMenu::Options().withDeletionCheck(owner));
    }

    void deleteKeyPressed(int) override { owner.removeSelectedPlugins(); }

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

    AudioGridderPluginListComponent& owner;
    KnownPluginList& list;
    std::set<String>& exlist;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TableModel)
};

AudioGridderPluginListComponent::AudioGridderPluginListComponent(AudioPluginFormatManager& manager,
                                                                 KnownPluginList& listToEdit, std::set<String>& exList,
                                                                 const File& deadMansPedal)
    : formatManager(manager), list(listToEdit), excludeList(exList), deadMansPedalFile(deadMansPedal) {
    tableModel.reset(new TableModel(*this, listToEdit, exList));

    TableHeaderComponent& header = table.getHeader();

    header.addColumn(TRANS("Name"), TableModel::nameCol, 200, 100, 700,
                     TableHeaderComponent::defaultFlags | TableHeaderComponent::sortedForwards);
    header.addColumn(TRANS("Format"), TableModel::typeCol, 80, 80, 80, TableHeaderComponent::notResizable);
    header.addColumn(TRANS("Category"), TableModel::categoryCol, 100, 100, 200);
    header.addColumn(TRANS("Manufacturer"), TableModel::manufacturerCol, 200, 100, 300);
    header.addColumn(TRANS("Description"), TableModel::descCol, 100, 100, 500, TableHeaderComponent::notSortable);

    table.setHeaderHeight(22);
    table.setRowHeight(20);
    table.setModel(tableModel.get());
    table.setMultipleSelectionEnabled(true);
    addAndMakeVisible(table);

    setSize(400, 600);
    list.addChangeListener(this);
    updateList();
    table.getHeader().reSortTable();

    PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal(list, deadMansPedalFile);
    deadMansPedalFile.deleteFile();
}

AudioGridderPluginListComponent::~AudioGridderPluginListComponent() { list.removeChangeListener(this); }

void AudioGridderPluginListComponent::resized() {
    auto r = getLocalBounds().reduced(2);
    table.setBounds(r);
}

void AudioGridderPluginListComponent::changeListenerCallback(ChangeBroadcaster*) {
    table.getHeader().reSortTable();
    updateList();
}

void AudioGridderPluginListComponent::updateList() {
    table.updateContent();
    table.repaint();
}

void AudioGridderPluginListComponent::removeSelectedPlugins() {
    auto selected = table.getSelectedRows();

    for (int i = table.getNumRows(); --i >= 0;) {
        if (selected.contains(i)) {
            removePluginItem(i);
        }
    }
}

void AudioGridderPluginListComponent::setTableModel(TableListBoxModel* model) {
    table.setModel(nullptr);
    tableModel.reset(model);
    table.setModel(tableModel.get());
    table.getHeader().reSortTable();
    table.updateContent();
    table.repaint();
}

void AudioGridderPluginListComponent::removeMissingPlugins() {
    auto types = list.getTypes();

    for (int i = types.size(); --i >= 0;) {
        auto type = types.getUnchecked(i);

        if (!formatManager.doesPluginStillExist(type)) list.removeType(type);
    }
}

void AudioGridderPluginListComponent::removePluginItem(int index) {
    if (index < list.getNumTypes()) {
        auto p = list.getTypes()[index];
        if (!p.pluginFormatName.compare("AudioUnit")) {
            excludeList.insert(p.descriptiveName);
        } else {
            excludeList.insert(p.fileOrIdentifier);
        }
        list.removeType(p);
        getApp()->getServer().saveConfig();
    }
}

void AudioGridderPluginListComponent::addPluginItem(int index) {
    if (index >= (list.getNumTypes() + list.getBlacklistedFiles().size())) {
        index -= list.getNumTypes();
        index -= list.getBlacklistedFiles().size();
        auto it = excludeList.begin();
        while (index-- > 0 && it != excludeList.end()) it++;
        if (it != excludeList.end()) {
            auto name = *it;
            excludeList.erase(it);
            // try to add plugin
            std::vector<String> v = {name};
            getApp()->getServer().addPlugins(v, [this, name](bool success) {
                if (!success) {
                    excludeList.insert(name);
                }
            });
        }
    }
}

void AudioGridderPluginListComponent::rescanPluginItem(int index) {
    if (index >= list.getNumTypes() && index < (list.getNumTypes() + list.getBlacklistedFiles().size())) {
        index -= list.getNumTypes();
        auto id = list.getBlacklistedFiles()[index];
        list.removeFromBlacklist(id);
        getApp()->getServer().saveKnownPluginList();
    }
}

PopupMenu AudioGridderPluginListComponent::createMenuForRow(int rowNumber) {
    PopupMenu menu;

    if (rowNumber >= 0 && rowNumber < tableModel->getNumRows()) {
        bool blacklisted = dynamic_cast<TableModel*>(tableModel.get())->isBlacklisted(rowNumber);
        bool excluded = dynamic_cast<TableModel*>(tableModel.get())->isExcluded(rowNumber);
        if (!blacklisted) {
            menu.addItem(PopupMenu::Item("Deactivate").setAction([this, rowNumber] { removePluginItem(rowNumber); }));
        } else if (excluded) {
            menu.addItem(PopupMenu::Item("Activate").setAction([this, rowNumber] { addPluginItem(rowNumber); }));
        } else {
            menu.addItem(
                PopupMenu::Item("Remove from blacklist (Force rescan at next start)").setAction([this, rowNumber] {
                    rescanPluginItem(rowNumber);
                }));
        }
    }

    return menu;
}

bool AudioGridderPluginListComponent::isInterestedInFileDrag(const StringArray& /*files*/) { return true; }

void AudioGridderPluginListComponent::filesDropped(const StringArray& files, int, int) {
    OwnedArray<PluginDescription> typesFound;
    list.scanAndAddDragAndDroppedFiles(formatManager, files, typesFound);
}

}  // namespace e47
