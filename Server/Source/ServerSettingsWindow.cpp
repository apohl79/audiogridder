/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ServerSettingsWindow.hpp"
#include "App.hpp"
#include "WindowPositions.hpp"

namespace e47 {

ServerSettingsWindow::ServerSettingsWindow(App* app)
    : DocumentWindow("Server Settings",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      LogTag("settings"),
      m_app(app) {
    traceScope();
    setUsingNativeTitleBar(true);

    int totalWidth = 500;
    int totalHeight = 80;
    int borderLR = 15;  // left/right border
    int borderTB = 15;  // top/bottom border
    int rowHeight = 30;

    int fieldWidth = 50;
    int wideFieldWidth = 250;
    int fieldHeight = 25;
    int labelWidth = 250;
    int labelHeight = 35;
    int headerHeight = 18;
    int checkBoxWidth = 25;
    int checkBoxHeight = 25;
    int largeFieldRows = 2;
    int largeFieldWidth = 250;
    int largeFieldHeight = largeFieldRows * rowHeight - 10;

    int saveButtonWidth = 125;
    int saveButtonHeight = 30;

    auto getLabelBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR, borderTB + r * rowHeight, labelWidth, labelHeight);
    };
    auto getFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - fieldWidth - borderLR, borderTB + r * rowHeight + 3, fieldWidth,
                                    fieldHeight);
    };
    auto getWideFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - wideFieldWidth - borderLR, borderTB + r * rowHeight + 3,
                                    wideFieldWidth, fieldHeight);
    };
    auto getCheckBoxBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - checkBoxWidth - borderLR, borderTB + r * rowHeight + 3, checkBoxWidth,
                                    checkBoxHeight);
    };
    auto getLargeFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - largeFieldWidth - borderLR, borderTB + r * rowHeight + 3,
                                    largeFieldWidth, largeFieldHeight);
    };
    auto getHeaderBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR, borderTB + r * rowHeight + 7, totalWidth - borderLR * 2, headerHeight);
    };

    int row = 0;
    String tmpStr;

    auto label = std::make_unique<Label>();
    label->setText("Server Name:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_nameText.setText(m_app->getServer().getName());
    m_nameText.setBounds(getWideFieldBounds(row));
    addChildAndSetID(&m_nameText, "name");

    row++;

    label = std::make_unique<Label>();
    label->setText("Server ID:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    int idConfig = m_app->getServer().getId(true);
    String id;
    id << idConfig;
    m_idText.setText(id);
    m_idText.setBounds(getFieldBounds(row));
    addChildAndSetID(&m_idText, "id");

    row++;

    int idReal = m_app->getServer().getId();
    if (idConfig != idReal) {
        // server started with -id where the passed id is different from the config
        label = std::make_unique<Label>();
        label->setText("Server ID (commandline override):", NotificationType::dontSendNotification);
        label->setBounds(getLabelBounds(row));
        label->setAlpha(0.5f);
        addChildAndSetID(label.get(), "lbl");
        m_components.push_back(std::move(label));

        String idOverride;
        idOverride << idReal;

        label = std::make_unique<Label>();
        label->setText(idOverride, NotificationType::dontSendNotification);
        label->setBounds(getFieldBounds(row));
        label->setAlpha(0.5f);
        addChildAndSetID(label.get(), "lbl");
        m_components.push_back(std::move(label));

        row++;
    }

    label = std::make_unique<Label>();
    label->setText("Plugin Formats", NotificationType::dontSendNotification);
    label->setJustificationType(Justification::centredTop);
    label->setBounds(getHeaderBounds(row));
    label->setColour(Label::backgroundColourId, Colours::white.withAlpha(0.08f));
    label->setColour(Label::outlineColourId, Colours::white.withAlpha(0.03f));
    label->setAlpha(0.6f);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    row++;

#ifdef JUCE_MAC
    label = std::make_unique<Label>();
    label->setText("AudioUnit Support:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_auSupport.setBounds(getCheckBoxBounds(row));
    m_auSupport.setToggleState(m_app->getServer().getEnableAU(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_auSupport, "au");

    row++;
#endif

    label = std::make_unique<Label>();
    label->setText("VST3 Support:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vst3Support.setBounds(getCheckBoxBounds(row));
    m_vst3Support.setToggleState(m_app->getServer().getEnableVST3(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_vst3Support, "vst");

    row++;

    label = std::make_unique<Label>();
    tmpStr = "VST3 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    label->setText(tmpStr, NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vst3Folders.setBounds(getLargeFieldBounds(row));
    m_vst3Folders.setMultiLine(true, false);
    m_vst3Folders.setReturnKeyStartsNewLine(true);
    addChildAndSetID(&m_vst3Folders, "vst3fold");

    tmpStr = "";
    for (auto& folder : m_app->getServer().getVST3Folders()) {
        tmpStr << folder << newLine;
    }
    m_vst3Folders.setText(tmpStr);

    row += largeFieldRows;

    label = std::make_unique<Label>();
    label->setText("VST2 Support:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vst2Support.setBounds(getCheckBoxBounds(row));
    m_vst2Support.setToggleState(m_app->getServer().getEnableVST2(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_vst2Support, "vst2");

    row++;

    label = std::make_unique<Label>();
    tmpStr = "VST2 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    label->setText(tmpStr, NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vst2Folders.setBounds(getLargeFieldBounds(row));
    m_vst2Folders.setMultiLine(true, false);
    m_vst2Folders.setReturnKeyStartsNewLine(true);
    addChildAndSetID(&m_vst2Folders, "vst2fold");

    tmpStr = "";
    for (auto& folder : m_app->getServer().getVST2Folders()) {
        tmpStr << folder << newLine;
    }
    m_vst2Folders.setText(tmpStr);

    row += largeFieldRows;

    label = std::make_unique<Label>();
    label->setText("Do not include VST standard folders:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vstNoStandardFolders.setBounds(getCheckBoxBounds(row));
    m_vstNoStandardFolders.setToggleState(m_app->getServer().getVSTNoStandardFolders(),
                                          NotificationType::dontSendNotification);
    addChildAndSetID(&m_vstNoStandardFolders, "vstnostandarddirs");

    row++;

    label = std::make_unique<Label>();
    label->setText("Screen Capturing", NotificationType::dontSendNotification);
    label->setJustificationType(Justification::centredTop);
    label->setBounds(getHeaderBounds(row));
    label->setColour(Label::backgroundColourId, Colours::white.withAlpha(0.08f));
    label->setColour(Label::outlineColourId, Colours::white.withAlpha(0.03f));
    label->setAlpha(0.6f);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    row++;

    label = std::make_unique<Label>();
    label->setText("Screen Capturing Mode:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_screenCapturingMode.setBounds(getWideFieldBounds(row));
    m_screenCapturingMode.addItem("FFmpeg (webp)", 1);
    m_screenCapturingMode.addItem("FFmpeg (mjpeg)", 2);
    m_screenCapturingMode.addItem("Legacy", 3);
    m_screenCapturingMode.addItem("Disabled", 4);
    int mode = 1;
    if (m_app->getServer().getScreenCapturingOff()) {
        mode = 4;
    } else if (!m_app->getServer().getScreenCapturingFFmpeg()) {
        mode = 3;
    } else {
        switch (m_app->getServer().getScreenCapturingFFmpegEncoder()) {
            case ScreenRecorder::WEBP:
                mode = 1;
                break;
            case ScreenRecorder::MJPEG:
                mode = 2;
                break;
        }
    }
    m_screenCapturingMode.setSelectedId(mode, NotificationType::dontSendNotification);
    m_screenCapturingMode.onChange = [this] {
        if (m_screenCapturingMode.getSelectedId() != 3) {
            m_screenDiffDetectionLbl.setAlpha(0.5);
            m_screenDiffDetection.setEnabled(false);
            m_screenDiffDetection.setAlpha(0.5);
            m_screenJpgQualityLbl.setAlpha(0.5);
            m_screenJpgQuality.setEnabled(false);
            m_screenJpgQuality.setAlpha(0.5);

            m_screenCapturingQualityLbl.setAlpha(1);
            m_screenCapturingQuality.setAlpha(1);
            m_screenCapturingQuality.setEnabled(true);
        } else {
            m_screenDiffDetectionLbl.setAlpha(1);
            m_screenDiffDetection.setEnabled(true);
            m_screenDiffDetection.setAlpha(1);
            m_screenJpgQualityLbl.setAlpha(1);
            m_screenJpgQuality.setEnabled(true);
            m_screenJpgQuality.setAlpha(1);

            m_screenCapturingQualityLbl.setAlpha(0.5);
            m_screenCapturingQuality.setAlpha(0.5);
            m_screenCapturingQuality.setEnabled(false);

            if (nullptr != m_screenDiffDetection.onClick) {
                m_screenDiffDetection.onClick();
            }
        }
    };
    m_screenCapturingMode.onChange();
    addChildAndSetID(&m_screenCapturingMode, "captmode");

    row++;

    label = std::make_unique<Label>();
    m_screenCapturingQualityLbl.setText("Screen Capturing Quality:", NotificationType::dontSendNotification);
    m_screenCapturingQualityLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_screenCapturingQualityLbl, "lbl");

    m_screenCapturingQuality.setBounds(getWideFieldBounds(row));
    m_screenCapturingQuality.addItem("High", ScreenRecorder::ENC_QUALITY_HIGH + 1);
    m_screenCapturingQuality.addItem("Medium", ScreenRecorder::ENC_QUALITY_MEDIUM + 1);
    m_screenCapturingQuality.addItem("Low", ScreenRecorder::ENC_QUALITY_LOW + 1);
    m_screenCapturingQuality.setSelectedId(m_app->getServer().getScreenCapturingFFmpegQuality() + 1);

    addChildAndSetID(&m_screenCapturingQuality, "captqual");

    row++;

    m_screenDiffDetectionLbl.setText("Legacy Diff Detection:", NotificationType::dontSendNotification);
    m_screenDiffDetectionLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_screenDiffDetectionLbl, "lbl");

    m_screenDiffDetection.setBounds(getCheckBoxBounds(row));
    m_screenDiffDetection.setToggleState(m_app->getServer().getScreenDiffDetection(),
                                         NotificationType::dontSendNotification);
    m_screenDiffDetection.onClick = [this] {
        if (m_screenCapturingMode.getSelectedId() == 2) {
            if (m_screenDiffDetection.getToggleState()) {
                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
            } else {
                m_screenJpgQualityLbl.setAlpha(1);
                m_screenJpgQuality.setEnabled(true);
                m_screenJpgQuality.setAlpha(1);
            }
        }
    };
    m_screenDiffDetection.onClick();
    addChildAndSetID(&m_screenDiffDetection, "diff");

    row++;

    m_screenJpgQualityLbl.setText("Legacy Quality (0.1-1.0):", NotificationType::dontSendNotification);
    m_screenJpgQualityLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_screenJpgQualityLbl, "lbl");

    String q;
    q << m_app->getServer().getScreenQuality();
    m_screenJpgQuality.setText(q);
    m_screenJpgQuality.setBounds(getFieldBounds(row));
    addChildAndSetID(&m_screenJpgQuality, "qual");

    row++;

    label = std::make_unique<Label>();
    label->setText("Startup", NotificationType::dontSendNotification);
    label->setJustificationType(Justification::centredTop);
    label->setBounds(getHeaderBounds(row));
    label->setColour(Label::backgroundColourId, Colours::white.withAlpha(0.08f));
    label->setColour(Label::outlineColourId, Colours::white.withAlpha(0.03f));
    label->setAlpha(0.6f);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    row++;

    label = std::make_unique<Label>();
    label->setText("Scan for Plugins at Startup:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_scanForPlugins.setBounds(getCheckBoxBounds(row));
    m_scanForPlugins.setToggleState(m_app->getServer().getScanForPlugins(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_scanForPlugins, "scan");

    row++;

    label = std::make_unique<Label>();
    label->setText("Allow Plugins to be loaded in parallel:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_parallelPluginLoad.setBounds(getCheckBoxBounds(row));
    m_parallelPluginLoad.setToggleState(m_app->getServer().getParallelPluginLoad(),
                                        NotificationType::dontSendNotification);
    addChildAndSetID(&m_parallelPluginLoad, "pload");

    row++;

    label = std::make_unique<Label>();
    label->setText("Diagnostics", NotificationType::dontSendNotification);
    label->setJustificationType(Justification::centredTop);
    label->setBounds(getHeaderBounds(row));
    label->setColour(Label::backgroundColourId, Colours::white.withAlpha(0.08f));
    label->setColour(Label::outlineColourId, Colours::white.withAlpha(0.03f));
    label->setAlpha(0.6f);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    row++;

    label = std::make_unique<Label>();
    label->setText("Logging:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_logger.setBounds(getCheckBoxBounds(row));
    m_logger.setToggleState(AGLogger::isEnabled(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_logger, "logger");

    row++;

    label = std::make_unique<Label>();
    label->setText("Tracing (please enable to report issues):", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_tracer.setBounds(getCheckBoxBounds(row));
    m_tracer.setToggleState(Tracer::isEnabled(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_tracer, "tracer");

    row++;

    totalHeight += row * rowHeight;

    m_saveButton.setButtonText("Save");
    m_saveButton.setBounds(totalWidth / 2 - saveButtonWidth / 2, totalHeight - borderTB - saveButtonHeight,
                           saveButtonWidth, saveButtonHeight);
    m_saveButton.onClick = [this, app] {
        traceScope();
        Tracer::setEnabled(m_tracer.getToggleState());
        AGLogger::setEnabled(m_logger.getToggleState());
        auto appCpy = app;
        appCpy->getServer().setId(m_idText.getText().getIntValue());
        appCpy->getServer().setName(m_nameText.getText());
        appCpy->getServer().setEnableAU(m_auSupport.getToggleState());
        appCpy->getServer().setEnableVST3(m_vst3Support.getToggleState());
        appCpy->getServer().setEnableVST2(m_vst2Support.getToggleState());
        appCpy->getServer().setScanForPlugins(m_scanForPlugins.getToggleState());
        appCpy->getServer().setParallelPluginLoad(m_parallelPluginLoad.getToggleState());
        switch (m_screenCapturingMode.getSelectedId()) {
            case 1:
                appCpy->getServer().setScreenCapturingFFmpeg(true);
                appCpy->getServer().setScreenCapturingFFmpegEncoder(ScreenRecorder::WEBP);
                appCpy->getServer().setScreenCapturingOff(false);
                break;
            case 2:
                appCpy->getServer().setScreenCapturingFFmpeg(true);
                appCpy->getServer().setScreenCapturingFFmpegEncoder(ScreenRecorder::MJPEG);
                appCpy->getServer().setScreenCapturingOff(false);
                break;
            case 3:
                appCpy->getServer().setScreenCapturingFFmpeg(false);
                appCpy->getServer().setScreenCapturingOff(false);
                break;
            case 4:
                appCpy->getServer().setScreenCapturingFFmpeg(false);
                appCpy->getServer().setScreenCapturingOff(true);
                break;
        }
        appCpy->getServer().setScreenCapturingFFmpegQuality(
            (ScreenRecorder::EncoderQuality)(m_screenCapturingQuality.getSelectedId() - 1));
        appCpy->getServer().setScreenDiffDetection(m_screenDiffDetection.getToggleState());
        float qual = m_screenJpgQuality.getText().getFloatValue();
        if (qual < 0.1) {
            qual = 0.1f;
        } else if (qual > 1) {
            qual = 1.0f;
        }
        appCpy->getServer().setScreenQuality(qual);
        if (m_vst3Folders.getText().length() > 0) {
            appCpy->getServer().setVST3Folders(StringArray::fromLines(m_vst3Folders.getText()));
        }
        if (m_vst2Folders.getText().length() > 0) {
            appCpy->getServer().setVST2Folders(StringArray::fromLines(m_vst2Folders.getText()));
        }
        appCpy->getServer().setVSTNoStandardFolders(m_vstNoStandardFolders.getToggleState());
        appCpy->getServer().saveConfig();
        appCpy->hideServerSettings();
        appCpy->restartServer();
    };
    addChildAndSetID(&m_saveButton, "save");

    setResizable(false, false);
    centreWithSize(totalWidth, totalHeight);
    setBounds(WindowPositions::get(WindowPositions::ServerSettings, getBounds()));
    setVisible(true);
    windowToFront(this);
}

ServerSettingsWindow::~ServerSettingsWindow() {
    WindowPositions::set(WindowPositions::ServerSettings, getBounds());
    clearContentComponent();
}

void ServerSettingsWindow::closeButtonPressed() {
    traceScope();
    m_app->hideServerSettings();
}

}  // namespace e47
