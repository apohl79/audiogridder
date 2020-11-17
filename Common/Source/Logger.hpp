/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Logger_hpp
#define Logger_hpp

#include <JuceHeader.h>
#include <iostream>
#include <fstream>

namespace e47 {
class AGLogger : public Thread {
  public:
    AGLogger(const String& appName, const String& filePrefix);
    ~AGLogger() override;
    void run() override;

    static void log(String msg);

    static void initialize(const String& appName, const String& filePrefix, const String& configFile);
    static std::shared_ptr<AGLogger> getInstance();
    static void cleanup();

    static bool isEnabled() { return m_enabled; }
    static void setEnabled(bool b);

  private:
    File m_file;
    std::ofstream m_outstream;
    std::queue<String> m_msgQ[2];
    size_t m_msgQIdx = 0;
    std::mutex m_mtx;
    std::condition_variable m_cv;

#ifdef JUCE_DEBUG
    bool m_logToErr = false;
#endif

    void logReal(String msg);

    static std::shared_ptr<AGLogger> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;

    static std::atomic_bool m_enabled;
};
}  // namespace e47

#endif /* Logger_hpp */
