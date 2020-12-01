/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef SharedInstance_hpp
#define SharedInstance_hpp

#include <thread>
#include <memory>

namespace e47 {

template <typename T>
class SharedInstance {
  public:
    virtual ~SharedInstance() {}

    static void initialize(std::function<void(std::shared_ptr<T>)> onInit = nullptr) {
        std::lock_guard<std::mutex> lock(m_instMtx);
        if (nullptr == m_inst) {
            m_inst = std::make_shared<T>();
            if (nullptr != onInit) {
                onInit(m_inst);
            }
        }
        m_instRefCount++;
    }

    static void cleanup(std::function<void(std::shared_ptr<T>)> onCleanup = nullptr) {
        std::lock_guard<std::mutex> lock(m_instMtx);
        m_instRefCount--;
        if (m_instRefCount == 0) {
            if (nullptr != onCleanup) {
                onCleanup(m_inst);
            }
            m_inst.reset();
        }
    }

    static std::shared_ptr<T> getInstance() {
        std::lock_guard<std::mutex> lock(m_instMtx);
        return m_inst;
    }

    static size_t getRefCount() {
        std::lock_guard<std::mutex> lock(m_instMtx);
        return m_instRefCount;
    }

  protected:
    static std::mutex& getInstanceMtx() { return m_instMtx; }

  private:
    static std::shared_ptr<T> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;
};

template <typename T>
std::shared_ptr<T> SharedInstance<T>::m_inst;

template <typename T>
std::mutex SharedInstance<T>::m_instMtx;

template <typename T>
size_t SharedInstance<T>::m_instRefCount = 0;

}  // namespace e47

#endif /* SharedInstance_hpp */
