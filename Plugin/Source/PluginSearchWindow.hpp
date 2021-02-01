/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef PluginSearchWindow_hpp
#define PluginSearchWindow_hpp

#include <JuceHeader.h>

#include "PluginProcessor.hpp"
#include "ServerPlugin.hpp"
#include "Defaults.hpp"

namespace e47 {

class PluginSearchWindow : public TopLevelWindow, public KeyListener, public LogTagDelegate {
  public:
    PluginSearchWindow(float x, float y, AudioGridderAudioProcessor& p);
    ~PluginSearchWindow() override;

    void paint(Graphics&) override;

    using ClickFuction = std::function<void(ServerPlugin s)>;
    void onClick(ClickFuction f) { m_onClick = f; }

    void hide();

    // Component
    using Component::keyPressed;

    void inputAttemptWhenModal() override {
        if (!isMouseOver(true)) {
            hide();
        }
    }

    // TopLevelWindow
    void activeWindowStatusChanged() override {
        if (!isActiveWindow()) {
            hide();
        }
    }

    // KeyListener
    bool keyPressed(const KeyPress& kp, Component*) override;

    // MouseListener
    void mouseMove(const MouseEvent& event) override;
    void mouseExit(const MouseEvent&) override;

  private:
    AudioGridderAudioProcessor& m_processor;
    TextEditor m_search;
    TreeView m_tree;
    bool m_showType;

    std::vector<ServerPlugin> m_recents;

    ClickFuction m_onClick;

    void updateTree(const String& filter = "");
    TreeViewItem* createPluginMenu(const String& name, MenuLevel& level,
                                   std::function<void(const ServerPlugin& plug)> addFn);

    void updateHeight();

    const static int ITEM_HEIGHT = 20;
    const static int SEPARATOR_HEIGHT = 5;
    const static int MAX_ITEMS_VISIBLE = 30;
    const static int MIN_ITEMS_VISIBLE = 5;

    class TreeRoot : public TreeViewItem {
      public:
        TreeRoot() { setOpen(true); }
        bool mightContainSubItems() override { return true; }
        bool canBeSelected() const override { return false; }
    };

    class TreeSeparator : public TreeViewItem {
      public:
        bool mightContainSubItems() override { return false; }
        bool canBeSelected() const override { return false; }
        int getItemHeight() const override { return PluginSearchWindow::SEPARATOR_HEIGHT; }

        void paintItem(Graphics& g, int w, int) override {
            Line<float> line(0, 5, (float)w - 5, 5);
            float dashs[] = {2.0, 1.5};
            g.setColour(Colours::white.withAlpha(0.1f));
            g.drawDashedLine(line, dashs, 2);
        }
    };

    class TreeFolder : public TreeViewItem {
      public:
        TreeFolder(const String& name, std::function<void()> f) : m_name(name), m_onOpenClose(f) {}

        bool mightContainSubItems() override { return true; }
        int getItemHeight() const override { return PluginSearchWindow::ITEM_HEIGHT; }

        void paintItem(Graphics& g, int width, int height) override {
            if (isSelected()) {
                g.setColour(Colour(Defaults::ACTIVE_COLOR).withAlpha(0.8f));
            } else {
                g.setColour(Colours::white.withAlpha(0.8f));
            }
            g.drawText(m_name, 8, 0, width, height, Justification::bottomLeft);
        }

        void paintOpenCloseButton(Graphics& g, const Rectangle<float>& r, Colour, bool) override {
            auto rect = r.withTrimmedBottom(3.0f);
            float len = jmin(rect.getWidth(), rect.getHeight());
            float thickness = 1.5f;
            Point<float> p1, p2, p3;
            Colour col;

            if (isOpen()) {
                col = Colour(Defaults::ACTIVE_COLOR);
                p1 = Point<float>(rect.getX(), rect.getBottom() - len);
                p2 = Point<float>(rect.getX() + len / 2, rect.getBottom());
                p3 = Point<float>(rect.getX() + len, rect.getBottom() - len);
            } else {
                col = Colours::white;
                p1 = Point<float>(rect.getX(), rect.getBottom() - len);
                p2 = Point<float>(rect.getX() + len, rect.getBottom() - len / 2);
                p3 = Point<float>(rect.getX(), rect.getBottom());
            }

            Path p;
            p.addTriangle(p1, p2, p3);

            g.setColour(col.withAlpha(0.2f));
            g.fillPath(p);

            g.setColour(col.withAlpha(0.8f));
            g.drawLine(Line<float>(p1, p2), thickness);
            g.drawLine(Line<float>(p2, p3), thickness);
            g.drawLine(Line<float>(p3, p1), thickness);
        }

        void itemClicked(const MouseEvent&) override { setOpen(!isOpen()); }

        void itemOpennessChanged(bool) override {
            if (m_onOpenClose) {
                m_onOpenClose();
            }
        }

      private:
        String m_name;
        std::function<void()> m_onOpenClose;
    };

    class TreePlugin : public TreeViewItem {
      public:
        TreePlugin(const ServerPlugin& p, ClickFuction f, bool st) : m_plugin(p), m_onClick(f), m_showType(st) {}

        bool mightContainSubItems() override { return false; }
        int getItemHeight() const override { return PluginSearchWindow::ITEM_HEIGHT; }

        void paintItem(Graphics& g, int width, int height) override {
            Colour col;
            if (isSelected()) {
                col = Colour(Defaults::ACTIVE_COLOR);
            } else {
                col = Colours::white;
            }
            g.setColour(col.withAlpha(0.8f));
            g.drawText(m_plugin.getName(), 8, 0, width - (m_showType ? 40 : 0), height, Justification::bottomLeft);
            if (m_showType) {
                g.setColour(col.withAlpha(0.1f));
                g.drawText(m_plugin.isInstrument() ? "Inst" : "Fx", width - 35, 0, 30, height,
                           Justification::bottomRight);
            }
        }

        void itemClicked(const MouseEvent&) override { itemClicked(); }

        void itemClicked() {
            if (m_onClick) {
                m_onClick(m_plugin);
            }
        }

        String getName() { return m_plugin.getName(); }

      private:
        ServerPlugin m_plugin;
        ClickFuction m_onClick;
        bool m_showType;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSearchWindow)
};

}  // namespace e47

#endif  // PluginSearchWindow_hpp
