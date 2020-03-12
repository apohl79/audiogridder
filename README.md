# AudioGridder

AudioGridder allows you to offload DSP processing from your local to
remote computers. This can come in handy when mixing complex projects
for instance. AudioGridder comes with a plugin and a server that is
enabling VST3 and AudioUnit plugins to be hosted across a local area
network. Simply run the server component on a machine in your
network. Using the AudioGridder AU/VST3 plugin you can add a remote
insert chain into your DAW's signal paths. The DSP code of the
inserted plugins will be executed on the remote machine. The plugin
UI's will be streamed over the wire. This allows for an experience
very close to hosting the plugins directly in your DAW but not using
your local CPU.

![](https://raw.githubusercontent.com/apohl79/audiogridder/master/images/overview.png "AudioGridder Overview")

This setup proves to be working very well on wired networks. Wireless
networks work as well given you have a proper connection that provides
low latency and enough bandwidth. There is basically no limitation on
the network side, but your DAW has some latency needs. So a common DSL
connection through a VPN might be problematic but not impossible.

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
can than easily address each instance from the AudioGridder plugin
via "server[:ID]".

You can also run multiple servers within your network and access
different servers from your DAW at the same time as needed.

**Server Setup:**

1. Install the PKG (includes server and pluging binaries)
2. Run the server (If it crashes, just re-run it until it successfilly
finishes the startup. Each plugin that does not work will be
blacklisted.)
3. Manage your plugins (if you do not want to enable all plugins)
4. If you want to run multiple servers on a singel machine, you will
have to assign a different server ID to each instance.

![](https://raw.githubusercontent.com/apohl79/audiogridder/master/images/server.png "AudioGridder Server")

## AudioGridder Plugin

The plugin is currently supported as VST3 and AudioUnit on OSX. AAX is
work in progress.

You are basically plugging a remote insert stack into your DAW's
channel insert stacks. From there you can insert any pluging available
on the connected remote server. Each plugin instance will connect to a
single remote server instance (which can be different for each
instance). 

**Plugin Setup:**

1. Install the PKG (includes server and pluging binaries)
2. Run your DAW and insert the AudioGridder plugin
3. Add your server endpoint(s) by clicking the server icon (this needs
to be done only once, as the server settings will be shared with new
plugin instances)
4. Add remote plugins

![](https://raw.githubusercontent.com/apohl79/audiogridder/master/images/plugin.png "AudioGridder Plugin")

## Compatibility

- OSX 64bit
- Supported plugin formats by the server: AudioUnit and VST3
- Support plugin formats: AudioUnit, VST3 
