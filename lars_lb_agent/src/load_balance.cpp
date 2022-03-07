#include "load_balance.h"
#include "main_server.h"


int load_balance::pull()
{
    lars::GetRouteRequest route_req;
    route_req.set_modid(modid_);
    route_req.set_cmdid(cmdid_);

    dns_queue->send(route_req);

    status = PULLING;

    return 0;
}

static void get_host_from_list(lars::GetHostResponse& rsp, host_list& ls)
{
    //选择list中第一个节点
    host_info* host = ls.front();

    //proto3协议没有提供proto中自定义类型的set方法，而是通过mutable_接口返回自定义类型的指针，可根据此指针进行赋值操作
    lars::HostInfo* hip = rsp.mutable_host();
    hip->set_ip(host->ip);
    hip->set_port(host->port);

    ls.pop_front();
    ls.push_back(host);
}

//从两个队列中获取一个host给到上层
int load_balance::choose_one_host(lars::GetHostResponse& rsp)
{
    if (idle_list_.empty())
    {
        if (access_cnt_ >= lb_config.probe_nums)
        {
            access_cnt_ = 0;
            get_host_from_list(rsp, overload_list_);
        }
        else
        {
            ++access_cnt_;
            return lars::RET_OVERLOAD;
        }
    }
    else
    {
        if (overload_list_.empty())
        {
            get_host_from_list(rsp, idle_list_);
        }
        else
        {
            if (access_cnt_ >= lb_config.probe_nums)
            {
                access_cnt_ = 0;
                get_host_from_list(rsp, overload_list_);
            }
            else
            {
                ++access_cnt_;
                get_host_from_list(rsp, idle_list_);
            }
        }
    }

    return lars::RET_SUCC;
}

void load_balance::update(lars::GetRouteResponse& rsp)
{
    long curr_time = time(NULL);

    assert(rsp.host_size() != 0);

    std::unordered_set<uint64_t> remote_hosts;
    std::unordered_set<uint64_t> need_delete;

    //1.插入新增的host信息到host_map中
    for (int i = 0; i < rsp.host_size(); i++)
    {
        const lars::HostInfo& h = rsp.host(i);

        uint64_t key = ((uint64_t)h.ip() << 32) + h.port();

        remote_hosts.insert(key);

        if (host_map_.find(key) == host_map_.end())
        {
            host_info* hi = new host_info(h.ip(), h.port(), lb_config.init_succ_cnt);
            if (hi == NULL)
            {
                fprintf(stderr, "new host_info error!\n");
                exit(1);
            }
            host_map_[key] = hi;

            idle_list_.push_back(hi);
        }
    }

    //2.删除减少的host信息从host_map中
    for (auto it = host_map_.begin(); it != host_map_.end(); ++it)
    {
        if (remote_hosts.find(it->first) == remote_hosts.end())
        {
            need_delete.insert(it->first);
        }
    }

    for (auto it = need_delete.begin(); it != need_delete.end(); ++it)
    {
        uint64_t key = *it;

        host_info* hi = host_map_[key];

        if (hi->overload)
        {
            overload_list_.remove(hi);
        }
        else
        {
            idle_list_.remove(hi);
        }

        delete hi;
        host_map_.erase(key);
    }

    last_update_time = curr_time;
    status = NEW;
}

void load_balance::report(int ip, int port, int retcode)
{
    long curr_time = time(NULL);

    uint64_t key = ((uint64_t)ip << 32) + port;
    if (host_map_.find(key) == host_map_.end())
    {
        return ;
    }

    //1.计数统计
    host_info* hi = host_map_[key];
    if (retcode == lars::RET_SUCC)
    {
        hi->vsucc++;
        hi->rsucc++;

        hi->contin_succ++;
        hi->contin_err = 0;
    }
    else
    {
        hi->verr++;
        hi->rerr++;

        hi->contin_err++;
        hi->contin_succ = 0;
    }

    //2.检查节点状态
    //检查idle节点是否满足overload条件，或者overload节点是否满足idle条件

    //如果是idle节点，则只有调用失败才有必要判断是否达到overload条件
    if (hi->overload == false && retcode != lars::RET_SUCC)
    {
        bool overload = false;
        //idle节点，检查是否达到判定位overload的条件
        //(1)计算失败率，如果大于预设值失败率，则为overload
        double err_rate = hi->verr * 1.0 / (hi->vsucc + hi->verr);
        if (err_rate > lb_config.err_rate)
        {
            overload = true;
        }

        //(2)连续失败次数达到阈值，判定为overload
        if (overload == false && hi->contin_err >= (uint32_t)lb_config.contin_err_limit)
        {
            overload = true;
        }

        if (overload)
        {
            struct in_addr saddr;
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d change overload, succ %u err %u\n", 
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);
            hi->set_overload();
            idle_list_.remove(hi);
            overload_list_.push_back(hi);
            return ;
        }
    }
    //如果是overload节点，则只有调用成功才有必要判断是否达到idle条件
    else if (hi->overload == true && retcode == lars::RET_SUCC)
    {
        bool idle = false;

        double succ_rate = hi->vsucc * 1.0 / (hi->vsucc + hi->verr);
        if (succ_rate > lb_config.succ_rate)
        {
            idle = true;
        }
        if (idle == false && hi->contin_succ >= (uint32_t)lb_config.contin_succ_limit)
        {
            idle = true;
        }

        if (idle)
        {
            struct in_addr saddr;
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d change idle, succ %u err %u\n",
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);
            hi->set_idle();
            overload_list_.remove(hi);
            idle_list_.push_back(hi);
            return ;
        }
    }

    //窗口检查和超时机制
    if (hi->overload == false)
    {
        //节点是idle状态
        if (curr_time - hi->idle_ts >= lb_config.idle_timeout)
        {
            //时间窗口到达，需要对idle节点清理负载均衡数据
            if (hi->check_window() == true)
            {
                struct in_addr saddr;
                saddr.s_addr = htonl(hi->ip);
                printf("[%d, %d] host %s:%d change to overload cause windows err rate too high, real succ %u, real err %u\n",
                        modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->rsucc, hi->rerr);
                hi->set_overload();
                idle_list_.remove(hi);
                overload_list_.push_back(hi);
            }
            else
            {
                //重置窗口，恢复默认的负载信息
                hi->set_idle();
            }
        }
    }
    else
    {
        if (curr_time - hi->overload_ts >= lb_config.overload_timeout)
        {
            struct in_addr saddr;
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d reset to idle, vsucc %u, verr %u\n",
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);
            hi->set_idle();
            overload_list_.remove(hi);
            idle_list_.push_back(hi);
        }   
    }
}

void load_balance::commit()
{
    if (this->empty())
    {
        return ;
    }

    lars::ReportStatusRequest req;
    req.set_modid(modid_);
    req.set_cmdid(cmdid_);
    req.set_ts(time(NULL));
    req.set_caller(lb_config.local_ip);

    for (auto it = idle_list_.begin(); it != idle_list_.end(); ++it)
    {
        host_info* hi = *it;
        lars::HostCallResult call_res;
        call_res.set_ip(hi->ip);
        call_res.set_port(hi->port);
        call_res.set_succ(hi->rsucc);
        call_res.set_err(hi->rerr);
        call_res.set_overload(false);

        req.add_results()->CopyFrom(call_res);
    }

    for (auto it = overload_list_.begin(); it != overload_list_.end(); ++it)
    {
        host_info* hi = *it;
        lars::HostCallResult call_res;
        call_res.set_ip(hi->ip);
        call_res.set_port(hi->port);
        call_res.set_succ(hi->rsucc);
        call_res.set_err(hi->rerr);
        call_res.set_overload(true);

        req.add_results()->CopyFrom(call_res);
    }

    report_queue->send(req);  
}

void load_balance::get_all_hosts(std::vector<host_info*>& vec)
{
    for (auto it = host_map_.begin(); it != host_map_.end(); ++it)
    {
        host_info* hi = it->second;
        vec.push_back(hi);
    }
}