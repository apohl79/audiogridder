/*
 * Copyright (c) 2021 SyncDNA Inc.
 *
 * Author: Andreas Pohl <andreas.pohl@syncdna.com>
 */

#include <JuceHeader.h>
#include <thread>
#include <signal.h>

#include "Utils.hpp"

#ifdef AG_UNIT_TEST_SERVER
#include "Server/ScanPluginsTest.hpp"
#include "Server/ProcessorChainTest.hpp"
#include "Server/SandboxPluginTest.hpp"
#include "Server/MultiMonoTest.hpp"
#endif

#ifdef AG_UNIT_TEST_PLUGIN_FX
#include "Plugin/AudioStreamerTest.hpp"
#endif

namespace e47 {

class ConsoleUnitTestRunner : public UnitTestRunner {
    void logMessage(const String& msg) override {
        setLogTagStatic("testrunner");
        logln(msg);
    }
};

class ConsoleApp : public JUCEApplicationBase {
  public:
    ConsoleApp() : testsThread(nullptr, "TestRunner") {}
    ~ConsoleApp() override {}

    void initialise(const String&) override {
        Logger::initialize();
        Tracer::initialize("Tests", "tests_");
        Tracer::setEnabled(true);

        int runs = 1;

        auto args = getCommandLineParameterArray();
        for (int i = 0; i < args.size(); i++) {
            if (args[i] == "-runs" && i + 1 < args.size()) {
                runs = args[i + 1].getIntValue();
                i++;
            }
        }

        testsThread.fn = [this, runs] {
            setLogTagStatic("testrunner");

            int tests = 0;
            int fails = 0;
            int remainingRuns = runs;

            do {
                ConsoleUnitTestRunner runner;
                runner.setAssertOnFailure(true);

                std::mutex mtx;
                std::condition_variable cv;

                FnThread timeoutThread(
                    [&] {
                        while (!Thread::currentThreadShouldExit()) {
                            std::unique_lock<std::mutex> lock(mtx);
                            if (cv.wait_for(lock, 5min) == std::cv_status::timeout) {
                                logln("test timeout");
                                raise(SIGABRT);
                            }
                        }
                    },
                    "TimeoutThread", true);

                auto resetTimeout = [&] {
                    std::lock_guard<std::mutex> lock(mtx);
                    cv.notify_one();
                };

                for (auto& test : UnitTest::getAllTests()) {
                    runner.runTests({test});
                    resetTimeout();
                    for (int i = 0; i < runner.getNumResults(); i++) {
                        tests++;
                        if (runner.getResult(i)->failures > 0) {
                            fails++;
                        }
                    }
                }

                timeoutThread.signalThreadShouldExit();
                resetTimeout();

                logln("Summary: " << (tests - fails) << " / " << tests << " test groups completed successfully");
            } while (--remainingRuns > 0 && fails == 0);

            if (fails > 0) {
                setApplicationReturnValue(1);
            }

            quit();
        };
        testsThread.startThread();
    }

    const String getApplicationName() override { return "sdna_test_runner"; }
    const String getApplicationVersion() override { return ""; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void anotherInstanceStarted(const String&) override {}

    void suspended() override {}
    void resumed() override {}
    void systemRequestedQuit() override {}
    void unhandledException(const std::exception*, const String&, int) override {}

    void shutdown() override {
        testsThread.stopThread(-1);
        Tracer::cleanup();
        Logger::cleanup();
    }

  private:
    FnThread testsThread;
};

}  // namespace e47

START_JUCE_APPLICATION(e47::ConsoleApp)
