/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef NumberConversion_hpp
#define NumberConversion_hpp

namespace e47 {

template <typename T>
static inline T as(int n) {
    return static_cast<T>(n);
}

template <typename T>
static inline T as(long int n) {
    return static_cast<T>(n);
}

template <typename T>
static inline T as(size_t n) {
    return static_cast<T>(n);
}

template <typename T>
static inline T as(unsigned int n) {
    return static_cast<T>(n);
}

}  // namespace e47

#endif /* NumberConversion_hpp */
