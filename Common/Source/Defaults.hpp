/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Defaults_hpp
#define Defaults_hpp

#include "Utils.hpp"

namespace e47 {
namespace Defaults {

static constexpr int CLIENT_PORT = 55055;
static constexpr int SERVER_PORT = 55056;

static constexpr int SCAREA_STEPS = 30;
static constexpr int SCAREA_FULLSCREEN = 0xFFFF;

#ifndef JUCE_WINDOWS
static const String SERVER_CONFIG_FILE_OLD = "~/.audiogridderserver";
static const String PLUGIN_CONFIG_FILE_OLD = "~/.audiogridder";
static const String KNOWN_PLUGINS_FILE_OLD = "~/.audiogridderserver.cache";

static const String SERVER_CONFIG_FILE = "~/.audiogridder/audiogridderserver.cfg";
static const String PLUGIN_CONFIG_FILE = "~/.audiogridder/audiogridderplugin.cfg";
static const String KNOWN_PLUGINS_FILE = "~/.audiogridder/audiogridderserver.cache";
static const String DEAD_MANS_FILE = "~/.audiogridder/audiogridderserver.crash";
static const String SERVER_RUN_FILE = "~/.audiogridder/audiogridderserver.running";
static const String WINDOW_POSITIONS_FILE = "~/.audiogridder/audiogridder.winpos";
#else
static const String SERVER_CONFIG_FILE_OLD =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver";
static const String PLUGIN_CONFIG_FILE_OLD =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridder";
static const String KNOWN_PLUGINS_FILE_OLD =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.cache";

static const String SERVER_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
    "\\AudioGridder\\audiogridderserver.cfg";
static const String PLUGIN_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
    "\\AudioGridder\\audiogridderplugin.cfg";
static const String KNOWN_PLUGINS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
    "\\AudioGridder\\audiogridderserver.cache";
static const String DEAD_MANS_FILE = File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
                                     "\\AudioGridder\\audiogridderserver.crash";
static const String SERVER_RUN_FILE = File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
                                      "\\AudioGridder\\audiogridderserver.running";
static const String WINDOW_POSITIONS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() +
    "\\AudioGridder\\audiogridder.winpos";
#endif

setLogTagStatic("defaults");

inline String getLogDirName() {
#ifdef JUCE_LINUX
    String path = "~/.audiogridder/log";
#else
    auto sep = File::getSeparatorString();
    String path = FileLogger::getSystemLogFileFolder().getFullPathName();
    path << sep << "AudioGridder";
#endif
    return path;
}

inline String getLogFileName(const String& appName, const String& filePrefix, const String& fileExtension) {
    auto sep = File::getSeparatorString();
    auto path = getLogDirName();
    path << sep << appName << sep << filePrefix << Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S")
         << fileExtension;
    return path;
}

enum ConfigFile { ConfigServer, ConfigServerRun, ConfigPlugin, ConfigPluginCache, ConfigDeadMan, WindowPositions };

inline String getConfigFileName(ConfigFile type) {
    String file;
    String fileOld;
    switch (type) {
        case ConfigServer:
            file = SERVER_CONFIG_FILE;
            fileOld = SERVER_CONFIG_FILE_OLD;
            break;
        case ConfigPlugin:
            file = PLUGIN_CONFIG_FILE;
            fileOld = PLUGIN_CONFIG_FILE_OLD;
            break;
        case ConfigPluginCache:
            file = KNOWN_PLUGINS_FILE;
            fileOld = KNOWN_PLUGINS_FILE_OLD;
            break;
        case ConfigServerRun:
            file = SERVER_RUN_FILE;
            break;
        case ConfigDeadMan:
            file = DEAD_MANS_FILE;
            break;
        case WindowPositions:
            file = WINDOW_POSITIONS_FILE;
            break;
    }
    if (fileOld.isNotEmpty()) {
        File fOld(fileOld);
        File fNew(file);
        if (fOld.existsAsFile()) {
            logln("migrating config file '" << fileOld << "' to '" << file << "'");
            if (!fNew.exists()) {
                fNew.create();
            }
            fOld.copyFileTo(fNew);
            fOld.deleteFile();
        }
    }
    return file;
}

static constexpr int DEFAULT_NUM_OF_BUFFERS = 8;
static constexpr int DEFAULT_NUM_RECENTS = 10;
static constexpr int DEFAULT_LOAD_PLUGIN_TIMEOUT = 15000;

static constexpr uint32 BG_COLOR = 0xff222222;
static constexpr uint32 BUTTON_COLOR = 0xff333333;
static constexpr uint32 SLIDERTRACK_COLOR = 0xffffc13b;
static constexpr uint32 SLIDERTHUMB_COLOR = 0xaaffffff;
static constexpr uint32 SLIDERBG_COLOR = 0xff606060;
static constexpr uint32 ACTIVE_COLOR = 0xffffc13b;
static constexpr uint32 CPU_LOW_COLOR = 0xff00ff00;
static constexpr uint32 CPU_MEDIUM_COLOR = 0xffffff00;
static constexpr uint32 CPU_HIGH_COLOR = 0xffff0000;
static constexpr uint32 PLUGIN_OK_COLOR = 0xff008000;
static constexpr uint32 PLUGIN_NOTOK_COLOR = 0xff8b0000;

static const String MDNS_SERVICE_NAME = "_audiogridder._tcp.local.";

}  // namespace Defaults
}  // namespace e47

#endif /* Defaults_hpp */
