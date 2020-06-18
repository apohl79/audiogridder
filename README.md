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

## Downloads

Please find the latest binaries to download in the
[releases](https://github.com/apohl79/audiogridder/releases) section.

## AudioGridder Server

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

**Server Setup:**

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

## AudioGridder Plugin

The plugin is currently supported as VST2, VST3 and AudioUnit on
OSX. AAX is likely never coming, as AVID does not seem to support open
source projects unfortunately. See the compatibility section for a
workaround.

You are basically plugging a remote insert stack into your DAW's
channel insert stacks. From there you can insert any pluging available
on the connected remote server. Each plugin instance will connect to a
single remote server instance (which can be different for each
instance). 

**Plugin Setup:**

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

## Compatibility

- Server: OSX 64bit 10.7+, Windows 7+
- Plugin: OSX 64bit 10.11+, Windows 7+
- The server supports AudioUnit (OSX only) and VST2/VST3 plugins
- The plugin is available as AudioUnit (OSX only) and VST2/VST3
- Tested DAWs: Cubase 10 Pro, Logic Pro X, Reaper, Ableton Live
- ProTools is reportedly working via Blue Cat's PatchWork

## Reporting of Issues

If you report a new issue, please be as precise as possible. I will
have to be able to reproduce it or at least get some conclusions from
the info you provide.

- Report only one issue at a time, if you have multiple problems,
  please create multiple issues
- If you see a crash, please attach the stack trace from apples crash
  dialog
- Attach the latest log files (see locations below)
- Minimize the logs (restart the DAW and server and possibly just do,
  what leads to the problem and stop)

### Logfile locations

**OS X:**

Server:

```~/Library/Logs/AudioGridderServer/AudioGridderServer_DATE_TIME.log```

FX Plugin:

```~/Library/Logs/AudioGridderFX/AudioGridderPlugin_DATE_TIME.log```

Instrument Plugin:

```~/Library/Logs/AudioGridderInstrument/AudioGridderPlugin_DATE_TIME.log```

**Windows:**

Server: 

```C:\Users\<Username>\AppData\Roaming\AudioGridderServer\AudioGridderServer_DATE_TIME.log```

FX Plugin:

```C:\Users\<Username>\AppData\Roaming\AudioGridderFX\AudioGridderPlugin_DATE_TIME.log```

Instrument Plugin:

```C:\Users\<Username>\AppData\Roaming\AudioGridderInstrument\AudioGridderPlugin_DATE_TIME.log```
