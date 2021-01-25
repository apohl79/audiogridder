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
#include "Defaults.hpp"

namespace e47 {

class SplashWindow : public TopLevelWindow {
  public:
    SplashWindow() : TopLevelWindow("AudioGridderServer", true) {
        auto& lf = getLookAndFeel();
        lf.setColour(ResizableWindow::backgroundColourId, Colour(Defaults::BG_COLOR));
        lf.setColour(PopupMenu::backgroundColourId, Colour(Defaults::BG_COLOR));
        lf.setColour(TextEditor::backgroundColourId, Colour(Defaults::BUTTON_COLOR));
        lf.setColour(TextButton::buttonColourId, Colour(Defaults::BUTTON_COLOR));
        lf.setColour(ComboBox::backgroundColourId, Colour(Defaults::BUTTON_COLOR));
        lf.setColour(ListBox::backgroundColourId, Colour(Defaults::BG_COLOR));
        lf.setColour(AlertWindow::backgroundColourId, Colour(Defaults::BG_COLOR));

        int w = 640;
        int h = 300;

        auto disp = Desktop::getInstance().getDisplays().getPrimaryDisplay();
        Rectangle<int> totalRect;
        if (nullptr != disp) {
            totalRect = disp->totalArea;
            setBounds(totalRect.getCentreX() - w / 2, totalRect.getCentreY() - h, w, h);
        } else {
            centreWithSize(w, h);
        }

        m_logo.setImage(ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
        m_logo.setBounds(70, 70, 74, 74);
        m_logo.setAlpha(0.9f);
        addChildAndSetID(&m_logo, "logo");

        m_logotxt.setImage(ImageCache::getFromMemory(Images::logotxt_png, Images::logotxt_pngSize));
        m_logotxt.setBounds(160, 70, 420, 79);
        m_logotxt.setAlpha(0.9f);
        addChildAndSetID(&m_logotxt, "logotxt");

        Font font;

        // m_title2.setText("SERVER", NotificationType::dontSendNotification);
        // font.setHeight(35);
        // font.setStyleFlags(Font::plain);
        // m_title2.setFont(font);
        // m_title2.setJustificationType(Justification::right);
        // m_title2.setAlpha(0.4f);
        // m_title2.setBounds(160, 130, 410, 40);
        // addChildAndSetID(&m_title2, "title2");

        m_version.setText(String("Version: ") + AUDIOGRIDDER_VERSION, NotificationType::dontSendNotification);
        font.setHeight(14);
        font.setStyleFlags(Font::plain);
        m_version.setFont(font);
        m_version.setJustificationType(Justification::left);
        m_version.setAlpha(0.4f);
        m_version.setBounds(5, getHeight() - 23, 200, 20);
        addChildAndSetID(&m_version, "version");

        m_date.setText(String("Build date: ") + AUDIOGRIDDER_BUILD_DATE, NotificationType::dontSendNotification);
        font.setHeight(14);
        font.setStyleFlags(Font::plain);
        m_date.setFont(font);
        m_date.setJustificationType(Justification::right);
        m_date.setAlpha(0.2f);
        m_date.setBounds(getWidth() - 400, getHeight() - 23, 395, 20);
        addChildAndSetID(&m_date, "date");

        m_info.setBounds(160, 190, 410, 25);
        font.setHeight(15);
        font.setStyleFlags(Font::plain);
        m_info.setAlpha(0.8f);
        m_info.setJustificationType(Justification::left);
        addChildAndSetID(&m_info, "info");

        for (auto* c : getChildren()) {
            c->addMouseListener(this, true);
        }

        setVisible(true);
        windowToFront(this);
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
    Label m_date;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashWindow)
};

}  // namespace e47

#endif /* SplashWindow_hpp */
