/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include <JuceHeader.h>
#include "GenericEditor.hpp"

namespace e47 {

GenericEditor::GenericEditor(PluginProcessor& processor) : LogTag("editor"), m_processor(processor) { traceScope(); }

GenericEditor::~GenericEditor() {}

void GenericEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));  // clear the background
}

void GenericEditor::resized() {
    traceScope();
    m_labels.clear();
    m_components.clear();
    m_clickHandlers.clear();
    m_gestureTrackers.clear();
    int active = m_processor.getActivePlugin();
    if (active < 0) {
        return;
    }

    m_processor.getAllParameterValues(active);

    int rowHeight = 20;
    int rowSpace = 2;
    int leftIndent = 5;
    int topIndent = 5;
    int labelWidth = 200;
    int rangeInfoWidth = 70;
    int componentWidth = 200;
    int row = 0;

    auto& plugin = m_processor.getLoadedPlugin(active);
    auto& params = plugin.getActiveParams();
    for (int i = 0; i < (int)params.size(); i++) {
        auto& param = params[(size_t)i];
        if (param.category > AudioProcessorParameter::genericParameter) {
            continue;  // Parameters like meters are not supported for now.
        }

        auto lbl = std::make_unique<Label>("lbl", param.name);
        lbl->setBounds(leftIndent, topIndent + (rowHeight + rowSpace) * row, labelWidth, rowHeight);
        addAndMakeVisible(lbl.get());
        m_labels.add(std::move(lbl));
        if (param.allValues.size() > 2) {
            auto c = std::make_unique<ComboBox>();
            for (int idx = 0; idx < param.allValues.size(); idx++) {
                c->addItem(param.allValues[idx], idx + 1);
            }
            c->setSelectedId((int)param.getValue() + 1, NotificationType::dontSendNotification);
            c->setBounds(leftIndent + labelWidth, topIndent + (rowHeight + rowSpace) * row, componentWidth, rowHeight);
            c->onChange = [this, active, channel = plugin.activeChannel, i] {
                auto* locComp = dynamic_cast<ComboBox*>(getComponent(i));
                auto& locParam = getParameter(i);
                locParam.setValue((float)locComp->getSelectedItemIndex());
                m_processor.updateParameterValue(active, channel, i, locParam.currentValue);
            };

            auto tracker = std::make_unique<GestureTracker>(this, i, plugin.activeChannel);
            c->addMouseListener(tracker.get(), true);
            m_gestureTrackers.add(std::move(tracker));

            addAndMakeVisible(c.get());
            m_components.add(std::move(c));
        } else {
            auto c = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            c->setTextValueSuffix(param.label);
            c->setNormalisableRange(param.range);
            if (param.isBoolean) {
                c->setNumDecimalPlacesToDisplay(0);
                c->setSliderSnapsToMousePosition(false);
                auto handler = std::make_unique<OnClick>(this, [this, i] {
                    auto* locComp = dynamic_cast<Slider*>(getComponent(i));
                    auto newVal = locComp->getValue() == 0.0 ? 1.0 : 0.0;
                    locComp->setValue(newVal);
                });
                c->addMouseListener(handler.get(), true);
                m_clickHandlers.add(std::move(handler));
            } else {
                c->setNumDecimalPlacesToDisplay(2);
            }
            c->setBounds(leftIndent + labelWidth, topIndent + (rowHeight + rowSpace) * row, componentWidth, rowHeight);
            c->setValue(param.getValue(), NotificationType::dontSendNotification);
            c->onValueChange = [this, active, channel = plugin.activeChannel, i] {
                auto* locComp = dynamic_cast<Slider*>(getComponent(i));
                auto& locParam = getParameter(i);
                locParam.setValue((float)locComp->getValue());
                m_processor.updateParameterValue(active, channel, i, locParam.currentValue);
            };

            auto tracker = std::make_unique<GestureTracker>(this, i, plugin.activeChannel);
            c->addMouseListener(tracker.get(), true);
            m_gestureTrackers.add(std::move(tracker));

            addAndMakeVisible(c.get());
            m_components.add(std::move(c));

            String rangeInfo;
            if (param.isBoolean) {
                rangeInfo << "off-on";
            } else {
                rangeInfo << String(param.range.start, 0) << "-" << String(param.range.end, 0);
            }
            lbl = std::make_unique<Label>("lbl", rangeInfo);
            lbl->setBounds(leftIndent + labelWidth + componentWidth, topIndent + (rowHeight + rowSpace) * row,
                           rangeInfoWidth, rowHeight);
            lbl->setAlpha(0.3f);
            auto fnt = lbl->getFont();
            fnt.setHeight(12);
            lbl->setFont(fnt);
            addAndMakeVisible(lbl.get());
            m_labels.add(std::move(lbl));
        }
        row++;
    }
    setSize(leftIndent + labelWidth + componentWidth + rangeInfoWidth, 20 + (rowHeight + rowSpace) * row);
}

Client::Parameter& GenericEditor::getParameter(int paramIdx) {
    traceScope();
    return m_processor.getLoadedPlugin(m_processor.getActivePlugin()).getActiveParams()[(size_t)paramIdx];
}

Component* GenericEditor::getComponent(int paramIdx) {
    traceScope();
    if (paramIdx >= 0 && paramIdx < m_components.size()) {
        return m_components.getReference(paramIdx).get();
    }
    return nullptr;
}

void GenericEditor::updateParamValue(int paramIdx) {
    traceScope();
    if (auto* comp = getComponent(paramIdx)) {
        if (!m_gestureTrackers.getReference(paramIdx)->isTracking) {
            auto& plugin = m_processor.getLoadedPlugin(m_processor.getActivePlugin());
            auto& param = plugin.getActiveParams()[(size_t)paramIdx];
            if (param.allValues.size() > 2) {
                if (auto* combo = dynamic_cast<ComboBox*>(comp)) {
                    combo->setSelectedId((int)param.getValue() + 1, NotificationType::dontSendNotification);
                }
            } else {
                if (auto* slider = dynamic_cast<Slider*>(comp)) {
                    slider->setValue(param.getValue(), NotificationType::dontSendNotification);
                }
            }
        }
    }
}

}  // namespace e47
