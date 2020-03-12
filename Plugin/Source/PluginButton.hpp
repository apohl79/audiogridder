/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef PluginButton_hpp
#define PluginButton_hpp

#include "../JuceLibraryCode/JuceHeader.h"

class PluginButton : public TextButton {
  public:
    PluginButton(const String& id, const String& name) : TextButton(name), m_id(id) {}
    virtual ~PluginButton() {}

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void buttonClicked(Button* button, const ModifierKeys& modifiers) = 0;
    };

    void setOnClickWithModListener(Listener* p) { m_listener = p; }
    void setActive(bool b) { m_active = b; }

    const String& getPluginId() const { return m_id; }

  protected:
    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    virtual void clicked(const ModifierKeys& modifiers) override;

  private:
    Listener* m_listener = nullptr;
    bool m_active = false;
    String m_id;
};

#endif /* PluginButton_hpp */
