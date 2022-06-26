/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.hpp"
#include "Tracer.hpp"

namespace e47 {

class GenericEditor : public Component, public LogTag {
  public:
    GenericEditor(PluginProcessor& processor);
    ~GenericEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void updateParamValue(int paramIdx);

  private:
    PluginProcessor& m_processor;

    Array<std::unique_ptr<Component>> m_labels;
    Array<std::unique_ptr<Component>> m_components;
    struct OnClick : MouseListener, LogTagDelegate {
        std::function<void()> func;

        OnClick(LogTag* tag, std::function<void()> f) : LogTagDelegate(tag), func(f) {}

        void mouseUp(const MouseEvent& ev) override {
            traceScope();
            if (func && ev.mouseDownTime < ev.eventTime) {
                func();
            }
        }
    };
    Array<std::unique_ptr<OnClick>> m_clickHandlers;

    struct GestureTracker : MouseListener, LogTagDelegate {
        int idx, channel;
        bool isTracking = false;
        PluginProcessor& processor;

        GestureTracker(GenericEditor* e, int i, int c)
            : LogTagDelegate(e), idx(i), channel(c), processor(e->m_processor) {}

        void mouseDown(const MouseEvent&) override {
            traceScope();
            isTracking = true;
            processor.updateParameterGestureTracking(processor.getActivePlugin(), channel, idx, true);
        }

        void mouseUp(const MouseEvent&) override {
            traceScope();
            isTracking = false;
            processor.updateParameterGestureTracking(processor.getActivePlugin(), channel, idx, false);
        }
    };
    Array<std::unique_ptr<GestureTracker>> m_gestureTrackers;

    Client::Parameter& getParameter(int paramIdx);
    Component* getComponent(int paramIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericEditor)
};

}  // namespace e47
