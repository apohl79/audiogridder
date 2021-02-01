/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginMonitor.hpp"
#include "PluginProcessor.hpp"
#include "Images.hpp"
#include "WindowPositions.hpp"
#include "Metrics.hpp"

namespace e47 {

PluginStatus::PluginStatus(AudioGridderAudioProcessor* plugin) {
    auto& client = plugin->getClient();
    ok = client.isReadyLockFree();
    auto track = plugin->getTrackProperties();
    channelName = track.name;
    channelColour = track.colour;
    loadedPlugins = client.getLoadedPluginsString();
    String statId = "audio.";
    statId << plugin->getId();
    auto ts = Metrics::getStatistic<TimeStatistic>(statId);
    perf95th = ts->get1minHistogram().nintyFifth;
    blocks = client.NUM_OF_BUFFERS;
}

PluginMonitorWindow::PluginMonitorWindow(PluginMonitor* mon, const String& mode)
    : TopLevelWindow("AudioGridder - " + mode, true), LogTagDelegate(mon), m_mon(mon) {
    traceScope();

    auto& lf = getLookAndFeel();
    lf.setColour(ResizableWindow::backgroundColourId, Colour(Defaults::BG_COLOR));

    m_logo.setImage(ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
    m_logo.setBounds(10, 10, 16, 16);
    m_logo.setAlpha(0.3f);
    m_logo.addMouseListener(this, true);
    addAndMakeVisible(m_logo);

    m_title.setText("Plugin Monitor - " + mode, NotificationType::dontSendNotification);
    m_title.setBounds(30, 10, m_totalWidth - 30, 16);
    auto f = m_title.getFont();
    f.setHeight(f.getHeight() - 2);
    f.setBold(true);
    m_title.setFont(f);
    m_title.setAlpha(0.8f);
    m_title.addMouseListener(this, true);
    addAndMakeVisible(m_title);

    updatePosition();
    setAlwaysOnTop(true);
    setVisible(true);
}

PluginMonitorWindow::~PluginMonitorWindow() {
    traceScope();
    WindowPositions::PositionType pt = WindowPositions::PluginMonFx;
#if JucePlugin_IsSynth
    pt = WindowPositions::PluginMonInst;
#elif JucePlugin_IsMidiEffect
    pt = WindowPositions::PluginMonMidi;
#endif
    WindowPositions::set(pt, {});
}

void PluginMonitorWindow::mouseUp(const MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        setVisible(false);
        PluginMonitor::setAlwaysShow(false);
        m_mon->hideWindow();
    } else {
        PopupMenu m;
        m.addItem("Show Channel Color", true, PluginMonitor::getShowChannelColor(), [] {
            PluginMonitor::setShowChannelColor(!PluginMonitor::getShowChannelColor());
            auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
            cfg["PluginMonChanColor"] = PluginMonitor::getShowChannelColor();
            configWriteFile(Defaults::getConfigFileName(Defaults::ConfigPlugin), cfg);
        });
        m.addItem("Show Channel Name", true, PluginMonitor::getShowChannelName(), [] {
            PluginMonitor::setShowChannelName(!PluginMonitor::getShowChannelName());
            auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
            cfg["PluginMonChanName"] = PluginMonitor::getShowChannelName();
            configWriteFile(Defaults::getConfigFileName(Defaults::ConfigPlugin), cfg);
        });
        m.show();
    }
}

void PluginMonitorWindow::update(const Array<PluginStatus>& status) {
    for (auto& comp : m_components) {
        removeChildComponent(comp.get());
    }
    m_components.clear();

    int borderLR = 15;  // left/right border
    int borderTB = 15;  // top/bottom border
    int rowHeight = 18;

    int colWidth[] = {m_channelColWidth, m_channelNameWidth, 190, 30, 65, 10};

    if (!PluginMonitor::getShowChannelColor()) {
        colWidth[0] = 0;
    }

    if (!PluginMonitor::getShowChannelName()) {
        colWidth[1] = 0;
    }

    auto getLabelBounds = [&](int r, int c, int span = 1) {
        int left = borderLR;
        for (int i = 0; i < c; i++) {
            left += colWidth[i];
        }
        int width = 0;
        for (int i = c; i < c + span; i++) {
            width += colWidth[i];
        }
        return juce::Rectangle<int>(left, borderTB + r * rowHeight, width, rowHeight);
    };

    auto getLineBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR + 2, borderTB + r * rowHeight - 1, getWidth() - borderLR * 2, 1);
    };

    int row = 1;

    if (PluginMonitor::getShowChannelName()) {
        addLabel("Channel", getLabelBounds(row, 0, 2), Justification::topLeft, 1.0f);
    } else if (PluginMonitor::getShowChannelColor()) {
        addLabel("Ch", getLabelBounds(row, 0, 2), Justification::topLeft, 1.0f);
    }
    addLabel("Loaded Chain", getLabelBounds(row, 2), Justification::topLeft, 1.0f);
    addLabel("Buf", getLabelBounds(row, 3), Justification::topRight, 1.0f);
    addLabel("Perf", getLabelBounds(row, 4), Justification::topRight, 1.0f);

    row++;

    for (auto& s : status) {
        auto line = std::make_unique<HirozontalLine>(getLineBounds(row));
        addChildAndSetID(line.get(), "line");
        m_components.push_back(std::move(line));

        if (PluginMonitor::getShowChannelColor()) {
            auto chan = std::make_unique<Channel>(getLabelBounds(row, 0), s.channelColour);
            addChildAndSetID(chan.get(), "led");
            m_components.push_back(std::move(chan));
        }
        if (PluginMonitor::getShowChannelName()) {
            addLabel(s.channelName, getLabelBounds(row, 1));
        }
        addLabel(s.loadedPlugins, getLabelBounds(row, 2));
        addLabel(String(s.blocks), getLabelBounds(row, 3), Justification::topRight);
        addLabel(String(s.perf95th, 2) + " ms", getLabelBounds(row, 4), Justification::topRight);
        auto led = std::make_unique<Status>(getLabelBounds(row, 5), s.ok);
        addChildAndSetID(led.get(), "led");
        m_components.push_back(std::move(led));

        row++;
    }

    for (auto* c : getChildren()) {
        c->addMouseListener(this, true);
    }

    m_totalHeight = rowHeight * row + borderTB + 5;
    updatePosition();
}

void PluginMonitorWindow::addLabel(const String& txt, juce::Rectangle<int> bounds, Justification just, float alpha) {
    auto label = std::make_unique<Label>();
    label->setText(txt, NotificationType::dontSendNotification);
    auto f = label->getFont();
    f.setHeight(f.getHeight() - 2);
    label->setFont(f);
    label->setAlpha(alpha);
    label->setBounds(bounds);
    label->setJustificationType(just);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));
}

void PluginMonitorWindow::updatePosition() {
    int width = m_totalWidth;
    if (!PluginMonitor::getShowChannelColor()) {
        width -= m_channelColWidth;
    }
    if (!PluginMonitor::getShowChannelName()) {
        width -= m_channelNameWidth;
    }

    auto disp = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (nullptr == disp) {
        logln("error: no primary display");
        return;
    }
    auto desktopRect = disp->totalArea;
    int x = desktopRect.getWidth() - width - 20;
    int y = 50;
    WindowPositions::PositionType pt = WindowPositions::PluginMonFx;
    juce::Rectangle<int> upperBounds;

#if JucePlugin_IsSynth
    pt = WindowPositions::PluginMonInst;
    upperBounds = WindowPositions::get(WindowPositions::PluginMonFx, {});
#elif JucePlugin_IsMidiEffect
    pt = WindowPositions::PluginMonMidi;
    upperBounds = WindowPositions::get(WindowPositions::PluginMonInst, {});
    if (upperBounds.isEmpty()) {
        upperBounds = WindowPositions::get(WindowPositions::PluginMonFx, {});
    }
#endif

    if (!upperBounds.isEmpty()) {
        y = upperBounds.getBottom() + 20;
    }

    setBounds(x, y, width, m_totalHeight);
    WindowPositions::set(pt, getBounds());
}

void PluginMonitorWindow::Channel::paint(Graphics& g) {
    int len = 12;
    int x = 4;
    int y = 2;
    g.setColour(m_col);
    g.fillRoundedRectangle((float)x, (float)y, (float)len, (float)len, 3.0f);
    g.setColour(Colours::white);
    g.setOpacity(0.1f);
    g.drawRoundedRectangle((float)x, (float)y, (float)len, (float)len, 3.0f, 1.0f);
}

void PluginMonitorWindow::Status::paint(Graphics& g) {
    int rad = 3;
    int x = getWidth() / 2 - rad;
    int y = getHeight() / 2 - rad;
    Path p;
    p.addEllipse((float)x, (float)y, (float)rad * 2, (float)rad * 2);
    g.setColour(m_col);
    g.setOpacity(0.9f);
    g.fillPath(p);
}

void PluginMonitorWindow::HirozontalLine::paint(Graphics& g) {
    g.setColour(Colours::white);
    g.setOpacity(0.05f);
    g.fillAll();
}

std::mutex PluginMonitor::m_pluginMtx;
Array<AudioGridderAudioProcessor*> PluginMonitor::m_plugins;

std::atomic_bool PluginMonitor::m_showChannelName{true};
std::atomic_bool PluginMonitor::m_showChannelColor{true};

void PluginMonitor::run() {
    traceScope();

    logln("plugin monitor started");

    String mode;
#if JucePlugin_IsSynth
    mode = "Instruments";
#elif JucePlugin_IsMidiEffect
    mode = "Midi";
#else
    mode = "FX";
#endif

    while (!currentThreadShouldExit()) {
        if (m_windowAlwaysShow || m_windowAutoShow || m_windowActive) {
            bool allOk = true;
            Array<PluginStatus> status;
            {
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                for (auto plugin : m_plugins) {
                    PluginStatus s(plugin);
                    allOk = allOk && s.ok;
                    status.add(std::move(s));
                }
            }

            bool show = !m_windowWantsHide && ((!allOk && m_windowAutoShow) || m_windowAlwaysShow);
            bool hide = m_windowWantsHide || (!m_windowAlwaysShow && (allOk || !m_windowAutoShow));
            if (show) {
                m_windowActive = true;
            } else if (hide) {
                m_windowActive = false;
            }
            m_windowWantsHide = false;

            runOnMsgThreadAsync([this, mode, status, show, hide] {
                traceScope();
                if (show && nullptr == m_window) {
                    logln("showing monitor window");
                    m_window = std::make_unique<PluginMonitorWindow>(this, mode);
                } else if (nullptr != m_window && hide) {
                    logln("hiding monitor window");
                    m_window.reset();
                }
                if (nullptr != m_window) {
                    m_window->update(status);
                }
            });
        }
        int sleepTime = m_windowActive ? 500 : 2000;
        sleepExitAwareWithCondition(sleepTime, [this]() -> bool { return !m_windowActive && m_windowAlwaysShow; });
    }

    logln("plugin monitor terminated");
}

void PluginMonitor::add(AudioGridderAudioProcessor* plugin) {
    std::lock_guard<std::mutex> lock(m_pluginMtx);
    m_plugins.addIfNotAlreadyThere(plugin);
}

void PluginMonitor::remove(AudioGridderAudioProcessor* plugin) {
    std::lock_guard<std::mutex> lock(m_pluginMtx);
    m_plugins.removeAllInstancesOf(plugin);
}

}  // namespace e47
