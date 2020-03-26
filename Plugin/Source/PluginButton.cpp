/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginButton.hpp"

void PluginButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto& lf = getLookAndFeel();

    if (!m_active || shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        auto bgcol = findColour(getToggleState() ? buttonOnColourId : buttonColourId);
        auto baseColour = bgcol.withMultipliedSaturation(hasKeyboardFocus(true) ? 1.3f : 0.9f)
                              .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f);
        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
        g.setColour(baseColour);
        g.fillRect(getLocalBounds());
    }

    if (m_active) {
        g.setColour(findColour(ComboBox::outlineColourId).withMultipliedAlpha(0.9));
        float dashs[] = {4.0, 2.0};
        g.drawDashedLine(Line<float>(0, 0, getWidth(), 0), dashs, 2);
        g.drawDashedLine(Line<float>(0, getHeight(), getWidth(), getHeight()), dashs, 2);
    }

    lf.drawButtonText(g, *this, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void PluginButton::clicked(const ModifierKeys& modifiers) {
    if (m_listener != nullptr) {
        m_listener->buttonClicked(this, modifiers);
    }
}
