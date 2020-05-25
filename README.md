# AudioGridder

AudioGridder allows you to offload DSP processing from your local to
remote computers. This can come in handy when mixing complex projects
for instance. AudioGridder comes with a plugin and a server that is
enabling VST3 and AudioUnit plugins to be hosted across the
network. Simply run the server component on a remote machine and
connect your DAW using the AudioGridder AU/VST3 plugin. You can add
remote insert chains into your DAW's signal paths that way. The DSP
code of the inserted plugins will be executed on the remote machine
and the plugin UI's will be streamed over the wire. This allows for an
experience very close to hosting the plugins directly in your DAW but
not using your local CPU.

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/overview.jpg" width="600" />
</p>

This setup proves to be working very well on wired networks. Wireless
networks work as well given you have a proper connection that provides
low latency and enough bandwidth. There is basically no limitation on
the network side, but your DAW has some latency needs. So a common DSL
connection through a VPN might be problematic but not impossible.

## Downloads

Please find the latest binaries to download in the [releases](https://github.com/apohl79/audiogridder/releases) section.

## AudioGridder Server

The server supports VST3 and AudioUnit plugin formats. Installation
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

1. Install the PKG on OSX or the Setup EXE on Windows (includes server and plugin binaries)
2. Grant AudioGridderServer the Accessibility permission (System
Preferences -> Security & Privacy -> Privacy Tab) - If you upgrade,
remove the existing entry and re-add it.
3. Run the server (If it crashes, just re-run it until it successfully
finishes the startup. Each plugin that does not work will be
blacklisted.)
4. Manage your plugins (if you do not want to enable all plugins)
5. If you want to run multiple servers on a singel machine, you will
have to assign a different server ID to each instance.

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/server.jpg" />
</p>

## AudioGridder Plugin

The plugin is currently supported as VST3 and AudioUnit on OSX. AAX is
likely never coming, as AVID does not seem to support open source
projects unfortunately. 

You are basically plugging a remote insert stack into your DAW's
channel insert stacks. From there you can insert any pluging available
on the connected remote server. Each plugin instance will connect to a
single remote server instance (which can be different for each
instance). 

**Plugin Setup:**

1. Install the PKG on OSX or the Setup EXE on Windows (includes server and plugin binaries)
2. Run your DAW and insert the AudioGridder plugin
3. Add your server endpoint(s) (IP or DNS name) by clicking the server icon (this needs
to be done only once, as the server settings will be shared with new
plugin instances)<br/>**Note:** Server and client have to be able to directly reach each other. This is because the server will have to connect the client at initialization time.
4. Add remote plugins

<p align="center">
<img src="https://raw.githubusercontent.com/apohl79/audiogridder/master/images/plugin.jpg" />
</p>

## Compatibility

- Server: OSX 64bit 10.7+, Windows 10
- Plugin: OSX 64bit 10.11+, Windows 10
- The server supports AudioUnit (Mac only) and VST3 plugins
- The plugin is available as AudioUnit (Mac only) and VST3
- Tested DAWs: Cubase 10 Pro, Logic Pro X, Reaper, Ableton Live

## Reporting of Issues

If you report a new issue, please be as precise as possible. I will have to be able to reproduce it or at least get some conclusions from the info you provide.

- Report only one issue at a time, if you have multiple problems, please create multiple issues
- If you see a crash, please attach the stack trace from apples crash dialog
- Attach the latest server log file (~/Library/Logs/AudioGridderServer/Main_DATE-TIME.log)
- Minimize the server log (restart the server and just do, what leads to the problem and stop)
