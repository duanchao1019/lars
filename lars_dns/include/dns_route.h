#pragma once

#include <pthread.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mysql.h"
#include "lars_reactor.h"

typedef std::unordered_set<uint64_t> host_set;
typedef std::unordered_set<uint64_t>::iterator host_set_it;

typedef std::unordered_map<uint64_t, host_set> route_map;
typedef std::unordered_map<uint64_t, host_set>::iterator route_map_it;

class Route
{
public:
    static void init()
    {
        instance_ = new Route();
    }

    static Route* instance()
    {
        pthread_once(&once_, init);
        return instance_;
    }

    //建立数据库连接
    void connect_db();
    //加载RouteData到data_pointer_（只在初始执行一次，后续加载由load_route_data负责）
    void build_maps();
    //加载版本号
    int load_version();
    //加载RouteData到temp_pointer_
    int load_route_data();
    //交换 data_pointer_与temp_pointer_;
    void swap();  
    //加载RouteChange得到修改的modid/cmdid，将结果放在change_list中
    void load_changes(std::vector<uint64_t>& change_list);
    //从RouteChange删除修改记录，remove_all为全部删除，否则默认删除当前版本之前的全部修改
    void remove_changes(bool remove_all = false);
    //获取modid/cmdid对应的主机列表
    host_set get_hosts(int modid, int cmdid);

private:
    Route();
    Route(const Route&);
    Route& operator=(const Route&);

private:
    static Route* instance_;
    static pthread_once_t once_;  //单例锁，确保init函数只执行一次

    MYSQL db_conn_;  //mysql连接
    char sql_[1000]; //sql语句

    route_map* data_pointer_;  //指向RouteDataMap_A，当前的关系map
    route_map* temp_pointer_;  //指向RouteDataMap_B，临时的关系map

    pthread_rwlock_t map_lock_;

    long version_;  //当前版本号
};