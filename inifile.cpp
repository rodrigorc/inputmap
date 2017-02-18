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

#include <cctype>
#include <algorithm>
#include <fstream>
#include "inifile.h"


std::string trim(const std::string &s)
{
   auto wsfront=std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
   auto wsback=std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
   return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}

void IniFile::Dump(std::ostream &os)
{
    for (auto &s : *this)
    {
        os << "[" << s.m_name << "]\n";
        for (auto &e : s.m_entries)
        {
            os << e.m_name << "=" << e.m_value << "\n";
        }
    }
}

IniFile::IniFile(const std::string &fileName)
{
    std::ifstream ifs(fileName);
    if (!ifs)
        throw std::runtime_error(fileName);
    load(ifs);
}

void IniFile::load(std::istream &is)
{
    std::string line;
    while (is)
    {
        std::string line0;
        std::getline(is, line0);
        line += line0;
        if (!line.empty() && line[line.size() - 1] == '\\')
        {
            line.resize(line.size() - 1);
            continue;
        }
        std::string full = trim(line);
        line.clear();

        if (full.empty() || full[0] == '#')
            continue;

        if (full[0] == '[')
        {
            if (full[full.size() - 1] != ']')
                throw std::runtime_error("wrong section name");

            m_sections.push_back(IniSection());
            m_sections.back().m_name = full.substr(1, full.size() - 2);
        }
        else
        {
            if (m_sections.empty())
                throw std::runtime_error("item without section");
            m_sections.back().add_line(full);
        }
    }
}

void IniSection::add_line(const std::string &line)
{
    IniEntry entry;
    size_t eq = line.find('=');
    if (eq == std::string::npos)
    {
        entry.m_name = line;
    }
    else
    {
        entry.m_name = trim(line.substr(0, eq));
        entry.m_value = trim(line.substr(eq + 1));
    }
    m_entries.push_back(entry);
}

const IniSection *IniFile::find_single_section(const std::string &name) const
{
    std::vector<const IniSection*> values = find_multi_section(name);
    if (values.size() > 1)
        throw std::runtime_error("multiple " + name);
    if (values.empty())
        return nullptr;
    else
        return values.front();
}

std::vector<const IniSection*> IniFile::find_multi_section(const std::string &name) const
{
    std::vector<const IniSection*> res;
    for (auto &s : m_sections)
    {
        if (s.m_name == name)
            res.push_back(&s);
    }
    return res;
}

std::string IniSection::find_single_value(const std::string &name) const
{
    std::vector<std::string> values = find_multi_value(name);
    if (values.size() > 1)
        throw std::runtime_error("multiple " + name);
    if (values.empty())
        return std::string();
    else
        return values.front();
}

std::vector<std::string> IniSection::find_multi_value(const std::string &name) const
{
    std::vector<std::string> res;
    for (auto &e : m_entries)
    {
        if (e.m_name == name)
            res.push_back(e.m_value);
    }
    return res;
}

