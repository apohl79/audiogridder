/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Server_hpp
#define Server_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "Worker.hpp"
#include "Defaults.hpp"
#include "ProcessorChain.hpp"

#include <set>
#include <thread>

namespace e47 {

class Server : public Thread {
  public:
    Server();
    virtual ~Server();
    void shutdown();
    void loadConfig();
    void saveConfig();
    int getId() const { return m_id; }
    void setId(int i) { m_id = i; }
    bool getEnableAU() const { return m_enableAU; }
    void setEnableAU(bool b) { m_enableAU = b; }
    bool getEnableVST() const { return m_enableVST; }
    void setEnableVST(bool b) { m_enableVST = b; }
    float getScreenQuality() const { return m_screenJpgQuality; }
    void setScreenQuality(float q) { m_screenJpgQuality = q; }
    bool getScreenDiffDetection() const { return m_screenDiffDetection; }
    void setScreenDiffDetection(bool b) { m_screenDiffDetection = b; }
    void run();
    const KnownPluginList& getPluginList() const { return m_pluginlist; }
    KnownPluginList& getPluginList() { return m_pluginlist; }
    bool shouldExclude(const String& name);
    bool shouldExclude(const String& name, const std::vector<String>& include);
    auto& getExcludeList() { return m_pluginexclude; }
    void addPlugins(const std::vector<String>& names, std::function<void(bool)> fn);
    void saveKnownPluginList();

  private:
    String m_host;
    int m_port = DEFAULT_SERVER_PORT;
    int m_id = 0;
    StreamingSocket m_masterSocket;
    using WorkerList = std::vector<std::unique_ptr<Worker>>;
    WorkerList m_workers;
    KnownPluginList m_pluginlist;
    std::set<String> m_pluginexclude;
    bool m_enableAU = true;
    bool m_enableVST = true;
    float m_screenJpgQuality = 0.9;
    bool m_screenDiffDetection = true;

    bool scanNextPlugin(PluginDirectoryScanner& scanner, String& name);
    void scanForPlugins();
    void scanForPlugins(const std::vector<String>& include);
    void loadKnownPluginList();
};

}  // namespace e47

#endif /* Server_hpp */
