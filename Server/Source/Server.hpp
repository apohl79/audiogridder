/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Server_hpp
#define Server_hpp

#include <JuceHeader.h>
#include <set>
#include <thread>

#include "Worker.hpp"
#include "Defaults.hpp"
#include "ProcessorChain.hpp"
#include "Utils.hpp"
#include "json.hpp"
#include "ScreenRecorder.hpp"
#include "Sandbox.hpp"

namespace e47 {

class Server : public Thread, public LogTag {
  public:
    Server(const json& opts = {});
    ~Server() override;

    void initialize();
    void shutdown();
    void run() override;

    void loadConfig();
    void saveConfig();

    enum SandboxMode : int { SANDBOX_NONE = 0, SANDBOX_CHAIN = 1, SANDBOX_PLUGIN = 2 };

    int getId(bool ignoreOpts = false) const;
    void setId(int i) { m_id = i; }
    void setHost(const String& h) { m_host = h; }
    const String& getName() const { return m_name; }
    void setName(const String& name);
    Uuid getUuid() const { return m_uuid; }

    bool getEnableAU() const { return m_enableAU; }
    void setEnableAU(bool b) { m_enableAU = b; }
    bool getEnableVST3() const { return m_enableVST3; }
    void setEnableVST3(bool b) { m_enableVST3 = b; }
    bool getEnableVST2() const { return m_enableVST2; }
    void setEnableVST2(bool b) { m_enableVST2 = b; }
    const StringArray& getVST3Folders() const { return m_vst3Folders; }
    void setVST3Folders(const StringArray& folders) { m_vst3Folders = folders; }
    const StringArray& getVST2Folders() const { return m_vst2Folders; }
    void setVST2Folders(const StringArray& folders) { m_vst2Folders = folders; }
    bool getVSTNoStandardFolders() const { return m_vstNoStandardFolders; }
    void setVSTNoStandardFolders(bool b) { m_vstNoStandardFolders = b; }
    float getScreenQuality() const { return m_screenJpgQuality; }
    void setScreenQuality(float q) { m_screenJpgQuality = q; }
    bool getScreenDiffDetection() const { return m_screenDiffDetection; }
    void setScreenDiffDetection(bool b) { m_screenDiffDetection = b; }
    bool getScreenCapturingFFmpeg() const { return m_sandboxModeRuntime != SANDBOX_PLUGIN && m_screenCapturingFFmpeg; }
    void setScreenCapturingFFmpeg(bool b) { m_screenCapturingFFmpeg = b; }
    ScreenRecorder::EncoderMode getScreenCapturingFFmpegEncoder() const { return m_screenCapturingFFmpegEncMode; }
    void setScreenCapturingFFmpegEncoder(ScreenRecorder::EncoderMode m) { m_screenCapturingFFmpegEncMode = m; }
    ScreenRecorder::EncoderQuality getScreenCapturingFFmpegQuality() const { return m_screenCapturingFFmpegQuality; }
    void setScreenCapturingFFmpegQuality(ScreenRecorder::EncoderQuality q) { m_screenCapturingFFmpegQuality = q; }
    bool getScreenCapturingOff() const { return m_sandboxModeRuntime == SANDBOX_PLUGIN || m_screenCapturingOff; }
    void setScreenCapturingOff(bool b) { m_screenCapturingOff = b; }
    bool getScreenLocalMode() const { return m_screenLocalMode; }
    void setScreenLocalMode(bool b) { m_screenLocalMode = b; }
    int getScreenMouseOffsetX() const { return m_screenMouseOffsetX; }
    void setScreenMouseOffsetX(int i) { m_screenMouseOffsetX = i; }
    int getScreenMouseOffsetY() const { return m_screenMouseOffsetY; }
    void setScreenMouseOffsetY(int i) { m_screenMouseOffsetY = i; }
    bool getPluginWindowsOnTop() const { return m_pluginWindowsOnTop; }
    void setPluginWindowsOnTop(bool b) { m_pluginWindowsOnTop = b; }
    bool getScanForPlugins() const { return m_scanForPlugins; }
    void setScanForPlugins(bool b) { m_scanForPlugins = b; }
    SandboxMode getSandboxMode() const { return m_sandboxMode; }
    SandboxMode getSandboxModeRuntime() const { return m_sandboxModeRuntime; }
    void setSandboxMode(SandboxMode m) { m_sandboxMode = m; }
    bool getCrashReporting() const { return m_crashReporting; }
    void setCrashReporting(bool b) { m_crashReporting = b; }

    const KnownPluginList& getPluginList() const { return m_pluginList; }
    KnownPluginList& getPluginList() { return m_pluginList; }
    const Array<AudioProcessor::BusesLayout>& getPluginLayouts(const String& id);

    bool shouldExclude(const String& name, const String& id);
    bool shouldExclude(const String& name, const String& id, const std::vector<String>& include);
    auto& getExcludeList() { return m_pluginExclude; }
    void addPlugins(const std::vector<String>& names, std::function<void(bool, const String&)> fn);

    static void loadKnownPluginList(KnownPluginList& plist, json& playouts, int srvId);
    static void saveKnownPluginList(KnownPluginList& plist, json& playouts, int srvId);

    void saveKnownPluginList(bool wipe = false);

    static bool scanPlugin(const String& id, const String& format, int srvId, bool secondRun = false);

    void sandboxShowEditor();
    void sandboxHideEditor();

    // SandboxMaster
    void handleMessageFromSandbox(SandboxMaster&, const SandboxMessage&);
    void handleDisconnectFromSandbox(SandboxMaster&);

    // SandboxSlave
    void handleMessageFromMaster(const SandboxMessage&);
    void handleDisconnectedFromMaster();
    void handleConnectedToMaster();

    int getNumSandboxes() { return m_sandboxes.size(); }
    int getNumLoadedBySandboxes() {
        int sum = 0;
        for (auto c : m_sandboxLoadedCount) {
            sum += c;
        }
        return sum;
    }

    void updateSandboxNetworkStats(const String& key, uint32 loaded, double bytesIn, double bytesOut, double rps,
                                   const std::vector<TimeStatistic::Histogram>& audioHists);

    double getProcessingTraceTresholdMs() const { return m_processingTraceTresholdMs; }
    void setProcessingTraceTresholdMs(double d) { m_processingTraceTresholdMs = d; }

    template <typename T>
    inline T getOpt(const String& name, T def) const {
        return jsonGetValue(m_opts, name, def);
    }

    template <typename T>
    inline void setOpt(const String& name, T val) {
        m_opts[name.toStdString()] = val;
    }

  private:
    json m_opts;

    String m_host;
    int m_port = Defaults::SERVER_PORT;
    int m_id = 0;
    String m_name;
    Uuid m_uuid;
    StreamingSocket m_masterSocket, m_masterSocketLocal;
    using WorkerList = Array<std::shared_ptr<Worker>>;
    WorkerList m_workers;
    KnownPluginList m_pluginList;
    json m_jpluginLayouts;
    std::unordered_map<String, Array<AudioProcessor::BusesLayout>> m_pluginLayouts;
    std::set<String> m_pluginExclude;
    bool m_enableAU = true;
    bool m_enableVST3 = true;
    bool m_enableVST2 = true;
    float m_screenJpgQuality = 0.9f;
    bool m_screenDiffDetection = true;
    bool m_screenCapturingFFmpeg = true;
    bool m_screenCapturingOff = false;
    bool m_screenLocalMode = false;
    int m_screenMouseOffsetX = 0;
    int m_screenMouseOffsetY = 0;
    bool m_pluginWindowsOnTop = false;
    ScreenRecorder::EncoderMode m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
    ScreenRecorder::EncoderQuality m_screenCapturingFFmpegQuality = ScreenRecorder::ENC_QUALITY_MEDIUM;
    StringArray m_vst3Folders;
    StringArray m_vst2Folders;
    bool m_vstNoStandardFolders;
    bool m_scanForPlugins = true;
    bool m_crashReporting = true;
    SandboxMode m_sandboxMode = SANDBOX_CHAIN, m_sandboxModeRuntime = SANDBOX_NONE;
    bool m_sandboxLogAutoclean = true;
    double m_processingTraceTresholdMs = 0.0;

    HashMap<String, std::shared_ptr<SandboxMaster>, DefaultHashFunctions, CriticalSection> m_sandboxes;

    std::unique_ptr<SandboxSlave> m_sandboxController;

    HashMap<String, uint32, DefaultHashFunctions, CriticalSection> m_sandboxLoadedCount;

    std::atomic_bool m_sandboxReady{false};
    std::atomic_bool m_sandboxConnectedToMaster{false};
    HandshakeRequest m_sandboxConfig;
    String m_sandboxHasScreen;

    struct SandboxDeleter : Thread {
        Array<std::shared_ptr<SandboxMaster>> sandboxes;
        std::mutex mtx;

        SandboxDeleter() : Thread("SandboxDeleter") { startThread(); }

        void add(std::shared_ptr<SandboxMaster> s) {
            std::lock_guard<std::mutex> lock(mtx);
            sandboxes.add(std::move(s));
        }

        void clear() {
            Array<std::shared_ptr<SandboxMaster>> sandboxesCpy;
            {
                std::lock_guard<std::mutex> lock(mtx);
                sandboxes.swapWith(sandboxesCpy);
            }
            for (auto sandbox : sandboxesCpy) {
                if (sandbox != nullptr) {
                    sandbox->killWorkerProcess();
                }
            }
        }

        void run() override {
            while (!threadShouldExit()) {
                clear();
                sleepExitAware(100);
            }
            clear();
        }
    };

    std::unique_ptr<SandboxDeleter> m_sandboxDeleter;

    void scanNextPlugin(const String& id, const String& name, const String& fmt, int srvId,
                        std::function<void(const String&)> onShellPlugin, bool secondRun = false);
    void scanForPlugins();
    void scanForPlugins(const std::vector<String>& include);

    void processScanResults(int id, std::set<String>& newBlacklistedPlugins);

    void loadKnownPluginList();
    bool parsePluginLayouts(const String& id = {});

    void checkPort();
    void runServer();
    void runSandboxChain();
    void runSandboxPlugin();

    bool sendHandshakeResponse(StreamingSocket* sock, bool sandboxEnabled = false, int sandboxPort = 0);
    bool createWorkerListener(std::shared_ptr<StreamingSocket> sock, bool isLocal, int& workerPort);
    void shutdownWorkers();

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* Server_hpp */
