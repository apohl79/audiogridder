/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _AUDIORINGBUFFER_HPP_
#define _AUDIORINGBUFFER_HPP_

#include <JuceHeader.h>

namespace e47 {

template <typename T>
class AudioRingBuffer {
  public:
    AudioRingBuffer() {}

    AudioRingBuffer(int numChannels, int numSamples, bool clearNewData = false)
        : m_channels((size_t)numChannels), m_samples((size_t)numSamples) {
        allocate(clearNewData);
    }

    void resize(int numChannels, int numSamples, bool clearNewData = false) {
        m_channels = (size_t)numChannels;
        m_samples = (size_t)numSamples;
        m_readOffset = 0;
        m_writeOffset = 0;
        allocate(clearNewData);
    }

    void clear() {
        for (size_t c = 0; c < m_buffer.size(); c++) {
            memset(m_buffer[c].data(), 0, m_samples * sizeof(T));
        }
    }

    int getNumChannels() const noexcept { return (int)m_channels; }
    int getNumSamples() const noexcept { return (int)m_samples; }

    void setReadOffset(int offset) {
        if (m_samples > 0) {
            m_readOffset = (size_t)offset;
            m_readOffset %= m_samples;
        }
    }

    void incReadOffset(int offsetToAdd) {
        if (m_samples > 0) {
            m_readOffset += (size_t)offsetToAdd;
            m_readOffset %= m_samples;
        }
    }

    void setWriteOffset(int offset) {
        if (m_samples > 0) {
            m_writeOffset = (size_t)offset;
            m_writeOffset %= m_samples;
        }
    }

    void incWriteOffset(int offsetToAdd) {
        if (m_samples > 0) {
            m_writeOffset += (size_t)offsetToAdd;
            m_writeOffset %= m_samples;
        }
    }

    int read(T* const* dst, int dstStartSample, int numSamples) {
        size_t samplesToRead = jmin(m_samples, (size_t)numSamples);
        if (m_readOffset + samplesToRead <= m_samples) {
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(dst[c] + dstStartSample, m_buffer[c].data() + m_readOffset, samplesToRead * sizeof(T));
            }
            incReadOffset((int)samplesToRead);
        } else {
            // read until the end of the buffer
            size_t samplesToReadPart1 = m_samples - m_readOffset;
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(dst[c] + dstStartSample, m_buffer[c].data() + m_readOffset, samplesToReadPart1 * sizeof(T));
            }
            incReadOffset((int)samplesToReadPart1);
            // read the remaining samples from the beginning
            size_t samplesToReadPart2 = samplesToRead - samplesToReadPart1;
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(dst[c] + dstStartSample + samplesToReadPart1, m_buffer[c].data() + m_readOffset,
                       samplesToReadPart2 * sizeof(T));
            }
            incReadOffset((int)samplesToReadPart2);
        }
        return (int)samplesToRead;
    }

    int write(const T* const* src, int srcStartSample, int numSamples) {
        size_t samplesToWrite = jmin(m_samples, (size_t)numSamples);
        if (m_writeOffset + samplesToWrite <= m_samples) {
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(m_buffer[c].data() + m_writeOffset, src[c] + srcStartSample, samplesToWrite * sizeof(T));
            }
            incWriteOffset((int)samplesToWrite);
        } else {
            // write until the end of the buffer
            size_t samplesToWritePart1 = m_samples - m_writeOffset;
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(m_buffer[c].data() + m_writeOffset, src[c] + srcStartSample, samplesToWritePart1 * sizeof(T));
            }
            incWriteOffset((int)samplesToWritePart1);
            // write the remaining samples from the beginning
            size_t samplesToWritePart2 = samplesToWrite - samplesToWritePart1;
            for (size_t c = 0; c < m_channels; c++) {
                memcpy(m_buffer[c].data() + m_writeOffset, src[c] + srcStartSample + samplesToWritePart1,
                       samplesToWritePart2 * sizeof(T));
            }
            incWriteOffset((int)samplesToWritePart2);
        }
        return (int)samplesToWrite;
    }

    void process(T* const* src, int numSamples) {
        int offset = 0;
        while (numSamples > 0) {
            int samples = jmin(numSamples, (int)m_samples / 2);
            write(src, offset, samples);
            read(src, offset, samples);
            offset += samples;
            numSamples -= samples;
        }
    }

    const T* getChannelData(int c) const { return m_buffer[(size_t)c].data(); }
    T* getChannelData(int c) { return m_buffer[(size_t)c].data(); }

  private:
    size_t m_channels = 0;
    size_t m_samples = 0;
    size_t m_readOffset = 0;
    size_t m_writeOffset = 0;

    std::vector<std::vector<T>> m_buffer;

    void allocate(bool clearNewData) {
        if (m_channels > 0 && m_samples > 0) {
            if (m_buffer.size() != m_channels) {
                m_buffer.resize(m_channels);
            }
            for (size_t c = 0; c < m_channels; c++) {
                if (m_buffer[c].size() != m_samples) {
                    if (clearNewData) {
                        m_buffer[c].resize(m_samples, 0);
                    } else {
                        m_buffer[c].resize(m_samples);
                    }
                }
            }
        }
    }
};

}  // namespace e47

#endif  // _AUDIORINGBUFFER_HPP_
