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
    PluginSearchWindow(float x, float y, PluginProcessor& p);
    ~PluginSearchWindow() override;

    void paint(Graphics&) override;

    using ClickFunction = std::function<void(ServerPlugin s, String layout)>;
    void onClick(ClickFunction f) { m_onClick = f; }

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
    PluginProcessor& m_processor;
    TextEditor m_search;
    TreeView m_tree;
    bool m_showType;

    Array<ServerPlugin> m_recents;
    std::unordered_map<String, ServerPlugin> m_pluginsByName;

    ClickFunction m_onClick;

    const ServerPlugin* getPlugin(const String& key);
    void updateTree(const String& filter = "");
    MenuLevel* addTypeMenu(MenuLevel* level, const String& type);
    MenuLevel* addCategoryMenu(MenuLevel* level, const String& category);
    MenuLevel* addCompanyMenu(MenuLevel* level, const String& company);
    MenuLevel* findPluginLevel(MenuLevel* level);
    TreeViewItem* createPluginMenu(const String& name, MenuLevel& level, ClickFunction addFn);

    void updateHeight();

    const String& normalizeCategory(const String& category);
    const String& normalizeCompany(const String& company);

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
                g.setColour(Colour(Defaults::BG_ACTIVE_COLOR));
                g.fillRect(g.getClipBounds().withTrimmedTop(5));
                g.setColour(Colour(Defaults::ACTIVE_COLOR).withAlpha(0.8f));
            } else {
                g.setColour(Colours::white.withAlpha(0.8f));
            }
            g.drawText(m_name, 8, 0, width, height, Justification::bottomLeft);
        }

        void paintOpenCloseButton(Graphics& g, const Rectangle<float>& r, Colour, bool) override {
            if (isSelected()) {
                g.setColour(Colour(Defaults::BG_ACTIVE_COLOR));
                g.fillRect(r.withTrimmedTop(5));
            }

            auto rect = r.withTrimmedBottom(3.0f).withTrimmedRight(2.0f);
            rect = rect.withPosition(rect.getX() + 3.0f, rect.getY());
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
        TreePlugin(const ServerPlugin& p, std::function<void()> f, bool st, bool sf = false)
            : m_plugin(p), m_onOpenClose(f), m_showType(st), m_showFormat(sf) {}
        bool mightContainSubItems() override { return true; }
        int getItemHeight() const override { return PluginSearchWindow::ITEM_HEIGHT; }

        void paintItem(Graphics& g, int width, int height) override {
            Colour col;
            if (isSelected()) {
                g.setColour(Colour(Defaults::BG_ACTIVE_COLOR));
                g.fillRect(g.getClipBounds().withTrimmedTop(5));
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
            } else if (m_showFormat) {
                g.setColour(col.withAlpha(0.1f));
                g.drawText(m_plugin.getType() == "AudioUnit" ? "AU" : m_plugin.getType(), width - 35, 0, 30, height,
                           Justification::bottomRight);
            }
        }

        void paintOpenCloseButton(Graphics& g, const Rectangle<float>& r, Colour, bool) override {
            if (isSelected()) {
                g.setColour(Colour(Defaults::BG_ACTIVE_COLOR));
                g.fillRect(r.withTrimmedTop(5.0f));
            }

            auto rect = r.withTrimmedTop(3.0f);
            rect = rect.withX(rect.getX() + 3);
            float len = jmin(rect.getWidth(), rect.getHeight());
            float thickness = 1.5f;
            Colour col;

            if (isOpen()) {
                col = Colour(Defaults::ACTIVE_COLOR);
            } else {
                col = Colours::white;
            }

            Path p;
            p.addCentredArc(rect.getCentreX(), rect.getCentreY(), len / 2, len / 2, MathConstants<float>::pi,
                            -15 * MathConstants<float>::pi / 180, 195 * MathConstants<float>::pi / 180, true);
            p.closeSubPath();

            p.startNewSubPath(rect.getCentreX() + 2, rect.getY() + 6);
            p.lineTo(rect.getCentreX() + 5, rect.getY() + 6);

            p.startNewSubPath(rect.getCentreX() + 2, rect.getBottom() - 6);
            p.lineTo(rect.getCentreX() + 5, rect.getBottom() - 6);

            p.startNewSubPath(rect.getX() - 4, rect.getCentreY());
            p.lineTo(rect.getX() - 1, rect.getCentreY());

            g.setColour(col.withAlpha(0.2f));
            g.fillPath(p);

            g.setColour(col.withAlpha(0.8f));
            g.strokePath(p, PathStrokeType(thickness));
        }

        void itemClicked(const MouseEvent&) override { setOpen(!isOpen()); }

        void itemOpennessChanged(bool) override {
            if (m_onOpenClose) {
                m_onOpenClose();
            }
        }

        String getName() { return m_plugin.getName(); }

      private:
        ServerPlugin m_plugin;
        std::function<void()> m_onOpenClose;
        bool m_showType;
        bool m_showFormat;
    };

    class TreeLayout : public TreeViewItem {
      public:
        TreeLayout(const ServerPlugin& p, const String& l, ClickFunction f) : m_plugin(p), m_layout(l), m_onClick(f) {}
        bool mightContainSubItems() override { return false; }
        int getItemHeight() const override { return PluginSearchWindow::ITEM_HEIGHT; }

        void paintItem(Graphics& g, int width, int height) override {
            Colour col;
            if (isSelected()) {
                g.setColour(Colour(Defaults::BG_ACTIVE_COLOR));
                g.fillRect(g.getClipBounds().withTrimmedTop(5));
                col = Colour(Defaults::ACTIVE_COLOR);
            } else {
                col = Colours::white;
            }
            g.setColour(col.withAlpha(0.8f));
            g.drawText(m_layout, 8, 0, width, height, Justification::bottomLeft);
        }

        void itemClicked(const MouseEvent&) override { itemClicked(); }

        void itemClicked() {
            if (m_onClick) {
                m_onClick(m_plugin, m_layout);
            }
        }

      private:
        ServerPlugin m_plugin;
        String m_layout;
        ClickFunction m_onClick;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSearchWindow)
};

}  // namespace e47

#endif  // PluginSearchWindow_hpp
