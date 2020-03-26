/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ServerSettingsWindow.hpp"
#include "App.hpp"

namespace e47 {

ServerSettingsWindow::ServerSettingsWindow(App* app)
    : DocumentWindow("Server Settings",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      m_app(app) {
    setUsingNativeTitleBar(true);

    auto label = std::make_unique<Label>();
    label->setText("Server ID:", NotificationType::dontSendNotification);
    label->setBounds(15, 40, 150, 30);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    String id;
    id << m_app->getServer().getId();
    m_idText.setText(id);
    m_idText.setBounds(180, 43, 50, 25);
    addChildAndSetID(&m_idText, "id");

    label = std::make_unique<Label>();
    label->setText("AudioUnit Support:", NotificationType::dontSendNotification);
    label->setBounds(15, 80, 150, 30);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_auSupport.setBounds(207, 83, 25, 25);
    m_auSupport.setToggleState(m_app->getServer().getEnableAU(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_auSupport, "au");

    label = std::make_unique<Label>();
    label->setText("VST3 Support:", NotificationType::dontSendNotification);
    label->setBounds(15, 120, 150, 30);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vstSupport.setBounds(207, 123, 25, 25);
    m_vstSupport.setToggleState(m_app->getServer().getEnableVST(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_vstSupport, "vst");

    m_saveButton.setButtonText("Save");
    m_saveButton.setBounds(63, 170, 125, 30);
    m_saveButton.onClick = [this] {
        m_app->getServer().setId(m_idText.getTextValue().toString().getIntValue());
        m_app->getServer().setEnableAU(m_auSupport.getToggleState());
        m_app->getServer().setEnableVST(m_vstSupport.getToggleState());
        m_app->getServer().saveConfig();
        m_app->hideServerSettings();
        m_app->restartServer();
    };
    addChildAndSetID(&m_saveButton, "save");

    setResizable(false, false);
    centreWithSize(250, 220);
    setVisible(true);
}

void ServerSettingsWindow::closeButtonPressed() { m_app->hideServerSettings(); }

}  // namespace e47
