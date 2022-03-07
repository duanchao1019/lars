#include <sstream>
#include <fstream>
#include <assert.h>
#include <string.h>
#include "config_file.h"


config_file* config_file::config = NULL;

config_file::~config_file()
{
    for (STR_MAP_ITER it = map_.begin(); it != map_.end(); ++it)
    {
        delete it->second;
    }
}

std::string config_file::GetString(const std::string& section, const std::string& key, const std::string& default_value)
{
    STR_MAP_ITER it = map_.find(section);
    if (it != map_.end())
    {
        std::unordered_map<std::string, std::string>::iterator it1 = it->second->find(key);
        if (it1 != it->second->end())
        {
            return it1->second;
        }
    }

    return default_value;
}

std::vector<std::string> config_file::GetStringList(const std::string& section, const std::string& key)
{
    std::vector<std::string> v;
    std::string str = GetString(section, key, "");
    std::string sep = ", \t";
    std::string substr;
    std::string::size_type start = 0;
    std::string::size_type index;

    while ((index = str.find_first_of(sep, start)) != std::string::npos)
    {
        substr = str.substr(start, index - start);
        v.push_back(substr);

        start = str.find_first_not_of(sep, index);
        if (start == std::string::npos)
            return v;
    }

    substr = str.substr(start);
    v.push_back(substr);

    return v;
}

unsigned config_file::GetNumber(const std::string& section, const std::string& key, unsigned default_value)
{
    STR_MAP_ITER it = map_.find(section);
    if (it != map_.end())
    {
        std::unordered_map<std::string, std::string>::iterator it1 = it->second->find(key);
        if (it1 != it->second->end())
        {
            return parseNumber(it1->second);
        }
    }

    return default_value;
}

bool config_file::GetBool(const std::string& section, const std::string& key, bool default_value)
{
    STR_MAP_ITER it = map_.find(section);
    if (it != map_.end())
    {
        std::unordered_map<std::string, std::string>::iterator it1 = it->second->find(key);
        if (it1 != it->second->end())
        {
            if (strcasecmp(it1->second.c_str(), "true") == 0)
                return true;
        }
    }

    return default_value;
}

float config_file::GetFloat(const std::string& section, const std::string& key, const float& default_value)
{
    std::ostringstream convert1;
    convert1 << default_value;
    //将浮点数转换成字符串，然后按照字符串业务处理
    std::string default_value_str = convert1.str();
    std::string text = GetString(section, key, default_value_str);
    std::istringstream convert2(text);
    float fresult;
    if (!(convert2 >> fresult))
        fresult = 0;    //如果fresult放不下text对应的浮点数，执行将返回0
    return fresult;
}

bool config_file::isSection(std::string line, std::string& section)
{
    section = trim(line);

    if (section.empty() || section.size() <= 2)
        return false;
    
    if (section[0] != '[' || section.back() != ']')
        return false;
    
    section = section.substr(1, section.size() - 2);
    section = trim(section);

    return true;
}

unsigned config_file::parseNumber(const std::string& s)
{
    std::istringstream iss(s);
    long long v = 0;
    iss >> v;
    return v;
}

std::string config_file::trimLeft(const std::string& s)
{
    size_t first_not_empty = 0;

    for (int i = 0; i < s.size(); i++)
    {
        if (s[i] != ' ')
        {
            first_not_empty = i;
            break;
        }
    }
    
    return s.substr(first_not_empty);
}

std::string config_file::trimRight(const std::string& s)
{
    size_t last_not_empty = s.size();
    
    for (int i = s.size() - 1; i >= 0; i--)
    {
        if (s[i] != ' ')
        {
            break;
        }
        last_not_empty--;
    }
    
    return s.substr(0, last_not_empty);
}

std::string config_file::trim(const std::string& s)
{
    return trimLeft(trimRight(s));
}

bool config_file::Load(const std::string& path)
{
    std::ifstream ifs(path.c_str());
    if (!ifs.good())
        return false;

    std::string line;
    std::unordered_map<std::string, std::string>* m = NULL;

    while (!ifs.eof() && ifs.good())
    {
        getline(ifs, line);
        std::string section;
        if (isSection(line, section))
        {
            STR_MAP_ITER it = map_.find(section);
            if (it == map_.end())
            {
                m = new std::unordered_map<std::string, std::string>();
                map_.insert(STR_MAP::value_type(section, m));
            }
            else
            {
                m = it->second;
            }
            continue;
        }

        size_t equ_pos = line.find('=');
        if (equ_pos == std::string::npos)
            continue;

        std::string key = line.substr(0, equ_pos);
        std::string value = line.substr(equ_pos + 1);
        key = trim(key);
        value = trim(value);

        if (key.empty())
            continue;
        if (key[0] == '#' || key[0] == '/') //忽略注释
            continue;

        std::unordered_map<std::string, std::string>::iterator it1 = m->find(key);
        if (it1 != m->end())
            it1->second = value;
        else
            m->insert(std::unordered_map<std::string, std::string>::value_type(key, value));
    }

    ifs.close();
    return true;
}

config_file* config_file::instance()
{
    assert(config != NULL);
    return config;
}

bool config_file::setPath(const std::string& path)
{
    assert(config == NULL);
    //创建对象
    config = new config_file();
    //加载配置文件
    return config->Load(path);
}