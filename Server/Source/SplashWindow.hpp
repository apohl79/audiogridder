/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef SplashWindow_hpp
#define SplashWindow_hpp

#include <JuceHeader.h>

#include "Images.hpp"
#include "Version.hpp"

namespace e47 {

class SplashWindow : public TopLevelWindow {
  public:
    SplashWindow() : TopLevelWindow("Splash", true) {
        auto& lf = getLookAndFeel();
        lf.setColour(ResizableWindow::backgroundColourId, Colour(DEFAULT_BG_COLOR));
        lf.setColour(PopupMenu::backgroundColourId, Colour(DEFAULT_BG_COLOR));
        lf.setColour(TextEditor::backgroundColourId, Colour(DEFAULT_BUTTON_COLOR));
        lf.setColour(TextButton::buttonColourId, Colour(DEFAULT_BUTTON_COLOR));
        lf.setColour(ComboBox::backgroundColourId, Colour(DEFAULT_BUTTON_COLOR));
        lf.setColour(ListBox::backgroundColourId, Colour(DEFAULT_BG_COLOR));
        lf.setColour(AlertWindow::backgroundColourId, Colour(DEFAULT_BG_COLOR));

        centreWithSize(450, 180);

        m_logo.setImage(ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
        m_logo.setBounds(10, 10, 100, 100);
        m_logo.setAlpha(0.9f);
        addChildAndSetID(&m_logo, "logo");

        m_logotxt.setImage(ImageCache::getFromMemory(Images::logotxt_png, Images::logotxt_pngSize));
        m_logotxt.setBounds(125, 10, 320, 60);
        m_logotxt.setAlpha(0.9f);
        addChildAndSetID(&m_logotxt, "logotxt");

        Font font;

        m_title2.setText("SERVER", NotificationType::dontSendNotification);
        font.setHeight(25);
        font.setStyleFlags(Font::plain);
        m_title2.setFont(font);
        m_title2.setJustificationType(Justification::right);
        m_title2.setAlpha(0.4f);
        m_title2.setBounds(200, 60, 240, 40);
        addChildAndSetID(&m_title2, "title2");

        m_version.setText(AUDIOGRIDDER_VERSION, NotificationType::dontSendNotification);
        font.setHeight(15);
        font.setStyleFlags(Font::bold);
        m_version.setFont(font);
        m_version.setJustificationType(Justification::right);
        m_version.setAlpha(0.2f);
        m_version.setBounds(200, 85, 240, 40);
        addChildAndSetID(&m_version, "version");

        m_info.setBounds(10, 140, 430, 25);
        font.setHeight(15);
        font.setStyleFlags(Font::plain);
        m_info.setAlpha(0.5);
        addChildAndSetID(&m_info, "info");

        for (auto* c : getChildren()) {
            c->addMouseListener(this, true);
        }

        setVisible(true);
    }

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
    }

    void setInfo(const String& txt, Justification just = Justification::left) {
        m_info.setJustificationType(just);
        m_info.setText(txt, NotificationType::dontSendNotification);
    }

    std::function<void(bool)> onClick = nullptr;

    void mouseUp(const MouseEvent& event) override {
        if (onClick) {
            onClick(event.eventComponent == &m_info);
        }
    }

  private:
    ImageComponent m_logo;
    ImageComponent m_logotxt;
    Label m_title2;
    Label m_info;
    Label m_version;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashWindow)
};

}  // namespace e47

#endif /* SplashWindow_hpp */
