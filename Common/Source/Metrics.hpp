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

#include "SharedInstance.hpp"
#include "Utils.hpp"

namespace e47 {

class BasicStatistic {
  public:
    virtual ~BasicStatistic() {}
    virtual void aggregate() = 0;
    virtual void aggregate1s() = 0;
    virtual void log(const String&) = 0;
};

class Meter : public BasicStatistic {
  public:
    Meter() : ALPHA_1min(alpha(60)) {}
    ~Meter() override {}

    inline void increment(uint32 i = 1) { m_counter += i; }
    inline double rate_1min() const { return m_rate_1min; }

    void aggregate() override {}
    void aggregate1s() override {
        auto c = m_counter.exchange(0, std::memory_order_relaxed);
        m_rate_1min = m_rate_1min * (1 - ALPHA_1min) + c * ALPHA_1min;
    }
    void log(const String&) override {}

  private:
    std::atomic_uint_fast64_t m_counter{0};
    double m_rate_1min = 0.0;
    const double ALPHA_1min;

    inline double alpha(int secs) { return 1 - std::exp(std::log(0.005) / secs); }
};

class TimeStatistic : public BasicStatistic, public LogTag {
  public:
    class Duration {
      public:
        Duration(std::shared_ptr<TimeStatistic> t = nullptr) : m_timer(t), m_start(Time::getHighResolutionTicks()) {}
        ~Duration() { update(); }

        void finish() {
            update();
            m_finished = true;
        }

        double update() {
            double ms = 0.0;
            if (!m_finished) {
                auto end = Time::getHighResolutionTicks();
                ms = Time::highResolutionTicksToSeconds(end - m_start) * 1000;
                if (nullptr != m_timer) {
                    m_timer->update(ms);
                }
                m_start = end;
            }
            return ms;
        }

        void reset() {
            m_start = Time::getHighResolutionTicks();
            m_finished = false;
        }

        void clear() { m_finished = true; }

        double getMillisecondsPassed() const {
            auto end = Time::getHighResolutionTicks();
            return Time::highResolutionTicksToSeconds(end - m_start) * 1000;
        }

      private:
        std::shared_ptr<TimeStatistic> m_timer;
        int64 m_start;
        bool m_finished = false;
    };

    class Timeout {
      public:
        Timeout(int millis) : m_milliseconds(millis) {}

        int getMillisecondsLeft() const {
            int passed = (int)lround(m_duration.getMillisecondsPassed());
            if (passed < m_milliseconds) {
                return m_milliseconds - passed;
            }
            return 0;
        }

      private:
        Duration m_duration;
        int m_milliseconds;
    };

    struct Histogram {
        double min = 0;
        double max = 0;
        double avg = 0;
        double sum = 0;
        double nintyFifth = 0;
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

    TimeStatistic(size_t numOfBins = 10, double binSize = 2 /* ms */)
        : LogTag("stats"), m_numOfBins(numOfBins), m_binSize(binSize) {}
    ~TimeStatistic() override {}

    void update(double t);
    void aggregate() override;
    void aggregate1s() override;
    Histogram get1minHistogram();
    void run();
    void log(const String& name) override;
    void setShowLog(bool b) { m_showLog = b; }

    Meter& getMeter() { return m_meter; }

    static Duration getDuration(const String& name, bool show = true);

  private:
    std::vector<double> m_times[2];
    std::mutex m_timesMtx;
    uint8 m_timesIdx = 0;
    std::vector<Histogram> m_1minValues;
    std::mutex m_1minValuesMtx;
    size_t m_numOfBins;
    double m_binSize;
    Meter m_meter;
    bool m_showLog = true;

    std::vector<Histogram> get1minValues();
};

class Metrics : public Thread, public LogTag, public SharedInstance<Metrics> {
  public:
    using StatsMap = std::unordered_map<String, std::shared_ptr<BasicStatistic>>;

    Metrics() : Thread("Metrics"), LogTag("metrics") { startThread(); }
    ~Metrics() { stopThread(-1); }
    void run();

    void aggregateAndShow(bool show);
    void aggregate1s();

    static void cleanup();

    static StatsMap getStats();

    template <typename T>
    static std::shared_ptr<T> getStatistic(const String& name) {
        std::lock_guard<std::mutex> lock(m_statsMtx);
        std::shared_ptr<T> stat;
        auto it = m_stats.find(name);
        if (m_stats.end() == it) {
            auto itnew = m_stats.emplace(name, std::make_shared<T>());
            stat = std::dynamic_pointer_cast<T>(itnew.first->second);
        } else {
            stat = std::dynamic_pointer_cast<T>(it->second);
        }
        return stat;
    }

  private:
    static StatsMap m_stats;
    static std::mutex m_statsMtx;
};

}  // namespace e47

#endif /* Metrics_hpp */
