/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "CPUInfo.hpp"

#if defined(JUCE_MAC)
#include <mach/mach.h>
#elif defined(JUCE_WINDOWS)
#include <windows.h>
#include <tchar.h>

#define SYSINFO_CLASS_BASICINFO 0x0
#define SYSINFO_CLASS_PROCINFO 0x8

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER Reserved1[2];
    ULONG Reserved2;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef struct _SYSTEM_BASIC_INFORMATION {
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    KAFFINITY ActiveProcessorsAffinityMask;
    CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION;

typedef DWORD(WINAPI* fpNtQuerySystemInformation)(DWORD infoClass, void* sysInfo, DWORD sysInfoSize, DWORD* retSize);
#endif

namespace e47 {

std::atomic<float> CPUInfo::m_usage{0.0f};

void CPUInfo::run() {
    traceScope();

#if defined(JUCE_WINDOWS)
    fpNtQuerySystemInformation pNtQuerySystemInformation =
        (fpNtQuerySystemInformation)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation");
    if (NULL == pNtQuerySystemInformation) {
        logln("failed to find NtQuerySystemInformation");
        return;
    }
#endif

    std::vector<float> lastValues(5);
    memset(lastValues.data(), 0, lastValues.size() * sizeof(float));
    size_t valueIdx = 0;

    while (!currentThreadShouldExit()) {
        const int waitTime = 1000;

#if defined(JUCE_MAC)
        natural_t procCount = 0;
        processor_cpu_load_info_t procInfoStart, procInfoEnd;
        mach_msg_type_number_t procInfoCount = 0;

        auto ret = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &procCount,
                                       (processor_info_array_t*)&procInfoStart, &procInfoCount);

        if (ret != KERN_SUCCESS) {
            logln("host_processor_info failed: " << mach_error_string(ret));
            return;
        }

        sleep(waitTime);

        ret = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &procCount,
                                  (processor_info_array_t*)&procInfoEnd, &procInfoCount);

        if (ret != KERN_SUCCESS) {
            logln("host_processor_info failed: " << mach_error_string(ret));
            return;
        }

        uint32 usageTime, idleTime;
        usageTime = idleTime = 0;
        for (natural_t i = 0; i < procCount; i++) {
            usageTime += procInfoEnd[i].cpu_ticks[CPU_STATE_SYSTEM] - procInfoStart[i].cpu_ticks[CPU_STATE_SYSTEM];
            usageTime += procInfoEnd[i].cpu_ticks[CPU_STATE_USER] - procInfoStart[i].cpu_ticks[CPU_STATE_USER];
            usageTime += procInfoEnd[i].cpu_ticks[CPU_STATE_NICE] - procInfoStart[i].cpu_ticks[CPU_STATE_NICE];
            idleTime += procInfoEnd[i].cpu_ticks[CPU_STATE_IDLE] - procInfoStart[i].cpu_ticks[CPU_STATE_IDLE];
        }
        float totalTime = (float)usageTime + idleTime;
        float usage = (float)usageTime / totalTime * 100;
#elif defined(JUCE_WINDOWS)
        DWORD retSize;
        SYSTEM_BASIC_INFORMATION sbi = {0};

        auto ret = pNtQuerySystemInformation(SYSINFO_CLASS_BASICINFO, &sbi, sizeof(SYSTEM_BASIC_INFORMATION), &retSize);

        if (ret != 0) {
            logln("failed to read basic info: NtQuerySystemInformation returned " << ret);
            return;
        }

        std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> spiStart(sbi.NumberOfProcessors, {0});
        std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> spiEnd(sbi.NumberOfProcessors, {0});

        ret = pNtQuerySystemInformation(SYSINFO_CLASS_PROCINFO, spiStart.data(),
                                        (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * sbi.NumberOfProcessors),
                                        &retSize);

        if (ret != 0) {
            logln("failed to read proc info (start): NtQuerySystemInformation returned " << ret);
            return;
        }

        sleep(waitTime);

        ret = pNtQuerySystemInformation(SYSINFO_CLASS_PROCINFO, spiEnd.data(),
                                        (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * sbi.NumberOfProcessors),
                                        &retSize);

        if (ret != 0) {
            logln("failed to read proc info (end): NtQuerySystemInformation returned " << ret);
            return;
        }

        ULONGLONG totalTime, idleTime;
        totalTime = idleTime = 0;
        for (int i = 0; i < sbi.NumberOfProcessors; i++) {
            auto totalStart = spiStart[i].KernelTime.QuadPart + spiStart[i].UserTime.QuadPart;
            auto totalEnd = spiEnd[i].KernelTime.QuadPart + spiEnd[i].UserTime.QuadPart;
            totalTime += totalEnd - totalStart;
            idleTime += spiEnd[i].IdleTime.QuadPart - spiStart[i].IdleTime.QuadPart;
        }
        auto usageTime = (float)totalTime - idleTime;
        float usage = usageTime / totalTime * 100;
#endif
        lastValues[valueIdx++ % lastValues.size()] = usage;
        usage = 0;
        for (auto u : lastValues) {
            usage += u;
        }
        m_usage = usage / lastValues.size();
    }
}

}  // namespace e47
