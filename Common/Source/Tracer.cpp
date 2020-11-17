/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Tracer.hpp"
#include "Utils.hpp"
#include "SharedInstance.hpp"
#include "Defaults.hpp"

#ifdef JUCE_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace e47 {
namespace Tracer {

#define NUM_OF_TRACE_RECORDS 250000  // ~50MB

std::atomic_bool l_tracerEnabled{false};
std::atomic_uint64_t l_index{0};
File l_file;
#ifdef JUCE_WINDOWS
HANDLE l_fd, l_mapped_hndl;
#else
int l_fd = -1;
#endif
char* l_data = nullptr;
size_t l_size = 0;

struct Inst : SharedInstance<Inst> {};

setLogTagStatic("tracer");

#define TRACE_STRCPY(dst, src)                              \
    do {                                                    \
        int len = jmin((int)sizeof(dst) - 1, src.length()); \
        strncpy(dst, src.getCharPointer(), (size_t)len);    \
        dst[len] = 0;                                       \
    } while (0)

Scope::Scope(const LogTag* t, const String& f, int l, const String& ff) {
    if (l_tracerEnabled) {
        enabled = true;
        tagId = t->getId();
        tagName = t->getName();
        tagExtra = t->getExtra();
        file = f;
        line = l;
        func = ff;
        start = Time::getHighResolutionTicks();
        traceMessage(tagId, tagName, tagExtra, file, line, func, "enter");
    }
}

Scope::Scope(const LogTagDelegate* t, const String& f, int l, const String& ff)
    : Scope(t->getLogTagSource(), f, l, ff) {}

void openTraceFile() {
    if (nullptr != l_data) {
        logln("trace file already opened");
        return;
    }
    void* m = nullptr;
    l_size = NUM_OF_TRACE_RECORDS * sizeof(TraceRecord);
#ifdef JUCE_WINDOWS
    l_fd = CreateFileA(l_file.getFullPathName().getCharPointer(), (GENERIC_READ | GENERIC_WRITE), FILE_SHARE_READ, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (l_fd == INVALID_HANDLE_VALUE) {
        logln("CreateFileA failed: " << GetLastErrorStr());
        return;
    }
    LONG sizeh = (l_size >> (sizeof(LONG) * 8));
    LONG sizel = (l_size & 0xffffffff);
    auto res = SetFilePointer(l_fd, sizel, &sizeh, FILE_BEGIN);
    if (INVALID_SET_FILE_POINTER == res) {
        logln("SetFilePointer failed: " << GetLastErrorStr());
        return;
    }
    if (!SetEndOfFile(l_fd)) {
        logln("SetEndOfFile failed: " << GetLastErrorStr());
        return;
    }
    l_mapped_hndl = CreateFileMappingA(l_fd, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (NULL == l_mapped_hndl) {
        logln("CreateFileMappingA failed: " << GetLastErrorStr());
        return;
    }
    m = MapViewOfFileEx(l_mapped_hndl, FILE_MAP_WRITE, 0, 0, 0, NULL);
    if (NULL == m) {
        logln("MapViewOfFileEx failed: " << GetLastErrorStr());
        return;
    }
#else
    l_fd = open(l_file.getFullPathName().getCharPointer(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
    if (l_fd < 0) {
        logln("open failed: " << strerror(errno));
        return;
    }
    if (ftruncate(l_fd, (off_t)l_size)) {
        logln("ftruncate failed: " << strerror(errno));
        return;
    }
    m = mmap(nullptr, l_size, PROT_WRITE | PROT_READ, MAP_SHARED, l_fd, 0);
    if (MAP_FAILED == m) {
        logln("mmap failed: " << strerror(errno));
        return;
    }
#endif
    l_data = static_cast<char*>(m);
    logln("trace file opened");
}

void closeTraceFile() {
    if (nullptr == l_data) {
        return;
    }
#ifdef JUCE_WINDOWS
    UnmapViewOfFile(l_data);
    CloseHandle(l_mapped_hndl);
    l_mapped_hndl = NULL;
    CloseHandle(l_fd);
    l_fd = NULL;
#else
    munmap(l_data, l_size);
    close(l_fd);
    l_fd = -1;
#endif
    l_data = nullptr;
    l_size = 0;
    logln("trace file closed");
}

void initialize(const String& appName, const String& filePrefix) {
    Inst::initialize([&](auto) {
        l_file = File(Defaults::getLogFileName(appName, filePrefix, ".trace")).getNonexistentSibling();
        // create dir if needed
        auto d = l_file.getParentDirectory();
        if (!d.exists()) {
            d.createDirectory();
        }
        cleanDirectory(d.getFullPathName(), filePrefix, ".trace");
    });
}

void cleanup() {
    Inst::cleanup([](auto) { closeTraceFile(); });
}

void setEnabled(bool b) {
    if (b && nullptr == l_data) {
        openTraceFile();
    }
    l_tracerEnabled = b;
}

bool isEnabled() { return l_tracerEnabled; }

TraceRecord* getRecord() {
    if (nullptr != l_data) {
        auto offset = (l_index.fetch_add(1, std::memory_order_relaxed) % NUM_OF_TRACE_RECORDS) * sizeof(TraceRecord);
        return reinterpret_cast<TraceRecord*>(l_data + offset);
    }
    return nullptr;
}

void traceMessage(const LogTag* tag, const String& file, int line, const String& func, const String& msg) {
    if (l_tracerEnabled) {
        traceMessage(tag->getId(), tag->getName(), tag->getExtra(), file, line, func, msg);
    }
}

void traceMessage(uint64 tagId, const String& tagName, const String& tagExtra, const String& file, int line,
                  const String& func, const String& msg) {
    if (l_tracerEnabled) {
        auto thread = Thread::getCurrentThread();
        String threadTag = "unknown";
        if (nullptr != thread) {
            threadTag = thread->getThreadName();
        } else {
            auto mm = MessageManager::getInstanceWithoutCreating();
            if (nullptr != mm && mm->isThisTheMessageThread()) {
                threadTag = "message_thread";
            }
        }
        auto* rec = getRecord();
        if (nullptr != rec) {
            rec->time = Time::getMillisecondCounterHiRes();
            rec->threadId = (uint64)Thread::getCurrentThreadId();
            rec->tagId = tagId;
            rec->line = line;
            TRACE_STRCPY(rec->threadName, threadTag);
            TRACE_STRCPY(rec->tagName, tagName);
            TRACE_STRCPY(rec->tagExtra, tagExtra);
            TRACE_STRCPY(rec->file, File::createFileWithoutCheckingPath(file).getFileName());
            TRACE_STRCPY(rec->func, func);
            TRACE_STRCPY(rec->msg, msg);
        } else {
            l_tracerEnabled = false;
            logln("failed to get trace record");
        }
    }
}

}  // namespace Tracer
}  // namespace e47
