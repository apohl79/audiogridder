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

class PluginListComponent : public Component, public FileDragAndDropTarget, private ChangeListener {
  public:
    PluginListComponent(AudioPluginFormatManager& formatManager, KnownPluginList& listToRepresent,
                        std::set<String>& exList, const File& deadMansPedalFile);
    ~PluginListComponent() override;

    PopupMenu createMenuForRow(int rowNumber);

  private:
    AudioPluginFormatManager& m_formatManager;
    KnownPluginList& m_list;
    std::set<String>& m_excludeList;
    File m_deadMansPedalFile;
    TableListBox m_table;
    String m_dialogTitle, m_dialogText;

    class TableModel;
    std::unique_ptr<TableModel> m_tableModel;

    void updateList();
    void removeMissingPlugins();
    void removePluginItems(const std::vector<int> indexes);
    void addPluginItems(const std::vector<int> indexes);
    void rescanPluginItems(const std::vector<int> indexes);

    void resized() override;
    bool isInterestedInFileDrag(const StringArray&) override { return false; }
    void filesDropped(const StringArray&, int, int) override {}
    void changeListenerCallback(ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListComponent)
};

}  // namespace e47

#endif  // PluginListComponent_hpp
