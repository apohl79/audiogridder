/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "StatisticsWindow.hpp"
#include "Client.hpp"
#include "Metrics.hpp"
#include "PluginEditor.hpp"
#include "WindowPositions.hpp"

#include <memory>
#include <thread>

namespace e47 {

std::unique_ptr<StatisticsWindow> StatisticsWindow::m_inst = nullptr;

struct Inst : SharedInstance<Inst> {};

StatisticsWindow::StatisticsWindow()
    : DocumentWindow("Plugin Statistics",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      LogTag("statistics"),
      m_updater(this) {
    traceScope();

    int totalWidth = 400;
    int totalHeight = 40;
    int borderLR = 15;  // left/right border
    int borderTB = 15;  // top/bottom border
    int rowHeight = 25;

    int fieldWidth = 80;
    int fieldHeight = 25;
    int labelWidth = 250;
    int labelHeight = 30;

    auto getLabelBounds = [&](int r, int indent = 0) {
        return juce::Rectangle<int>(borderLR + indent, borderTB + r * rowHeight, labelWidth, labelHeight);
    };
    auto getFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - fieldWidth - borderLR, borderTB + r * rowHeight + 3, fieldWidth,
                                    fieldHeight);
    };
    auto getLineBounds = [&](int r) {
        return juce::Rectangle<int>(5, borderTB + r * rowHeight, totalWidth - borderLR, rowHeight);
    };

    int row = 1;
    String tmpStr;

    String mode;
#if JucePlugin_IsSynth
    mode = "Instrument";
#elif JucePlugin_IsMidiEffect
    mode = "Midi";
#else
    mode = "FX";
#endif

    setName(mode + " " + Component::getName());

    addLabel("Number of loaded " + mode + " plugins:", getLabelBounds(row));
    m_totalClients.setBounds(getFieldBounds(row));
    m_totalClients.setJustificationType(Justification::right);
    addChildAndSetID(&m_totalClients, "totalclients");

    row++;

    auto line = std::make_unique<HirozontalLine>(getLineBounds(row++));
    addChildAndSetID(line.get(), "line");
    m_components.push_back(std::move(line));

    addLabel("Audio/MIDI", getLabelBounds(row++));
    addLabel("Messages per second:", getLabelBounds(row, 15));
    m_audioRPS.setBounds(getFieldBounds(row));
    m_audioRPS.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioRPS, "audioptavg");

    row++;

    addLabel("Processing time (95th percentile):", getLabelBounds(row, 15));
    m_audioPT95th.setBounds(getFieldBounds(row));
    m_audioPT95th.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioPT95th, "audiopt95");

    row++;

    addLabel("Processing time (average):", getLabelBounds(row, 15));
    m_audioPTavg.setBounds(getFieldBounds(row));
    m_audioPTavg.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioPTavg, "audioptavg");

    row++;

    addLabel("Processing time (min):", getLabelBounds(row, 15));
    m_audioPTmin.setBounds(getFieldBounds(row));
    m_audioPTmin.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioPTmin, "audioptmin");

    row++;

    addLabel("Processing time (max):", getLabelBounds(row, 15));
    m_audioPTmax.setBounds(getFieldBounds(row));
    m_audioPTmax.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioPTmax, "audioptmax");

    row++;

    line = std::make_unique<HirozontalLine>(getLineBounds(row++));
    addChildAndSetID(line.get(), "line");
    m_components.push_back(std::move(line));

    addLabel("Network I/O", getLabelBounds(row++));
    addLabel("Outbound:", getLabelBounds(row, 15));
    m_audioBytesOut.setBounds(getFieldBounds(row));
    m_audioBytesOut.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioBytesOut, "netout");

    row++;

    addLabel("Inbound:", getLabelBounds(row, 15));
    m_audioBytesIn.setBounds(getFieldBounds(row));
    m_audioBytesIn.setJustificationType(Justification::right);
    addChildAndSetID(&m_audioBytesIn, "netin");

    row++;

    totalHeight += row * rowHeight;

    auto audioTime = Metrics::getStatistic<TimeStatistic>("audio");
    auto bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
    auto bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");

    m_updater.set([this, audioTime, bytesOutMeter, bytesInMeter] {
        traceScope();
        m_totalClients.setText(String(Client::count), NotificationType::dontSendNotification);
        auto hist = audioTime->get1minHistogram();
        auto rps = audioTime->getMeter().rate_1min();
        m_audioRPS.setText(String(lround(rps)), NotificationType::dontSendNotification);
        m_audioPT95th.setText(String(hist.nintyFifth, 2) + " ms", NotificationType::dontSendNotification);
        m_audioPTavg.setText(String(hist.avg, 2) + " ms", NotificationType::dontSendNotification);
        m_audioPTmin.setText(String(hist.min, 2) + " ms", NotificationType::dontSendNotification);
        m_audioPTmax.setText(String(hist.max, 2) + " ms", NotificationType::dontSendNotification);

        auto netOut = bytesOutMeter->rate_1min();
        auto netIn = bytesInMeter->rate_1min();
        String dataUnitOut = " B/s";
        String dataUnitIn = " B/s";
        if (netOut > 1024) {
            netOut /= 1024;
            dataUnitOut = " KB/s";
        }
        if (netOut > 1024) {
            netOut /= 1024;
            dataUnitOut = " MB/s";
        }
        if (netIn > 1024) {
            netIn /= 1024;
            dataUnitIn = " KB/s";
        }
        if (netIn > 1024) {
            netIn /= 1024;
            dataUnitIn = " MB/s";
        }
        m_audioBytesOut.setText(String(netOut, 2) + dataUnitOut, NotificationType::dontSendNotification);
        m_audioBytesIn.setText(String(netIn, 2) + dataUnitIn, NotificationType::dontSendNotification);
    });
    m_updater.startThread();

    centreWithSize(totalWidth, totalHeight);
    auto pt = WindowPositions::PluginStatsFx;
#if JucePlugin_IsSynth
    pt = WindowPositions::PluginStatsInst;
#elif JucePlugin_IsMidiEffect
    pt = WindowPositions::PluginStatsMidi;
#endif
    setBounds(WindowPositions::get(pt, getBounds()));
    setVisible(true);
    windowToFront(this);
}

StatisticsWindow::~StatisticsWindow() {
    traceScope();
    auto pt = WindowPositions::PluginStatsFx;
#if JucePlugin_IsSynth
    pt = WindowPositions::PluginStatsInst;
#elif JucePlugin_IsMidiEffect
    pt = WindowPositions::PluginStatsMidi;
#endif
    WindowPositions::set(pt, getBounds());
    m_updater.stopThread(-1);
    clearContentComponent();
}

void StatisticsWindow::closeButtonPressed() {
    traceScope();
    m_updater.signalThreadShouldExit();
    hide();
}

void StatisticsWindow::addLabel(const String& txt, juce::Rectangle<int> bounds) {
    auto label = std::make_unique<Label>();
    label->setText(txt, NotificationType::dontSendNotification);
    label->setBounds(bounds);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));
}

void StatisticsWindow::HirozontalLine::paint(Graphics& g) {
    g.setColour(Colours::white);
    g.setOpacity(0.3f);
    int y = getHeight() / 2 + 3;
    juce::Rectangle<int> r(getX(), y, getWidth(), 5);
    Line<float> line(r.toFloat().getTopLeft(), r.toFloat().getTopRight());
    float dashs[] = {6.0, 4.0};
    g.drawDashedLine(line, dashs, 2);
}

void StatisticsWindow::initialize() { Inst::initialize(); }

void StatisticsWindow::cleanup() {
    Inst::cleanup([](auto) { hide(); });
}

void StatisticsWindow::show() {
    if (nullptr == m_inst) {
        m_inst = std::make_unique<StatisticsWindow>();
    } else {
        windowToFront(m_inst.get());
    }
}

void StatisticsWindow::hide() { m_inst.reset(); }

}  // namespace e47
