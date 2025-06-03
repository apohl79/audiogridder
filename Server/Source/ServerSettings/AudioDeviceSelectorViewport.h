#pragma once

#include <JuceHeader.h>
#include "TabCommon.h"

class AudioDeviceSelectorViewport : public juce::Viewport
{
public:
    AudioDeviceSelectorViewport(IOSettings ioSettings) :
        audioSettings (ioSettings.deviceManager,
                       ioSettings.minAudioInputChannels,
                       ioSettings.maxAudioInputChannels,
                       ioSettings.minAudioOutputChannels,
                       ioSettings.maxAudioOutputChannels,
                       ioSettings.showMidiInputOptions,
                       ioSettings.showMidiOutputSelector,
                       ioSettings.showChannelsAsStereoPairs,
                       ioSettings.hideAdvancedOptionsWithButton)
    {
        addAndMakeVisible(audioSettings);
        setViewedComponent(&audioSettings);
    }

    void resized() override
    {
        audioSettings.setBounds(getLocalBounds());
    }
private:
    juce::AudioDeviceSelectorComponent audioSettings;
};