/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef KeyAndMouse_hpp
#define KeyAndMouse_hpp

#if defined(JUCE_MAC) || defined(JUCE_WINDOWS)

#include <stdint.h>
#include <string>
#include <unordered_map>

#include "KeyAndMouseCommon.hpp"

namespace e47 {

static inline bool isShiftKey(uint16_t kc) { return kc == 0x38; }
static inline bool isControlKey(uint16_t kc) { return kc == 0x3B; }
static inline bool isAltKey(uint16_t kc) { return kc == 0x3A; }
static inline bool isCopyKey(uint16_t kc) { return kc == COPYKEY; }
static inline bool isPasteKey(uint16_t kc) { return kc == PASTEKEY; }
static inline bool isCutKey(uint16_t kc) { return kc == CUTKEY; }
static inline bool isSelectAllKey(uint16_t kc) { return kc == SELECTALLKEY; }

void setShiftKey(uint64_t& flags);
void setControlKey(uint64_t& flags);
void setAltKey(uint64_t& flags);
void setCopyKeys(uint16_t& key, uint64_t& flags);
void setPasteKeys(uint16_t& key, uint64_t& flags);
void setCutKeys(uint16_t& key, uint64_t& flags);
void setSelectAllKeys(uint16_t& key, uint64_t& flags);

void mouseEvent(MouseEvType t, float x, float y, uint64_t flags = 0);
void mouseScrollEvent(float x, float y, float deltaX, float deltaY, bool isSmooth);
void keyEventDown(uint16_t keyCode, uint64_t flags = 0, bool currentProcessOnly = false, void* nativeHandle = nullptr);
void keyEventUp(uint16_t keyCode, uint64_t flags = 0, bool currentProcessOnly = false, void* nativeHandle = nullptr);

}  // namespace e47

#endif

#endif /* KeyAndMouse_hpp */
