/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef __WINDOWPOSITIONS_H_
#define __WINDOWPOSITIONS_H_

#include <JuceHeader.h>

#include "Utils.hpp"
#include "SharedInstance.hpp"
#include "MemoryFile.hpp"

namespace e47 {

class WindowPositions : public LogTag, public SharedInstance<WindowPositions> {
  public:
    WindowPositions();

    using Position = juce::Rectangle<int>;

    struct Positions {
        Position ServerSettings;
        Position ServerStats;
        Position ServerPlugins;
        Position PluginMonFx;
        Position PluginMonInst;
        Position PluginMonMidi;
        Position PluginStatsFx;
        Position PluginStatsInst;
        Position PluginStatsMidi;
    };

    enum PositionType {
        ServerSettings,
        ServerStats,
        ServerPlugins,
        PluginMonFx,
        PluginMonInst,
        PluginMonMidi,
        PluginStatsFx,
        PluginStatsInst,
        PluginStatsMidi
    };

    Position getPosition(PositionType t, const Position& def) const;
    void setPosition(PositionType t, Position p);

    static Position get(PositionType t, const Position& def);
    static void set(PositionType t, Position p);

  private:
    MemoryFile m_file;
    Positions* m_positions = nullptr;
};

}  // namespace e47

#endif  // __WINDOWPOSITIONS_H_
