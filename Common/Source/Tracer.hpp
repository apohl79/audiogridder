/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ThreadTracer_hpp
#define ThreadTracer_hpp

#include <JuceHeader.h>

namespace e47 {

class LogTag;
class LogTagDelegate;

namespace Tracer {

void traceMessage(const LogTag* tag, const String& file, int line, const String& func, const String& msg);
void traceMessage(uint64 tagId, const String& tagName, const String& tagExtra, const String& file, int line,
                  const String& func, const String& msg);

void initialize(const String& appName, const String& filePrefix);
void cleanup();

void setEnabled(bool b);
bool isEnabled();

struct Scope {
    bool enabled = false;
    uint64 tagId;
    String tagName;
    String tagExtra;
    String file;
    int line;
    String func;
    int64 start;

    Scope(const LogTag* t, const String& f, int l, const String& ff);
    Scope(const LogTagDelegate* t, const String& f, int l, const String& ff);
    ~Scope() {
        if (enabled) {
            auto end = Time::getHighResolutionTicks();
            double ms = Time::highResolutionTicksToSeconds(end - start) * 1000;
            traceMessage(tagId, tagName, tagExtra, file, line, func, "exit (took " + String(ms) + "ms)");
        }
    }
};

struct TraceRecord {
    double time;
    uint64 threadId;
    char threadName[16];
    uint64 tagId;
    char tagName[16];
    char tagExtra[32];
    char file[32];
    int line;
    char func[32];
    char msg[64];
};

}  // namespace Tracer
}  // namespace e47

#endif /* ThreadTracer_hpp */
