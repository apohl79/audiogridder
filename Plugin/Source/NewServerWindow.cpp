/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "NewServerWindow.hpp"
#include "NumberConversion.hpp"

#include <JuceHeader.h>

namespace e47 {

NewServerWindow::NewServerWindow(float x, float y) : TopLevelWindow("New Server", true) {
    setBounds(e47::as<int>(lroundf(x)), e47::as<int>(lroundf(y)), 196, 70);

    addChildAndSetID(&m_server, "server");
    m_server.setBounds(5, 5, 188, 25);

    addChildAndSetID(&m_cancel, "cancel");
    m_cancel.setBounds(5, 35, 90, 25);
    m_cancel.setButtonText("Cancel");
    m_cancel.addListener(this);

    addChildAndSetID(&m_ok, "ok");
    m_ok.setBounds(100, 35, 90, 25);
    m_ok.setButtonText("Add");
    m_ok.addListener(this);

    setVisible(true);
}

NewServerWindow::~NewServerWindow() {}

void NewServerWindow::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));  // clear the background
}

void NewServerWindow::buttonClicked(Button* button) {
    if (button->getButtonText() == "Add" && m_onOk != nullptr) {
        m_onOk(m_server.getTextValue().toString());
    }
    delete this;
}

void NewServerWindow::activeWindowStatusChanged() {
    TopLevelWindow::activeWindowStatusChanged();
    if (!isActiveWindow()) {
        delete this;
    }
}

}  // namespace e47
