/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
*/

#include "DiagnosticsTab.hpp"
#include "Logger.hpp"
#include "Tracer.hpp"

namespace e47 {

DiagnosticsTab::DiagnosticsTab(bool crashReporting)
{
    int row = 0;

    m_loggerLbl.setText("Logging:", NotificationType::dontSendNotification);
    m_loggerLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_loggerLbl);

    m_logger.setBounds(getCheckBoxBounds(row));
    m_logger.setToggleState(Logger::isEnabled(), NotificationType::dontSendNotification);
    addAndMakeVisible(m_logger);

    row++;

    m_tracerLbl.setText("Tracing (please enable to report issues):", NotificationType::dontSendNotification);
    m_tracerLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_tracerLbl);

    m_tracer.setBounds(getCheckBoxBounds(row));
    m_tracer.setToggleState(Tracer::isEnabled(), NotificationType::dontSendNotification);
    addChildAndSetID(&m_tracer, "tracer");

    row++;

    m_crashReportingLbl.setText("Send crash reports (please enable if you have issues!):", NotificationType::dontSendNotification);
    m_crashReportingLbl.setBounds(getLabelBounds(row));
    addAndMakeVisible(m_crashReportingLbl);

    m_crashReporting.setBounds(getCheckBoxBounds(row));
    m_crashReporting.setToggleState(crashReporting, NotificationType::dontSendNotification);
    addChildAndSetID(&m_crashReporting, "dumps");
}

void DiagnosticsTab::paint (Graphics& g)
{
    auto bgColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
    g.setColour(bgColour);
}

void DiagnosticsTab::resized()
{

}

}  // namespace e47