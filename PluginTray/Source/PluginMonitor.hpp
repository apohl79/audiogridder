/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef __PLUGINMONITOR_H_
#define __PLUGINMONITOR_H_

#include <JuceHeader.h>

#include "SharedInstance.hpp"
#include "Utils.hpp"
#include "Defaults.hpp"

namespace e47 {

class App;
class PluginMonitor;

class PluginMonitorWindow : public TopLevelWindow, public LogTagDelegate {
  public:
    PluginMonitorWindow(PluginMonitor* mon, App* app);
    ~PluginMonitorWindow() override;

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
    }

    void mouseUp(const MouseEvent& event) override;

    void update();

  private:
    class PluginMonitorComponent : public Component {
      public:
        void paint(Graphics& g) override {
            g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
        }
        friend class PluginMonitorWindow;
    };

    class Channel : public Component {
      public:
        Channel(juce::Rectangle<int> bounds, Colour col) : m_col(col) {
            setBounds(bounds);
            if (m_col.isTransparent()) {
                m_col = Colours::white.withAlpha(0.1f);
            }
        }
        void paint(Graphics& g) override;

      private:
        Colour m_col;
    };

    class Status : public Component {
      public:
        Status() {}
        Status(juce::Rectangle<int> bounds, bool connected, bool loadedPluginsOk) {
            setBounds(bounds);
            setColor(connected, loadedPluginsOk);
        }

        void setColor(bool connected, bool loadedPluginsOk) {
            m_col = connected
                        ? loadedPluginsOk ? Colour(Defaults::PLUGIN_OK_COLOR) : Colour(Defaults::PLUGIN_NOTLOADED_COLOR)
                        : Colour(Defaults::PLUGIN_NOTCONNECTED_COLOR);
        }

        void paint(Graphics& g) override;

      private:
        Colour m_col;
    };

    class HirozontalLine : public Component {
      public:
        HirozontalLine(juce::Rectangle<int> bounds, bool bold) : m_bold(bold) { setBounds(bounds); }
        void paint(Graphics& g) override;

      private:
        bool m_bold;
    };

    PluginMonitor* m_mon;
    App* m_app;
    PluginMonitorComponent m_main;
    Viewport m_viewPort;
    ImageComponent m_logo;
    Label m_title;
    Status m_legendOk, m_legendNotConnected, m_legendNotLoaded;
    Label m_legendOkLbl, m_legendNotConnectedLbl, m_legendNotLoadedLbl;
    int m_totalWidth = 665;
    int m_totalHeight = 32;
    int m_legendWidth = 230;
    int m_channelColWidth = 20;
    int m_channelColIdx = 0;
    int m_channelNameWidth = 100;
    int m_channelNameIdx = 1;
    int m_bufferWidth = 30;
    int m_bufferAvgIdx = 5;
    int m_buffer95thIdx = 6;
    int m_readErrWidth = 50;
    int m_readErrIdx = 7;
    int m_perfProcessWidth = 65;
    int m_perfProcessIdx = 8;
    std::vector<std::unique_ptr<Component>> m_components;
    TooltipWindow m_tooltipWindow;

    void addLabel(const String& txt, const String& tooltip, juce::Rectangle<int> bounds,
                  Justification just = Justification::topLeft, Colour col = Colours::white, float alpha = 0.6f);
    int getConditionalWidth();
    void updatePosition();
};

class PluginMonitor : public LogTag, public Timer {
  public:
    bool showChannelName = true;
    bool showChannelColor = true;
    bool showBufferAvg = false;
    bool showBuffer95th = false;
    bool showReadErrors = true;
    bool showPerfProcess = false;
    bool windowAutoShow = true;
    bool windowAlwaysShow = false;
    bool windowActive = false;

    PluginMonitor(App* app) : LogTag("monitor"), m_app(app) { startTimer(100); }
    ~PluginMonitor() override {}

    void hideWindow() {
        windowAlwaysShow = false;
        m_hideCounter = 0;
        m_window.reset();
    }

    void refresh() { m_needsUpdate = true; }

    void timerCallback() override;

  private:
    App* m_app;
    std::unique_ptr<PluginMonitorWindow> m_window;
    std::atomic_bool m_needsUpdate{false};
    std::atomic_int m_hideCounter;

    void update();
};

}  // namespace e47

#endif  // __PLUGINMONITOR_H_
