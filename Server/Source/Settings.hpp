/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Settings_hpp
#define Settings_hpp

#include <unordered_map>

namespace e47 {

class Settings {
  public:
    Settings() {}
    ~Settings() {}

    class Param {
      public:
        enum Type : uint8_t { STRING, INT };

        Param(const String& s) : type(STRING), string(s) {}
        Param(const int i) : type(INT), integer(i) {}

        Type type;
        String string;
        int integer = 0;
    };

    void addParameter(const String& name, const String& val) { m_params.emplace_back(std::make_pair(name, val)); }
    void addParameter(const String& name, int val) { m_params.emplace_back(std::make_pair(name, val)); }

    template <typename T>
    void writeInt(MemoryBlock& dest, T val) {
        dest.append(&val, sizeof(T));
    }

    void writeString(MemoryBlock& dest, const String& s) {
        writeInt(dest, s.length());
        dest.append(s.getCharPointer(), s.length());
    }

    void serialize(MemoryBlock& dest) {
        writeInt(dest, m_params.size());
        for (auto& p : m_params) {
            writeString(dest, p.first);
            writeInt(dest, p.second.type);
            switch (p.second.type) {
                case Param::STRING:
                    writeString(dest, p.second.string);
                    break;
                case Param::INT:
                    writeInt(dest, p.second.integer);
                    break;
            }
        }
    }

    template <typename T>
    T readInt(const void* data, size_t* offset) {
        const char* p = static_cast<const char*>(data) + *offset;
        T val = *reinterpret_cast<const T*>(p);
        *offset += sizeof(T);
        return val;
    }

    String readString(const void* data, size_t* offset) {
        int len = readInt<int>(data, offset);
        const char* s = static_cast<const char*>(data) + *offset;
        *offset += len;
        return String(s, len);
    }

    void deserialize(const void* data, int size) {
        size_t offset = 0;
        size_t num = readInt<size_t>(data, &offset);
        for (size_t i = 0; i < num; i++) {
            String name = readString(data, &offset);
            Param::Type type = readInt<Param::Type>(data, &offset);
            switch (type) {
                case Param::STRING:
                    addParameter(name, readString(data, &offset));
                    break;
                case Param::INT:
                    addParameter(name, readInt<int>(data, &offset));
                    break;
            }
        }
    }

    void printAll() const {
        for (auto it = m_params.begin(); it != m_params.end(); it++) {
            std::cout << "Settings[" << it->first << "] = ";
            switch (it->second.type) {
                case Param::STRING:
                    std::cout << it->second.string << std::endl;
                    break;
                case Param::INT:
                    std::cout << it->second.integer << std::endl;
                    break;
            }
        }
    }

  private:
    std::vector<std::pair<String, Param>> m_params;
};

}  // namespace e47

#endif /* Settings_hpp */
