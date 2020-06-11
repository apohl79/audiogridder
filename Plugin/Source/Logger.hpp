/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Logger_hpp
#define Logger_hpp

#include "../JuceLibraryCode/JuceHeader.h"

#if JUCE_DEBUG
#define dbgln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     AGLogger::getInstance()->log(__str);)
#else
#define dbgln(M)
#endif

#define logln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     AGLogger::getInstance()->log(__str);)

#define logln_clnt(C, M)                                                                     \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)C << "] " << M; \
                                     AGLogger::getInstance()->log(__str);)

namespace e47 {
class AGLogger : public Thread {
  public:
    AGLogger(const String& appName, const String& filePrefix);
    ~AGLogger() override;
    void run() override;
    void log(String msg);

    static void initialize(const String& appName, const String& filePrefix);
    static std::shared_ptr<AGLogger> getInstance();
    static void cleanup();

  private:
    FileLogger* m_logger;
    std::queue<String> m_msgQ[2];
    size_t m_msgQIdx = 0;
    std::mutex m_mtx;
    std::condition_variable m_cv;

    static std::shared_ptr<AGLogger> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;
};
}  // namespace e47

#endif /* Logger_hpp */
