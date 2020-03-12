/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginButton.hpp"

void PluginButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto& lf = getLookAndFeel();

    auto bgcol = findColour(getToggleState() ? buttonOnColourId : buttonColourId);
    auto baseColour = bgcol.withMultipliedSaturation(hasKeyboardFocus(true) ? 1.3f : 0.9f)
                          .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f);
    if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
        baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);

    g.setColour(baseColour);
    if (!m_active || shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        g.fillRect(getLocalBounds());
    }

    g.setColour(findColour(ComboBox::outlineColourId).withMultipliedAlpha(0.3));

    lf.drawButtonText(g, *this, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void PluginButton::clicked(const ModifierKeys& modifiers) {
    if (m_listener != nullptr) {
        m_listener->buttonClicked(this, modifiers);
    }
}
