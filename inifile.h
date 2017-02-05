/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef INICONF_H_INCLUDED
#define INICONF_H_INCLUDED

#include <string>
#include <vector>
#include <sstream>

inline bool parse_bool(const std::string &txt, bool def)
{
    if (txt.empty())
        return def;
    return txt == "Y" || txt == "y" || txt == "1";
}

inline int parse_int(const std::string &txt, int def)
{
    if (txt.empty())
        return def;
    std::istringstream ifs(txt);
    ifs >> def;
    return def;
}

inline int parse_hex_int(const std::string &txt, int def)
{
    if (txt.empty())
        return def;
    std::istringstream ifs(txt);
    ifs >> std::hex >> def;
    return def;
}

class IniEntry
{
friend class IniFile;
friend class IniSection;
public:
    const std::string name() const
    { return m_name; }
    const std::string value() const
    { return m_value; }
private:
    std::string m_name;
    std::string m_value;
};

class IniSection
{
friend class IniFile;
public:
    const std::string name() const
    { return m_name; }

    std::string find_single_value(const std::string &name) const;
    std::vector<std::string> find_multi_value(const std::string &name) const;

    typedef std::vector<IniEntry>::const_iterator entry_iterator;
    entry_iterator begin() const
    { return m_entries.begin(); }
    entry_iterator end() const
    { return m_entries.end(); }
private:
    std::string m_name;
    std::vector<IniEntry> m_entries;

    void add_line(const std::string &line);
};

class IniFile
{
public:
    IniFile(const std::string &fileName);
    void Dump(std::ostream &os);

    const IniSection *find_single_section(const std::string &name) const;
    std::vector<const IniSection*> find_multi_section(const std::string &name) const;

    std::vector<IniSection>::const_iterator begin() const
    { return m_sections.begin(); }
    std::vector<IniSection>::const_iterator end() const
    { return m_sections.end(); }
private:
    std::vector<IniSection> m_sections;
    void load(std::istream &is);
};

#endif /* INICONF_H_INCLUDED */
