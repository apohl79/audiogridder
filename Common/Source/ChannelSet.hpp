/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _CHANNELSET_HPP_
#define _CHANNELSET_HPP_

#include <JuceHeader.h>
#include <bitset>

#include "Defaults.hpp"

namespace e47 {

class ChannelSet {
  public:
#if AG_PLUGIN
    ChannelSet() {
#if JucePlugin_IsSynth || JucePlugin_IsMidiEffect
        m_outputOffset = 0;
#else
        m_outputOffset = Defaults::PLUGIN_CHANNELS_MAX / 2;
#endif
    }
    ChannelSet(uint64 i) : ChannelSet() { m_channels = i; }
#else
    ChannelSet() {}
    ChannelSet(uint64 i, bool withInput)
        : m_channels(i), m_outputOffset(withInput ? Defaults::PLUGIN_CHANNELS_MAX / 2 : 0) {}
#endif

    ChannelSet(uint64 channels, int numInputs, int numOutputs)
        : m_channels(channels),
          m_outputOffset(numInputs > 0 ? (size_t)jmin(numInputs, Defaults::PLUGIN_CHANNELS_MAX) : 0),
          m_numInputs(jmin(numInputs, Defaults::PLUGIN_CHANNELS_MAX)),
          m_numOutputs(jmin(numOutputs, Defaults::PLUGIN_CHANNELS_MAX)) {}

    ChannelSet& operator=(uint64 i) {
        m_channels = i;
        return *this;
    }

    uint64 toInt() const { return m_channels.to_ullong(); }

    void setNumChannels(int numInputs, int numOutputs) {
        m_numInputs = jmin(numInputs, Defaults::PLUGIN_CHANNELS_MAX);
        m_numOutputs = jmin(numOutputs, Defaults::PLUGIN_CHANNELS_MAX);
    }

    void setNumChannels(int numInputs, int numOutputs, size_t outputOffset) {
        setNumChannels(numInputs, numOutputs);
        m_outputOffset = outputOffset;
    }

    void setWithInput(bool withInput) { m_outputOffset = withInput ? Defaults::PLUGIN_CHANNELS_MAX / 2 : 0; }

    bool isInput(int ch) const { return (size_t)ch < m_outputOffset; }
    bool isOutput(int ch) const { return (size_t)ch >= m_outputOffset && (size_t)ch < Defaults::PLUGIN_CHANNELS_MAX; }
    void setActive(size_t ch, bool input, bool active = true) { setBit(getChannelIndex(ch, input), active); }
    void setInputActive(int ch, bool active = true) { setActive((size_t)ch, true, active); }
    void setOutputActive(int ch, bool active = true) { setActive((size_t)ch, false, active); }
    bool isActive(size_t ch, bool input) const { return isSet(getChannelIndex(ch, input)); }
    bool isInputActive(int ch) const { return isActive((size_t)ch, true); }
    bool isOutputActive(int ch) const { return isActive((size_t)ch, false); }

    void setRangeActive(size_t start = 0, size_t end = Defaults::PLUGIN_CHANNELS_MAX, bool active = true) {
        for (size_t ch = start; ch < jmin(end, (size_t)Defaults::PLUGIN_CHANNELS_MAX); ch++) {
            setBit(ch, active);
        }
    }

    bool isRangeActive(size_t start = 0, size_t end = Defaults::PLUGIN_CHANNELS_MAX) const {
        for (auto ch = start; ch < jmin(end, (size_t)Defaults::PLUGIN_CHANNELS_MAX); ch++) {
            if (!m_channels.test(ch)) {
                return false;
            }
        }
        return true;
    }

    void setInputRangeActive(bool active = true) { setRangeActive(getStart(true), getEnd(true), active); }
    void setOutputRangeActive(bool active = true) { setRangeActive(getStart(false), getEnd(false), active); }
    bool isInputRangeActive() const { return isRangeActive(getStart(true), getEnd(true)); }
    bool isOutputRangeActive() const { return isRangeActive(getStart(false), getEnd(false)); }

    int getNumChannels(bool input) const { return input ? m_numInputs : m_numOutputs; }
    int getNumChannelsCombined() const { return jmax(getNumChannels(true), getNumChannels(false)); }

    int getNumActiveChannels(size_t start, size_t end = Defaults::PLUGIN_CHANNELS_MAX) const {
        int ret = 0;
        for (auto ch = start; ch < jmin(end, (size_t)Defaults::PLUGIN_CHANNELS_MAX); ch++) {
            ret += m_channels.test(ch) ? 1 : 0;
        }
        return ret;
    }

    int getNumActiveChannels(bool input) const { return getNumActiveChannels(getStart(input), getEnd(input)); }
    int getNumActiveChannelsCombined() const { return jmax(getNumActiveChannels(true), getNumActiveChannels(false)); }

    std::vector<size_t> getActiveChannels(bool input) const {
        std::vector<size_t> ret;
        for (auto ch = getStart(input); ch < getEnd(input); ch++) {
            if (isSet(ch)) {
                ret.push_back(input ? ch : ch - m_outputOffset);
            }
        }
        return ret;
    }

    String toString() const {
        String out;
        StringArray inputs;
        for (auto ch : getActiveChannels(true)) {
            inputs.add(String(ch));
        }
        StringArray outputs;
        for (auto ch : getActiveChannels(false)) {
            outputs.add(String(ch));
        }
        if (!inputs.isEmpty()) {
            out << "inputs: " << inputs.joinIntoString(",");
        }
        if (!outputs.isEmpty()) {
            out << "outputs: " << outputs.joinIntoString(",");
        }
        return out;
    }

    static String toString(uint64 c, int numInputs, int numOutputs) {
        ChannelSet cs(c, numInputs, numOutputs);
        return cs.toString();
    }

  private:
    std::bitset<e47::Defaults::PLUGIN_CHANNELS_MAX> m_channels;
    size_t m_outputOffset = 0;
    int m_numInputs = -1;
    int m_numOutputs = -1;

    inline size_t getChannelIndex(size_t ch, bool input) const { return ch + (input ? 0 : m_outputOffset); }

    inline void setBit(size_t ch, bool active = true) {
        if (ch < Defaults::PLUGIN_CHANNELS_MAX) {
            m_channels.set(ch, active);
        }
    }

    inline size_t getStart(bool input) const { return input ? 0 : m_outputOffset; }

    inline size_t getEnd(bool input) const {
        return input ? (m_numInputs > -1 ? (size_t)m_numInputs : m_outputOffset)
                     : (m_numOutputs > -1 ? (size_t)m_numOutputs + m_outputOffset
                                          : (size_t)Defaults::PLUGIN_CHANNELS_MAX);
    }

    inline bool isSet(size_t ch) const { return ch < Defaults::PLUGIN_CHANNELS_MAX ? m_channels.test(ch) : false; }
};

}  // namespace e47

#endif  // _CHANNELSET_HPP_
