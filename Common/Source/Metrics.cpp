/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Metrics.hpp"
#include <cstddef>
#include <memory>
#include "SharedInstance.hpp"

namespace e47 {

Metrics::StatsMap Metrics::m_stats;
std::mutex Metrics::m_statsMtx;

void TimeStatistic::update(double t) {
    m_meter.increment();
    std::lock_guard<std::mutex> lock(m_timesMtx);
    m_times[m_timesIdx].push_back(t);
}

void TimeStatistic::aggregate() {
    auto& data = m_times[m_timesIdx];
    {
        // switch to the other buffer
        std::lock_guard<std::mutex> lock(m_timesMtx);
        m_timesIdx = (m_timesIdx + 1) % 2;
    }
    Histogram hist(m_numOfBins, m_binSize);
    if (!data.empty()) {
        std::sort(data.begin(), data.end());
        hist.min = data.front();
        hist.max = data.back();
        hist.count = data.size();
        for (auto t : data) {
            hist.sum += t;
        }
        hist.avg = hist.sum / hist.count;
        auto nfIdx = (size_t)(hist.count * 0.95);
        hist.nintyFifth = data[nfIdx];

        // calc the distribution over m_numOfBins bins with a size of m_binSize seconds
        std::size_t count = 0;
        std::size_t i = 0;
        auto upper = m_binSize;
        for (auto d : data) {
            if (d < upper + 1 || i == m_numOfBins) {
                // value fits into the bin
                count++;
            } else {
                // create pair
                hist.updateBin(i, count);
                // next bin
                while (d > upper && i < m_numOfBins) {
                    i++;
                    upper += m_binSize;
                }
                count = 1;
            }
        }
        hist.updateBin(i, count);
        data.clear();
    }
    std::lock_guard<std::mutex> lock(m_1minValuesMtx);
    m_1minValues.push_back(std::move(hist));
    if (m_1minValues.size() > 6) {
        m_1minValues.erase(m_1minValues.begin());
    }
}

void TimeStatistic::aggregate1s() { m_meter.aggregate1s(); }

std::vector<TimeStatistic::Histogram> TimeStatistic::get1minValues() {
    std::lock_guard<std::mutex> lock(m_1minValuesMtx);
    return m_1minValues;  // copy
}

TimeStatistic::Histogram TimeStatistic::get1minHistogram() {
    auto values = get1minValues();
    Histogram aggregate(m_numOfBins, m_binSize);
    if (values.size() > 0) {
        aggregate.min = std::numeric_limits<double>::max();
        for (auto& hist : values) {
            aggregate.sum += hist.sum;
            aggregate.count += hist.count;
            aggregate.nintyFifth += hist.nintyFifth;
            for (std::size_t i = 0; i < m_numOfBins + 1; ++i) {
                aggregate.updateBin(i, hist.dist[i].second);
            }
            if (aggregate.min > hist.min) {
                aggregate.min = hist.min;
            }
            if (aggregate.max < hist.max) {
                aggregate.max = hist.max;
            }
        }
        if (aggregate.count > 0) {
            aggregate.avg = aggregate.sum / aggregate.count;
        }
        aggregate.nintyFifth /= values.size();
    }
    return aggregate;
}

void TimeStatistic::log(const String& name) {
    if (m_showLog) {
        auto hist = get1minHistogram();
        if (hist.count > 0) {
            logln(name << ": total " << hist.count << ", rps " << String(m_meter.rate_1min(), 2) << ", 95th "
                       << String(hist.nintyFifth) << "ms, avg " << String(hist.avg, 2) << "ms, min "
                       << String(hist.min, 2) << "ms, max " << String(hist.max, 2) << "ms");
            String out = name;
            out << ":  dist ";
            size_t count = 0;
            for (auto& p : hist.dist) {
                if (count > 0) {
                    out << ", ";
                }
                double perc = 0;
                if (hist.count > 0) {
                    perc = static_cast<double>(p.second) / hist.count * 100;
                }
                if (count < hist.dist.size() - 1) {
                    out << p.first << "-" << p.first + m_binSize << "ms ";
                } else {
                    out << ">" << p.first << "ms ";
                }
                out << String(perc, 2) << "%";
                ++count;
            }
            logln(out);
        }
    }
}

void Metrics::aggregateAndShow(bool show) {
    for (auto s : getStats()) {
        s.second->aggregate();
        if (show) {
            s.second->log(s.first);
        }
    }
}

void Metrics::aggregate1s() {
    for (auto s : getStats()) {
        s.second->aggregate1s();
    }
}

void Metrics::run() {
    traceScope();
    int count = 1;
    while (!currentThreadShouldExit()) {
        int sleepstep = 50;
        int sleepfor = 10000 / sleepstep;
        int sleepcount = 0;
        while (!currentThreadShouldExit() && sleepfor-- > 0) {
            Thread::sleep(sleepstep);
            sleepcount += sleepstep;
            if (sleepcount % 1000 == 0) {
                // every second
                aggregate1s();
            }
        }
        if (!currentThreadShouldExit()) {
            aggregateAndShow(count == 0);
            ++count %= 6;
        }
    }
}

TimeStatistic::Duration TimeStatistic::getDuration(const String& name, bool show) {
    auto ts = Metrics::getStatistic<TimeStatistic>(name);
    ts->setShowLog(show);
    return Duration(ts);
}

Metrics::StatsMap Metrics::getStats() {
    std::lock_guard<std::mutex> lock(m_statsMtx);
    return m_stats;
}

void Metrics::cleanup() { SharedInstance::cleanup(); }

}  // namespace e47
