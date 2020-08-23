/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef PluginButton_hpp
#define PluginButton_hpp

#include <JuceHeader.h>

class PluginButton : public TextButton {
  public:
    PluginButton(const String& id, const String& name, bool extraButtons = true);
    virtual ~PluginButton() override {}

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void buttonClicked(Button* button, const ModifierKeys& modifiers) = 0;
    };

    void setOnClickWithModListener(Listener* p) { m_listener = p; }
    void setActive(bool b) { m_active = b; }

    const String& getPluginId() const { return m_id; }

    void mouseUp(const MouseEvent& event) override;

    enum AreaType { MAIN, BYPASS, MOVE_DOWN, MOVE_UP, DELETE };

    AreaType getAreaType() const;

  protected:
    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void clicked(const ModifierKeys& modifiers) override;
    void drawText(Graphics& g, int left, int right);

  private:
    Listener* m_listener = nullptr;
    bool m_active = false;
    String m_id;
    bool m_withExtraButtons = true;
    Rectangle<int> m_bypassArea, m_moveUpArea, m_moveDownArea, m_deleteArea;
    Point<int> m_lastMousePosition;
};

#endif /* PluginButton_hpp */
