#pragma once

#include "host_info.h"
#include "lars.pb.h"
#include <list>
#include <unordered_map>

typedef std::unordered_map<uint64_t, host_info*> host_map;
typedef std::unordered_map<uint64_t, host_info*>::iterator host_map_it;

typedef std::list<host_info*> host_list;
typedef std::list<host_info*>::iterator host_list_it;


class load_balance
{
public:
    load_balance(int modid, int cmdid)
    : status(PULLING),
      last_update_time(0),
      modid_(modid),
      cmdid_(cmdid)
    {
        //load_balance初始化构造函数
    }

    //判断是否已经没有host在当前LB中
    bool empty() const
    {
        return host_map_.empty();
    }

    //从当前的双队列中获取host信息
    int choose_one_host(lars::GetHostResponse& rsp);

    //如果list中没有host信息，需要从远程的DNS Service发送GetRouteHost请求申请
    int pull();

    //根据DNS Service远程返回的结果，更新host_map_
    void update(lars::GetRouteResponse& rsp);

    //上报当前host主机调用情况给远端reporter service
    void report(int ip, int port, int retcode);

    //提交host的调用结果给远程reporter service上报结果
    void commit();

    //获取当前挂载下的全部host信息，添加到vec中
    void get_all_hosts(std::vector<host_info*>& vec);

    //当前load_balance模块的状态
    enum STATUS
    {
        PULLING,  //正在从远程dns service通过网络拉取主机信息
        NEW       //正在创建新的load_balance模块
    };
    STATUS status;

    long last_update_time;  //最后更新host_map_的时间戳

private:
    int modid_;
    int cmdid_;
    int access_cnt_; //请求次数，每次请求+1，判断是否超过probe_num的值

    host_map host_map_;      //当前load_balance模块所管理的全部ip+port为主键的host信息集合

    host_list idle_list_;      //空闲队列
    host_list overload_list_;  //过载队列
};