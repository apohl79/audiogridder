/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef App_hpp
#define App_hpp

#include <JuceHeader.h>

#include "Defaults.hpp"
#include "Server.hpp"
#include "Screen.h"
#include "ScreenRecorder.hpp"
#include "PluginListWindow.hpp"
#include "ServerSettingsWindow.hpp"
#include "SplashWindow.hpp"
#include "Utils.hpp"

namespace e47 {

class Server;
class PluginListWindow;

class App : public JUCEApplication, public MenuBarModel, public LogTag {
  public:
    using WindowCaptureCallbackNative = std::function<void(std::shared_ptr<Image> image, int width, int height)>;
    using WindowCaptureCallbackFFmpeg = ScreenRecorder::CaptureCallback;

    App();

    const String getApplicationName() override { return ProjectInfo::projectName; }
    const String getApplicationVersion() override { return ProjectInfo::versionString; }
    void initialise(const String& commandLineParameters) override;
    void shutdown() override;
    void systemRequestedQuit() override { quit(); }

    const KnownPluginList& getPluginList();
    Server& getServer() { return *m_server; }

    void restartServer(bool rescan = false);

    void showEditor(std::shared_ptr<AGProcessor> proc, Thread::ThreadID tid, WindowCaptureCallbackNative func);
    void showEditor(std::shared_ptr<AGProcessor> proc, Thread::ThreadID tid, WindowCaptureCallbackFFmpeg func);
    void hideEditor(Thread::ThreadID tid = nullptr);

    void resetEditor();
    void restartEditor();
    void forgetEditorIfNeeded();

    void updateScreenCaptureArea(int val);

    Point<float> localPointToGlobal(Point<float> lp);

    class MenuBarWindow : public DocumentWindow, public SystemTrayIconComponent {
      public:
        MenuBarWindow(App* app)
            : DocumentWindow(ProjectInfo::projectName, Colours::lightgrey, DocumentWindow::closeButton), m_app(app) {
            PopupMenu m;
            m.addItem("About AudioGridder", [app] {
                app->showSplashWindow([app](bool isInfo) {
                    if (isInfo) {
                        URL("https://audiogridder.com").launchInDefaultBrowser();
                    }
                    app->hideSplashWindow();
                });
                String info = "Copyright (c) 2020 by Andreas Pohl, https://audiogridder.com (MIT license)";
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
            setMacMainMenu(app, &m);
#endif
        }

        ~MenuBarWindow() override {
#ifdef JUCE_MAC
            setMacMainMenu(nullptr);
#endif
        }

        void mouseUp(const MouseEvent& /* event */) override {
#ifdef JUCE_MAC
            auto menu = m_app->getMenuForIndex(0, "Tray");
            menu.addSeparator();
            menu.addItem("Quit", [this] { m_app->systemRequestedQuit(); });
            showDropdownMenu(menu);
#else
            auto menu = m_app->getMenuForIndex(0, "Tray");
            menu.addSeparator();
            menu.addItem("About AudioGridder", [this] {
                m_app->showSplashWindow([this](bool isInfo) {
                    if (isInfo) {
                        URL("https://audiogridder.com").launchInDefaultBrowser();
                    }
                    m_app->hideSplashWindow();
                });
                String info = "Copyright (c) 2020 by Andreas Pohl, https://audiogridder.com (MIT license)";
                m_app->setSplashInfo(info);
            });
            menu.addItem("Quit", [this] { m_app->systemRequestedQuit(); });
            menu.show();
#endif
        }

      private:
        App* m_app;
    };

    // MenuBarModel
    StringArray getMenuBarNames() override {
        StringArray names;
        names.add("Settings");
        return names;
    }

    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& /* menuName */) override {
        PopupMenu menu;
        if (topLevelMenuIndex == 0) {  // Settings
            menu.addItem("Plugins", [this] {
                m_pluginListWindow =
                    std::make_unique<PluginListWindow>(this, m_server->getPluginList(), DEAD_MANS_FILE);
            });
            menu.addItem("Server Settings",
                         [this] { m_srvSettingsWindow = std::make_unique<ServerSettingsWindow>(this); });
            menu.addSeparator();
            menu.addItem("Rescan", [this] { restartServer(true); });
            menu.addItem("Wipe Cache & Rescan", [this] {
                m_server->getPluginList().clear();
                m_server->saveKnownPluginList();
                restartServer(true);
            });
        }
        return menu;
    }

    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex */) override {}

    void hidePluginList() { m_pluginListWindow.reset(); }
    void hideServerSettings() { m_srvSettingsWindow.reset(); }

    void showSplashWindow(std::function<void(bool)> onClick = nullptr) {
        m_splashWindow = std::make_unique<SplashWindow>();
        if (onClick) {
            m_splashWindow->onClick = onClick;
        }
    }
    // called from the server thread
    void hideSplashWindow() {
        MessageManager::callAsync([this] { m_splashWindow.reset(); });
    }
    void setSplashInfo(const String& txt) {
        MessageManager::callAsync([this, txt] {
            if (nullptr != m_splashWindow) {
                m_splashWindow->setInfo(txt);
            }
        });
    }

    class ProcessorWindow : public DocumentWindow, private Timer, public LogTag {
      public:
        ProcessorWindow(std::shared_ptr<AGProcessor> proc, WindowCaptureCallbackNative func)
            : DocumentWindow(proc->getName(), Colours::lightgrey, 0),
              LogTag("procwindow"),
              m_processor(proc),
              m_callbackNative(func),
              m_callbackFFmpeg(nullptr) {
            traceScope();
            if (m_processor->hasEditor()) {
                createEditor();
            }
        }

        ProcessorWindow(std::shared_ptr<AGProcessor> proc, WindowCaptureCallbackFFmpeg func)
            : DocumentWindow(proc->getName(), Colours::lightgrey, 0),
              LogTag("procwindow"),
              m_processor(proc),
              m_callbackNative(nullptr),
              m_callbackFFmpeg(func) {
            traceScope();
            if (m_processor->hasEditor()) {
                createEditor();
            }
        }

        ~ProcessorWindow() override {
            traceScope();
            stopCapturing();
            if (m_editor != nullptr) {
                delete m_editor;
                m_editor = nullptr;
            }
        }

        BorderSize<int> getBorderThickness() override { return {}; }

        void forgetEditor() {
            traceScope();
            // Allow a processor to delete his editor, so we should not delete it again
            m_editor = nullptr;
            stopCapturing();
        }

        Rectangle<int> getScreenCaptureRect() {
            traceScope();
            if (nullptr != m_editor && nullptr != m_processor) {
                auto rect = m_editor->getScreenBounds();
                rect.setSize(rect.getWidth() + m_processor->getAdditionalScreenCapturingSpace(),
                             rect.getHeight() + m_processor->getAdditionalScreenCapturingSpace());
                if (rect.getRight() > m_totalRect.getRight()) {
                    rect.setRight(m_totalRect.getRight());
                }
                if (rect.getBottom() > m_totalRect.getBottom()) {
                    rect.setBottom(m_totalRect.getBottom());
                }
                return rect;
            }
            traceln("m_editor=" << (uint64)m_editor << " m_processor=" << (uint64)m_processor.get());
            return m_screenCaptureRect;
        }

        void updateScreenCaptureArea() {
            traceScope();
            auto rect = getScreenCaptureRect();
            if (m_screenRec.isRecording() && m_processor->hasEditor() && nullptr != m_editor &&
                m_screenCaptureRect != rect) {
                traceln("updating area");
                m_screenCaptureRect = rect;
                m_screenRec.stop();
                m_screenRec.resume(m_screenCaptureRect);
            }
        }

        void stopCapturing() {
            traceScope();
            if (m_callbackNative) {
                stopTimer();
            } else {
                m_screenRec.stop();
            }
        }

        void resized() override {
            traceScope();
            ResizableWindow::resized();
            updateScreenCaptureArea();
        }

        void setVisible(bool b) override {
            traceScope();
            if (!b) {
                stopCapturing();
            }
            Component::setVisible(b);
        }

      private:
        std::shared_ptr<AGProcessor> m_processor;
        AudioProcessorEditor* m_editor = nullptr;
        ScreenRecorder m_screenRec;
        WindowCaptureCallbackNative m_callbackNative;
        WindowCaptureCallbackFFmpeg m_callbackFFmpeg;
        Rectangle<int> m_screenCaptureRect, m_totalRect;

        void createEditor() {
            traceScope();
            m_totalRect = Desktop::getInstance().getDisplays().getMainDisplay().totalArea;
            m_editor = m_processor->createEditorIfNeeded();
            setContentNonOwned(m_editor, true);
            setTitleBarHeight(30);
            setVisible(true);
            setAlwaysOnTop(true);
            if (!getApp()->getServer().getScreenCapturingOff()) {
                if (m_callbackNative) {
                    startTimer(50);
                } else {
                    m_screenCaptureRect = getScreenCaptureRect();
                    m_screenRec.start(m_screenCaptureRect, m_callbackFFmpeg);
                }
            }
        }

        void timerCallback() override { captureWindow(); }

        void captureWindow() {
            traceScope();
            if (m_editor == nullptr) {
                traceln("no editor");
                return;
            }
            if (m_callbackNative) {
                m_screenCaptureRect = getScreenCaptureRect();
                m_callbackNative(captureScreenNative(m_screenCaptureRect), m_screenCaptureRect.getWidth(),
                                 m_screenCaptureRect.getHeight());
            } else {
                traceln("no callback");
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorWindow)
    };

  private:
    std::unique_ptr<Server> m_server;
    std::unique_ptr<std::thread> m_child;
    std::unique_ptr<ProcessorWindow> m_window;
    std::unique_ptr<PluginListWindow> m_pluginListWindow;
    std::unique_ptr<ServerSettingsWindow> m_srvSettingsWindow;
    std::unique_ptr<SplashWindow> m_splashWindow;
    Thread::ThreadID m_windowOwner;
    std::shared_ptr<AGProcessor> m_windowProc;
    WindowCaptureCallbackNative m_windowFuncNative;
    WindowCaptureCallbackFFmpeg m_windowFuncFFmpeg;
    std::mutex m_windowMtx;
    std::unique_ptr<MenuBarWindow> m_menuWindow;
    std::atomic_bool m_stopChild{false};
};

}  // namespace e47

#endif /* App_hpp */
