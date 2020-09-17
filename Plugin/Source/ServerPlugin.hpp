/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ServerPlugin_hpp
#define ServerPlugin_hpp

#include <JuceHeader.h>

class ServerPlugin {
  public:
    ServerPlugin() noexcept {}

    ServerPlugin(const String& name, const String& company, const String& id, const String& type,
                 const String& category) noexcept
        : m_name(name), m_company(company), m_id(id), m_type(type), m_category(category) {
        if (m_category.isEmpty()) {
            m_category = "Unknown";
        }
    }

    ServerPlugin(const ServerPlugin& other) noexcept {
        m_name = other.m_name;
        m_company = other.m_company;
        m_id = other.m_id;
        m_type = other.m_type;
        m_category = other.m_category;
    }

    ServerPlugin& operator=(const ServerPlugin& other) noexcept {
        m_name = other.m_name;
        m_company = other.m_company;
        m_id = other.m_id;
        m_type = other.m_type;
        m_category = other.m_category;
        return *this;
    }

    const String& getName() const { return m_name; }
    const String& getCompany() const { return m_company; }
    const String& getId() const { return m_id; }
    const String& getType() const { return m_type; }
    const String& getCategory() const { return m_category; }

    static ServerPlugin fromString(const String& s) {
        auto parts = StringArray::fromTokens(s, ";", "");
        return ServerPlugin(parts[0], parts[1], parts[2], parts[3], parts[4]);
    }

  private:
    String m_name;
    String m_company;
    String m_id;
    String m_type;
    String m_category;
};

#endif /* ServerPlugin_hpp */
