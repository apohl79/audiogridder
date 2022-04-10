/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef _MENUBARWINDOW_HPP_
#define _MENUBARWINDOW_HPP_

#include <JuceHeader.h>

namespace e47 {

class App;

class MenuBarWindow : public DocumentWindow, public SystemTrayIconComponent {
  public:
    MenuBarWindow(App* app);
    ~MenuBarWindow() override;

    void mouseUp(const MouseEvent&) override;

  private:
    App* m_app;
};

}  // namespace e47

#endif  // _MENUBARWINDOW_HPP_
