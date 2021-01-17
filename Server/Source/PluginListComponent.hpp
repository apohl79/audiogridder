/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Heavily based on juce_audio_processors/scanning/juce_PluginListComponent.h
 */

#ifndef PluginListComponent_hpp
#define PluginListComponent_hpp

#include <JuceHeader.h>
#include <set>

namespace e47 {

class AudioGridderPluginListComponent : public Component, public FileDragAndDropTarget, private ChangeListener {
  public:
    AudioGridderPluginListComponent(AudioPluginFormatManager& formatManager, KnownPluginList& listToRepresent,
                                    std::set<String>& exList, const File& deadMansPedalFile);

    ~AudioGridderPluginListComponent() override;

    PopupMenu createMenuForRow(int rowNumber);

    void removeSelectedPlugins();

    void setTableModel(TableListBoxModel*);

    TableListBox& getTableListBox() noexcept { return table; }

  private:
    AudioPluginFormatManager& formatManager;
    KnownPluginList& list;
    std::set<String>& excludeList;
    File deadMansPedalFile;
    TableListBox table;
    String dialogTitle, dialogText;

    class TableModel;
    std::unique_ptr<TableListBoxModel> tableModel;

    void updateList();
    void removeMissingPlugins();
    void removePluginItem(int index);
    void addPluginItem(int index);
    void rescanPluginItem(int index);

    void resized() override;
    bool isInterestedInFileDrag(const StringArray&) override;
    void filesDropped(const StringArray&, int, int) override;
    void changeListenerCallback(ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGridderPluginListComponent)
};

}  // namespace e47

#endif  // PluginListComponent_hpp
