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
    GenericEditor(AudioGridderAudioProcessor& processor);
    ~GenericEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

  private:
    AudioGridderAudioProcessor& m_processor;

    Array<std::unique_ptr<Component>> m_labels;
    Array<std::unique_ptr<Component>> m_components;
    struct OnClick : public MouseListener, public LogTagDelegate {
        std::function<void()> func;
        OnClick(std::function<void()> f) : func(f) {}
        void mouseUp(const MouseEvent& ev) override {
            traceScope();
            if (func && ev.mouseDownTime < ev.eventTime) {
                func();
            }
        }
    };
    Array<std::unique_ptr<OnClick>> m_clickHandlers;

    Client::Parameter& getParameter(int paramIdx);
    Component* getComponent(int paramIdx);

    void updateParameter(int paramIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericEditor)
};

}  // namespace e47
