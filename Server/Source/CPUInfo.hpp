/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef CPUInfo_hpp
#define CPUInfo_hpp

#include <JuceHeader.h>

#include "SharedInstance.hpp"
#include "Utils.hpp"

namespace e47 {

class CPUInfo : public Thread, public LogTag, public SharedInstance<CPUInfo> {
  public:
    CPUInfo() : Thread("CPUInfo"), LogTag("cpuinfo") { startThread(); }
    ~CPUInfo() override { stopThread(-1); }

    void run() override;

    static float getUsage() { return m_usage; }

  private:
    static std::atomic<float> m_usage;
};

}  // namespace e47

#endif
