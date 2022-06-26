/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "PluginButton.hpp"

using namespace e47;

PluginButton::PluginButton(const String& id, const String& name, bool extraButtons)
    : TextButton(name), m_id(id), m_withExtraButtons(extraButtons) {}

void PluginButton::paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    auto bgcol = findColour(getToggleState() ? buttonOnColourId : buttonColourId);
    auto baseColour = bgcol.withMultipliedSaturation(hasKeyboardFocus(true) ? 1.3f : 0.9f)
                          .withMultipliedAlpha(isEnabled() ? 0.5f : 0.7f);
    if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
    }
    auto fgColor = findColour(getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                       .withMultipliedAlpha(isEnabled() ? 0.7f : 0.4f);

    Colour txtColor, bgColor;
    float symLineThikness = 0.7f;

    if (!m_active || shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted) {
        g.setColour(baseColour);
        g.fillRect(getLocalBounds());
        txtColor = fgColor;
        bgColor = baseColour;
    }

    if (m_active) {
        txtColor = Colours::black;
        bgColor = fgColor;
        symLineThikness = 1.3f;
        g.setColour(bgColor);
        g.fillRect(getLocalBounds());
        g.setColour(baseColour);
        float dashs[] = {4.0, 2.0};
        g.drawDashedLine(Line<float>(0.0f, 0.0f, (float)getWidth(), 0.0f), dashs, 2);
        g.drawDashedLine(Line<float>(0.0f, (float)getHeight(), (float)getWidth(), (float)getHeight()), dashs, 2);
        g.drawDashedLine(Line<float>(0.0f, 0.0f, 0.0f, (float)getHeight()), dashs, 2);
        g.drawDashedLine(Line<float>((float)getWidth(), 0.0f, (float)getWidth(), (float)getHeight()), dashs, 2);
    }

    int textIndentLeft = 0;
    int textIndentRight = 0;

    if (m_withExtraButtons) {
        // bypass button
        int indent = 5;
        int width = getHeight() - indent * 2;
        textIndentLeft = indent * 2 + width;

        m_bypassArea = Rectangle<int>(indent, indent, width, width);

        int space = 4;
        int indent_right = 6;
        width = getHeight() - indent_right * 2;
        int rightButtonCount = 3;
#if JucePlugin_IsSynth
        rightButtonCount = 1;
#endif
        textIndentRight = indent + (space + width) * rightButtonCount;
        m_moveDownArea = Rectangle<int>(getWidth() - (width + space) * 3, indent_right, width, width);
        m_moveUpArea = Rectangle<int>(getWidth() - (width + space) * 2, indent_right, width, width);
        m_deleteArea = Rectangle<int>(getWidth() - width - space, indent_right, width, width);

        // bypass
        g.setColour(txtColor);
        g.drawEllipse(m_bypassArea.toFloat(), symLineThikness);
        g.drawLine((float)m_bypassArea.getCentreX(), (float)m_bypassArea.getY() - 1, (float)m_bypassArea.getCentreX(),
                   (float)m_bypassArea.getY() + 5, symLineThikness);

        Rectangle<float> rect;
#if !JucePlugin_IsSynth
        // down
        Path down;
        PathStrokeType stroke(symLineThikness);
        rect = m_moveDownArea.toFloat();
        down.addTriangle(rect.getX(), rect.getY(), rect.getRight(), rect.getY(), rect.getCentreX(), rect.getBottom());
        g.strokePath(down, stroke);

        // up
        Path up;
        rect = m_moveUpArea.toFloat();
        up.addTriangle(rect.getCentreX(), rect.getY(), rect.getX(), rect.getBottom(), rect.getRight(),
                       rect.getBottom());
        g.strokePath(up, stroke);
#endif

        // delete
        rect = m_deleteArea.toFloat();
        g.drawLine(rect.getX(), rect.getY(), rect.getRight(), rect.getBottom(), symLineThikness);
        g.drawLine(rect.getX(), rect.getBottom(), rect.getRight(), rect.getY(), symLineThikness);
    }

    drawText(g, textIndentLeft, textIndentRight);
}

void PluginButton::clicked(const ModifierKeys& modifiers) {
    auto area = getAreaType();
    if (m_listener != nullptr && (m_enabled || area == PluginButton::DELETE)) {
        m_listener->buttonClicked(this, modifiers, area);
    }
}

void PluginButton::drawText(Graphics& g, int left, int right) {
    auto& lf = getLookAndFeel();
    Font font(lf.getTextButtonFont(*this, getHeight()));
    Colour col = findColour(TextButton::textColourOffId);
    if (m_active) {
        col = Colours::black;
        font.setBold(true);
    }
    if (!m_enabled) {
        col = col.withAlpha(0.5f);
    }
    g.setFont(font);
    g.setColour(col);

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

void PluginButton::mouseMove(const MouseEvent& event) {
    m_lastMousePosition = event.getPosition();
    Button::mouseMove(event);
}

PluginButton::AreaType PluginButton::getAreaType() const {
    if (!m_withExtraButtons) {
        return PluginButton::MAIN;
    }
    if (isWithinArea(m_bypassArea, m_lastMousePosition)) {
        return PluginButton::BYPASS;
#if !JucePlugin_IsSynth
    } else if (isWithinArea(m_moveUpArea, m_lastMousePosition)) {
        return PluginButton::MOVE_UP;
    } else if (isWithinArea(m_moveDownArea, m_lastMousePosition)) {
        return PluginButton::MOVE_DOWN;
#endif
    } else if (isWithinArea(m_deleteArea, m_lastMousePosition)) {
        return PluginButton::DELETE;
    }
    return PluginButton::MAIN;
}
