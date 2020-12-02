/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include <cstddef>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <boost/program_options.hpp>

using int64 = long long;
using uint64 = unsigned long long;

namespace bpo = boost::program_options;

struct TraceRecord {
    double time;
    uint64 threadId;
    char threadName[16];
    uint64 tagId;
    char tagName[16];
    char tagExtra[32];
    char file[32];
    int line;
    char func[32];
    char msg[64];
};

struct StatsRecord {
    std::string name;
    std::string file;
    uint64 calls;
    double timeEntered;
    double timeTotol;
};

int COL_WIDTH_THREAD = 0;
int COL_WIDTH_TAG = 0;
int COL_WIDTH_EXTRA = 0;
int COL_WIDTH_FILE = 0;
int COL_WIDTH_FUNC = 0;
bool REV_TIME = false;
double FIRST_TIME = 0.0;
double LAST_TIME = 0.0;

enum FilterType { STR, HEX, DEC };

#define DARKY "\x1b[30;1m"
#define RED "\x1b[31m"
#define RED_BRIGHT "\x1b[31;1m"
#define GREEN "\x1b[32m"
#define GREEN_BRIGHT "\x1b[32;1m"
#define YELLOW "\x1b[33m"
#define YELLOW_BRIGHT "\x1b[33;1m"
#define BLUE_BRIGHT "\x1b[34;1m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define MAGENTA_BRIGHT "\x1b[35;1m"
#define CYAN "\x1b[36m"
#define CYAN_BRIGHT "\x1b[36;1m"
#define WHITE "\x1b[37;1m"
#define RESET "\x1b[0m"

std::vector<std::string> colors = {DARKY,         RED,         RED_BRIGHT, GREEN,          GREEN_BRIGHT, YELLOW,
                                   YELLOW_BRIGHT, BLUE_BRIGHT, MAGENTA,    MAGENTA_BRIGHT, CYAN,         CYAN_BRIGHT,
                                   WHITE,         RED,         RED_BRIGHT, GREEN,          GREEN_BRIGHT, YELLOW,
                                   YELLOW_BRIGHT, BLUE_BRIGHT, MAGENTA,    MAGENTA_BRIGHT, CYAN,         CYAN_BRIGHT,
                                   WHITE};

size_t fnv(const std::string& str) {
    auto* p = reinterpret_cast<const unsigned char*>(str.data());
    std::uint32_t h = 2166136261;
    for (std::size_t i = 0; i < str.size(); ++i) {
        h = (h * 16777619) ^ p[i];
    }
    return h;
}

size_t getColorIdx(const std::string& str) { return fnv(str) % colors.size(); }

std::string getTimeStr(double timediff, bool keepShort = false) {
    std::stringstream str;
    int64 seconds = (int64)timediff / 1000;
    double ms = (((int64)timediff % 1000) + timediff - (int64)timediff);
    int64 h = seconds / 3600;
    int64 m = (seconds - h * 3600) / 60;
    int64 s = seconds % 60;

    auto format = [&](int w) {
        if (!keepShort) {
            str << std::setfill('0') << std::setw(w);
        }
    };

    if (!keepShort || h) {
        format(2);
        str << h << ":";
    }
    if (!keepShort || m) {
        format(2);
        str << m << ":";
    }
    if (!keepShort || s) {
        format(2);
        str << s << ",";
    }
    format(10);
    str << ms;

    return str.str();
}

std::string colorize(const std::string& str, int col = -1) {
    col = col == -1 ? (int)getColorIdx(str) : col;
    std::string ret;
    ret += colors[(size_t)col];
    ret += str;
    ret += RESET;
    return ret;
}

void printColumn(std::stringstream& col, int& width, bool color, bool updateWidth) {
    if (updateWidth) {
        int wnew = (int)col.str().length();
        if (wnew > width) {
            width = wnew;
        }
    } else {
        auto colIdx = getColorIdx(col.str());
        std::cout << (color ? colors[colIdx] : "") << std::setw(width) << col.str() << (color ? RESET : "") << " | ";
    }
    col.str("");
}

void printRecord(const TraceRecord& rec, bool updateCols = false) {
    std::stringstream col;

    if (!updateCols) {
        double timediff;
        if (REV_TIME) {
            timediff = (LAST_TIME - rec.time);
        } else {
            timediff = (rec.time - FIRST_TIME);
        }
        col << getTimeStr(timediff);
        int w = 0;
        printColumn(col, w, false, false);
    }

    col << rec.threadName << ":" << std::hex << rec.threadId << std::dec;
    printColumn(col, COL_WIDTH_THREAD, true, updateCols);

    col << rec.tagName << ":" << std::hex << rec.tagId << std::dec;
    printColumn(col, COL_WIDTH_TAG, true, updateCols);

    col << rec.tagExtra;
    printColumn(col, COL_WIDTH_EXTRA, false, updateCols);

    std::stringstream filestr;
    filestr << rec.file << ":" << rec.line;
    col << filestr.str();
    printColumn(col, COL_WIDTH_FILE, true, updateCols);

    col << rec.func;
    printColumn(col, COL_WIDTH_FUNC, true, updateCols);

    if (!updateCols) {
        std::cout << rec.msg << std::endl;
    }
}

void printThreadHeader(const std::string& thread, size_t msgCount) {
    std::stringstream msgs;
    msgs << "(" << msgCount << " messages)";
    size_t dashCount =
        (size_t)(71 + COL_WIDTH_THREAD + COL_WIDTH_TAG + COL_WIDTH_EXTRA + COL_WIDTH_FILE + COL_WIDTH_FUNC);
    dashCount -= 4 + thread.length() + msgs.str().length();
    std::cout << "--- " << colorize(thread) << " " << msgs.str() << " " << std::string(dashCount, '-') << std::endl;
}

void updateColumns(const TraceRecord& rec) { printRecord(rec, true); }

bool compRecords(const TraceRecord& lhs, const TraceRecord& rhs) { return lhs.time < rhs.time; }

bool compRecordsByTime(const StatsRecord& lhs, const StatsRecord& rhs) {
    if (lhs.timeTotol == rhs.timeTotol) {
        return lhs.file < rhs.file;
    } else {
        return lhs.timeTotol > rhs.timeTotol;
    }
}

bool compRecordsByCalls(const StatsRecord& lhs, const StatsRecord& rhs) {
    if (lhs.calls == rhs.calls) {
        return lhs.file < rhs.file;
    } else {
        return lhs.calls > rhs.calls;
    }
}

bool startsWith(const std::string& str, const std::string& starts) {
    auto pos = str.find(starts);
    return pos == 0;
}

FilterType getFilterType(const std::string& s) {
    if (startsWith(s, "0x")) {
        return FilterType::HEX;
    } else if (startsWith(s, "s:")) {
        return FilterType::STR;
    }
    return FilterType::DEC;
}

void updateFilter(const std::string& f, std::set<uint64>& idFilter, std::set<std::string>& nameFilter) {
    auto type = getFilterType(f);
    if (type == FilterType::STR) {
        nameFilter.insert(f.substr(2));
    } else {
        std::stringstream s;
        if (type == FilterType::HEX) {
            s << std::hex;
        }
        s << f;
        uint64 id;
        s >> id;
        idFilter.insert(id);
    }
}

template <typename T>
bool exists(const std::set<T>& filter, const T& lookFor) {
    return filter.find(lookFor) != filter.end();
}

int main(int argc, char** argv) {
    // clang-format off
    bpo::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help screen")
        ("file,f", bpo::value<std::string>(), "Trace file")
        ("info,i", "Show a summary of the trace file")
        ("log", "Log file mode, order messages by time instead of by thread")
        ("stats", "Statistics mode")
        ("number,n", bpo::value<int>()->default_value(10), "Number of messages per thread (0 for all)")
        ("thread,t", bpo::value<std::vector<std::string>>(), "Show specific thread(s)\n(format: 0x<hex id> | s:<name> | <decimal id>)")
        ("tag,x", bpo::value<std::vector<std::string>>(), "Show specific tag(s)\n(format: 0x<hex id> | s:<name> | <decimal id>)")
        ("rt", "Reverse time display")
        ;
    // clang-format on

    bpo::variables_map opts;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, desc), opts);
        bpo::notify(opts);
    } catch (bpo::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    if (opts.count("help") || !opts.count("file")) {
        std::cout << "Usage: " << argv[0] << " -f <trace file> [Options]" << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    bool summary = opts.count("info");
    bool logmode = opts.count("log");
    bool statsmode = opts.count("stats");

    REV_TIME = opts.count("rt");

    using IDFilter = std::set<uint64>;
    using NameFilter = std::set<std::string>;

    IDFilter threadIdFilter;
    NameFilter threadNameFilter;
    if (opts.count("thread")) {
        for (auto& t : opts["thread"].as<std::vector<std::string>>()) {
            updateFilter(t, threadIdFilter, threadNameFilter);
        }
    }

    IDFilter tagIdFilter;
    NameFilter tagNameFilter;
    if (opts.count("tag")) {
        for (auto& t : opts["tag"].as<std::vector<std::string>>()) {
            updateFilter(t, tagIdFilter, tagNameFilter);
        }
    }

    auto fd = open(opts["file"].as<std::string>().data(), O_RDONLY, 0);
    if (fd < 0) {
        std::cerr << "failed to open file: " << strerror(errno) << std::endl;
        return 1;
    }

    std::vector<TraceRecord> dataByTime;
    std::map<uint64, std::vector<TraceRecord>> dataByThread;
    std::map<std::string, StatsRecord> dataStats;
    std::map<std::string, StatsRecord> dataStatsCombined;
    std::vector<StatsRecord> dataStatsSorted;
    std::map<uint64, std::string> threadNameMap;

    FIRST_TIME = std::numeric_limits<double>::max();

    TraceRecord rec;
    ssize_t bytes;
    uint64 recCount = 0;
    do {
        bytes = read(fd, &rec, sizeof(rec));
        if (bytes == sizeof(rec) && rec.time > 0) {
            if (rec.time < FIRST_TIME) {
                FIRST_TIME = rec.time;
            }
            if (rec.time > LAST_TIME) {
                LAST_TIME = rec.time;
            }
            threadNameMap[rec.threadId] = rec.threadName;
            updateColumns(rec);
            dataByTime.push_back(rec);
            if (statsmode) {
            }
            dataByThread[rec.threadId].push_back(std::move(rec));
            recCount++;
        }
    } while (bytes == sizeof(rec));

    close(fd);

    std::sort(dataByTime.begin(), dataByTime.end(), compRecords);

    size_t maxStatsKeyLen = 0;

    if (statsmode) {
        for (auto& srec : dataByTime) {
            bool isEnter = !strncmp(srec.msg, "enter", 5);
            bool isExit = !strncmp(srec.msg, "exit", 4);
            if (isEnter || isExit) {
                std::stringstream statsKey;
                statsKey << srec.threadId << ":" << srec.tagId << ":" << srec.file << ":" << srec.line;
                bool exists = dataStats.find(statsKey.str()) != dataStats.end();
                auto& statsSrec = dataStats[statsKey.str()];
                if (!exists) {
                    std::stringstream file;
                    file << srec.file << ":" << srec.line;
                    std::stringstream name;
                    name << srec.func << " (" << file.str() << ")";
                    statsSrec.name = name.str();
                    statsSrec.file = file.str();
                    statsSrec.calls = 0;
                    statsSrec.timeEntered = 0.0;
                    statsSrec.timeTotol = 0.0;
                    if (maxStatsKeyLen < statsSrec.name.size()) {
                        maxStatsKeyLen = statsSrec.name.size();
                    }
                }
                if (isEnter) {
                    statsSrec.timeEntered = srec.time;
                    statsSrec.calls++;
                } else if (isExit) {
                    if (statsSrec.timeEntered > 0.0) {
                        statsSrec.timeTotol += (srec.time - statsSrec.timeEntered);
                        statsSrec.timeEntered = 0.0;
                    }
                }
            }
        }
        for (auto& kv : dataStats) {
            bool isNew = dataStatsCombined.find(kv.second.name) == dataStatsCombined.end();
            auto& statsRec = dataStatsCombined[kv.second.name];
            if (isNew) {
                statsRec.name = kv.second.name;
                statsRec.file = kv.second.file;
                statsRec.calls = 0;
                statsRec.timeTotol = 0.0;
                statsRec.timeEntered = 0.0;
            }
            statsRec.calls += kv.second.calls;
            statsRec.timeTotol += kv.second.timeTotol;
        }
        for (auto& kv : dataStatsCombined) {
            dataStatsSorted.push_back(kv.second);
        }
    }

    if (summary) {
        std::cout << "messages: " << recCount << std::endl;
        std::cout << " threads: " << threadNameMap.size() << std::endl;
    } else if (logmode) {
        for (auto& r : dataByTime) {
            if (threadIdFilter.size() > 0 && !exists(threadIdFilter, r.threadId)) {
                continue;
            }
            if (threadNameFilter.size() > 0 && !exists(threadNameFilter, std::string(r.threadName))) {
                continue;
            }
            if (tagIdFilter.size() > 0 && !exists(tagIdFilter, r.tagId)) {
                continue;
            }
            if (tagNameFilter.size() > 0 && !exists(tagNameFilter, std::string(r.tagName))) {
                continue;
            }
            printRecord(r);
        }
    } else if (statsmode) {
        std::cout << "--- functions by time spent ---" << std::endl;
        std::sort(dataStatsSorted.begin(), dataStatsSorted.end(), compRecordsByTime);
        for (auto& statsRec : dataStatsSorted) {
            size_t colIdx = (size_t)getColorIdx(statsRec.name);
            std::cout << colors[colIdx] << std::setw((int)maxStatsKeyLen) << statsRec.name << RESET << ": "
                      << getTimeStr(statsRec.timeTotol, true) << std::endl;
        }
        std::cout << std::endl;
        std::cout << "--- functions by calls ---" << std::endl;
        std::sort(dataStatsSorted.begin(), dataStatsSorted.end(), compRecordsByCalls);
        for (auto& statsRec : dataStatsSorted) {
            size_t colIdx = (size_t)getColorIdx(statsRec.name);
            std::cout << colors[colIdx] << std::setw((int)maxStatsKeyLen) << statsRec.name << RESET << ": "
                      << statsRec.calls << " calls" << std::endl;
        }
    } else {
        for (auto& kv : dataByThread) {
            if (threadIdFilter.size() > 0 && !exists(threadIdFilter, kv.first)) {
                continue;
            }
            std::string threadName;
            auto it = threadNameMap.find(kv.first);
            if (it != threadNameMap.end()) {
                threadName = it->second;
            }
            if (threadNameFilter.size() > 0 && !exists(threadNameFilter, threadName)) {
                continue;
            }
            size_t show = (size_t)opts["number"].as<int>();
            bool first = true;
            std::sort(kv.second.begin(), kv.second.end(), compRecords);
            if (show > kv.second.size()) {
                show = kv.second.size();
            }
            std::stringstream threadNameId;
            threadNameId << threadName << ":" << std::hex << kv.first;
            if (tagIdFilter.size() > 0 || tagNameFilter.size() > 0) {
                std::vector<TraceRecord> filter;
                for (auto& r : kv.second) {
                    if (exists(tagIdFilter, r.tagId)) {
                        filter.push_back(r);
                    } else if (exists(tagNameFilter, std::string(r.tagName))) {
                        filter.push_back(r);
                    }
                }
                if (show > filter.size()) {
                    show = filter.size();
                }
                for (auto it2 = (show ? filter.end() - (int)show : filter.begin()); it2 != filter.end(); it2++) {
                    if (first) {
                        printThreadHeader(threadNameId.str(), kv.second.size());
                        first = false;
                    }
                    printRecord(*it2);
                }
            } else {
                for (auto it2 = (show ? kv.second.end() - (int)show : kv.second.begin()); it2 != kv.second.end();
                     it2++) {
                    if (first) {
                        printThreadHeader(threadNameId.str(), kv.second.size());
                        first = false;
                    }
                    printRecord(*it2);
                }
            }
        }
    }
    return 0;
}
