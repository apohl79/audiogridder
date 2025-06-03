/*
* Copyright (c) 2024 Andreas Pohl
* Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
*
* Author: Kieran Coulter
 */

#pragma once

#include <JuceHeader.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioDeviceSelectorViewport.h"
#include "TabCommon.h"

namespace e47 {

class IOTab  : public juce::Component
{
  public:
    IOTab(IOSettings ioSettings);
    void paint (Graphics& g) override;
    void resized() override;
  private:
    AudioDeviceSelectorViewport m_deviceSelector;
};

}  // namespace e47
