/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "WindowPositions.hpp"
#include "Defaults.hpp"

namespace e47 {

WindowPositions::WindowPositions()
    : LogTag("winpos"),
      m_file(this, Defaults::getConfigFileName(Defaults::WindowPositions), sizeof(WindowPositions::Positions)) {
    m_file.open();
    if (m_file.isOpen()) {
        m_positions = reinterpret_cast<Positions*>(m_file.data());
        logln("opened window positions file");
    }
}

WindowPositions::Position WindowPositions::getPosition(WindowPositions::PositionType t, const Position& def) const {
    Position ret;
    if (nullptr != m_positions) {
        switch (t) {
            case ServerSettings:
                ret = m_positions->ServerSettings;
                break;
            case ServerStats:
                ret = m_positions->ServerStats;
                break;
            case ServerPlugins:
                ret = m_positions->ServerPlugins;
                break;
            case PluginMonFx:
                ret = m_positions->PluginMonFx;
                break;
            case PluginMonInst:
                ret = m_positions->PluginMonInst;
                break;
            case PluginMonMidi:
                ret = m_positions->PluginMonMidi;
                break;
            case PluginStatsFx:
                ret = m_positions->PluginStatsFx;
                break;
            case PluginStatsInst:
                ret = m_positions->PluginStatsInst;
                break;
            case PluginStatsMidi:
                ret = m_positions->PluginStatsMidi;
                break;
        }
    }
    if (!ret.isEmpty()) {
        ret.setHeight(def.getHeight());
        ret.setWidth(def.getWidth());
    }
    return ret.isEmpty() ? def : ret;
}

void WindowPositions::setPosition(WindowPositions::PositionType t, WindowPositions::Position p) {
    if (nullptr != m_positions) {
        switch (t) {
            case ServerSettings:
                m_positions->ServerSettings = p;
                break;
            case ServerStats:
                m_positions->ServerStats = p;
                break;
            case ServerPlugins:
                m_positions->ServerPlugins = p;
                break;
            case PluginMonFx:
                m_positions->PluginMonFx = p;
                break;
            case PluginMonInst:
                m_positions->PluginMonInst = p;
                break;
            case PluginMonMidi:
                m_positions->PluginMonMidi = p;
                break;
            case PluginStatsFx:
                m_positions->PluginStatsFx = p;
                break;
            case PluginStatsInst:
                m_positions->PluginStatsInst = p;
                break;
            case PluginStatsMidi:
                m_positions->PluginStatsMidi = p;
                break;
        }
    }
}

WindowPositions::Position WindowPositions::get(WindowPositions::PositionType t, const WindowPositions::Position& def) {
    auto inst = getInstance();
    if (nullptr != inst) {
        return inst->getPosition(t, def);
    }
    return def;
}

void WindowPositions::set(WindowPositions::PositionType t, WindowPositions::Position p) {
    auto inst = getInstance();
    if (nullptr != inst) {
        inst->setPosition(t, p);
    }
}

}  // namespace e47
