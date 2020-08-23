/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Logger_hpp
#define Logger_hpp

#include <JuceHeader.h>

namespace e47 {
class AGLogger : public Thread {
  public:
    AGLogger(const String& appName, const String& filePrefix);
    ~AGLogger() override;
    void run() override;

    static void log(String msg);

    static void initialize(const String& appName, const String& filePrefix);
    static std::shared_ptr<AGLogger> getInstance();
    static void cleanup();

  private:
    FileLogger* m_logger;
    std::queue<String> m_msgQ[2];
    size_t m_msgQIdx = 0;
    std::mutex m_mtx;
    std::condition_variable m_cv;

    void logReal(String msg);

    static std::shared_ptr<AGLogger> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;
};
}  // namespace e47

#endif /* Logger_hpp */
