/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef KeyAndMouse_hpp
#define KeyAndMouse_hpp

#include <stdint.h>
#include <string>
#include <unordered_map>

#include "KeyAndMouseCommon.hpp"

namespace e47 {

static inline bool isShiftKey(uint16_t kc) { return kc == 0x38; }
static inline bool isControlKey(uint16_t kc) { return kc == 0x3B; }
static inline bool isAltKey(uint16_t kc) { return kc == 0x3A; }

void setShiftKey(uint64_t& flags);
void setControlKey(uint64_t& flags);
void setAltKey(uint64_t& flags);

void mouseEvent(MouseEvType t, float x, float y, uint64_t flags = 0);
void mouseScrollEvent(float x, float y, float deltaX, float deltaY, bool isSmooth);
void keyEventDown(uint16_t keyCode, uint64_t flags = 0);
void keyEventUp(uint16_t keyCode, uint64_t flags = 0);

}  // namespace e47

#endif /* KeyAndMouse_hpp */
