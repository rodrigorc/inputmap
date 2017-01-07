#ifndef INICONF_H_INCLUDED
#define INICONF_H_INCLUDED

#include <string>
#include <vector>

inline bool parse_bool(const std::string &txt, bool def)
{
    if (txt.empty())
        return def;
    return txt == "Y" || txt == "y" || txt == "1";
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
