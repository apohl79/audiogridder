/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Defaults_hpp
#define Defaults_hpp

static constexpr int DEFAULT_CLIENT_PORT = 55055;
static constexpr int DEFAULT_SERVER_PORT = 55056;

static const String SERVER_CONFIG_FILE = "~/.audiogridderserver";
static const String PLUGIN_CONFIG_FILE = "~/.audiogridder";
static const String DEAD_MANS_FILE = "~/.audiogridderserver.crash";

static constexpr int DEFAULT_NUM_OF_BUFFERS = 8;

#endif /* Defaults_hpp */
