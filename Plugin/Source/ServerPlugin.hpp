/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ServerPlugin_hpp
#define ServerPlugin_hpp

#include "../JuceLibraryCode/JuceHeader.h"

class ServerPlugin {
  public:
    ServerPlugin(const String& name, const String& company, const String& id, const String& type)
        : m_name(name), m_company(company), m_id(id), m_type(type) {}

    const String& getName() const { return m_name; }
    const String& getCompany() const { return m_company; }
    const String& getId() const { return m_id; }
    const String& getType() const { return m_type; }

    static ServerPlugin fromString(const String& s) {
        auto parts = StringArray::fromTokens(s, "|", "");
        return ServerPlugin(parts[0], parts[1], parts[2], parts[3]);
    }

  private:
    String m_name;
    String m_company;
    String m_id;
    String m_type;
};

#endif /* ServerPlugin_hpp */
