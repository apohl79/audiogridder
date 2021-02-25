# AudioGridder

AudioGridder is a plugin host, that allows you to offload the DSP processing of
audio plugins to remote computers running macOS or Windows. This can come in handy
when mixing complex projects or running CPU intensive instruments for instance.
AudioGridder comes with a plugin and a server and supports VST2, VST3 and
AudioUnit plugin formats. Plugins can be hosted and accessed across the network:
simply run the AudioGridder server on a remote machine and connect your DAW
using the AudioGridder plugin. This allows you to add remote insert chains or
instruments into your DAW's signal paths. The DSP code of the loaded remote
plugins will be executed on the remote machine and the remote plugin UI's will
be streamed over the wire. With AudioGridder you get an experience very close to
hosting the plugins directly in your DAW but not using your local CPU.

For more information and intstallation instructions, please visit
[https://audiogridder.com](https://audiogridder.com).

<p align="center">
<img src="https://audiogridder.com/wp-content/uploads/2021/02/diagram-back.png" width="500" />
</p>

# Help / Bugs / Ideas

Please report bugs, discuss ideas or ask questions in the
[discussions](https://github.com/apohl79/audiogridder/discussions) area!
Issues will only be created as a result of a discussion going forward.

:point_right: **Note: Please do NOT create issues. Please create discussion threads.**

:exclamation: You have to follow [the bug reporting guide](https://audiogridder.com/bug-reports/) when reporting bugs!

# Installation

Please find the latest installers in the 
[download](https://audiogridder.com/download/) section.

## macOS Homebrew

On macOS you can install AudioGridder via homebrew:

```
brew install audiogridder-plugin
```
```
brew install audiogridder-server
```

# Features

- VST2 / VST3 / AudioUnit (macOS only)
- Effect & Instrument plugins
- Latency compensation
- 32/64 bit float processing
- Audio over network
- Midi over network
- Unlimited remote effect plugin chains
- Streaming of plugin UIs
- Local control of remote plugin UI's
- Generic Plugin Parameter Editor
- Automation

# Compatibility

- Server: macOS 10.7+, Windows 7+
- Plugin: macOS 10.7+, Windows 7+, Linux 64bit
- The server supports AudioUnit (macOS only) and VST2/VST3 plugins
- The plugin is available as AudioUnit (macOS only) and VST2/VST3
- Tested DAWs: Cubase 10 Pro, Logic Pro X, Reaper, Ableton Live
- ProTools is reportedly working via wrapper plugins that can host
non AAX plugins within ProTools


# Donation

AudioGridder is free. If you use it, please consider supporting the project with a donation:

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=MF9TGYY8P8GG4)
[![donorbox](https://d1iczxrky3cnb2.cloudfront.net/button-small-blue.png)](https://donorbox.org/audiogridder?default_interval=o)
