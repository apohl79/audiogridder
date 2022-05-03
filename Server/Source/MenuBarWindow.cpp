/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "MenuBarWindow.hpp"
#include "App.hpp"
#include "Images.hpp"

namespace e47 {

MenuBarWindow::MenuBarWindow(App* app)
    : DocumentWindow(ProjectInfo::projectName, Colours::lightgrey, DocumentWindow::closeButton), m_app(app) {
    PopupMenu m;
    m.addItem("About AudioGridder", [app] {
        app->showSplashWindow([app](bool isInfo) {
            if (isInfo) {
                URL("https://audiogridder.com").launchInDefaultBrowser();
            }
            app->hideSplashWindow();
        });
#ifdef JUCE_WINDOWS
        String info = L"\xa9 2020-2022 Andreas Pohl, https://audiogridder.com";
#else
        String info = L"© 2020-2022 Andreas Pohl, https://audiogridder.com";
#endif
        app->setSplashInfo(info);
    });
    const char* logoNoMac = Images::logowintray_png;
    int logoNoMacSize = Images::logowintray_pngSize;
#ifdef JUCE_WINDOWS
    bool lightTheme =
        WindowsRegistry::getValue(
            "HKEY_CURRENT_"
            "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\SystemUsesLightTheme",
            "1") == "1";
    if (lightTheme) {
        logoNoMac = Images::logowintraylight_png;
        logoNoMacSize = Images::logowintraylight_pngSize;
    }
#endif
    setIconImage(ImageCache::getFromMemory(logoNoMac, logoNoMacSize),
                 ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
#ifdef JUCE_MAC
    app->setMacMainMenu(app, &m);
#endif
}

MenuBarWindow::~MenuBarWindow() {
#ifdef JUCE_MAC
    m_app->setMacMainMenu(nullptr);
#endif
}

void MenuBarWindow::mouseUp(const MouseEvent& /* event */) {
    auto menu = m_app->getMenuForIndex(0, "Tray");
    menu.addSeparator();
    menu.addItem("About AudioGridder", [this] {
        m_app->showSplashWindow([this](bool isInfo) {
            if (isInfo) {
                URL("https://audiogridder.com").launchInDefaultBrowser();
            }
            m_app->hideSplashWindow();
        });
#ifdef JUCE_WINDOWS
        String info = L"\xa9 2020-2022 Andreas Pohl, https://audiogridder.com";
#else
        String info = L"© 2020-2022 Andreas Pohl, https://audiogridder.com";
#endif
        m_app->setSplashInfo(info);
    });
    menu.addItem("Restart", [this] { m_app->prepareShutdown(App::EXIT_RESTART); });
    menu.addItem("Quit", [this] { m_app->prepareShutdown(); });
#ifdef JUCE_MAC
    showDropdownMenu(menu);
#else
    menu.show();
#endif
}

}  // namespace e47
