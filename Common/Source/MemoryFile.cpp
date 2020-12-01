/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "MemoryFile.hpp"

#ifndef JUCE_WINDOWS
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace e47 {

MemoryFile::MemoryFile(LogTag* tag, const String& path, size_t size)
    : LogTagDelegate(tag), m_file(path), m_size(size) {}

MemoryFile::MemoryFile(LogTag* tag, const File& file, size_t size) : LogTagDelegate(tag), m_file(file), m_size(size) {}

MemoryFile::~MemoryFile() { close(); }

void MemoryFile::open(bool overwriteIfExists) {
    if (isOpen()) {
        logln("file already opened");
        return;
    }
    void* m = nullptr;
#ifdef JUCE_WINDOWS
    m_fd = CreateFileA(m_file.getFullPathName().getCharPointer(), (GENERIC_READ | GENERIC_WRITE), FILE_SHARE_READ, NULL,
                       overwriteIfExists ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (m_fd == INVALID_HANDLE_VALUE) {
        logln("CreateFileA failed: " << GetLastErrorStr());
        return;
    }
    LONG sizeh = (m_size >> (sizeof(LONG) * 8));
    LONG sizel = (m_size & 0xffffffff);
    auto res = SetFilePointer(m_fd, sizel, &sizeh, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res) {
        logln("SetFilePointer failed: " << GetLastErrorStr());
        return;
    }
    if (!SetEndOfFile(m_fd)) {
        logln("SetEndOfFile failed: " << GetLastErrorStr());
        return;
    }
    m_mapped_hndl = CreateFileMappingA(m_fd, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (NULL == m_mapped_hndl) {
        logln("CreateFileMappingA failed: " << GetLastErrorStr());
        return;
    }
    m = MapViewOfFileEx(m_mapped_hndl, FILE_MAP_WRITE, 0, 0, 0, NULL);
    if (NULL == m) {
        logln("MapViewOfFileEx failed: " << GetLastErrorStr());
        return;
    }
#else
    auto openFlags = O_CREAT | O_RDWR;
    if (overwriteIfExists) {
        openFlags |= O_TRUNC;
    }
    m_fd = ::open(m_file.getFullPathName().getCharPointer(), openFlags, S_IRWXU);
    if (m_fd < 0) {
        logln("open failed: " << strerror(errno));
        return;
    }
    if (ftruncate(m_fd, (off_t)m_size)) {
        logln("ftruncate failed: " << strerror(errno));
        return;
    }
    m = mmap(nullptr, m_size, PROT_WRITE | PROT_READ, MAP_SHARED, m_fd, 0);
    if (MAP_FAILED == m) {
        logln("mmap failed: " << strerror(errno));
        return;
    }
#endif
    m_data = static_cast<char*>(m);
}

void MemoryFile::close() {
    if (!isOpen()) {
        return;
    }
#ifdef JUCE_WINDOWS
    UnmapViewOfFile(m_data);
    CloseHandle(m_mapped_hndl);
    m_mapped_hndl = NULL;
    CloseHandle(m_fd);
    m_fd = NULL;
#else
    munmap(m_data, m_size);
    ::close(m_fd);
    m_fd = -1;
#endif
    m_data = nullptr;
}

}  // namespace e47
