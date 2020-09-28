/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef CPUInfo_hpp
#define CPUInfo_hpp

#include <JuceHeader.h>

#include "Utils.hpp"

namespace e47 {

class CPUInfo : public Thread, public LogTag {
  public:
    CPUInfo() : Thread("CPUInfo"), LogTag("cpuinfo") {}
    ~CPUInfo() { stopThread(-1); }

    void run();

    static void initialize();
    static void cleanup();
    static float getUsage();

  private:
    static std::unique_ptr<CPUInfo> m_inst;
    static std::atomic<float> m_usage;
};

}  // namespace e47

#endif
