# AudioGridder

AudioGridder is a plugin host, that allows you to offload DSP
processing to remote computers running OS X or Windows. This can come
in handy when mixing complex projects for instance. AudioGridder comes
with a plugin and a server and supports VST2, VST3 and AudioUnit
formats. Plugins can be hosted across the network. Simply run the
server component on a remote machine and connect your DAW using the
AudioGridder plugin. This allows you to add remote insert chains or
instruments into your DAW's signal paths. The DSP code of the loaded
remote plugins will be executed on the remote machine and the remote
plugin UI's will be streamed over the wire. With AudioGridder you get
an experience very close to hosting the plugins directly in your DAW
but not using your local CPU.

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/overview.jpg" width="600" />
</p>

This setup proves to be working very well on wired networks. Wireless
networks work as well given you have a proper connection that provides
low latency and enough bandwidth. There is basically no limitation on
the network side, but your DAW has some latency needs. So a common DSL
connection through a VPN might be problematic but not impossible.

# Table of Contents

   * [Downloads](#downloads)
   * [Features](#features)
   * [Compatibility](#compatibility)
   * [Reporting of Bugs/Issues](#reporting-of-bugsissues)
   * [AudioGridder Server](#audiogridder-server)
      * [Server Installation](#server-installation)
   * [AudioGridder Plugin](#audiogridder-plugin)
      * [Plugin Installation](#plugin-installation)
   * [Bug Report Diagnostics Locations](#bug-report-diagnostics-locations)
   * [Donation](#donation)

# Downloads

Please find the latest binaries to download in the
[releases](https://github.com/apohl79/audiogridder/releases) section.

# Features

- VST2 / VST3 / AudioUnit (OSX only)
- Effects & Instrument Plugins
- Latency compensation
- Local control of remote plugin UI's
- Unlimited remote plugin chains

# Compatibility

- Server: OSX 64bit 10.7+, Windows 7+
- Plugin: OSX 64bit 10.11+, Windows 7+
- The server supports AudioUnit (OSX only) and VST2/VST3 plugins
- The plugin is available as AudioUnit (OSX only) and VST2/VST3
- Tested DAWs: Cubase 10 Pro, Logic Pro X, Reaper, Ableton Live
- ProTools is reportedly working via Blue Cat's PatchWork

# Reporting of Bugs/Issues

If you report a new issue, please be as precise as possible. To
identify the root cause of an issue, it is necessary to be able to
reproduce it or at least get some conclusions from the info you
provide.

- Report only one issue at a time, if you have multiple problems,
  please create multiple issues.
- Minimize the log files:
  - Wipe all AudioGridder log folders (see below).
  - Restart the server.
  - Restart the DAW.
  - Load only the minimal amount of plugins needed to run into the
    problem you have.
  - Take only the actions that lead to the problem you have.
- Attach the log files to the issue (see locations below) as zip.
- If you see a crash on OSX, please attach the crash report to
  the issue (crash report dialog or see locations below) as zip.
- If you see a crash on Windows, the log file directories contain dump
  files, just follow the steps to "minimize the log files" and you are
  good.

# AudioGridder Server

The server supports VST2, VST3 and AudioUnit plugin formats. Installation
and setup is straight forward. There is multiple possibilities for
your setup. It's suggested, that you dedicate each server to a single
remote DAW workspace. That is because each server can only stream a
single UI at the same time.

You can run multiple parallel UI user sessions on a Mac. Setup a user
for each remote workspace, create a UI session (via VNC for example)
and run a separate server instance in each session. You need to assign
a different server ID to each instance (in the server settings). You
can easily address each instance from the AudioGridder plugin
via "server[:ID]".

You can also run multiple servers within your network and access
different servers from your DAW at the same time.

## Server Installation

1. Install the PKG on OSX or the Setup EXE on Windows. (The installer
includes the server and plugin binaries.)
2. **On OSX**: Grant AudioGridderServer the "Accessibility" (Mojave,
Catalina) and "Screen Recording" (Catalina) permissions (System
Preferences -> Security & Privacy -> Privacy Tab) - This is also
required after upgrading to a new version! **You will not be able to
see/control the remote UI otherwise**.
3. **On Windows**: It's recommended to deactivate scaling. Even though
AudioGridder works with scaling enabled, it has a negative impact
on the performance.
4. Run the server
5. Manage your plugins (if you do not want to enable all plugins).
6. If you want to run multiple servers on a singel machine, you will
have to assign a different server ID to each instance.

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/server.jpg" />
</p>

# AudioGridder Plugin

The plugin is currently supported as VST2, VST3 and AudioUnit on
OSX. AAX is likely never coming, as AVID does not seem to support open
source projects unfortunately. See the compatibility section for a
workaround.

You are basically plugging a remote insert stack into your DAW's
channel insert stacks. From there you can insert any pluging available
on the connected remote server. Each plugin instance will connect to a
single remote server instance (which can be different for each
instance). 

## Plugin Installation

1. Install the PKG on OSX or the Setup EXE on Windows. (The installer
includes the server and plugin binaries.)
2. Run your DAW and insert the AudioGridder plugin.
3. Add your server endpoint(s) (IP or DNS name) by clicking the server
icon (this needs to be done only once, as the server settings will be
shared with new plugin instances)<br/>**Note:** Server and client have
to be able to directly reach each other. This is because the server
will have to connect the client at initialization time.
4. Add remote plugins.

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/plugin.jpg" />
</p>

# Bug Report Diagnostics Locations

**OS X:**

Server Logs:

```~/Library/Logs/AudioGridderServer/AudioGridderServer_DATE_TIME.log```

FX Plugin Logs:

```~/Library/Logs/AudioGridderFX/AudioGridderPlugin_DATE_TIME.log```

Instrument Plugin Logs:

```~/Library/Logs/AudioGridderInstrument/AudioGridderPlugin_DATE_TIME.log```

Crash Reports:

```~/Library/Logs/DiagnosticReports```

```/Library/Logs/DiagnosticReports``` (system wide)

If the server crashes the report will be named
AudioGridderServer_DATE-TIME*. If a plugin crashes, it will crash
the DAW. Thus the crash report will be prefixed by the DAW name.

**Windows:**

Server Logs:

```C:\Users\<Username>\AppData\Roaming\AudioGridderServer\AudioGridderServer_DATE_TIME.log```

FX Plugin Logs:

```C:\Users\<Username>\AppData\Roaming\AudioGridderFX\AudioGridderPlugin_DATE_TIME.log```

Instrument Plugin Logs:

```C:\Users\<Username>\AppData\Roaming\AudioGridderInstrument\AudioGridderPlugin_DATE_TIME.log```

# Donation

If you like AudioGridder and want to support its further development, you are welcome to donate. :-)

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=MF9TGYY8P8GG4)
