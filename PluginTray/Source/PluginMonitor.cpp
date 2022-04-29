/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "PluginMonitor.hpp"
#include "Images.hpp"
#include "WindowPositions.hpp"

namespace e47 {

PluginMonitorWindow::PluginMonitorWindow(PluginMonitor* mon, App* app)
    : TopLevelWindow("AudioGridder PluginMon", true), LogTagDelegate(mon), m_mon(mon), m_app(app) {
    auto& lf = getLookAndFeel();
    lf.setColour(ResizableWindow::backgroundColourId, Colour(Defaults::BG_COLOR));

    m_logo.setImage(ImageCache::getFromMemory(Images::logo_png, Images::logo_pngSize));
    m_logo.setBounds(10, 10, 16, 16);
    m_logo.setAlpha(0.3f);
    m_logo.addMouseListener(this, true);
    addAndMakeVisible(m_logo);

    m_title.setText("AGridder Monitor", NotificationType::dontSendNotification);
    m_title.setBounds(30, 10, m_totalWidth / 2, 16);
    auto font = m_title.getFont();
    font.setHeight(font.getHeight() - 2);
    font.setBold(true);
    m_title.setFont(font);
    m_title.setAlpha(0.8f);
    m_title.addMouseListener(this, true);
    addAndMakeVisible(m_title);

    int legendX = 260;
    font.setBold(false);

    auto addLegend = [&](Status& legend, Label& label, const String& text) {
        legend.setBounds(legendX, 10, 6, 16);
        legendX += 8;
        addAndMakeVisible(legend);
        label.setText(text, NotificationType::dontSendNotification);
        label.setFont(font);
        label.setAlpha(0.3f);
        label.setBounds(legendX, 10, font.getStringWidth(text) + 18, 16);
        addAndMakeVisible(label);
        legendX += label.getWidth();
    };

    m_legendOk.setColor(true, true);
    addLegend(m_legendOk, m_legendOkLbl, "ok");
    m_legendNotLoaded.setColor(true, false);
    addLegend(m_legendNotLoaded, m_legendNotLoadedLbl, "not loaded");
    m_legendNotConnected.setColor(false, false);
    addLegend(m_legendNotConnected, m_legendNotConnectedLbl, "not connected");

    m_main.setBounds(0, 0, m_totalWidth, m_totalHeight);
    m_viewPort.setViewedComponent(&m_main, false);
    m_viewPort.setBounds(0, 35, m_totalWidth, m_totalHeight);
    addAndMakeVisible(m_viewPort);

    updatePosition();
    setAlwaysOnTop(true);
    setVisible(true);
}

PluginMonitorWindow::~PluginMonitorWindow() {
    traceScope();
    WindowPositions::PositionType pt = WindowPositions::PluginMonFx;
    WindowPositions::set(pt, {});
}

void PluginMonitorWindow::mouseUp(const MouseEvent& event) {
    if (event.mods.isLeftButtonDown()) {
        setVisible(false);
        m_mon->hideWindow();
    } else {
        PopupMenu menu;
        m_app->getPopupMenu(menu, false);
        menu.show();
    }
}

void PluginMonitorWindow::update() {
    for (auto& comp : m_components) {
        m_main.removeChildComponent(comp.get());
    }
    m_components.clear();

    int borderLR = 15;  // left/right border
    int borderTB = 0;  // top/bottom border
    int rowHeight = 19;

    int colWidth[] = {m_channelColWidth, m_channelNameWidth, 190, 45, 30, 65, 10};

    if (!m_mon->showChannelColor) {
        colWidth[0] = 0;
    }

    if (!m_mon->showChannelName) {
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
        return juce::Rectangle<int>(left, borderTB + r * rowHeight + 1, width, rowHeight - 1);
    };

    auto getLineBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR + 2, borderTB + r * rowHeight - 1, getWidth() - borderLR * 2, 1);
    };

    int row = 0;

    if (m_mon->showChannelName) {
        addLabel("Channel", "", getLabelBounds(row, 0, 2), Justification::topLeft, 1.0f);
    } else if (m_mon->showChannelColor) {
        addLabel("Ch", "", getLabelBounds(row, 0, 2), Justification::topLeft, 1.0f);
    }
    addLabel("Inserts", "", getLabelBounds(row, 2), Justification::topLeft, 1.0f);
    addLabel("I/O", "", getLabelBounds(row, 3), Justification::topRight, 1.0f);
    addLabel("Buf", "", getLabelBounds(row, 4), Justification::topRight, 1.0f);
    addLabel("Perf", "", getLabelBounds(row, 5), Justification::topRight, 1.0f);

    row++;

    auto addRow = [&](App::Connection::Status& s, bool boldLine) {
        auto line = std::make_unique<HirozontalLine>(getLineBounds(row), boldLine);
        m_main.addChildAndSetID(line.get(), "line");
        m_components.push_back(std::move(line));

        if (m_mon->showChannelColor) {
            auto chan = std::make_unique<Channel>(getLabelBounds(row, 0), Colour(s.colour));
            m_main.addChildAndSetID(chan.get(), "led");
            m_components.push_back(std::move(chan));
        }
        if (m_mon->showChannelName) {
            addLabel(s.name, s.loadedPluginsErr, getLabelBounds(row, 1));
        }
        String io;
        io << s.channelsIn << ":" << s.channelsOut;
        if (s.channelsSC > 0) {
            io << "+" << s.channelsSC;
        }
        addLabel(s.loadedPlugins, s.loadedPluginsErr, getLabelBounds(row, 2));
        addLabel(io, s.loadedPluginsErr, getLabelBounds(row, 3), Justification::topRight);
        addLabel(String(s.blocks), s.loadedPluginsErr, getLabelBounds(row, 4), Justification::topRight);
        addLabel(String(s.perf95th, 2) + " ms", s.loadedPluginsErr, getLabelBounds(row, 5), Justification::topRight);
        auto led = std::make_unique<Status>(getLabelBounds(row, 6), s.connected, s.loadedPluginsOk);
        m_main.addChildAndSetID(led.get(), "led");
        m_components.push_back(std::move(led));

        row++;
    };

    bool first = true;

    for (auto& c : m_app->getServer().getConnections()) {
        if (!c->status.connected || !c->status.loadedPluginsOk) {
            addRow(c->status, first);
            first = false;
        }
    }

    first = true;

    for (auto& c : m_app->getServer().getConnections()) {
        if (c->status.connected && c->status.loadedPluginsOk) {
            addRow(c->status, first);
            first = false;
        }
    }

    for (auto* c : getChildren()) {
        c->addMouseListener(this, true);
    }

    m_totalHeight = rowHeight * row + borderTB + 5;
    updatePosition();
}

void PluginMonitorWindow::addLabel(const String& txt, const String& tooltip, juce::Rectangle<int> bounds,
                                   Justification just, float alpha) {
    auto label = std::make_unique<Label>();
    label->setText(txt, NotificationType::dontSendNotification);
    label->setTooltip(tooltip);
    auto f = label->getFont();
    f.setHeight(f.getHeight() - 2);
    label->setFont(f);
    label->setAlpha(alpha);
    label->setBounds(bounds);
    label->setJustificationType(just);
    m_main.addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));
}

void PluginMonitorWindow::updatePosition() {
    int width = m_totalWidth;
    if (!m_mon->showChannelColor) {
        width -= m_channelColWidth;
    }
    if (!m_mon->showChannelName) {
        width -= m_channelNameWidth;
    }

    auto disp = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (nullptr == disp) {
        logln("error: no primary display");
        return;
    }
    auto desktopRect = disp->userArea;
    int x = desktopRect.getWidth() - width - 20;
    int y = desktopRect.getY() + 20;
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

    int totalHeight = jmin(m_totalHeight, 600);
    m_main.setBounds(m_main.getBounds().withHeight(m_totalHeight));
    m_viewPort.setBounds(m_viewPort.getBounds().withHeight(totalHeight));
    m_viewPort.setScrollBarsShown(totalHeight < m_totalHeight, false);
    setBounds(x, y, width, totalHeight + 40);
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
    g.setOpacity(m_bold ? 0.10f : 0.05f);
    g.fillAll();
}

void PluginMonitor::update() {
    bool allOk = true;
    for (auto& c : m_app->getServer().getConnections()) {
        allOk = allOk && c->status.connected && c->status.loadedPluginsOk;
        if (!allOk) {
            break;
        }
    }

    bool show = ((!allOk && windowAutoShow) || windowAlwaysShow);
    bool hide = (!windowAlwaysShow && (allOk || !windowAutoShow));

    if (show) {
        windowActive = true;
        m_hideCounter = 0;
    } else if (hide) {
        m_hideCounter = 20;
    }

    if (show && nullptr == m_window) {
        m_window = std::make_unique<PluginMonitorWindow>(this, m_app);
    }

    if (nullptr != m_window) {
        m_window->update();
    }
}

void PluginMonitor::timerCallback() {
    if (m_needsUpdate) {
        update();
        m_needsUpdate = false;
    }
    if (m_hideCounter > 0) {
        m_hideCounter--;
        if (m_hideCounter == 0) {
            windowActive = false;
            m_window.reset();
        }
    }
}

}  // namespace e47
