/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef KeyAndMouseCommon_hpp
#define KeyAndMouseCommon_hpp

#include <unordered_map>

enum MouseEvType : uint8_t {
    MOVE,
    LEFT_DOWN,
    LEFT_UP,
    LEFT_DRAG,
    RIGHT_DOWN,
    RIGHT_UP,
    RIGHT_DRAG,
    OTHER_DOWN,
    OTHER_UP,
    OTHER_DRAG,
    WHEEL
};

const std::unordered_map<std::string, int> VKeyMap = {
    {"A", 0x00},         {"S", 0x01},         {"D", 0x02},      {"F", 0x03},         {"H", 0x04},
    {"G", 0x05},         {"Z", 0x06},         {"X", 0x07},      {"C", 0x08},         {"V", 0x09},
    {"B", 0x0B},         {"Q", 0x0C},         {"W", 0x0D},      {"E", 0x0E},         {"R", 0x0F},
    {"Y", 0x10},         {"T", 0x11},         {"1", 0x12},      {"2", 0x13},         {"3", 0x14},
    {"4", 0x15},         {"6", 0x16},         {"5", 0x17},      {"=", 0x18},         {"9", 0x19},
    {"7", 0x1A},         {"-", 0x1B},         {"8", 0x1C},      {"0", 0x1D},         {"{", 0x1E},
    {"O", 0x1F},         {"U", 0x20},         {"}", 0x21},      {"I", 0x22},         {"P", 0x23},
    {"L", 0x25},         {"J", 0x26},         {"'", 0x27},      {"K", 0x28},         {";", 0x29},
    {"\\", 0x2A},        {",", 0x2B},         {"/", 0x2C},      {"N", 0x2D},         {"M", 0x2E},
    {".", 0x2F},         {"`", 0x32},         {"/", 0x41},      {"Return", 0x24},    {"Tab", 0x30},
    {"Space", 0x31},     {"Backspace", 0x33}, {"Escape", 0x35}, {"Command", 0x37},   {"Shift", 0x38},
    {"Option", 0x3A},    {"Control", 0x3B},   {"F17", 0x40},    {"F18", 0x4F},       {"F19", 0x50},
    {"F20", 0x5A},       {"F5", 0x60},        {"F6", 0x65},     {"F11", 0x67},       {"F13", 0x69},
    {"F16", 0x6A},       {"F14", 0x6B},       {"F10", 0x6D},    {"F12", 0x6F},       {"F15", 0x71},
    {"Home", 0x73},      {"PageUp", 0x74},    {"Delete", 0x75}, {"F4", 0x76},        {"End", 0x77},
    {"F2", 0x78},        {"PageDown", 0x79},  {"F1", 0x7A},     {"LeftArrow", 0x7B}, {"RightArrow", 0x7C},
    {"DownArrow", 0x7D}, {"UpArrow", 0x7E}};

const uint16_t NOKEY = 0xFF;

static inline uint16_t getKeyCode(std::string s) {
    auto it = VKeyMap.find(s);
    if (it != VKeyMap.end()) {
        return (uint16_t)it->second;
    }
    return NOKEY;
}

static inline std::string getKeyName(uint16_t code) {
    for (auto& p : VKeyMap) {
        if (p.second == code) {
            return p.first;
        }
    }
    return "";
}

#endif /* KeyAndMouseCommon_hpp */
