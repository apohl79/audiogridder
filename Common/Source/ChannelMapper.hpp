/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _CHANNELMAPPER_HPP_
#define _CHANNELMAPPER_HPP_

#include <JuceHeader.h>
#include <set>

#include "Utils.hpp"
#include "ChannelSet.hpp"

namespace e47 {

class ChannelMapper : public LogTagDelegate {
  public:
    ChannelMapper(LogTag* tag) : LogTagDelegate(tag) {}
    ChannelMapper(LogTag* tag, const ChannelSet& activeChannels) : ChannelMapper(tag) { createMapping(activeChannels); }

    // Creates a mapping to copy channels one buffer to a reduced buffer containing only the active channels provided
    void createMapping(const ChannelSet& activeChannels) {
        reset();
        int chSrc = 0, chDst = 0;
        if (activeChannels.getNumChannels(true)) {  // fx
#if AG_PLUGIN
            auto& fwdMap = m_fwdMap;
            auto& revMap = m_revMap;
#else
            // invert directions for the server side
            auto& fwdMap = m_revMap;
            auto& revMap = m_fwdMap;
#endif
            // input channels exists, so we map from a larger buffer to a smaller buffer and back
            for (; chSrc < activeChannels.getNumChannelsCombined(); chSrc++) {
                if (activeChannels.isInputActive(chSrc)) {
                    fwdMap[chSrc] = chDst;
                    if (activeChannels.isOutputActive(chSrc)) {
                        // reverse mapping only for active outputs
                        revMap[chDst] = chSrc;
                    }
                    chDst++;
                }
            }
        } else {  // inst
            for (; chSrc < activeChannels.getNumChannelsCombined(); chSrc++) {
                if (activeChannels.isOutputActive(chSrc)) {
#if AG_PLUGIN
                    // no input channels, we just create a reverse map for the plugin side
                    m_revMap[chDst++] = chSrc;
#else
                    // and also invert the direction for the server
                    m_revMap[chSrc] = chDst++;
#endif
                }
            }
        }
    }

    void reset() {
        m_fwdMap.clear();
        m_revMap.clear();
    }

    template <typename T>
    void map(const AudioBuffer<T>* src, AudioBuffer<T>* dst) const {
        traceScope();
        mapInternal(src, dst, false);
    }

    template <typename T>
    void mapReverse(const AudioBuffer<T>* src, AudioBuffer<T>* dst) const {
        traceScope();
        mapInternal(src, dst, true);
    }

    void print() const {
        logln("channel mapping:");
        for (int ch = 0; ch < Defaults::PLUGIN_CHANNELS_MAX; ch++) {
            int chMapped = getMappedChannel(ch);
            bool forward = false;
            bool backward = false;
            if (chMapped > -1) {
                forward = true;
                if (getMappedChannelReverse(chMapped) == ch) {
                    backward = true;
                }
            } else {
                // try to find a backwards mapping, if no forward mapping exists
                for (int ch2 = 0; ch2 < Defaults::PLUGIN_CHANNELS_MAX && !backward; ch2++) {
                    if (getMappedChannelReverse(ch2) == ch) {
                        chMapped = ch2;
                        backward = true;
                    }
                }
            }
            if (chMapped > -1) {
                logln("  " << LogTag::getStrWithLeadingZero(ch) << (backward ? " <" : " -") << "-"
                           << (forward ? "> " : "- ") << LogTag::getStrWithLeadingZero(chMapped));
            }
        }
    }

  private:
    std::unordered_map<int, int> m_fwdMap, m_revMap;

    template <typename T>
    void mapInternal(const AudioBuffer<T>* src, AudioBuffer<T>* dst, bool reverse) const {
        if (src == dst) {
            return;
        }
        std::set<int> mapped;
        for (int ch = 0; ch < src->getNumChannels(); ch++) {
            int chMapped = reverse ? getMappedChannelReverse(ch) : getMappedChannel(ch);
            if (chMapped > -1) {
                copyChannel(src, ch, dst, chMapped);
                mapped.insert(chMapped);
            }
        }
        // clear any other channel in the dst buffer, that can't be mapped
        for (int ch = 0; ch < dst->getNumChannels(); ch++) {
            if (mapped.find(ch) == mapped.end()) {
                traceln("clearing unmapped channel " << ch);
                dst->clear(ch, 0, dst->getNumSamples());
            }
        }
    }

    int getMappedChannel(int ch) const {
        auto it = m_fwdMap.find(ch);
        if (it != m_fwdMap.end()) {
            return it->second;
        }
        return -1;
    }

    int getMappedChannelReverse(int ch) const {
        auto it = m_revMap.find(ch);
        if (it != m_revMap.end()) {
            return it->second;
        }
        return -1;
    }

    template <typename T>
    void copyChannel(const AudioBuffer<T>* src, int chSrc, AudioBuffer<T>* dst, int chDst) const {
        traceScope();
        traceln("copying channel " << chSrc << " to " << chDst);
        if (chSrc < 0 || chSrc >= src->getNumChannels()) {
            logln("channel mapper can't copy ch " << chSrc << " to " << chDst << ": src channel out of range");
            return;
        }
        if (chDst < 0 || chDst >= dst->getNumChannels()) {
            logln("channel mapper can't copy ch " << chSrc << " to " << chDst << ": dst channel out of range");
            return;
        }
        if (src->getNumSamples() != dst->getNumSamples()) {
            logln("channel mapper can't copy ch " << chSrc << " to " << chDst
                                                  << ": src and dst buffers have different numbers of samples");
            return;
        }
        dst->copyFrom(chDst, 0, *src, chSrc, 0, src->getNumSamples());
    }
};

}  // namespace e47
#endif  // _CHANNELMAPPER_HPP_
