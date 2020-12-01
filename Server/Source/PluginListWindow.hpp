/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef PluginListWindow_hpp
#define PluginListWindow_hpp

#include <JuceHeader.h>

#include "PluginListComponent.hpp"

namespace e47 {

class App;

class PluginListWindow : public DocumentWindow {
  public:
    PluginListWindow(App* app, KnownPluginList& list, const String& deadMansPedalFile);
    ~PluginListWindow() override;

    void closeButtonPressed() override;

  private:
    App* m_app;
    AudioPluginFormatManager m_plugmgr;
    KnownPluginList& m_pluginlist;
    File m_deadMansPedalFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListWindow)
};

}  // namespace e47

#endif /* PluginListWindow_hpp */
