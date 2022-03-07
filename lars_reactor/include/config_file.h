#pragma once

#include <string>
#include <vector>
#include <unordered_map>

//定义一个存放配置信息的unordered_map
//key是一个string, value是一个unordered_map的指针,存放该标题下所有的key-value键值对
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>* > STR_MAP;
typedef STR_MAP::iterator STR_MAP_ITER;


//设计成单例
class config_file
{
public:
    ~config_file();

    std::string GetString(const std::string& section, const std::string& key, 
                          const std::string& default_value = "");
    
    std::vector<std::string> GetStringList(const std::string& section, const std::string& key);

    unsigned GetNumber(const std::string& section, const std::string& key, unsigned default_value = 0);

    bool GetBool(const std::string& section, const std::string& key, bool default_value = false);

    float GetFloat(const std::string& section, const std::string& key, const float& default_value);

    //设置配置文件所在路径
    static bool setPath(const std::string& path);

    //获取单例
    static config_file* instance();

private:
    config_file() {} //构造函数私有化

    //用来解析字符串配置文件的基础方法
    bool isSection(std::string line, std::string& section);
    unsigned parseNumber(const std::string& s);
    std::string trimLeft(const std::string& s);
    std::string trimRight(const std::string& s);
    std::string trim(const std::string& s);
    bool Load(const std::string& path);

    static config_file* config;  //唯一读取配置文件的实例

    STR_MAP map_;
};