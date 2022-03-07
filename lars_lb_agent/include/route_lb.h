#pragma once

#include "load_balance.h"

typedef std::unordered_map<uint64_t, load_balance*> route_map;
typedef std::unordered_map<uint64_t, load_balance*>::iterator route_map_it;

//每个route_lb分管不同的modid/cmdid，modid/cmdid的值做hash决定由哪个route_lb负责
//目前设计由3个，和udp_server数量一致
class route_lb
{
public:
    route_lb(int id);

    int get_host(int modid, int cmdid, lars::GetHostResponse& rsp);

    int update_host(int modid, int cmdid, lars::GetRouteResponse& rsp);

    void report_host(lars::ReportHostRequest req);  

    void reset_lb_status();  

    int get_route(int modid, int cmdid, lars::GetRouteResponse& rsp);                                                            

private:
    route_map route_lb_map_;  //当前route_lb下管理的load_balance
    pthread_mutex_t mutex_;
    int id_;  //当前route_lb的编号
};