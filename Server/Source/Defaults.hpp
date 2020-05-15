/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Defaults_hpp
#define Defaults_hpp

#include "../JuceLibraryCode/JuceHeader.h"

static constexpr int DEFAULT_CLIENT_PORT = 55055;
static constexpr int DEFAULT_SERVER_PORT = 55056;

#ifdef JUCE_MAC
static const String SERVER_CONFIG_FILE = "~/.audiogridderserver";
static const String PLUGIN_CONFIG_FILE = "~/.audiogridder";
static const String KNOWN_PLUGINS_FILE = "~/.audiogridderserver.cache";
static const String DEAD_MANS_FILE = "~/.audiogridderserver.crash";
#else
static const String SERVER_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver";
static const String PLUGIN_CONFIG_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridder";
static const String KNOWN_PLUGINS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.cache";
static const String DEAD_MANS_FILE =
    File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "\\.audiogridderserver.crash";
#endif

static constexpr int DEFAULT_NUM_OF_BUFFERS = 8;
static constexpr int DEFAULT_NUM_RECENTS = 10;

static constexpr uint32 DEFAULT_BG_COLOR = 0xff222222;
static constexpr uint32 DEFAULT_BUTTON_COLOR = 0xff333333;

#endif /* Defaults_hpp */
