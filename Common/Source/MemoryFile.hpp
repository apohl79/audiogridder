/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef __MEMORYFILE_H_
#define __MEMORYFILE_H_

#include <JuceHeader.h>

#ifdef JUCE_WINDOWS
#include <windows.h>
#endif

#include "Utils.hpp"

namespace e47 {

class MemoryFile : LogTagDelegate {
  public:
    MemoryFile() {}
    MemoryFile(LogTag* tag, const String& path, size_t size);
    MemoryFile(LogTag* tag, const File& file, size_t size);
    ~MemoryFile();

    bool exists() const { return m_file.exists(); }
    bool isOpen() const { return nullptr != m_data; }
    void open(bool overwriteIfExists = false);
    void close();

    char* data() { return m_data; }
    size_t size() { return m_size; }

  private:
    File m_file;
#ifdef JUCE_WINDOWS
    HANDLE m_fd = NULL;
    HANDLE m_mapped_hndl = NULL;
#else
    int m_fd = -1;
#endif
    char* m_data = nullptr;
    size_t m_size = 0;
};

}  // namespace e47

#endif  // __MEMORYFILE_H_
