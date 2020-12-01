/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "StatisticsWindow.hpp"
#include "App.hpp"
#include "CPUInfo.hpp"
#include "Metrics.hpp"
#include "WindowPositions.hpp"

namespace e47 {

StatisticsWindow::StatisticsWindow(App* app)
    : DocumentWindow("Server Statistics",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      LogTag("statistics"),
      m_app(app),
      m_updater(this) {
    traceScope();
    setUsingNativeTitleBar(true);

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

    int row = 0;
    String tmpStr;

    addLabel("CPU Load:", getLabelBounds(row));
    m_cpu.setBounds(getFieldBounds(row));
    m_cpu.setJustificationType(Justification::right);
    addChildAndSetID(&m_cpu, "cpu");

    row++;

    auto line = std::make_unique<HirozontalLine>(getLineBounds(row++));
    addChildAndSetID(line.get(), "line");
    m_components.push_back(std::move(line));

    addLabel("Total workers:", getLabelBounds(row));
    m_totalWorkers.setBounds(getFieldBounds(row));
    m_totalWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_totalWorkers, "totalworkers");

    row++;

    addLabel("Active workers:", getLabelBounds(row));
    m_activeWorkers.setBounds(getFieldBounds(row));
    m_activeWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_activeWorkers, "activeworkers");

    row++;

    addLabel("Total audio workers:", getLabelBounds(row));
    m_totalAudioWorkers.setBounds(getFieldBounds(row));
    m_totalAudioWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_totalAudioWorkers, "totalaudioworkers");

    row++;

    addLabel("Active audio workers:", getLabelBounds(row));
    m_activeAudioWorkers.setBounds(getFieldBounds(row));
    m_activeAudioWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_activeAudioWorkers, "activeaudioworkers");

    row++;

    addLabel("Total screen workers:", getLabelBounds(row));
    m_totalScreenWorkers.setBounds(getFieldBounds(row));
    m_totalScreenWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_totalScreenWorkers, "totalscreenworkers");

    row++;

    addLabel("Active screen workers:", getLabelBounds(row));
    m_activeScreenWorkers.setBounds(getFieldBounds(row));
    m_activeScreenWorkers.setJustificationType(Justification::right);
    addChildAndSetID(&m_activeScreenWorkers, "activescreenworkers");

    row++;

    addLabel("Number of processors:", getLabelBounds(row));
    m_processors.setBounds(getFieldBounds(row));
    m_processors.setJustificationType(Justification::right);
    addChildAndSetID(&m_processors, "processors");

    row++;

    addLabel("Loaded plugins:", getLabelBounds(row));
    m_plugins.setBounds(getFieldBounds(row));
    m_plugins.setJustificationType(Justification::right);
    addChildAndSetID(&m_plugins, "plugins");

    row++;

    line = std::make_unique<HirozontalLine>(getLineBounds(row++));
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
        m_cpu.setText(String(CPUInfo::getUsage(), 2) + "%", NotificationType::dontSendNotification);
        m_totalWorkers.setText(String(Worker::count), NotificationType::dontSendNotification);
        m_activeWorkers.setText(String(Worker::runCount), NotificationType::dontSendNotification);
        m_totalAudioWorkers.setText(String(AudioWorker::count), NotificationType::dontSendNotification);
        m_activeAudioWorkers.setText(String(AudioWorker::runCount), NotificationType::dontSendNotification);
        m_totalScreenWorkers.setText(String(ScreenWorker::count), NotificationType::dontSendNotification);
        m_activeScreenWorkers.setText(String(ScreenWorker::runCount), NotificationType::dontSendNotification);
        m_processors.setText(String(AGProcessor::count), NotificationType::dontSendNotification);
        m_plugins.setText(String(AGProcessor::loadedCount), NotificationType::dontSendNotification);

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
    setBounds(WindowPositions::get(WindowPositions::ServerStats, getBounds()));
    setVisible(true);
    windowToFront(this);
}

StatisticsWindow::~StatisticsWindow() {
    WindowPositions::set(WindowPositions::ServerStats, getBounds());
    m_updater.stopThread(-1);
    clearContentComponent();
}

void StatisticsWindow::closeButtonPressed() {
    traceScope();
    m_updater.signalThreadShouldExit();
    m_app->hideStatistics();
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

}  // namespace e47
