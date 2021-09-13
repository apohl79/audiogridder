/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _SENTRY_HPP_
#define _SENTRY_HPP_

namespace e47 {
namespace Sentry {

void initialize();
void cleanup();
void setEnabled(bool b);
bool isEnabled();

}  // namespace Sentry
}  // namespace e47

#endif  // _SENTRY_HPP_
