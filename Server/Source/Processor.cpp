/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Processor.hpp"
#include "ProcessorChain.hpp"
#include "App.hpp"
#include "Server.hpp"

namespace e47 {

std::atomic_uint32_t Processor::loadedCount{0};
// std::mutex Processor::m_pluginLoaderMtx;

Processor::Processor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize, bool isClient)
    : LogTagDelegate(chain.getLogTagSource()),
      m_chain(chain),
      m_id(id),
      m_sampleRate(sampleRate),
      m_blockSize(blockSize),
      m_isClient(isClient),
      m_fmt(id.startsWith("VST3")  ? VST3
            : id.startsWith("VST") ? VST
                                   : AU) {
    initAsyncFunctors();
}

Processor::Processor(ProcessorChain& chain, const String& id, double sampleRate, int blockSize)
    : Processor(chain, id, sampleRate, blockSize,
                getApp()->getServer()->getSandboxMode() == Server::SANDBOX_PLUGIN &&
                    getApp()->getServer()->getSandboxModeRuntime() == Server::SANDBOX_NONE) {}

Processor::~Processor() {
    unload();
    stopAsyncFunctors();
}

String Processor::createPluginID(const PluginDescription& d) {
    return d.pluginFormatName + "-" + String::toHexString(d.uniqueId);
}

String Processor::createPluginIDWithName(const PluginDescription& d) {
    return d.pluginFormatName + (d.name.isNotEmpty() ? "-" + d.name : "") + "-" + String::toHexString(d.uniqueId);
}

String Processor::createPluginIDDepricated(const PluginDescription& d) {
    return d.pluginFormatName + (d.name.isNotEmpty() ? "-" + d.name : "") + "-" + String::toHexString(d.deprecatedUid);
}

String Processor::convertJUCEtoAGPluginID(const String& id) {
    // JUCE uses the fromat: <AU|VST|VST3>-<Name>-<File Name Hash>-<Plugin ID>
    int pos = -1;
    String format, name, fileHash, pluginId;

    int numberOfMinuses = 0;
    while ((pos = id.indexOfChar(pos + 1, '-')) > -1) {
        numberOfMinuses++;
    }

    if (numberOfMinuses != 3) {
        return {};
    }

    if ((pos = id.indexOfChar(0, '-')) > -1) {
        format = id.substring(0, pos);
        if (format != "AudioUnit" && format != "VST" && format != "VST3") {
            return {};
        }
        name = id.substring(pos + 1);
    } else {
        return {};
    }
    if ((pos = name.lastIndexOfChar('-')) > -1) {
        pluginId = name.substring(pos + 1);
        name = name.substring(0, pos);
    } else {
        return {};
    }
    if ((pos = name.lastIndexOfChar('-')) > -1) {
        fileHash = name.substring(pos + 1).toLowerCase();
        name = name.substring(0, pos);
    } else {
        return {};
    }

    for (auto c : fileHash) {
        // only hex chars allowed
        if (c < '0' || (c > '9' && c < 'a') || c > 'f') {
            return {};
        }
    }

    auto convertedId = format + "-" + name + "-" + pluginId;

    setLogTagStatic("processor");
    logln("sucessfully converted JUCE ID " << id << " to AG ID " << convertedId);

    return convertedId;
}

std::unique_ptr<PluginDescription> Processor::findPluginDescritpion(const String& id) {
    return findPluginDescritpion(id, getApp()->getPluginList());
}

std::unique_ptr<PluginDescription> Processor::findPluginDescritpion(const String& id, const KnownPluginList& pluglist) {
    std::unique_ptr<PluginDescription> plugdesc;
    setLogTagStatic("processor");
    traceScope();
    // the passed ID could be a JUCE ID, lets try to convert it to an AG ID
    auto convertedId = convertJUCEtoAGPluginID(id);
    for (auto& desc : pluglist.getTypes()) {
        auto descId = createPluginID(desc);
        auto descIdWithName = createPluginIDWithName(desc);
        auto descIdDepricated = createPluginIDDepricated(desc);
        if (descId == id || descIdWithName == id || descIdWithName == convertedId || descIdDepricated == id ||
            descIdDepricated == convertedId) {
            plugdesc = std::make_unique<PluginDescription>(desc);
        }
    }
    // fallback with filename
    if (nullptr == plugdesc) {
        plugdesc = pluglist.getTypeForFile(id);
    }
    return plugdesc;
}

std::shared_ptr<AudioPluginInstance> Processor::loadPlugin(const PluginDescription& plugdesc, double sampleRate,
                                                           int blockSize, String& err) {
    setLogTagStatic("processor");
    traceScope();
    String err2;
    AudioPluginFormatManager plugmgr;
    plugmgr.addDefaultFormats();
    std::shared_ptr<AudioPluginInstance> inst;
    runOnMsgThreadSync([&] {
        traceScope();
        inst = std::shared_ptr<AudioPluginInstance>(
            plugmgr.createPluginInstance(plugdesc, sampleRate, blockSize, err2).release(),
            [](AudioPluginInstance* p) { MessageManager::callAsync([p] { delete p; }); });
    });
    if (nullptr == inst) {
        err = "failed loading plugin ";
        err << plugdesc.fileOrIdentifier << ": " << err2;
        logln(err);
    }
    return inst;
}

std::shared_ptr<AudioPluginInstance> Processor::loadPlugin(const String& id, double sampleRate, int blockSize,
                                                           String& err) {
    setLogTagStatic("processor");
    traceScope();
    auto plugdesc = findPluginDescritpion(id);
    if (nullptr != plugdesc) {
        return loadPlugin(*plugdesc, sampleRate, blockSize, err);
    } else {
        err = "Plugin with ID " + id + " not found";
        logln(err);
    }
    return nullptr;
}

void Processor::setCallbacks(ParamValueChangeCallback valueChangeFn, ParamGestureChangeCallback gestureChangeFn,
                             KeysFromSandboxCallback keysFn, StatusChangeFromSandbox statusChangeFn) {
    onParamValueChange = valueChangeFn;
    onParamGestureChange = gestureChangeFn;
    onKeysFromSandbox = keysFn;
    onStatusChangeFromSandbox = statusChangeFn;

    if (auto client = getClient()) {
        client->onParamValueChange = [this](int paramIdx, float value) {
            onParamValueChange(m_chainIdx, paramIdx, value);
        };
        client->onParamGestureChange = [this](int paramIdx, bool value) {
            onParamGestureChange(m_chainIdx, paramIdx, value);
        };
        client->onKeysFromSandbox = [this](Message<Key>& msg) { onKeysFromSandbox(msg); };
        client->onStatusChange = [this](bool ok, const String& err) { onStatusChangeFromSandbox(m_chainIdx, ok, err); };
    }
}

bool Processor::load(const String& settings, String& err, const PluginDescription* plugdesc) {
    traceScope();

    traceln("m_isClient = " << (int)m_isClient);

    if (isLoaded()) {
        return false;
    }

    bool loaded = false;
    if (m_isClient) {
        std::shared_ptr<ProcessorClient> client;

        {
            client = std::make_shared<ProcessorClient>(m_id, m_chain.getConfig());
            std::lock_guard<std::mutex> lock(m_pluginMtx);
            m_client = client;
        }

        if (client->init()) {
            loaded = client->load(settings, err);
            if (loaded) {
                client->startThread();
            }
        } else {
            err = "failed to initialize sandbox";
            if (client->getError().isNotEmpty()) {
                err << ": " << client->getError();
            }
        }
    } else {
        std::shared_ptr<AudioPluginInstance> p;
        if (nullptr != plugdesc) {
            p = loadPlugin(*plugdesc, m_sampleRate, m_blockSize, err);
        } else {
            p = loadPlugin(m_id, m_sampleRate, m_blockSize, err);
        }
        if (nullptr != p) {
            {
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                m_plugin = p;
            }
            if (m_chain.initPluginInstance(this, err)) {
                loaded = true;
                for (auto* param : m_plugin->getParameters()) {
                    param->addListener(this);
                }
                if (settings.isNotEmpty()) {
                    MemoryBlock block;
                    block.fromBase64Encoding(settings);
                    runOnMsgThreadSync([&] { m_plugin->setStateInformation(block.getData(), (int)(block.getSize())); });
                }
            } else {
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                m_plugin.reset();
            }
        }
    }

    if (loaded) {
        loadedCount++;
    }

    return loaded;
}

void Processor::unload() {
    traceScope();

    if (!isLoaded()) {
        return;
    }

    if (m_isClient) {
        std::shared_ptr<ProcessorClient> client;
        {
            std::lock_guard<std::mutex> lock(m_pluginMtx);
            client = std::move(m_client);
        }
        client->unload();
        client->shutdown();
        client->waitForThreadToExit(-1);
        client.reset();
    } else {
        std::shared_ptr<AudioPluginInstance> plugin;
        {
            std::lock_guard<std::mutex> lock(m_pluginMtx);
            plugin = std::move(m_plugin);
        }
        if (m_prepared) {
            plugin->releaseResources();
        }
        for (auto* param : plugin->getParameters()) {
            param->removeListener(this);
        }
        if (auto* e = plugin->getActiveEditor()) {
            runOnMsgThreadAsync([this, e] {
                traceScope();
                delete e;
            });
        }
        plugin.reset();
    }

    loadedCount--;
}

bool Processor::isLoaded(std::shared_ptr<AudioPluginInstance>* plugin, std::shared_ptr<ProcessorClient>* client) {
    if (m_isClient) {
        if (auto c = getClient()) {
            if (client) {
                *client = c;
            }
            return c->isOk() && c->isLoaded();
        } else {
            return false;
        }
    } else {
        if (auto p = getPlugin()) {
            if (plugin) {
                *plugin = p;
            }
            return true;
        }
        return false;
    }
}

template <typename T>
bool Processor::processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();

    auto fn = [&](auto p) {
        TimeTrace::addTracePoint("proc_got_backend");
        if (!p->isSuspended()) {
            p->processBlock(buffer, midiMessages);
            TimeTrace::addTracePoint("proc_process");
        } else {
            if (m_lastKnownLatency > 0) {
                processBlockBypassed(buffer);
            }
        }
    };

    std::shared_ptr<AudioPluginInstance> plugin;
    std::shared_ptr<ProcessorClient> client;

    if (isLoaded(&plugin, &client)) {
        TimeTrace::addTracePoint("proc_loaded_ok");
        if (m_isClient) {
            fn(client);
        } else {
            fn(plugin);
        }
        return true;
    }

    TimeTrace::addTracePoint("proc_loaded_not_ok");

    return false;
}

bool Processor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    return processBlockInternal(buffer, midiMessages);
}

bool Processor::processBlock(AudioBuffer<double>& buffer, MidiBuffer& midiMessages) {
    return processBlockInternal(buffer, midiMessages);
}

template <typename T>
void Processor::processBlockBypassedInternal(AudioBuffer<T>& buffer, Array<Array<T>>& bypassBuffer) {
    auto totalNumInputChannels = m_chain.getTotalNumInputChannels();
    auto totalNumOutputChannels = m_chain.getTotalNumOutputChannels();

    if (totalNumInputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main input channels");
        totalNumInputChannels = buffer.getNumChannels();
    }
    if (totalNumOutputChannels > buffer.getNumChannels()) {
        logln("buffer has less channels than main output channels");
        totalNumOutputChannels = buffer.getNumChannels();
    }

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    if (bypassBuffer.size() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed, buffer: " << bypassBuffer.size()
                                                                      << ", needed: " << totalNumOutputChannels);
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    for (auto c = 0; c < totalNumOutputChannels; ++c) {
        auto& buf = bypassBuffer.getReference(c);
        for (auto s = 0; s < buffer.getNumSamples(); ++s) {
            buf.add(buffer.getSample(c, s));
            buffer.setSample(c, s, buf.getFirst());
            buf.remove(0);
        }
    }
}

void Processor::processBlockBypassed(AudioBuffer<float>& buffer) {
    processBlockBypassedInternal(buffer, m_bypassBufferF);
}

void Processor::processBlockBypassed(AudioBuffer<double>& buffer) {
    processBlockBypassedInternal(buffer, m_bypassBufferD);
}

void Processor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    traceScope();
    if (isLoaded()) {
        if (!m_isClient) {
            if (m_fmt == VST3) {
                runOnMsgThreadSync([&] { getPlugin()->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock); });
            } else {
                getPlugin()->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
            }
        }
        m_prepared = true;
    }
}

void Processor::releaseResources() {
    traceScope();
    if (isLoaded()) {
        if (!m_isClient) {
            getPlugin()->releaseResources();
        }
        m_prepared = false;
    }
}

void Processor::suspendProcessing(const bool shouldBeSuspended) {
    traceScope();
    if (isLoaded()) {
        if (shouldBeSuspended) {
            callBackendWithArgs1(suspendProcessing, true);
            if (!m_isClient) {
                getPlugin()->releaseResources();
            }
        } else {
            if (!m_isClient) {
                if (m_fmt == VST3) {
                    runOnMsgThreadSync(
                        [&] { getPlugin()->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize()); });
                } else {
                    getPlugin()->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize());
                }
            }
            callBackendWithArgs1(suspendProcessing, false);
        }
    }
}

void Processor::updateLatencyBuffers() {
    traceScope();
    logln("updating latency buffers for " << m_lastKnownLatency << " samples");
    int channels = getTotalNumOutputChannels();
    while (m_bypassBufferF.size() < channels) {
        Array<float> buf;
        for (int i = 0; i < m_lastKnownLatency; i++) {
            buf.add(0);
        }
        m_bypassBufferF.add(std::move(buf));
    }
    while (m_bypassBufferD.size() < channels) {
        Array<double> buf;
        for (int i = 0; i < m_lastKnownLatency; i++) {
            buf.add(0);
        }
        m_bypassBufferD.add(std::move(buf));
    }
    for (int c = 0; c < channels; c++) {
        auto& bufF = m_bypassBufferF.getReference(c);
        while (bufF.size() > m_lastKnownLatency) {
            bufF.remove(0);
        }
        while (bufF.size() < m_lastKnownLatency) {
            bufF.add(0);
        }
        auto& bufD = m_bypassBufferD.getReference(c);
        while (bufD.size() > m_lastKnownLatency) {
            bufD.remove(0);
        }
        while (bufD.size() < m_lastKnownLatency) {
            bufD.add(0);
        }
    }
}

void Processor::enableAllBuses() {
    traceScope();
    if (auto p = getPlugin()) {
        p->enableAllBuses();
    }
}

void Processor::setProcessingPrecision(AudioProcessor::ProcessingPrecision prec) {
    traceScope();
    if (auto p = getPlugin()) {
        p->setProcessingPrecision(prec);
    }
}

AudioProcessorEditor* Processor::createEditorIfNeeded() {
    traceScope();
    if (auto p = getPlugin()) {
        return p->createEditorIfNeeded();
    }
    return nullptr;
}

AudioProcessorEditor* Processor::getActiveEditor() {
    traceScope();
    if (auto p = getPlugin()) {
        return p->getActiveEditor();
    }
    return nullptr;
}

void Processor::showEditor(int x, int y) {
    traceScope();
    if (auto c = getClient()) {
        c->showEditor(x, y);
    }
}

void Processor::hideEditor() {
    traceScope();
    if (auto c = getClient()) {
        c->hideEditor();
    }
}

void Processor::updateScreenCaptureArea(int val) {
    traceScope();
    if (val == Defaults::SCAREA_FULLSCREEN) {
        m_fullscreen = !m_fullscreen;
    } else {
        m_additionalScreenSpace = m_additionalScreenSpace + val > 0 ? m_additionalScreenSpace + val : 0;
    }
}

int Processor::getAdditionalScreenCapturingSpace() {
    traceScope();
    return m_additionalScreenSpace;
}

bool Processor::isFullscreen() {
    traceScope();
    return m_fullscreen;
}

juce::Rectangle<int> Processor::getScreenBounds() {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getScreenBounds();
        } else {
            juce::Rectangle<int> ret;
            runOnMsgThreadSync([&] {
                if (auto* e = getPlugin()->getActiveEditor()) {
                    ret = e->getScreenBounds();
                }
            });
            return ret;
        }
    }
    return {};
}

int Processor::getLatencySamples() {
    traceScope();
    if (isLoaded()) {
        auto getLatency = [this] {
            callBackendWithReturn(getLatencySamples);
            return 0;
        };
        int latency = getLatency();
        if (latency != m_lastKnownLatency) {
            m_lastKnownLatency = latency;
            updateLatencyBuffers();
        }
        return latency;
    }
    return 0;
}

void Processor::setExtraChannels(int in, int out) {
    m_extraInChannels = in;
    m_extraOutChannels = out;
}

void Processor::parameterValueChanged(int parameterIndex, float newValue) {
    traceScope();
    if (onParamValueChange) {
        onParamValueChange(m_chainIdx, parameterIndex, newValue);
    }
}

void Processor::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {
    traceScope();
    if (onParamGestureChange) {
        onParamGestureChange(m_chainIdx, parameterIndex, gestureIsStarting);
    }
}

json Processor::getParameters() {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getParameters();
        } else {
            json jparams = json::array();
            runOnMsgThreadSync([this, &jparams] {
                auto plugin = getPlugin();
                for (auto* param : plugin->getParameters()) {
                    json jparam = {{"idx", param->getParameterIndex()},
                                   {"name", param->getName(32).toStdString()},
                                   {"defaultValue", param->getDefaultValue()},
                                   {"currentValue", param->getValue()},
                                   {"category", param->getCategory()},
                                   {"label", param->getLabel().toStdString()},
                                   {"numSteps", param->getNumSteps()},
                                   {"isBoolean", param->isBoolean()},
                                   {"isDiscrete", param->isDiscrete()},
                                   {"isMeta", param->isMetaParameter()},
                                   {"isOrientInv", param->isOrientationInverted()},
                                   {"minValue", param->getText(0.0f, 20).toStdString()},
                                   {"maxValue", param->getText(1.0f, 20).toStdString()}};
                    jparam["allValues"] = json::array();
                    for (auto& val : param->getAllValueStrings()) {
                        jparam["allValues"].push_back(val.toStdString());
                    }
                    if (jparam["allValues"].size() == 0 && param->isDiscrete() && param->getNumSteps() < 64) {
                        // try filling values manually
                        float step = 1.0f / (param->getNumSteps() - 1);
                        for (int i = 0; i < param->getNumSteps(); i++) {
                            auto val = param->getText(step * i, 32);
                            if (val.isEmpty()) {
                                break;
                            }
                            jparam["allValues"].push_back(val.toStdString());
                        }
                    }
                    jparams.push_back(jparam);
                }
            });
            return jparams;
        }
    }
    return {};
}

void Processor::setParameterValue(int paramIdx, float value) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setParameterValue(paramIdx, value);
        } else {
            if (auto* param = getPlugin()->getParameters()[paramIdx]) {
                param->setValue(value);
            }
        }
    }
}

float Processor::getParameterValue(int paramIdx) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getParameterValue(paramIdx);
        } else {
            for (auto& param : getPlugin()->getParameters()) {
                if (paramIdx == param->getParameterIndex()) {
                    return param->getValue();
                }
            }
        }
    }
    return 0.0f;
}

std::vector<Srv::ParameterValue> Processor::getAllParamaterValues() {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getAllParameterValues();
        } else {
            std::vector<Srv::ParameterValue> ret;
            for (auto* param : getPlugin()->getParameters()) {
                ret.push_back({param->getParameterIndex(), param->getValue()});
            }
            return ret;
        }
    }
    return {};
}

}  // namespace e47
