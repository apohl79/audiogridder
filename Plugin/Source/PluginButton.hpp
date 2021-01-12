/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef PluginButton_hpp
#define PluginButton_hpp

#include <JuceHeader.h>

namespace e47 {

class PluginButton : public TextButton {
  public:
    PluginButton(const String& id, const String& name, bool extraButtons = true);
    virtual ~PluginButton() override {}

    enum AreaType { MAIN, BYPASS, MOVE_DOWN, MOVE_UP, DELETE };

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void buttonClicked(Button* button, const ModifierKeys& modifiers, PluginButton::AreaType area) = 0;
    };

    void setOnClickWithModListener(Listener* p) { m_listener = p; }
    void setActive(bool b) { m_active = b; }

    const String& getPluginId() const { return m_id; }

    void mouseUp(const MouseEvent& event) override;
    void mouseMove(const MouseEvent& event) override;

    AreaType getAreaType() const;

    void setEnabled(bool b) {
        m_enabled = b;
        repaint();
    }
    bool isEnabled() const { return m_enabled; }

  protected:
    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void clicked(const ModifierKeys& modifiers) override;
    void drawText(Graphics& g, int left, int right);

    // Avoid hidden overload warning
    using Button::clicked;

  private:
    Listener* m_listener = nullptr;
    bool m_active = false;
    bool m_enabled = true;
    String m_id;
    bool m_withExtraButtons = true;
    Rectangle<int> m_bypassArea, m_moveUpArea, m_moveDownArea, m_deleteArea;
    Point<int> m_lastMousePosition;

    inline bool isWithinArea(const Rectangle<int>& area, const Point<int>& point) const {
        return point.getX() >= area.getX() && point.getX() <= area.getRight();
    }
};

}  // namespace e47

#endif /* PluginButton_hpp */
