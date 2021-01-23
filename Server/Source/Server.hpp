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

namespace e47 {

using json = nlohmann::json;

class Server : public Thread, public LogTag {
  public:
    Server(json opts = {});
    ~Server();
    void initialize();
    void shutdown();
    void loadConfig();
    void saveConfig();
    int getId(bool ignoreOpts = false) const;
    void setId(int i) { m_id = i; }
    const String& getName() const { return m_name; }
    void setName(const String& name);
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
    bool getScreenCapturingFFmpeg() const { return m_screenCapturingFFmpeg; }
    void setScreenCapturingFFmpeg(bool b) { m_screenCapturingFFmpeg = b; }
    ScreenRecorder::EncoderMode getScreenCapturingFFmpegEncoder() const { return m_screenCapturingFFmpegEncMode; }
    void setScreenCapturingFFmpegEncoder(ScreenRecorder::EncoderMode m) { m_screenCapturingFFmpegEncMode = m; }
    ScreenRecorder::EncoderQuality getScreenCapturingFFmpegQuality() const { return m_screenCapturingFFmpegQuality; }
    void setScreenCapturingFFmpegQuality(ScreenRecorder::EncoderQuality q) { m_screenCapturingFFmpegQuality = q; }
    bool getScreenCapturingOff() const { return m_screenCapturingOff; }
    void setScreenCapturingOff(bool b) { m_screenCapturingOff = b; }
    bool getScanForPlugins() const { return m_scanForPlugins; }
    void setScanForPlugins(bool b) { m_scanForPlugins = b; }
    bool getParallelPluginLoad() const { return m_parallelPluginLoad; }
    void setParallelPluginLoad(bool b) { m_parallelPluginLoad = b; }
    void run();
    const KnownPluginList& getPluginList() const { return m_pluginlist; }
    KnownPluginList& getPluginList() { return m_pluginlist; }
    bool shouldExclude(const String& name);
    bool shouldExclude(const String& name, const std::vector<String>& include);
    auto& getExcludeList() { return m_pluginexclude; }
    void addPlugins(const std::vector<String>& names, std::function<void(bool)> fn);
    void saveKnownPluginList();

    static bool scanPlugin(const String& id, const String& format);

  private:
    json m_opts;

    String m_host;
    int m_port = Defaults::SERVER_PORT;
    int m_id = 0;
    String m_name;
    StreamingSocket m_masterSocket;
    using WorkerList = Array<std::shared_ptr<Worker>>;
    WorkerList m_workers;
    KnownPluginList m_pluginlist;
    std::set<String> m_pluginexclude;
    bool m_enableAU = true;
    bool m_enableVST3 = true;
    bool m_enableVST2 = true;
    float m_screenJpgQuality = 0.9f;
    bool m_screenDiffDetection = true;
    bool m_screenCapturingFFmpeg = true;
    bool m_screenCapturingOff = false;
    ScreenRecorder::EncoderMode m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
    ScreenRecorder::EncoderQuality m_screenCapturingFFmpegQuality = ScreenRecorder::ENC_QUALITY_MEDIUM;
    StringArray m_vst3Folders;
    StringArray m_vst2Folders;
    bool m_vstNoStandardFolders;
    bool m_scanForPlugins = true;
    bool m_parallelPluginLoad = false;

    void scanNextPlugin(const String& id, const String& fmt);
    void scanForPlugins();
    void scanForPlugins(const std::vector<String>& include);

    void loadKnownPluginList();
    static void loadKnownPluginList(KnownPluginList& plist);
    static void saveKnownPluginList(KnownPluginList& plist);

    template <typename T>
    inline T getOpt(const String& name, T def) const {
        return jsonGetValue(m_opts, name, def);
    }

    ENABLE_ASYNC_FUNCTORS();
};

}  // namespace e47

#endif /* Server_hpp */
