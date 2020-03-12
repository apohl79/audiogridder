/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include <JuceHeader.h>

class NewServerWindow : public TopLevelWindow, public TextButton::Listener {
  public:
    NewServerWindow(float x, float y);
    ~NewServerWindow();

    void paint(Graphics&) override;

    virtual void buttonClicked(Button* button) override;

    using OkFuction = std::function<void(String s)>;
    void onOk(OkFuction f) { m_onOk = f; }

    void activeWindowStatusChanged() override;

  private:
    TextEditor m_server;
    TextButton m_ok;
    TextButton m_cancel;

    OkFuction m_onOk;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewServerWindow)
};
