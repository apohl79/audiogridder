/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Metrics.hpp"

namespace e47 {

std::unique_ptr<TimeStatistics::Aggregator> TimeStatistics::m_aggregator;
TimeStatistics::StatsMap TimeStatistics::m_stats;
std::mutex TimeStatistics::m_aggregatorMtx;
size_t TimeStatistics::m_aggregatorRefCount = 0;

void TimeStatistics::update(double t) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_times[m_timesIdx].push_back(t);
}

void TimeStatistics::aggregate() {
    auto& data = m_times[m_timesIdx];
    {
        // switch to the other buffer
        std::lock_guard<std::mutex> lock(m_mtx);
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
    m_1minValues.push_back(std::move(hist));
    if (m_1minValues.size() > 6) {
        m_1minValues.erase(m_1minValues.begin());
    }
}

TimeStatistics::Histogram TimeStatistics::get1minHistogram() {
    Histogram aggregate(m_numOfBins, m_binSize);
    if (m_1minValues.size() > 0) {
        aggregate.min = std::numeric_limits<double>::max();
        for (auto& hist : m_1minValues) {
            aggregate.sum += hist.sum;
            aggregate.count += hist.count;
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
    }
    return aggregate;
}

void TimeStatistics::log(const String& name) {
    auto hist = get1minHistogram();
    if (hist.count > 0) {
        logln(name << ": total " << hist.count << ", avg " << String(hist.avg, 2) << "ms, min " << String(hist.min, 2)
                   << "ms, max " << String(hist.max, 2) << "ms");
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

void TimeStatistics::Aggregator::run() {
    traceScope();
    int count = 1;
    while (!currentThreadShouldExit()) {
        int sleepfor = 10000 / 50;
        while (!currentThreadShouldExit() && sleepfor-- > 0) {
            Thread::sleep(50);
        }
        if (!currentThreadShouldExit()) {
            StatsMap hmcpy;
            {
                std::lock_guard<std::mutex> lock(m_aggregatorMtx);
                hmcpy = m_stats;
            }
            for (auto s : hmcpy) {
                s.second->aggregate();
                if (count == 0) {
                    s.second->log(s.first);
                }
            }
            ++count %= 6;
        }
    }
}

TimeStatistics::Duration TimeStatistics::getDuration(const String& name) {
    std::lock_guard<std::mutex> lock(m_aggregatorMtx);
    std::shared_ptr<TimeStatistics> stat;
    auto it = m_stats.find(name);
    if (m_stats.end() == it) {
        auto itnew = m_stats.emplace(name, std::make_shared<TimeStatistics>());
        stat = itnew.first->second;
    } else {
        stat = it->second;
    }
    return Duration(stat);
}

void TimeStatistics::initialize() {
    std::lock_guard<std::mutex> lock(m_aggregatorMtx);
    if (nullptr == m_aggregator) {
        m_aggregator = std::make_unique<Aggregator>();
        m_aggregator->startThread();
    }
    m_aggregatorRefCount++;
}

void TimeStatistics::cleanup() {
    std::lock_guard<std::mutex> lock(m_aggregatorMtx);
    m_aggregatorRefCount--;
    if (m_aggregatorRefCount == 0) {
        m_aggregator->signalThreadShouldExit();
        m_aggregator.reset();
    }
}

}  // namespace e47
