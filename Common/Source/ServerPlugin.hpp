/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ServerPlugin_hpp
#define ServerPlugin_hpp

#include <JuceHeader.h>

#include "Utils.hpp"

using json = nlohmann::json;

namespace e47 {

class ServerPlugin {
  public:
    ServerPlugin() noexcept {}

    ServerPlugin(const String& name, const String& company, const String& id, const String& idDeprecated,
                 const String& type, const String& category, bool isInstrument) noexcept
        : m_name(name),
          m_company(company),
          m_id(id),
          m_idDeprecated(idDeprecated),
          m_type(type),
          m_category(category),
          m_isInstrument(isInstrument) {
        if (m_id.isEmpty()) {
            m_id = m_idDeprecated;
        }
        if (m_category.isEmpty()) {
            m_category = "Unknown";
        }
    }

    ServerPlugin(const ServerPlugin& other) noexcept {
        m_name = other.m_name;
        m_company = other.m_company;
        m_id = other.m_id;
        m_idDeprecated = other.m_idDeprecated;
        m_type = other.m_type;
        m_category = other.m_category;
        m_isInstrument = other.m_isInstrument;
    }

    ServerPlugin& operator=(const ServerPlugin& other) noexcept {
        m_name = other.m_name;
        m_company = other.m_company;
        m_id = other.m_id;
        m_idDeprecated = other.m_idDeprecated;
        m_type = other.m_type;
        m_category = other.m_category;
        m_isInstrument = other.m_isInstrument;
        return *this;
    }

    friend bool operator==(const ServerPlugin& lhs, const ServerPlugin& rhs) {
        return lhs.m_name == rhs.m_name && lhs.m_company == rhs.m_company && lhs.m_id == rhs.m_id &&
               lhs.m_type == rhs.m_type && lhs.m_category == rhs.m_category && lhs.m_isInstrument == rhs.m_isInstrument;
    }

    const String& getName() const { return m_name; }
    const String& getCompany() const { return m_company; }
    const String& getId() const { return m_id; }
    const String& getIdDeprecated() const { return m_idDeprecated; }
    const String& getType() const { return m_type; }
    const String& getCategory() const { return m_category; }
    bool isInstrument() const { return m_isInstrument; }

    static ServerPlugin fromString(const String& s) {
        try {
            auto j = json::parse(s.toStdString());
            return ServerPlugin(jsonGetValue(j, "name", String()), jsonGetValue(j, "company", String()),
                                jsonGetValue(j, "id2", String()), jsonGetValue(j, "id", String()),
                                jsonGetValue(j, "type", String()), jsonGetValue(j, "category", String()),
                                jsonGetValue(j, "isInstrument", false));
        } catch (json::parse_error&) {
            auto parts = StringArray::fromTokens(s, ";", "");
            return ServerPlugin(parts[0], parts[1], parts[2], parts[2], parts[3], parts[4], false);
        }
    }

    String toString() const {
        json j;
        j["name"] = m_name.toStdString();
        j["company"] = m_company.toStdString();
        j["id"] = m_id.toStdString();
        j["idDeprecated"] = m_idDeprecated.toStdString();
        j["type"] = m_type.toStdString();
        j["category"] = m_category.toStdString();
        j["isInstrument"] = m_isInstrument;
        return j.dump();
    }

  private:
    String m_name;
    String m_company;
    String m_id;
    String m_idDeprecated;
    String m_type;
    String m_category;
    bool m_isInstrument;
};

struct MenuLevel {
    std::unique_ptr<std::map<String, ServerPlugin>> entryMap;
    std::unique_ptr<std::map<String, MenuLevel>> subMap;
};

}  // namespace e47

#endif /* ServerPlugin_hpp */
