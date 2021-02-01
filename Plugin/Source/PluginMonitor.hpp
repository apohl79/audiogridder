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

class AudioGridderAudioProcessor;
class PluginMonitor;

struct PluginStatus {
    PluginStatus(AudioGridderAudioProcessor* plugin);
    String channelName;
    Colour channelColour;
    String loadedPlugins;
    double perf95th;
    int blocks;
    bool ok;
};

class PluginMonitorWindow : public TopLevelWindow, public LogTagDelegate {
  public:
    PluginMonitorWindow(PluginMonitor* mon, const String& mode);
    ~PluginMonitorWindow() override;

    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
    }

    void mouseUp(const MouseEvent& event) override;

    void update(const Array<PluginStatus>& status);

  private:
    PluginMonitor* m_mon;
    ImageComponent m_logo;
    Label m_title;
    int m_totalWidth = 445;
    int m_totalHeight = 32;
    int m_channelColWidth = 20;
    int m_channelNameWidth = 100;
    std::vector<std::unique_ptr<Component>> m_components;

    void addLabel(const String& txt, juce::Rectangle<int> bounds, Justification just = Justification::topLeft,
                  float alpha = 0.6f);
    void updatePosition();

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
        Status(juce::Rectangle<int> bounds, bool ok) {
            setBounds(bounds);
            m_col = ok ? Colour(Defaults::PLUGIN_OK_COLOR) : Colour(Defaults::PLUGIN_NOTOK_COLOR);
        }

        void paint(Graphics& g) override;

      private:
        Colour m_col;
    };

    class HirozontalLine : public Component {
      public:
        HirozontalLine(juce::Rectangle<int> bounds) { setBounds(bounds); }
        void paint(Graphics& g) override;
    };
};

class PluginMonitor : public Thread, public LogTag, public SharedInstance<PluginMonitor> {
  public:
    PluginMonitor() : Thread("PluginMonitor"), LogTag("monitor") {
        traceScope();
        initAsyncFunctors();
        startThread();
    }

    ~PluginMonitor() override {
        traceScope();
        stopAsyncFunctors();
        stopThread(-1);
    }

    void run() override;

    static void add(AudioGridderAudioProcessor* plugin);
    static void remove(AudioGridderAudioProcessor* plugin);

    static void setAutoShow(bool b) {
        auto inst = getInstance();
        if (nullptr != inst) {
            inst->m_windowAutoShow = b;
        }
    }

    static bool getAutoShow() {
        auto inst = getInstance();
        if (nullptr != inst) {
            return inst->m_windowAutoShow;
        }
        return false;
    }

    static void setAlwaysShow(bool b) {
        auto inst = getInstance();
        if (nullptr != inst) {
            inst->m_windowAlwaysShow = b;
        }
    }

    static bool getShowChannelName() {
        auto inst = getInstance();
        if (nullptr != inst) {
            return inst->m_showChannelName;
        }
        return false;
    }

    static void setShowChannelName(bool b) {
        auto inst = getInstance();
        if (nullptr != inst) {
            inst->m_showChannelName = b;
        }
    }

    static bool getShowChannelColor() {
        auto inst = getInstance();
        if (nullptr != inst) {
            return inst->m_showChannelColor;
        }
        return false;
    }

    static void setShowChannelColor(bool b) {
        auto inst = getInstance();
        if (nullptr != inst) {
            inst->m_showChannelColor = b;
        }
    }

    void hideWindow() { m_windowWantsHide = true; }

  private:
    static std::mutex m_pluginMtx;
    static Array<AudioGridderAudioProcessor*> m_plugins;

    static std::atomic_bool m_showChannelName;
    static std::atomic_bool m_showChannelColor;

    std::unique_ptr<PluginMonitorWindow> m_window;
    std::atomic_bool m_windowAutoShow{true};
    std::atomic_bool m_windowAlwaysShow{false};
    std::atomic_bool m_windowActive{false};
    std::atomic_bool m_windowWantsHide{false};

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif  // __PLUGINMONITOR_H_
