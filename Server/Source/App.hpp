/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef App_hpp
#define App_hpp

#include <JuceHeader.h>

#include "ProcessorWindow.hpp"
#include "Utils.hpp"

namespace e47 {

class Server;
class Processor;
class PluginListWindow;
class ServerSettingsWindow;
class PluginListWindow;
class StatisticsWindow;
class SplashWindow;
class MenuBarWindow;

class App : public JUCEApplication, public MenuBarModel, public LogTag {
  public:
    enum ExitCodes : uint32 {
        EXIT_OK = 0,
        EXIT_RESTART = 1,
        EXIT_SANDBOX_INIT_ERROR = 101,
        EXIT_SANDBOX_BIND_ERROR = 102,
        EXIT_SANDBOX_NO_MASTER = 103,
        EXIT_SANDBOX_PARAM_ERROR = 104
    };

    App();
    ~App() override;

    const String getApplicationName() override { return ProjectInfo::projectName; }
    const String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const String& commandLineParameters) override;
    void shutdown() override;
    void systemRequestedQuit() override { quit(); }

    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& /* menuName */) override;
    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex */) override {}

    // MenuBarModel
    StringArray getMenuBarNames() override { return {"Settings"}; }

    void prepareShutdown(uint32 exitCode = 0);
    const KnownPluginList& getPluginList();
    void restartServer(bool rescan = false);
    std::shared_ptr<Server> getServer() { return m_server; }

    void showEditor(Thread::ThreadID tid, std::shared_ptr<Processor> proc, ProcessorWindow::CaptureCallbackFFmpeg func,
                    std::function<void()> onHide, int x = 0, int y = 0);
    void showEditor(Thread::ThreadID tid, std::shared_ptr<Processor> proc, ProcessorWindow::CaptureCallbackNative func,
                    std::function<void()> onHide, int x = 0, int y = 0);

    void hideEditor(Thread::ThreadID tid = nullptr, bool updateMacOSDock = true);
    void resetEditor(Thread::ThreadID tid);
    void bringEditorToFront(Thread::ThreadID tid);
    void moveEditor(Thread::ThreadID tid, int x, int y);
    std::shared_ptr<Processor> getCurrentWindowProc(Thread::ThreadID tid);
    void restartEditor(Thread::ThreadID tid);
    void updateScreenCaptureArea(Thread::ThreadID tid, int val = 0);
    Point<float> localPointToGlobal(Thread::ThreadID tid, Point<float> lp);
    void addKeyListener(Thread::ThreadID tid, KeyListener* l);

    using ErrorCallback = std::function<void(const String&)>;
    ErrorCallback getWorkerErrorCallback(Thread::ThreadID tid);
    void setWorkerErrorCallback(Thread::ThreadID tid, ErrorCallback fn);

    void hidePluginList();
    void hideServerSettings();
    void hideStatistics();
    void showSplashWindow(std::function<void(bool)> onClick = nullptr);
    void hideSplashWindow(int wait = 0);
    void setSplashInfo(const String& txt);
    void enableCancelScan(int srvId, std::function<void()> onCancel);
    void disableCancelScan(int srvId);

    void updateDockIcon() {
#ifdef JUCE_MAC
        bool show = nullptr != m_srvSettingsWindow || nullptr != m_statsWindow || nullptr != m_pluginListWindow ||
                    nullptr != m_splashWindow;
        Process::setDockIconVisible(show);
#endif
    }

  private:
    std::shared_ptr<Server> m_server;
    std::unique_ptr<std::thread> m_child;
    std::atomic_bool m_stopChild{false};
    std::atomic_bool m_preparingShutdown{false};

    std::unordered_map<uint64, std::shared_ptr<Processor>> m_processors;
    std::mutex m_processorsMtx;

    std::unordered_map<uint64, ErrorCallback> m_workerErrorCallbacks;
    std::mutex m_workerErrorCallbacksMtx;

    std::unique_ptr<PluginListWindow> m_pluginListWindow;
    std::unique_ptr<ServerSettingsWindow> m_srvSettingsWindow;
    std::unique_ptr<StatisticsWindow> m_statsWindow;
    std::shared_ptr<SplashWindow> m_splashWindow;
    std::unique_ptr<MenuBarWindow> m_menuWindow;

    FnThread m_splashHider;

    uint32 m_exitCode = 0;

    template <typename T>
    void showEditorInternal(Thread::ThreadID tid, std::shared_ptr<Processor> proc, T func, std::function<void()> onHide,
                            int x, int y);

    std::shared_ptr<Processor> getCurrentWindowProcInternal(Thread::ThreadID tid);
    std::shared_ptr<ProcessorWindow> getCurrentWindow(Thread::ThreadID tid);

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* App_hpp */
