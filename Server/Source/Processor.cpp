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

#if JUCE_WINDOWS
#include <signal.h>
#endif

namespace e47 {

#define callBackendWithReturn(method)          \
    do {                                       \
        if (isLoaded()) {                      \
            if (m_isClient) {                  \
                return getClient()->method();  \
            } else {                           \
                return getPlugin(0)->method(); \
            }                                  \
        }                                      \
    } while (0)

#define callBackendWithArgs1Return(method, arg1)   \
    do {                                           \
        if (isLoaded()) {                          \
            if (m_isClient) {                      \
                return getClient()->method(arg1);  \
            } else {                               \
                return getPlugin(0)->method(arg1); \
            }                                      \
        }                                          \
    } while (0)

std::atomic_uint32_t Processor::loadedCount{0};

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

std::unique_ptr<PluginDescription> Processor::findPluginDescritpion(const String& id, String* idNormalized) {
    return findPluginDescritpion(id, getApp()->getPluginList(), idNormalized);
}

std::unique_ptr<PluginDescription> Processor::findPluginDescritpion(const String& id, const KnownPluginList& pluglist,
                                                                    String* idNormalized) {
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
            if (nullptr != idNormalized) {
                *idNormalized = descId;
            }
        }
    }
    // fallback with filename
    if (nullptr == plugdesc) {
        plugdesc = pluglist.getTypeForFile(id);
        if (nullptr != idNormalized) {
            *idNormalized = id;
        }
    }
    return plugdesc;
}

Array<AudioProcessor::BusesLayout> Processor::findSupportedLayouts(std::shared_ptr<AudioPluginInstance> proc,
                                                                   bool checkOnly, int srvId) {
    setLogTagStatic("processor");

    int busesIn = proc->getBusCount(true);
    int busesOut = proc->getBusCount(false);
    int channelsIn = busesIn > 0 ? Defaults::PLUGIN_FX_CHANNELS_IN + Defaults::PLUGIN_FX_CHANNELS_SC
                                 : Defaults::PLUGIN_INST_CHANNELS_IN;
    int channelsOut = busesIn > 0 ? Defaults::PLUGIN_FX_CHANNELS_OUT : Defaults::PLUGIN_INST_CHANNELS_OUT;

    logln("the processor has " << busesIn << " input and " << busesOut << " output buses");
    logln("testing with " << channelsIn << " input and " << channelsOut << " output channels");

    Array<AudioProcessor::BusesLayout> layouts;

    layouts.add(proc->getBusesLayout());

    auto addChannelSets = [](Array<AudioChannelSet>& channelSets, int numStereo, int numMono) {
        channelSets.clear();
        for (int ch = 0; ch < numStereo; ch++) {
            channelSets.add(AudioChannelSet::stereo());
        }
        for (int ch = 0; ch < numMono; ch++) {
            channelSets.add(AudioChannelSet::mono());
        }
    };

    auto addLayouts = [&](int chOut, int chInMax) {
        if (busesOut == 1 && chOut > 2) {
            // try layouts with one bus with the exact number of channels
            for (auto channelSet : AudioChannelSet::channelSetsWithNumberOfChannels(chOut)) {
                AudioProcessor::BusesLayout tmp;

                tmp.outputBuses.add(channelSet);

                if (busesIn == 0) {
                    // no inputs
                    layouts.addIfNotAlreadyThere(tmp);
                } else if (busesIn == 1) {
                    // matching inputs & outputs
                    tmp.inputBuses.add(channelSet);
                    layouts.addIfNotAlreadyThere(tmp);
                } else if (busesIn == 2) {
                    // stereo sidechain
                    tmp.inputBuses.add(AudioChannelSet::stereo());
                    layouts.addIfNotAlreadyThere(tmp);
                    tmp.inputBuses.remove(1);

                    // mono sidechain
                    tmp.inputBuses.add(AudioChannelSet::mono());
                    layouts.addIfNotAlreadyThere(tmp);
                }

                if (busesIn == 1) {
                    // try to add layouts with less inputs
                    for (int chIn = chInMax; chIn > 0; chIn--) {
                        if (chIn == chOut) {
                            continue;
                        }
                        for (auto channelSet2 : AudioChannelSet::channelSetsWithNumberOfChannels(chIn)) {
                            tmp.inputBuses.clear();
                            tmp.inputBuses.add(channelSet2);
                            layouts.addIfNotAlreadyThere(tmp);
                        }
                    }
                }
            }
        }

        // try layouts with different combinations of stereo and mono buses
        int numStereoOut = chOut / 2;

        while (numStereoOut >= 0) {
            AudioProcessor::BusesLayout tmp;

            int numMonoOut = chOut - numStereoOut * 2;

            if (busesOut == numStereoOut + numMonoOut) {
                addChannelSets(tmp.outputBuses, numStereoOut, numMonoOut);

                if (busesIn == 0) {
                    // no inputs
                    layouts.addIfNotAlreadyThere(tmp);
                }

                for (int chIn = chInMax; chIn > 0; chIn--) {
                    int numStereoIn = chIn / 2;

                    while (numStereoIn >= 0) {
                        int numMonoIn = chIn - numStereoIn * 2;

                        if (busesIn == numStereoIn + numMonoIn) {
                            addChannelSets(tmp.inputBuses, numStereoIn, numMonoIn);
                            layouts.addIfNotAlreadyThere(tmp);
                        }

                        numStereoIn--;
                    }
                }
            }

            numStereoOut--;
        }
    };

    Array<AudioProcessor::BusesLayout> ret;

    for (int channelsOutWorking = channelsOut; channelsOutWorking > 0; channelsOutWorking--) {
        addLayouts(channelsOutWorking, channelsIn);
    }

    logln("trying " << layouts.size() << " layouts (checkOnly=" << (int)checkOnly << ")...");

    // checkBusesLayoutSupported() returns false in many cases where a layout still works, so we prefer
    // setBusesLayout(). This - on the other hand - might "hang" for some apple AUs for some specific layouts, so we
    // give it 2 seconds and kill the process, as this should only be called with checkOnly=false from the
    // scanner. If we kill the process, we will launch the scanner again and pass checkOnly=true the second time.
    std::unique_ptr<FnThread> timeoutThread;
    std::atomic_bool timeoutActive{false};

    if (!checkOnly) {
        timeoutThread = std::make_unique<FnThread>(
            [&] {
                while (!Thread::currentThreadShouldExit()) {
                    if (timeoutActive) {
                        sleepExitAwareWithCondition(2000, [&] { return !timeoutActive.load(); });
                        if (timeoutActive) {
                            raise(SIGABRT);
                        }
                    }
                    sleepExitAware(50);
                }
            },
            "TimeoutThread", true);
    }

    // Create an error file that we remove after testing, if we fail, the scanner will trigger a second run
    auto errFile = File(Defaults::getConfigFileName(Defaults::ScanLayoutError, {{"id", String(srvId)}}));
    errFile.create();

    for (auto& l : layouts) {
        if (!checkOnly) {
            timeoutActive = true;
        }

        bool supported = checkOnly ? proc->checkBusesLayoutSupported(l) : proc->setBusesLayout(l);

        if (!checkOnly) {
            timeoutActive = false;
        }

        if (supported) {
            logln("  " << describeLayout(l) << ": OK");
        }

        if (supported) {
            ret.add(l);
        }
    }

    errFile.deleteFile();

    return ret;
}

Array<AudioProcessor::BusesLayout> Processor::findSupportedLayouts(Processor* proc, bool checkOnly, int srvId) {
    return findSupportedLayouts(proc->getPlugin(0), checkOnly, srvId);
}

const Array<AudioProcessor::BusesLayout>& Processor::getSupportedBusLayouts() const {
#ifndef AG_UNIT_TESTS
    return getApp()->getServer()->getPluginLayouts(m_idNormalized);
#else
    static Array<AudioProcessor::BusesLayout> ret;
    return ret;
#endif
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
                                                           String& err, String* idNormalized) {
    setLogTagStatic("processor");
    traceScope();
    auto plugdesc = findPluginDescritpion(id, idNormalized);
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
        client->onParamValueChange = [this](int channel, int paramIdx, float value) {
            onParamValueChange(m_chainIdx, channel, paramIdx, value);
        };
        client->onParamGestureChange = [this](int channel, int paramIdx, bool value) {
            onParamGestureChange(m_chainIdx, channel, paramIdx, value);
        };
        client->onKeysFromSandbox = [this](Message<Key>& msg) { onKeysFromSandbox(msg); };
        client->onStatusChange = [this](bool ok, const String& err) { onStatusChangeFromSandbox(m_chainIdx, ok, err); };
    }
}

bool Processor::load(const String& settings, const String& layout, uint64 monoChannels, String& err,
                     const PluginDescription* plugdesc) {
    traceScope();

    traceln("m_isClient = " << (int)m_isClient);

    if (isLoaded()) {
        return false;
    }

    bool loaded = false;

    if (m_isClient) {
#ifndef AG_UNIT_TESTS
        bool found = nullptr != findPluginDescritpion(m_id, &m_idNormalized);
#else
        bool found = true;
        m_idNormalized = m_id;
#endif
        if (found) {
            std::shared_ptr<ProcessorClient> client;

            {
                client = std::make_shared<ProcessorClient>(m_idNormalized, m_chain.getConfig());
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                m_client = client;
            }

            if (client->init()) {
                loaded = client->load(settings, layout, monoChannels, err);
                if (loaded) {
                    client->startThread();
                    loadedCount++;
                    m_windows.resize(1);
                }
            } else {
                err = "failed to initialize sandbox";
                if (client->getError().isNotEmpty()) {
                    err << ": " << client->getError();
                }
            }
        } else {
            err = "Plugin with ID " + m_id + " not found";
        }
    } else {
        StringArray settingsByChannel;

        if (layout == "Multi-Mono") {
            m_channels = m_chain.getTotalNumOutputChannels();
            m_monoChannels.setNumChannels(0, m_chain.getTotalNumOutputChannels(), 0);
            if (monoChannels > 0) {
                m_monoChannels = monoChannels;
            } else {
                m_monoChannels.setOutputRangeActive();
            }
            settingsByChannel = StringArray::fromTokens(settings, "|", "");
            jassert(settingsByChannel.size() == m_channels);

            logln("creating " << m_channels << " plugin instances for multi-mono layout");
        }

        m_plugins.resize((size_t)m_channels);
        m_windows.resize((size_t)m_channels);
        m_listners.resize((size_t)m_channels);
        m_multiMonoBypassBuffersF.resize((size_t)m_channels);
        m_multiMonoBypassBuffersD.resize((size_t)m_channels);

        std::shared_ptr<AudioPluginInstance> p;

        for (size_t ch = 0; ch < (size_t)m_channels; ch++) {
            if (nullptr != plugdesc) {
                p = loadPlugin(*plugdesc, m_sampleRate, m_blockSize, err);
            } else {
                p = loadPlugin(m_id, m_sampleRate, m_blockSize, err, &m_idNormalized);
            }
            if (nullptr != p) {
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                m_plugins[ch] = p;
                m_listners[ch] = std::make_unique<Listener>(this, ch);
                m_multiMonoBypassBuffersF[ch] = std::make_unique<AudioRingBuffer<float>>();
                m_multiMonoBypassBuffersD[ch] = std::make_unique<AudioRingBuffer<double>>();
            }
        }

        if (m_chain.initPluginInstance(this, m_channels > 1 ? "Mono" : layout, err)) {
            loaded = true;
            loadedCount++;

            for (size_t ch = 0; ch < (size_t)m_channels; ch++) {
                for (auto* param : m_plugins[ch]->getParameters()) {
                    param->addListener(m_listners[ch].get());
                }
                if (settings.isNotEmpty()) {
                    MemoryBlock block;
                    block.fromBase64Encoding(m_channels > 1 ? settingsByChannel.getReference((int)ch) : settings);
                    runOnMsgThreadSync(
                        [&] { m_plugins[ch]->setStateInformation(block.getData(), (int)(block.getSize())); });
                }
            }
        } else {
            std::lock_guard<std::mutex> lock(m_pluginMtx);
            m_plugins.clear();
            loaded = false;
        }
    }

    if (loaded) {
        m_layout = layout;
    }

    return loaded;
}

void Processor::unload() {
    traceScope();

    if (!isLoaded()) {
        return;
    }

    for (auto& w : m_windows) {
        if (nullptr != w) {
            runOnMsgThreadSync([&] { w.reset(); });
        }
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
        loadedCount--;
    } else {
        std::shared_ptr<AudioPluginInstance> plugin;
        std::unique_ptr<Listener> listener;
        for (size_t ch = 0; ch < (size_t)m_channels; ch++) {
            {
                std::lock_guard<std::mutex> lock(m_pluginMtx);
                plugin = std::move(m_plugins[ch]);
                listener = std::move(m_listners[ch]);
            }
            if (m_prepared) {
                plugin->releaseResources();
            }
            for (auto* param : plugin->getParameters()) {
                param->removeListener(listener.get());
            }
            plugin.reset();
            listener.reset();
            loadedCount--;
        }
        m_channels = 1;
    }
}

bool Processor::isLoaded() {
    if (m_isClient) {
        if (auto c = getClient()) {
            return c->isOk() && c->isLoaded();
        } else {
            return false;
        }
    } else {
        for (int ch = 0; ch < m_channels; ch++) {
            auto p = getPlugin(ch);
            if (p == nullptr) {
                return false;
            }
        }
        return true;
    }
}

template <typename T>
bool Processor::processBlockInternal(AudioBuffer<T>& buffer, MidiBuffer& midiMessages) {
    traceScope();

    traceln("  processor: isClient=" << (int)m_isClient << ", multiMono=" << (int)(m_channels > 1)
                                     << ", latency=" << m_lastKnownLatency);
    traceln("  buffer: channels=" << buffer.getNumChannels() << ", samples=" << buffer.getNumSamples());

    auto fn = [&](auto p, int ch, bool isPlugin) {
        TimeTrace::addTracePoint("proc_got_backend");
        traceln("  processing ch " << ch << ": suspended=" << (int)p->isSuspended());
        if (!p->isSuspended()) {
            if (isPlugin && m_channels > 1) {
                // multi-mono
                AudioBuffer<T> chBuffer(buffer.getArrayOfWritePointers() + ch, 1, buffer.getNumSamples());
                p->processBlock(chBuffer, midiMessages);
            } else {
                p->processBlock(buffer, midiMessages);
            }
            TimeTrace::addTracePoint("proc_process_" + String(ch));
        } else {
            if (m_lastKnownLatency > 0) {
                if (isPlugin && m_channels > 1) {
                    // multi-mono
                    AudioBuffer<T> chBuffer(buffer.getArrayOfWritePointers() + ch, 1, buffer.getNumSamples());
                    processBlockBypassedMultiMono(chBuffer, ch);
                } else {
                    processBlockBypassed(buffer);
                }
            }
        }
    };

    if (isLoaded()) {
        TimeTrace::addTracePoint("proc_loaded_ok");
        if (m_isClient) {
            fn(getClient(), 0, false);
        } else {
            for (int ch = 0; ch < m_channels; ch++) {
                fn(getPlugin(ch), ch, true);
            }
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
void Processor::processBlockBypassedInternal(AudioBuffer<T>& buffer, AudioRingBuffer<T>& bypassBuffer) {
    traceScope();

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

    if (bypassBuffer.getNumChannels() < totalNumOutputChannels) {
        logln("bypass buffer has less channels than needed, buffer: " << bypassBuffer.getNumChannels()
                                                                      << ", needed: " << totalNumOutputChannels);
        for (auto i = 0; i < totalNumOutputChannels; ++i) {
            buffer.clear(i, 0, buffer.getNumSamples());
        }
        return;
    }

    bypassBuffer.process(buffer.getArrayOfWritePointers(), buffer.getNumSamples());
}

void Processor::processBlockBypassed(AudioBuffer<float>& buffer) {
    processBlockBypassedInternal(buffer, m_bypassBufferF);
}

void Processor::processBlockBypassed(AudioBuffer<double>& buffer) {
    processBlockBypassedInternal(buffer, m_bypassBufferD);
}

template <typename T>
void Processor::processBlockBypassedMultiMonoInternal(AudioBuffer<T>& buffer, AudioRingBuffer<T>& bypassBuffer) {
    traceScope();
    traceln("  buffer: channels=" << buffer.getNumChannels() << ", samples=" << buffer.getNumSamples());
    traceln("  bypass buffer: channels=" << bypassBuffer.getNumChannels()
                                         << ", samples=" << bypassBuffer.getNumSamples());
    bypassBuffer.process(buffer.getArrayOfWritePointers(), buffer.getNumSamples());
}

void Processor::processBlockBypassedMultiMono(AudioBuffer<float>& buffer, int ch) {
    processBlockBypassedMultiMonoInternal(buffer, *m_multiMonoBypassBuffersF[(size_t)ch]);
}

void Processor::processBlockBypassedMultiMono(AudioBuffer<double>& buffer, int ch) {
    processBlockBypassedMultiMonoInternal(buffer, *m_multiMonoBypassBuffersD[(size_t)ch]);
}

void Processor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    traceScope();
    if (isLoaded()) {
        if (!m_isClient) {
            ChannelSet cs;
            {
                std::lock_guard<std::mutex> lock(m_monoChannelsMtx);
                cs = m_monoChannels;
            }
            for (int ch = 0; ch < m_channels; ch++) {
                if (m_channels == 1 || cs.isOutputActive(ch)) {
                    if (m_fmt == VST3) {
                        runOnMsgThreadSync(
                            [&] { getPlugin(ch)->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock); });
                    } else {
                        getPlugin(ch)->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
                    }
                } else {
                    getPlugin(ch)->suspendProcessing(true);
                }
            }
        }
        m_prepared = true;
    }
}

void Processor::releaseResources() {
    traceScope();
    if (isLoaded()) {
        if (!m_isClient) {
            for (int ch = 0; ch < m_channels; ch++) {
                getPlugin(ch)->releaseResources();
            }
        }
        m_prepared = false;
    }
}

void Processor::suspendProcessing(const bool shouldBeSuspended) {
    traceScope();
    if (isLoaded()) {
        if (shouldBeSuspended) {
            if (m_isClient) {
                getClient()->suspendProcessing(true);
            } else {
                for (int ch = 0; ch < m_channels; ch++) {
                    auto plugin = getPlugin(ch);
                    if (!plugin->isSuspended()) {
                        plugin->suspendProcessing(true);
                        plugin->releaseResources();
                    }
                }
            }
        } else {
            if (m_isClient) {
                getClient()->suspendProcessing(false);
            } else {
                for (int ch = 0; ch < m_channels; ch++) {
                    if (m_channels > 1 && m_monoChannels.isOutputActive(ch)) {
                        auto plugin = getPlugin(ch);
                        if (m_fmt == VST3) {
                            runOnMsgThreadSync(
                                [&] { plugin->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize()); });
                        } else {
                            plugin->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize());
                        }
                        plugin->suspendProcessing(false);
                    }
                }
            }
        }
    }
}

void Processor::updateLatencyBuffers() {
    traceScope();
    int channels = getTotalNumOutputChannels();
    logln("updating latency buffers of " << getName() << " to " << m_lastKnownLatency << " samples and " << channels
                                         << " channels");
    m_bypassBufferF.resize(channels, m_lastKnownLatency * 2);
    m_bypassBufferF.clear();
    m_bypassBufferF.setReadOffset(m_lastKnownLatency);
    m_bypassBufferD.resize(channels, m_lastKnownLatency * 2);
    m_bypassBufferD.clear();
    m_bypassBufferD.setReadOffset(m_lastKnownLatency);

    if (m_channels > 1) {
        for (int ch = 0; ch < m_channels; ch++) {
            if (auto& b = m_multiMonoBypassBuffersF[(size_t)ch]) {
                b->resize(1, m_lastKnownLatency * 2);
                b->clear();
                b->setReadOffset(m_lastKnownLatency);
            }
            if (auto& b = m_multiMonoBypassBuffersD[(size_t)ch]) {
                b->resize(1, m_lastKnownLatency * 2);
                b->clear();
                b->setReadOffset(m_lastKnownLatency);
            }
        }
    }
}

void Processor::enableAllBuses() {
    traceScope();
    if (isLoaded()) {
        for (int ch = 0; ch < m_channels; ch++) {
            getPlugin(ch)->enableAllBuses();
        }
    }
}

void Processor::setMonoChannels(uint64 channels) {
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setMonoChannels(channels);
        } else {
            bool changed = false;
            {
                std::lock_guard<std::mutex> lock(m_monoChannelsMtx);
                if (m_monoChannels.toInt() != channels) {
                    m_monoChannels = channels;
                    changed = true;
                }
            }
            if (changed) {
                ChannelSet cs(channels, 0, m_channels);
                logln("setting mono channels to: " << cs.toString());
                for (int ch = 0; ch < m_channels; ch++) {
                    if (auto plugin = getPlugin(ch)) {
                        if (cs.isOutputActive(ch)) {
                            if (plugin->isSuspended()) {
                                if (m_fmt == VST3) {
                                    runOnMsgThreadSync([&] {
                                        plugin->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize());
                                    });
                                } else {
                                    plugin->prepareToPlay(m_chain.getSampleRate(), m_chain.getBlockSize());
                                }
                                plugin->suspendProcessing(false);
                            }
                        } else {
                            if (!plugin->isSuspended()) {
                                plugin->suspendProcessing(true);
                                plugin->releaseResources();
                            }
                        }
                    }
                }
            }
        }
    }
}

bool Processor::isMonoChannelActive(int ch) {
    std::lock_guard<std::mutex> lock(m_monoChannelsMtx);
    return m_monoChannels.isOutputActive(ch);
}

void Processor::setProcessingPrecision(AudioProcessor::ProcessingPrecision prec) {
    traceScope();
    if (isLoaded()) {
        for (int ch = 0; ch < m_channels; ch++) {
            getPlugin(ch)->setProcessingPrecision(prec);
        }
    }
}

AudioProcessorEditor* Processor::createEditorIfNeeded() {
    traceScope();
    if (auto p = getPlugin(m_activeWindowChannel)) {
        return p->createEditorIfNeeded();
    }
    return nullptr;
}

AudioProcessorEditor* Processor::getActiveEditor() {
    traceScope();
    if (auto p = getPlugin(m_activeWindowChannel)) {
        return p->getActiveEditor();
    }
    return nullptr;
}

std::shared_ptr<ProcessorWindow> Processor::getOrCreateEditorWindow(Thread::ThreadID tid,
                                                                    ProcessorWindow::CaptureCallbackFFmpeg func,
                                                                    std::function<void()> onHide, int x, int y) {
    return getOrCreateEditorWindowInternal(tid, func, onHide, x, y);
}

std::shared_ptr<ProcessorWindow> Processor::getOrCreateEditorWindow(Thread::ThreadID tid,
                                                                    ProcessorWindow::CaptureCallbackNative func,
                                                                    std::function<void()> onHide, int x, int y) {
    return getOrCreateEditorWindowInternal(tid, func, onHide, x, y);
}

template <typename T>
std::shared_ptr<ProcessorWindow> Processor::getOrCreateEditorWindowInternal(Thread::ThreadID tid, T func,
                                                                            std::function<void()> onHide, int x,
                                                                            int y) {
    if (auto& w = m_windows[getWindowIndex()]) {
        return w;
    }

    auto w = std::make_shared<ProcessorWindow>(shared_from_this(), tid, func, onHide, x, y);
    m_windows[getWindowIndex()] = w;

    return w;
}

std::shared_ptr<ProcessorWindow> Processor::recreateEditorWindow() {
    if (auto& w = m_windows[getWindowIndex()]) {
        auto pos = w->getPosition();
        auto tid = w->getTid();
        auto onHide = w->getOnHide();
        if (auto func = w->getCaptureCallbackFFmpeg()) {
            w.reset();
            return getOrCreateEditorWindowInternal(tid, func, onHide, pos.x, pos.y);
        }
        if (auto func = w->getCaptureCallbackNative()) {
            w.reset();
            return getOrCreateEditorWindowInternal(tid, func, onHide, pos.x, pos.y);
        }
    }

    logln("error: can't recreate editor as no window exists");

    return nullptr;
}

void Processor::showEditor(int x, int y) {
    traceScope();
    if (auto c = getClient()) {
        c->showEditor(m_activeWindowChannel, x, y);
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
                if (auto* e = getPlugin(m_activeWindowChannel)->getActiveEditor()) {
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

json Processor::getParameters() {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getParameters();
        } else {
            json jparams = json::array();
            runOnMsgThreadSync([this, &jparams] {
                // just loading the parameters of the first instance, as all instances have the same params
                auto plugin = getPlugin(0);
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

void Processor::setParameterValue(int channel, int paramIdx, float value) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setParameterValue(channel, paramIdx, value);
        } else {
            if (auto plugin = getPlugin(channel)) {
                if (auto* param = plugin->getParameters()[paramIdx]) {
                    param->setValue(value);
                }
            } else {
                logln("error in setParameterValue: no plugin for channel " << channel);
            }
        }
    }
}

float Processor::getParameterValue(int channel, int paramIdx) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getParameterValue(channel, paramIdx);
        } else {
            if (auto plugin = getPlugin(channel)) {
                for (auto& param : plugin->getParameters()) {
                    if (paramIdx == param->getParameterIndex()) {
                        return param->getValue();
                    }
                }
            } else {
                logln("error in getParameterValue: no plugin for channel " << channel);
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
            for (int ch = 0; ch < m_channels; ch++) {
                for (auto* param : getPlugin(ch)->getParameters()) {
                    ret.push_back({param->getParameterIndex(), param->getValue(), ch});
                }
            }
            return ret;
        }
    }
    return {};
}

const String Processor::getName() {
    callBackendWithReturn(getName);
    return {};
}

bool Processor::hasEditor() {
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->hasEditor();
        } else {
            bool ret;
            runOnMsgThreadSync([&] { ret = getPlugin(0)->hasEditor(); });
            return ret;
        }
    }
    return false;
}

bool Processor::supportsDoublePrecisionProcessing() {
    callBackendWithReturn(supportsDoublePrecisionProcessing);
    return false;
}

bool Processor::isSuspended() {
    callBackendWithReturn(isSuspended);
    return true;
}

double Processor::getTailLengthSeconds() {
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getTailLengthSeconds();
        } else {
            double ret = 0.0;
            for (int ch = 0; ch < m_channels; ch++) {
                ret = jmax(ret, getPlugin(ch)->getTailLengthSeconds());
            }
            return ret;
        }
    }
    return 0.0;
}

void Processor::getStateInformation(String& settings) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->getStateInformation(settings);
        } else {
            StringArray saSettings;
            runOnMsgThreadSync([&] {
                for (int ch = 0; ch < m_channels; ch++) {
                    MemoryBlock block;
                    getPlugin(ch)->getStateInformation(block);
                    saSettings.add(block.toBase64Encoding());
                }
            });
            settings = saSettings.joinIntoString("|");
        }
    }
}

void Processor::setStateInformation(const String& settings) {
    traceScope();
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setStateInformation(settings);
        } else {
            auto saSettings = StringArray::fromTokens(settings, "|", "");
            jassert(saSettings.size() == m_channels);
            std::vector<MemoryBlock> blocks((size_t)m_channels);
            for (int ch = 0; ch < m_channels; ch++) {
                blocks[(size_t)ch].fromBase64Encoding(saSettings.getReference(ch));
            }
            runOnMsgThreadSync([&] {
                for (int ch = 0; ch < m_channels; ch++) {
                    getPlugin(ch)->setStateInformation(blocks[(size_t)ch].getData(), (int)blocks[(size_t)ch].getSize());
                }
            });
        }
    }
}

bool Processor::checkBusesLayoutSupported(const AudioProcessor::BusesLayout& layout) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->checkBusesLayoutSupported(layout);
        }
        return true;
    }
    return false;
}

bool Processor::setBusesLayout(const AudioProcessor::BusesLayout& layout) {
    if (isLoaded()) {
        if (!m_isClient) {
            for (int ch = 0; ch < m_channels; ch++) {
                if (!getPlugin(ch)->setBusesLayout(layout)) {
                    return false;
                }
            }
        }
        return true;
    }
    return false;
}

AudioProcessor::BusesLayout Processor::getBusesLayout() {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->getBusesLayout();
        }
    }
    return {};
}

int Processor::getBusCount(bool isInput) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->getBusCount(isInput);
        }
    }
    return 0;
}

bool Processor::canAddBus(bool isInput) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->canAddBus(isInput);
        }
    }
    return false;
}

bool Processor::canRemoveBus(bool isInput) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->canRemoveBus(isInput);
        }
    }
    return false;
}

bool Processor::addBus(bool isInput) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->addBus(isInput);
        }
    }
    return false;
}

bool Processor::removeBus(bool isInput) {
    if (isLoaded()) {
        if (!m_isClient) {
            return getPlugin(0)->removeBus(isInput);
        }
    }
    return false;
}

void Processor::setPlayHead(AudioPlayHead* phead) {
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setPlayHead(phead);
        } else {
            for (int ch = 0; ch < m_channels; ch++) {
                getPlugin(ch)->setPlayHead(phead);
            }
        }
    }
}

int Processor::getNumPrograms() {
    callBackendWithReturn(getNumPrograms);
    return 1;
}

const String Processor::getProgramName(int idx) {
    callBackendWithArgs1Return(getProgramName, idx);
    return {};
}

void Processor::setCurrentProgram(int idx, int channel) {
    if (isLoaded()) {
        if (m_isClient) {
            getClient()->setCurrentProgram(idx);
        } else {
            if (auto plugin = getPlugin(channel)) {
                plugin->setCurrentProgram(idx);
            } else {
                logln("error in setCurrentProgram: no plugin for channel " << channel);
            }
        }
    }
}

int Processor::getTotalNumOutputChannels() {
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getTotalNumOutputChannels();
        } else {
            if (m_channels > 1) {
                return m_channels;
            } else {
                return getPlugin(0)->getTotalNumOutputChannels();
            }
        }
    }
    return 0;
}

int Processor::getChannelInstances() {
    if (isLoaded()) {
        if (m_isClient) {
            return getClient()->getChannelInstances();
        } else {
            return m_channels;
            ;
        }
    }
    return 0;
}

}  // namespace e47
