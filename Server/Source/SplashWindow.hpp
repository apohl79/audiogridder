/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef SplashWindow_hpp
#define SplashWindow_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "Images.hpp"

namespace e47 {

class SplashWindow : public TopLevelWindow {
  public:
    SplashWindow() : TopLevelWindow("Splash", true) {
        centreWithSize(400, 180);

        m_logo.setImage(ImageCache::getFromMemory(Images::serverinv_png, Images::serverinv_pngSize));
        m_logo.setBounds(10, 10, 100, 100);
        m_logo.setAlpha(0.6);
        addChildAndSetID(&m_logo, "logo");

        m_title.setText("AudioGridder", NotificationType::dontSendNotification);
        Font font(40, Font::bold);
        m_title.setFont(font);
        m_title.setAlpha(0.9);
        m_title.setBounds(130, 10, 260, 60);
        addChildAndSetID(&m_title, "title");

        m_title2.setText("Server", NotificationType::dontSendNotification);
        font.setHeight(25);
        font.setStyleFlags(Font::plain);
        m_title2.setFont(font);
        m_title2.setAlpha(0.4);
        m_title2.setBounds(300, 50, 100, 40);
        addChildAndSetID(&m_title2, "title2");

        m_info.setBounds(10, 140, 380, 25);
        m_info.setAlpha(0.5);
        addChildAndSetID(&m_info, "info");

        setVisible(true);
    }

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
    }

    void setInfo(const String& txt) { m_info.setText(txt, NotificationType::dontSendNotification); }

  private:
    ImageComponent m_logo;
    Label m_title;
    Label m_title2;
    Label m_info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashWindow)
};

}  // namespace e47

#endif /* SplashWindow_hpp */
