/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef App_hpp
#define App_hpp

#include <CoreFoundation/CoreFoundation.h>

#include "../JuceLibraryCode/JuceHeader.h"
#include "Defaults.hpp"
#include "Server.hpp"
#include "Screen.h"
#include "PluginListWindow.hpp"
#include "ServerSettingsWindow.hpp"
#include "SplashWindow.hpp"

namespace e47 {

class Server;
class PluginListWindow;

class App : public JUCEApplication, public MenuBarModel {
  public:
    using WindowCaptureCallback = std::function<void(std::shared_ptr<Image> image, int width, int height)>;

    App();

    const String getApplicationName() override { return ProjectInfo::projectName; }
    const String getApplicationVersion() override { return ProjectInfo::versionString; }
    void initialise(const String& commandLineParameters) override;
    void shutdown() override;
    void systemRequestedQuit() override { quit(); }

    const KnownPluginList& getPluginList();
    Server& getServer() { return *m_server; }

    void restartServer();

    void showEditor(std::shared_ptr<AudioProcessor> proc, Thread::ThreadID tid, WindowCaptureCallback func);
    void hideEditor(Thread::ThreadID tid = 0);

    Point<float> localPointToGlobal(Point<float> lp) { return m_window->localPointToGlobal(lp); }

    class MenuBarWindow : public DocumentWindow {
      public:
        MenuBarWindow(App* app)
            : DocumentWindow(ProjectInfo::projectName, Colours::lightgrey, DocumentWindow::allButtons) {
            setMacMainMenu(app);
        }
        ~MenuBarWindow() { setMacMainMenu(nullptr); }
    };

    // MenuBarModel
    StringArray getMenuBarNames() override {
        StringArray names;
        names.add("Settings");
        return names;
    }

    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override {
        PopupMenu menu;
        if (topLevelMenuIndex == 0) {  // Settings
            menu.addItem("Plugins", [this] {
                m_pluginListWindow =
                    std::make_unique<PluginListWindow>(this, m_server->getPluginList(), DEAD_MANS_FILE);
            });
            menu.addItem("Server", [this] { m_srvSettingsWindow = std::make_unique<ServerSettingsWindow>(this); });
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override {}

    void hidePluginList() { m_pluginListWindow.reset(); }
    void hideServerSettings() { m_srvSettingsWindow.reset(); }

    void showSplashWindow() { m_splashWindow = std::make_unique<SplashWindow>(); }
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

    class ProcessorWindow : public DocumentWindow, private Timer {
      public:
        ProcessorWindow(std::shared_ptr<AudioProcessor> proc, WindowCaptureCallback func)
            : DocumentWindow(proc->getName(), Colours::lightgrey, 0), m_processor(proc), m_callback(func) {
            if (proc->hasEditor()) {
                m_editor = std::unique_ptr<AudioProcessorEditor>(proc->createEditor());
                addChildAndSetID(m_editor.get(), "edit");
                setSize(m_editor->getWidth(), m_editor->getHeight());
                setTitleBarHeight(10);
                setVisible(true);
                startTimer(50);
            }
        }

        void hide() {
            if (m_callback) {
                m_callback(nullptr, 0, 0);
            }
        }

      private:
        std::shared_ptr<AudioProcessor> m_processor;
        std::unique_ptr<AudioProcessorEditor> m_editor;
        WindowCaptureCallback m_callback;

        void timerCallback() override { captureWindow(); }

        void captureWindow() {
            if (getWidth() != m_editor->getWidth() || getHeight() != m_editor->getWidth()) {
                setSize(m_editor->getWidth(), m_editor->getHeight());
            }
            if (m_callback) {
                m_callback(captureScreen(m_editor->getScreenBounds()), m_editor->getWidth(), m_editor->getHeight());
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProcessorWindow)
    };

  private:
    std::unique_ptr<Server> m_server;
    std::unique_ptr<ProcessorWindow> m_window;
    std::unique_ptr<PluginListWindow> m_pluginListWindow;
    std::unique_ptr<ServerSettingsWindow> m_srvSettingsWindow;
    std::unique_ptr<SplashWindow> m_splashWindow;
    Thread::ThreadID m_windowOwner;
    std::mutex m_windowMtx;
    FileLogger* m_logger;
    MenuBarWindow m_menuWindow;
};

}  // namespace e47

#endif /* App_hpp */
