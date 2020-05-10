/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginButton.hpp"

PluginButton::PluginButton(const String& id, const String& name, bool extraButtons)
    : TextButton(name), m_id(id), m_withExtraButtons(extraButtons) {}

void PluginButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bgcol = findColour(getToggleState() ? buttonOnColourId : buttonColourId);
    auto baseColour = bgcol.withMultipliedSaturation(hasKeyboardFocus(true) ? 1.3f : 0.9f)
                          .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f);
    if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
    }
    auto fgColor = findColour(getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                       .withMultipliedAlpha(isEnabled() ? 0.7f : 0.4f);

    if (!m_active || shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        g.setColour(baseColour);
        g.fillRect(getLocalBounds());
    }

    if (m_active) {
        g.setColour(findColour(ComboBox::outlineColourId).withMultipliedAlpha(0.9));
        float dashs[] = {4.0, 2.0};
        g.drawDashedLine(Line<float>(0, 0, getWidth(), 0), dashs, 2);
        g.drawDashedLine(Line<float>(0, getHeight(), getWidth(), getHeight()), dashs, 2);
    }

    int textIndentLeft = 0;
    int textIndentRight = 0;

    drawText(g, textIndentLeft, textIndentRight);
}

void PluginButton::clicked(const ModifierKeys& modifiers) {
    if (m_listener != nullptr) {
        m_listener->buttonClicked(this, modifiers);
    }
}

void PluginButton::drawText(Graphics& g, int left, int right) {
    auto& lf = getLookAndFeel();
    Font font(lf.getTextButtonFont(*this, getHeight()));
    g.setFont(font);
    g.setColour(findColour(getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                    .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f));

    const int yIndent = jmin(4, proportionOfHeight(0.3f));
    const int cornerSize = jmin(getHeight(), getWidth()) / 2;

    const int fontHeight = roundToInt(font.getHeight() * 0.6f);
    const int leftIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnLeft() ? 4 : 2)) + left;
    const int rightIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnRight() ? 4 : 2)) + right;
    const int textWidth = getWidth() - leftIndent - rightIndent;

    if (textWidth > 0)
        g.drawFittedText(getButtonText(), leftIndent, yIndent, textWidth, getHeight() - yIndent * 2,
                         Justification::centred, 2);
}

void PluginButton::mouseUp(const MouseEvent& event) {
    m_lastMousePosition = event.getPosition();
    Button::mouseUp(event);
}

PluginButton::AreaType PluginButton::getAreaType() const {
    if (!m_withExtraButtons) {
        return PluginButton::MAIN;
    }
    return PluginButton::MAIN;
}
