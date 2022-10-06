/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef App_hpp
#define App_hpp

#include <JuceHeader.h>

#include "Defaults.hpp"
#include "Utils.hpp"
#include "Images.hpp"
#include "Message.hpp"
#include "ServerPlugin.hpp"
#include "PluginMonitor.hpp"

namespace e47 {

class App : public JUCEApplication, public LogTag {
  public:
    App() : LogTag("app"), m_srv(this), m_mon(this) {
#if !JUCE_LINUX
        m_tray = std::make_unique<Tray>(this);
#endif
        loadConfig();
    }
    ~App() override {}

    const String getApplicationName() override { return ProjectInfo::projectName; }
    const String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const String& commandLineParameters) override;
    void shutdown() override;
    void systemRequestedQuit() override { quit(); }

    void loadConfig();
    void saveConfig();

    void getPopupMenu(PopupMenu& menu, bool withShowMonitorOption = true);

    bool getKeepRunning() const { return m_keepRunning; }

    class Tray : public SystemTrayIconComponent, public MenuBarModel {
      public:
        Tray(App* app) : m_app(app) {
            setIconImage(ImageCache::getFromMemory(Images::wintray_png, Images::wintray_pngSize),
                         ImageCache::getFromMemory(Images::tray_png, Images::tray_pngSize));
#ifdef JUCE_MAC
            setMacMainMenu(this);
#endif
            auto& lf = getLookAndFeel();
            lf.setColour(PopupMenu::backgroundColourId, Colour(Defaults::BG_COLOR));
        }

        ~Tray() override {
#ifdef JUCE_MAC
            setMacMainMenu(nullptr);
#endif
        }

        StringArray getMenuBarNames() override { return {}; }
        PopupMenu getMenuForIndex(int, const String&) override;
        void menuItemSelected(int, int) override {}

        void mouseUp(const MouseEvent&) override {
            auto menu = getMenuForIndex(0, "");
#ifdef JUCE_MAC
            showDropdownMenu(menu);
#else
            menu.show();
#endif
        }

      private:
        App* m_app;
    };

    class Connection : public InterprocessConnection, public LogTagDelegate {
      public:
        bool connected = false;
        bool initialized = false;

        Connection(App* app) : LogTagDelegate(app), m_app(app) {}
        ~Connection() override {
            disconnect();
            logln("connection " << String::toHexString((uint64)this) << " deleted");
        }

        struct Status {
            String name;
            int channelsIn = 0;
            int channelsOut = 0;
            int channelsSC = 0;
            bool instrument = false;
            uint32 colour = 0;
            String loadedPlugins;
            double perfProcess = 0.0;
            double perfStream = 0.0;
            int blocks = 0;
            String serverNameId;
            String serverHost;
            bool connected = false;
            bool connectedMonTriggered = false;
            bool loadedPluginsOk = false;
            bool loadedPluginsOkMonTriggered = false;
            String loadedPluginsErr;
            int64 lastUpdated = 0;
            size_t rqAvg = 0;
            size_t rq95th = 0;
            int readTimeout = 0;
            uint64_t readErrors = 0;
        };

        Status status;

        void connectionMade() override { connected = true; }
        void connectionLost() override { connected = false; }
        void messageReceived(const MemoryBlock& message) override;
        void sendMessage(const PluginTrayMessage& msg);

      private:
        App* m_app;
    };

    class Server : public InterprocessConnectionServer, public Timer, public LogTagDelegate {
      public:
        Server(App* app) : LogTagDelegate(app), m_app(app) { startTimer(1000); }

        void timerCallback() override {
            MessageManager::callAsync([this] { checkConnections(); });
        }

        void checkConnections();

        const Array<Connection*> getConnections() {
            Array<Connection*> ret;
            for (auto& c : m_connections) {
                if (c->initialized) {
                    ret.add(c.get());
                }
            }
            return ret;
        }

      protected:
        InterprocessConnection* createConnectionObject() override {
            auto c = std::make_shared<Connection>(m_app);
            c->status.lastUpdated = Time::currentTimeMillis();
            MessageManager::callAsync([this, c] { m_connections.add(c); });
            return c.get();
        }

      private:
        App* m_app;
        Array<std::shared_ptr<Connection>> m_connections;
        int m_noConnectionCounter = 0;
    };

    Server& getServer() { return m_srv; }
    void handleMessage(const PluginTrayMessage& msg, Connection& sender);
    void sendRecents(const String& srv, Connection* target = nullptr);

    static String getServerString(Connection* c) { return c->status.serverNameId + " (" + c->status.serverHost + ")"; }
    static String getServerString(const ServerInfo& srvInfo, bool withVersion) {
        return srvInfo.getNameAndID() + " (" + srvInfo.getHost() + ")" +
               (withVersion ? " [" + srvInfo.getVersion() + "]" : "");
    }

    PluginMonitor& getMonitor() { return m_mon; }

  private:
    bool m_keepRunning = false;
    std::unique_ptr<Tray> m_tray;
    Server m_srv;
    std::unordered_map<String, Array<ServerPlugin>> m_recents;
    PluginMonitor m_mon;
};

}  // namespace e47

#endif  // APP_H
