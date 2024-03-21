/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "ServerSettingsWindow.hpp"
#include "App.hpp"
#include "Server.hpp"
#include "WindowPositions.hpp"

#include "Images.hpp"

namespace e47 {

ServerSettingsWindow::ServerSettingsWindow(App* app)
    : DocumentWindow("Server Settings",
                     LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton),
      LogTag("settings"),
      m_app(app)
#if OLD_LAYOUT
#else
      ,
      m_tabbedComponent(TabbedButtonBar::TabsAtTop),
      m_mainTab(app->getServer()->getMainSettings()),
      m_pluginFormatsTab(app->getServer()->getFormatSettings()),
      m_screenCapturingTab(app->getServer()->getCaptureSettings()),
      m_startupTab(app->getServer()->getScanForPlugins()),
      m_diagnosticsTab(app->getServer()->getCrashReporting())
#endif
{
    traceScope();
    setUsingNativeTitleBar(true);

    auto srv = m_app->getServer();
    if (nullptr == srv) {
        logln("error: no server object");
        return;
    }

    int totalWidth = 600;
    int saveButtonWidth = 125;
    int saveButtonHeight = 30;

#if OLD_LAYOUT
    int totalHeight = 80;
    int borderLR = 15;  // left/right border
    int borderTB = 15;  // top/bottom border
    int rowHeight = 30;
    int extraBorderTB = 0;

    int fieldWidth = 50;
    int wideFieldWidth = 250;
    int fieldHeight = 25;
    int labelWidth = 350;
    int labelHeight = 35;
    int headerHeight = 18;
    int checkBoxWidth = 25;
    int checkBoxHeight = 25;
    int largeFieldRows = 2;
    int largeFieldWidth = 250;
    int largeFieldHeight = largeFieldRows * rowHeight - 10;

#ifdef JUCE_LINUX
    setMenuBar(app);
    extraBorderTB = 20;
    totalHeight += extraBorderTB;
#endif

    auto getLabelBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR, extraBorderTB + borderTB + r * rowHeight, labelWidth, labelHeight);
    };
    auto getFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - fieldWidth - borderLR, extraBorderTB + borderTB + r * rowHeight + 3,
                                    fieldWidth, fieldHeight);
    };
    auto getWideFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - wideFieldWidth - borderLR,
                                    extraBorderTB + borderTB + r * rowHeight + 3, wideFieldWidth, fieldHeight);
    };
    auto getCheckBoxBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - checkBoxWidth - borderLR, extraBorderTB + borderTB + r * rowHeight + 3,
                                    checkBoxWidth, checkBoxHeight);
    };
    auto getLargeFieldBounds = [&](int r) {
        return juce::Rectangle<int>(totalWidth - largeFieldWidth - borderLR,
                                    extraBorderTB + borderTB + r * rowHeight + 3, largeFieldWidth, largeFieldHeight);
    };
    auto getHeaderBounds = [&](int r) {
        return juce::Rectangle<int>(borderLR, extraBorderTB + borderTB + r * rowHeight + 7, totalWidth - borderLR * 2,
                                    headerHeight);
    };

    int row = 0;
    String tmpStr;
    String tooltip;

    auto label = std::make_unique<Label>();
    label->setText("Server Name:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_nameText.setText(srv->getName());
    m_nameText.setBounds(getWideFieldBounds(row));
    addChildAndSetID(&m_nameText, "name");

    row++;

    label = std::make_unique<Label>();
    label->setText("Server ID:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    String id(srv->getId());
    label = std::make_unique<Label>();
    label->setText(id, NotificationType::dontSendNotification);
    label->setBounds(getFieldBounds(row));
    label->setJustificationType(Justification::right);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    row++;

    tooltip << "Chain Isolation: Each AG plugin chain created by an AG plugin will run in a dedicated process."
            << newLine << newLine
            << "Plugin Isolation: Each plugin loaded into an AG plugin chain will run in a dedicated process.";
    label = std::make_unique<Label>();
    label->setText("Sandbox Mode:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    label->setTooltip(tooltip);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_sandboxMode.setBounds(getWideFieldBounds(row));
    m_sandboxMode.addItem("Disabled", 1);
    m_sandboxMode.addItem("Chain Isolation", 2);
    m_sandboxMode.addItem("Plugin Isolation", 3);
    m_sandboxMode.setSelectedItemIndex(srv->getSandboxMode());
    m_sandboxMode.setTooltip(tooltip);
    addChildAndSetID(&m_sandboxMode, "sandbox");

    tooltip.clear();
    row++;

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
    m_auSupport.setToggleState(srv->getEnableAU(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_auSupport, "au");

    row++;
#endif

    label = std::make_unique<Label>();
    label->setText("VST3 Support:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vst3Support.setBounds(getCheckBoxBounds(row));
    m_vst3Support.setToggleState(srv->getEnableVST3(), NotificationType::dontSendNotification);
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
    for (auto& folder : srv->getVST3Folders()) {
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
    m_vst2Support.setToggleState(srv->getEnableVST2(), NotificationType::dontSendNotification);
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
    for (auto& folder : srv->getVST2Folders()) {
        tmpStr << folder << newLine;
    }
    m_vst2Folders.setText(tmpStr);

    row += largeFieldRows;

    tooltip = "If you select this, only custom folders will be scanned.";
    label = std::make_unique<Label>();
    label->setText("Do not include VST standard folders:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    label->setTooltip(tooltip);

    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_vstNoStandardFolders.setBounds(getCheckBoxBounds(row));
    m_vstNoStandardFolders.setToggleState(srv->getVSTNoStandardFolders(), NotificationType::dontSendNotification);
    m_vstNoStandardFolders.setTooltip(tooltip);
    addChildAndSetID(&m_vstNoStandardFolders, "vstnostandarddirs");

    tooltip.clear();
    row++;

    label = std::make_unique<Label>();
    label->setText("LV2 Support:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_lv2Support.setBounds(getCheckBoxBounds(row));
    m_lv2Support.setToggleState(srv->getEnableLV2(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_lv2Support, "lv2");

    row++;

    label = std::make_unique<Label>();
    tmpStr = "LV2 Custom Folders";
    tmpStr << newLine << "(one folder per line):";
    label->setText(tmpStr, NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_lv2Folders.setBounds(getLargeFieldBounds(row));
    m_lv2Folders.setMultiLine(true, false);
    m_lv2Folders.setReturnKeyStartsNewLine(true);
    addChildAndSetID(&m_lv2Folders, "lv2fold");

    tmpStr = "";
    for (auto& folder : srv->getLV2Folders()) {
        tmpStr << folder << newLine;
    }
    m_lv2Folders.setText(tmpStr);

    row += largeFieldRows;

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

    tooltip << "FFmpeg: Use FFmpeg for screen capturing. This is recommended as it gives best quality at lowest "
               "bandwidth costs."
            << newLine << newLine;
    tooltip << "Legacy: This mode takes screenshots every 50ms. Use this only if FFmpeg does not work for you."
            << newLine << newLine;
    tooltip << "Disabled (Local Mode): If you run AG server and your DAW on the same computer you should enable this "
               "mode. It positions the plugin windows next to the AG plugin window and allows you to open multiple "
               "plugin windows at the same time."
            << newLine << newLine;
    tooltip << "Disabled: No screen capturing.";
    label = std::make_unique<Label>();
    label->setText("Screen Capturing Mode:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    label->setTooltip(tooltip);
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_screenCapturingMode.setBounds(getWideFieldBounds(row));
    m_screenCapturingMode.setTooltip(tooltip);
    m_screenCapturingMode.addItem("FFmpeg", 1);
    // m_screenCapturingMode.addItem("FFmpeg (mjpeg)", 2);
    m_screenCapturingMode.addItem("Legacy", 3);
    m_screenCapturingMode.addItem("Disabled (Local Mode)", 4);
    m_screenCapturingMode.addItem("Disabled", 5);
    int mode = 1;
    if (srv->getScreenCapturingOff()) {
        if (srv->getScreenLocalMode()) {
            mode = 4;
        } else {
            mode = 5;
        }
    } else if (!srv->getScreenCapturingFFmpeg()) {
        mode = 3;
    }
    // else {
    //    switch (srv->getScreenCapturingFFmpegEncoder()) {
    //        case ScreenRecorder::WEBP:
    //            mode = 1;
    //            break;
    //        case ScreenRecorder::MJPEG:
    //            mode = 2;
    //            break;
    //    }
    //}
    m_screenCapturingMode.setSelectedId(mode, NotificationType::dontSendNotification);
    m_screenCapturingMode.onChange = [this] {
        switch (m_screenCapturingMode.getSelectedId()) {
            case 1:
            case 2:
                m_screenCapturingQualityLbl.setAlpha(1);
                m_screenCapturingQuality.setAlpha(1);
                m_screenCapturingQuality.setEnabled(true);

                m_pluginWindowsOnTopLbl.setAlpha(0.5);
                m_pluginWindowsOnTop.setEnabled(false);
                m_pluginWindowsOnTop.setAlpha(0.5);

                m_screenDiffDetectionLbl.setAlpha(0.5);
                m_screenDiffDetection.setEnabled(false);
                m_screenDiffDetection.setAlpha(0.5);

                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
                break;
            case 3:
                m_screenDiffDetectionLbl.setAlpha(1);
                m_screenDiffDetection.setEnabled(true);
                m_screenDiffDetection.setAlpha(1);

                m_screenJpgQualityLbl.setAlpha(1);
                m_screenJpgQuality.setEnabled(true);
                m_screenJpgQuality.setAlpha(1);

                m_screenCapturingQualityLbl.setAlpha(0.5);
                m_screenCapturingQuality.setAlpha(0.5);
                m_screenCapturingQuality.setEnabled(false);

                m_pluginWindowsOnTopLbl.setAlpha(0.5);
                m_pluginWindowsOnTop.setEnabled(false);
                m_pluginWindowsOnTop.setAlpha(0.5);

                if (nullptr != m_screenDiffDetection.onClick) {
                    m_screenDiffDetection.onClick();
                }
                break;
            case 4:
            case 5:
                m_screenCapturingQualityLbl.setAlpha(0.5);
                m_screenCapturingQuality.setAlpha(0.5);
                m_screenCapturingQuality.setEnabled(false);

                m_pluginWindowsOnTopLbl.setAlpha(1);
                m_pluginWindowsOnTop.setEnabled(true);
                m_pluginWindowsOnTop.setAlpha(1);

                m_screenDiffDetectionLbl.setAlpha(0.5);
                m_screenDiffDetection.setEnabled(false);
                m_screenDiffDetection.setAlpha(0.5);

                m_screenJpgQualityLbl.setAlpha(0.5);
                m_screenJpgQuality.setEnabled(false);
                m_screenJpgQuality.setAlpha(0.5);
                break;
        }
    };
    m_screenCapturingMode.onChange();
    addChildAndSetID(&m_screenCapturingMode, "captmode");

    tooltip.clear();
    row++;

    label = std::make_unique<Label>();
    m_screenCapturingQualityLbl.setText("Screen Capturing Quality:", NotificationType::dontSendNotification);
    m_screenCapturingQualityLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_screenCapturingQualityLbl, "lbl");

    m_screenCapturingQuality.setBounds(getWideFieldBounds(row));
    m_screenCapturingQuality.addItem("High", ScreenRecorder::ENC_QUALITY_HIGH + 1);
    m_screenCapturingQuality.addItem("Medium", ScreenRecorder::ENC_QUALITY_MEDIUM + 1);
    m_screenCapturingQuality.addItem("Low", ScreenRecorder::ENC_QUALITY_LOW + 1);
    m_screenCapturingQuality.setSelectedId(srv->getScreenCapturingFFmpegQuality() + 1);

    addChildAndSetID(&m_screenCapturingQuality, "captqual");

    row++;

    m_screenDiffDetectionLbl.setText("Legacy Diff Detection:", NotificationType::dontSendNotification);
    m_screenDiffDetectionLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_screenDiffDetectionLbl, "lbl");

    m_screenDiffDetection.setBounds(getCheckBoxBounds(row));
    m_screenDiffDetection.setToggleState(srv->getScreenDiffDetection(), NotificationType::dontSendNotification);
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
    q << srv->getScreenQuality();
    m_screenJpgQuality.setText(q);
    m_screenJpgQuality.setBounds(getFieldBounds(row));
    addChildAndSetID(&m_screenJpgQuality, "qual");

    row++;

    m_pluginWindowsOnTopLbl.setText("Keep Plugin Windows on Top:", NotificationType::dontSendNotification);
    m_pluginWindowsOnTopLbl.setBounds(getLabelBounds(row));
    addChildAndSetID(&m_pluginWindowsOnTopLbl, "lbl");

    m_pluginWindowsOnTop.setToggleState(srv->getPluginWindowsOnTop(), NotificationType::dontSendNotification);
    m_pluginWindowsOnTop.setBounds(getCheckBoxBounds(row));
    addChildAndSetID(&m_pluginWindowsOnTop, "ontop");

    row++;

    label = std::make_unique<Label>();
    label->setText("Mouse Offset Correction:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_screenMouseOffsetXY.setBounds(getWideFieldBounds(row));
    m_screenMouseOffsetXY.setText(String(srv->getScreenMouseOffsetX()) + "x" + String(srv->getScreenMouseOffsetY()));
    m_screenMouseOffsetXY.setInputFilter(new TextEditor::LengthAndCharacterRestriction(11, "0123456789x-,"), true);
    addChildAndSetID(&m_screenMouseOffsetXY, "offsetXY");
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

    tooltip << "Enter the IDs of servers that you want to start automatically. An ID must be a number in the range of "
               "0-31. Example: 0,1,4-8"
            << newLine << newLine << "Note: You have to restart manually for taking changes into effect.";

    label = std::make_unique<Label>();
    label->setText("Autostart servers with IDs:", NotificationType::dontSendNotification);
    label->setTooltip(tooltip);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigServerStartup));
    if (jsonHasValue(cfg, "IDs")) {
        m_idText.setText(jsonGetValue(cfg, "IDs", String()));
    }
    m_idText.setInputFilter(new TextEditor::LengthAndCharacterRestriction(103, "0123456789-,"), true);
    m_idText.setBounds(getWideFieldBounds(row));
    m_idText.setTooltip(tooltip);
    addChildAndSetID(&m_idText, "id");
    tooltip.clear();
    row++;

    label = std::make_unique<Label>();
    label->setText("Scan for Plugins at Startup:", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_scanForPlugins.setBounds(getCheckBoxBounds(row));
    m_scanForPlugins.setToggleState(srv->getScanForPlugins(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_scanForPlugins, "scan");

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
    m_logger.setToggleState(Logger::isEnabled(), NotificationType::dontSendNotification);
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

    label = std::make_unique<Label>();
    label->setText("Send crash reports (please enable if you have issues!):", NotificationType::dontSendNotification);
    label->setBounds(getLabelBounds(row));
    addChildAndSetID(label.get(), "lbl");
    m_components.push_back(std::move(label));

    m_crashReporting.setBounds(getCheckBoxBounds(row));
    m_crashReporting.setToggleState(srv->getCrashReporting(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_crashReporting, "dumps");

    row++;

    totalHeight += row * rowHeight;

    m_saveButton.setButtonText("Save");
    m_saveButton.setBounds(totalWidth / 2 - saveButtonWidth / 2, totalHeight - borderTB - saveButtonHeight,
                           saveButtonWidth, saveButtonHeight);
    m_saveButton.onClick = [this, app] {
        traceScope();

        Tracer::setEnabled(m_tracer.getToggleState());
        Logger::setEnabled(m_logger.getToggleState());

        auto appCpy = app;

        if (auto srv2 = appCpy->getServer()) {
            srv2->setName(m_nameText.getText());
            srv2->setEnableAU(m_auSupport.getToggleState());
            srv2->setEnableVST3(m_vst3Support.getToggleState());
            srv2->setEnableVST2(m_vst2Support.getToggleState());
            srv2->setEnableLV2(m_lv2Support.getToggleState());
            srv2->setScanForPlugins(m_scanForPlugins.getToggleState());
            srv2->setSandboxMode((Server::SandboxMode)m_sandboxMode.getSelectedItemIndex());
            srv2->setCrashReporting(m_crashReporting.getToggleState());

            switch (m_screenCapturingMode.getSelectedId()) {
                case 1:
                    srv2->setScreenCapturingFFmpeg(true);
                    srv2->setScreenCapturingFFmpegEncoder(ScreenRecorder::WEBP);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 2:
                    srv2->setScreenCapturingFFmpeg(true);
                    srv2->setScreenCapturingFFmpegEncoder(ScreenRecorder::MJPEG);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 3:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 4:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(true);
                    srv2->setScreenLocalMode(true);
                    srv2->setPluginWindowsOnTop(m_pluginWindowsOnTop.getToggleState());
                    break;
                case 5:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(true);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(m_pluginWindowsOnTop.getToggleState());
                    break;
            }

            srv2->setScreenCapturingFFmpegQuality(
                (ScreenRecorder::EncoderQuality)(m_screenCapturingQuality.getSelectedId() - 1));
            srv2->setScreenDiffDetection(m_screenDiffDetection.getToggleState());

            float qual = m_screenJpgQuality.getText().getFloatValue();
            if (qual < 0.1) {
                qual = 0.1f;
            } else if (qual > 1) {
                qual = 1.0f;
            }
            srv2->setScreenQuality(qual);

            if (m_vst3Folders.getText().length() > 0) {
                srv2->setVST3Folders(StringArray::fromLines(m_vst3Folders.getText()));
            }
            if (m_vst2Folders.getText().length() > 0) {
                srv2->setVST2Folders(StringArray::fromLines(m_vst2Folders.getText()));
            }
            srv2->setVSTNoStandardFolders(m_vstNoStandardFolders.getToggleState());

            if (m_lv2Folders.getText().length() > 0) {
                srv2->setLV2Folders(StringArray::fromLines(m_lv2Folders.getText()));
            }

            auto offsetParts = StringArray::fromTokens(m_screenMouseOffsetXY.getText(), "x", "");
            if (offsetParts.size() >= 2) {
                srv2->setScreenMouseOffsetX(offsetParts[0].getIntValue());
                srv2->setScreenMouseOffsetY(offsetParts[1].getIntValue());
            } else {
                srv2->setScreenMouseOffsetX(0);
                srv2->setScreenMouseOffsetY(0);
            }

            srv2->saveConfig();

            // startup servers
            auto ranges = StringArray::fromTokens(m_idText.getText(), ",", "");
            String valid;
            for (auto& r : ranges) {
                String start, end;
                auto parts = StringArray::fromTokens(r, "-", "");
                for (auto& p : parts) {
                    if (p.isNotEmpty()) {
                        if (start.isEmpty()) {
                            start = p;
                        } else if (end.isEmpty()) {
                            end = p;
                            break;
                        }
                    }
                }
                if (start.isNotEmpty()) {
                    if (valid.isNotEmpty()) {
                        valid << ",";
                    }
                    valid << start;
                    if (end.isNotEmpty()) {
                        valid << "-" << end;
                    }
                }
            }
            configWriteFile(Defaults::getConfigFileName(Defaults::ConfigServerStartup), {{"IDs", valid.toStdString()}});
        }

        appCpy->hideServerSettings();
        appCpy->restartServer();
    };

    addChildAndSetID(&m_saveButton, "save");
#else
    int totalHeight = 435;
    int saveButtonRegionHeight = 50;
    addAndMakeVisible(&m_tabbedComponent);

    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);

    m_tabbedComponent.addTab("Main", bgColour, &m_mainTab, true);
    m_tabbedComponent.addTab("Formats", bgColour, &m_pluginFormatsTab, true);
    m_tabbedComponent.addTab("Capture", bgColour, &m_screenCapturingTab, true);
    m_tabbedComponent.addTab("Startup", bgColour, &m_startupTab, true);
    m_tabbedComponent.addTab("Diagnostics", bgColour, &m_diagnosticsTab, true);
    m_tabbedComponent.setBounds(0, 0, totalWidth, totalHeight - saveButtonRegionHeight);

    m_saveButton.setButtonText("Save");
    m_saveButton.setBounds(totalWidth / 2 - saveButtonWidth / 2,
                           totalHeight - saveButtonRegionHeight / 2 - saveButtonHeight / 2, saveButtonWidth,
                           saveButtonHeight);
    addAndMakeVisible(&m_saveButton);

    m_saveButton.onClick = [this, app] {
        traceScope();

        Tracer::setEnabled(m_diagnosticsTab.getTracerEnabled());
        Logger::setEnabled(m_diagnosticsTab.getLoggerEnabled());

        auto appCpy = app;

        if (auto srv2 = appCpy->getServer()) {
            srv2->setName(m_mainTab.getNameText());
            srv2->setEnableAU(m_pluginFormatsTab.getAuSupport());
            srv2->setEnableVST3(m_pluginFormatsTab.getVst3Support());
            srv2->setEnableVST2(m_pluginFormatsTab.getVst2Support());
            srv2->setEnableLV2(m_pluginFormatsTab.getLv2Support());
            srv2->setScanForPlugins(m_startupTab.getScanForPlugins());
            srv2->setSandboxMode((Server::SandboxMode)m_mainTab.getSandboxSelectedIndex());
            srv2->setCrashReporting(m_diagnosticsTab.getCrashReportingEnabled());

            switch (m_screenCapturingTab.getModeSelectedId()) {
                case 1:
                    srv2->setScreenCapturingFFmpeg(true);
                    srv2->setScreenCapturingFFmpegEncoder(ScreenRecorder::WEBP);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 2:
                    srv2->setScreenCapturingFFmpeg(true);
                    srv2->setScreenCapturingFFmpegEncoder(ScreenRecorder::MJPEG);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 3:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(false);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(false);
                    break;
                case 4:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(true);
                    srv2->setScreenLocalMode(true);
                    srv2->setPluginWindowsOnTop(m_screenCapturingTab.getWindowsOnTopEnabled());
                    break;
                case 5:
                    srv2->setScreenCapturingFFmpeg(false);
                    srv2->setScreenCapturingOff(true);
                    srv2->setScreenLocalMode(false);
                    srv2->setPluginWindowsOnTop(m_screenCapturingTab.getWindowsOnTopEnabled());
                    break;
            }

            srv2->setScreenCapturingFFmpegQuality(
                (ScreenRecorder::EncoderQuality)(m_screenCapturingTab.getQualitySelectedId() - 1));
            srv2->setScreenDiffDetection(m_screenCapturingTab.getDiffDetectionEnabled());

            float qual = m_screenCapturingTab.getJpgQualityText().getFloatValue();
            if (qual < 0.1) {
                qual = 0.1f;
            } else if (qual > 1) {
                qual = 1.0f;
            }
            srv2->setScreenQuality(qual);

            if (m_pluginFormatsTab.getVst3FoldersText().length() > 0) {
                srv2->setVST3Folders(StringArray::fromLines(m_pluginFormatsTab.getVst3FoldersText()));
            }
            if (m_pluginFormatsTab.getVst2FoldersText().length() > 0) {
                srv2->setVST2Folders(StringArray::fromLines(m_pluginFormatsTab.getVst2FoldersText()));
            }
            srv2->setVSTNoStandardFolders(m_pluginFormatsTab.getVstNoStandardFolders());

            if (m_pluginFormatsTab.getLv2FoldersText().length() > 0) {
                srv2->setLV2Folders(StringArray::fromLines(m_pluginFormatsTab.getLv2FoldersText()));
            }

            auto offsetParts = StringArray::fromTokens(m_screenCapturingTab.getMouseOffsetXYText(), "x", "");
            if (offsetParts.size() >= 2) {
                srv2->setScreenMouseOffsetX(offsetParts[0].getIntValue());
                srv2->setScreenMouseOffsetY(offsetParts[1].getIntValue());
            } else {
                srv2->setScreenMouseOffsetX(0);
                srv2->setScreenMouseOffsetY(0);
            }

            // startup servers
            auto ranges = StringArray::fromTokens(m_mainTab.getIdText(), ",", "");
            String valid;
            for (auto& r : ranges) {
                String start, end;
                auto parts = StringArray::fromTokens(r, "-", "");
                for (auto& p : parts) {
                    if (p.isNotEmpty()) {
                        if (start.isEmpty()) {
                            start = p;
                        } else if (end.isEmpty()) {
                            end = p;
                            break;
                        }
                    }
                }
                if (start.isNotEmpty()) {
                    if (valid.isNotEmpty()) {
                        valid << ",";
                    }
                    valid << start;
                    if (end.isNotEmpty()) {
                        valid << "-" << end;
                    }
                }
            }
            configWriteFile(Defaults::getConfigFileName(Defaults::ConfigServerStartup), {{"IDs", valid.toStdString()}});
        }

        appCpy->hideServerSettings();
        appCpy->restartServer();
    };
#endif

    setResizable(false, false);
    centreWithSize(totalWidth, totalHeight);
    setBounds(WindowPositions::get(WindowPositions::ServerSettings, getBounds()));
    setVisible(true);
#ifdef JUCE_LINUX
    setMinimised(true);
#else
    windowToFront(this);
#endif
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
