#pragma once

#include "lars_reactor.h"
#include "lars.pb.h"
#include "route_lb.h"

extern thread_queue<lars::ReportStatusRequest>* report_queue;
extern thread_queue<lars::GetRouteRequest>* dns_queue;
extern route_lb* r_lb[3];

void start_UDP_servers(void);

void start_report_client(void);

void start_dns_client(void);

struct load_balance_config
{
    int probe_nums;
    int init_succ_cnt;
    int init_err_cnt;
    float err_rate;
    float succ_rate;
    int contin_err_limit;
    int contin_succ_limit;
    uint32_t local_ip;
    float window_err_rate;
    int idle_timeout;
    int overload_timeout;
    long update_timeout;
};

extern struct load_balance_config lb_config;