#include "route_lb.h"
#include "main_server.h"
#include "lars.pb.h"

//构造初始化
route_lb::route_lb(int id)
: id_(id)
{
    pthread_mutex_init(&mutex_, NULL);
}

//agent获取一个modid/cmdid对应的host主机，将返回的主机结果存放在rsp中
int route_lb::get_host(int modid, int cmdid, lars::GetHostResponse& rsp)
{
    int ret = lars::RET_SUCC;

    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);
    if (route_lb_map_.find(key) != route_lb_map_.end())
    {
        load_balance* lb = route_lb_map_[key];
        if (lb->empty())
        {
            assert(lb->status == load_balance::PULLING);
            rsp.set_retcode(lars::RET_NOEXIST);
            ret = lars::RET_NOEXIST;
        }
        else
        {
            ret = lb->choose_one_host(rsp);
            rsp.set_retcode(ret);

            //超时重拉路由
            if (lb->status == load_balance::NEW && time(NULL) - lb->last_update_time > lb_config.update_timeout)
            {
                lb->pull();
            }
        }
    }
    else
    {
        load_balance* lb = new load_balance(modid, cmdid);
        if (lb == NULL)
        {
            fprintf(stderr, "no more space to create loadbalance\n");
            exit(1);
        }
        route_lb_map_[key] = lb;
        lb->pull();

        rsp.set_retcode(lars::RET_NOEXIST);
        ret = lars::RET_NOEXIST;
    }

    pthread_mutex_unlock(&mutex_);

    return ret;
}

int route_lb::update_host(int modid, int cmdid, lars::GetRouteResponse& rsp)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);

    if (route_lb_map_.find(key) != route_lb_map_.end())
    {
        load_balance* lb = route_lb_map_[key];

        if (rsp.host_size() == 0)
        {
            delete lb;
            route_lb_map_.erase(key);
        }
        else
        {
            lb->update(rsp);
        }
    }

    pthread_mutex_unlock(&mutex_);

    return 0;
}

void route_lb::report_host(lars::ReportHostRequest req)
{
    int modid = req.modid();
    int cmdid = req.cmdid();
    int retcode = req.retcode();
    int ip = req.host().ip();
    int port = req.host().port();

    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);
    if (route_lb_map_.find(key) != route_lb_map_.end())
    {
        load_balance* lb = route_lb_map_[key];
        lb->report(ip, port, retcode);
        lb->commit();
    }
    pthread_mutex_unlock(&mutex_);
}

void route_lb::reset_lb_status()
{
    pthread_mutex_lock(&mutex_);
    for (auto it = route_lb_map_.begin(); it != route_lb_map_.end(); ++it)
    {
        load_balance* lb = it->second;
        if (lb->status == load_balance::PULLING)
        {
            lb->status = load_balance::NEW;
        }
    }
    pthread_mutex_unlock(&mutex_);
}

//agent获取全部modid/cmdid对应的host主机，将返回的主机结果存放在rsp中
int route_lb::get_route(int modid, int cmdid, lars::GetRouteResponse& rsp)
{
    int ret = lars::RET_SUCC;

    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);
    if (route_lb_map_.find(key) != route_lb_map_.end())
    {
        load_balance* lb = route_lb_map_[key];
        std::vector<host_info*> vec;
        lb->get_all_hosts(vec);

        for (auto it = vec.begin(); it != vec.end(); ++it)
        {
            lars::HostInfo host;
            host.set_ip((*it)->ip);
            host.set_port((*it)->port);
            rsp.add_host()->CopyFrom(host);
        }

        if (lb->status == load_balance::NEW && time(NULL) - lb->last_update_time > lb_config.update_timeout)
        {
            lb->pull();
        }
    }
    else
    {
        load_balance* lb = new load_balance(modid, cmdid);
        if (lb == NULL)
        {
            fprintf(stderr, "no more space to create loadbalance\n");
            exit(1);
        }

        route_lb_map_[key] = lb;

        lb->pull();

        ret = lars::RET_NOEXIST;
    }
    pthread_mutex_unlock(&mutex_);

    return ret;
}