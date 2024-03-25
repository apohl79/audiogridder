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
      m_app(app),
      m_tabbedComponent(TabbedButtonBar::TabsAtTop),
      m_mainTab(app->getServer()->getMainSettings()),
      m_pluginFormatsTab(app->getServer()->getFormatSettings()),
      m_screenCapturingTab(app->getServer()->getCaptureSettings()),
      m_startupTab(app->getServer()->getScanForPlugins()),
      m_diagnosticsTab(app->getServer()->getCrashReporting())
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
