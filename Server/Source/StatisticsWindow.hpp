/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef StatisticsWindow_hpp
#define StatisticsWindow_hpp

#include <JuceHeader.h>

#include "Utils.hpp"

namespace e47 {

class App;

class StatisticsWindow : public DocumentWindow, public LogTag {
  public:
    StatisticsWindow(App* app);
    ~StatisticsWindow() override;

    void closeButtonPressed() override;

    class HirozontalLine : public Component {
      public:
        HirozontalLine(juce::Rectangle<int> bounds) { setBounds(bounds); }
        void paint(Graphics& g) override;
    };

  private:
    App* m_app;
    std::vector<std::unique_ptr<Component>> m_components;
    Label m_cpu, m_totalWorkers, m_activeWorkers, m_totalAudioWorkers, m_activeAudioWorkers, m_totalScreenWorkers,
        m_activeScreenWorkers, m_processors, m_plugins, m_audioRPS, m_audioPTavg, m_audioPTmin, m_audioPTmax,
        m_audioPT95th, m_audioBytesOut, m_audioBytesIn;

    class Updater : public Thread, public LogTagDelegate {
      public:
        Updater(LogTag* tag) : Thread("StatsUpdater"), LogTagDelegate(tag) {
            traceScope();
            initAsyncFunctors();
        }

        ~Updater() override {
            traceScope();
            stopAsyncFunctors();
        }

        void set(std::function<void()> fn) { m_fn = fn; }

        void run() override {
            traceScope();
            while (!currentThreadShouldExit()) {
                runOnMsgThreadAsync([this] { m_fn(); });
                // Relax
                sleepExitAware(1000);
            }
        }

      private:
        std::function<void()> m_fn;

        ENABLE_ASYNC_FUNCTORS();
    };
    Updater m_updater;

    void addLabel(const String& txt, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatisticsWindow)
};

}  // namespace e47

#endif /* StatisticsWindow_hpp */
