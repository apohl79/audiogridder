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
#include "MemoryFile.hpp"

namespace e47 {
namespace Tracer {

#define NUM_OF_TRACE_RECORDS 250000  // ~50MB

std::atomic_bool l_tracerEnabled{false};
std::atomic_uint64_t l_index{0};
MemoryFile l_file;

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

void initialize(const String& appName, const String& filePrefix) {
    Inst::initialize([&](auto) {
        auto f = File(Defaults::getLogFileName(appName, filePrefix, ".trace")).getNonexistentSibling();
        l_file = MemoryFile(getLogTagSource(), f, NUM_OF_TRACE_RECORDS * sizeof(TraceRecord));
        // create dir if needed
        auto d = f.getParentDirectory();
        if (!d.exists()) {
            d.createDirectory();
        }
        cleanDirectory(d.getFullPathName(), filePrefix, ".trace");
    });
}

void cleanup() {
    Inst::cleanup([](auto) { l_file.close(); });
}

void setEnabled(bool b) {
    if (b && !l_file.isOpen()) {
        l_file.open(true);
    }
    l_tracerEnabled = b;
}

bool isEnabled() { return l_tracerEnabled; }

TraceRecord* getRecord() {
    if (l_file.isOpen()) {
        auto offset = (l_index.fetch_add(1, std::memory_order_relaxed) % NUM_OF_TRACE_RECORDS) * sizeof(TraceRecord);
        return reinterpret_cast<TraceRecord*>(l_file.data() + offset);
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
