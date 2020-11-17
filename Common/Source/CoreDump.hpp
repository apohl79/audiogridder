/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef CoreDump_hpp
#define CoreDump_hpp

#include <JuceHeader.h>

namespace e47 {
namespace CoreDump {

void initialize(const String& appName, const String& filePrefix, bool showMessage);

}  // namespace CoreDump

}  // namespace e47

#endif /* CoreDump_hpp */
