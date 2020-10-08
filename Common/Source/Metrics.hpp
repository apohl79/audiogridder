/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Metrics_hpp
#define Metrics_hpp

#include <JuceHeader.h>
#include <unordered_map>

#include "Utils.hpp"

namespace e47 {

class TimeStatistics : public LogTag {
  public:
    class Duration {
      public:
        Duration(std::shared_ptr<TimeStatistics> t) : m_timer(t), m_start(Time::getHighResolutionTicks()) {}
        ~Duration() { update(); }

        void finish() {
            update();
            m_finished = true;
        }

        void update() {
            if (!m_finished) {
                auto end = Time::getHighResolutionTicks();
                double ms = Time::highResolutionTicksToSeconds(end - m_start) * 1000;
                m_timer->update(ms);
                m_start = end;
            }
        }

        void reset() { m_start = Time::getHighResolutionTicks(); }

      private:
        std::shared_ptr<TimeStatistics> m_timer;
        int64 m_start;
        bool m_finished = false;
    };

    struct Histogram {
        double min = 0;
        double max = 0;
        double avg = 0;
        double sum = 0;
        size_t count = 0;
        std::vector<std::pair<double, size_t>> dist;

        Histogram(size_t num_of_bins, double bin_size) {
            double lower = 0;
            for (size_t i = 0; i < num_of_bins + 1; ++i) {
                dist.emplace_back(std::make_pair(lower, 0));
                lower += bin_size;
            }
        }

        void updateBin(size_t bin, size_t c) { dist[bin].second += c; }
    };

    class Aggregator : public Thread, public LogTag {
      public:
        friend TimeStatistics;
        Aggregator() : Thread("Aggregator"), LogTag("stats_aggregator") {}
        ~Aggregator() { stopThread(3000); }
        void run();
    };

    TimeStatistics(size_t numOfBins = 10, double binSize = 2 /* ms */)
        : LogTag("stats"), m_numOfBins(numOfBins), m_binSize(binSize) {}
    ~TimeStatistics() {}

    void update(double t);
    void aggregate();
    Histogram get1minHistogram();
    void run();
    void log(const String& name);

    static Duration getDuration(const String& name);

    static void initialize();
    static void cleanup();

  private:
    std::vector<double> m_times[2];
    uint8 m_timesIdx = 0;
    std::vector<Histogram> m_1minValues;
    std::mutex m_mtx;
    size_t m_numOfBins;
    double m_binSize;

    using StatsMap = std::unordered_map<String, std::shared_ptr<TimeStatistics>>;
    static std::unique_ptr<Aggregator> m_aggregator;
    static StatsMap m_stats;
    static std::mutex m_aggregatorMtx;
    static size_t m_aggregatorRefCount;
};

}  // namespace e47

#endif /* Metrics_hpp */
