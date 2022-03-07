#pragma once

#include "lars_reactor.h"
#include <string>

typedef std::pair<std::string, int> ip_port;
typedef std::vector<ip_port> route_set;
typedef route_set::iterator route_set_it;

class lars_client
{
public:
    lars_client();
    ~lars_client();

    int get_host(int modid, int cmdid, std::string& ip, int& port);

    void report(int modid, int cmdid, const std::string& ip, int port, int retcode);

    int get_route(int modid, int cmdid, route_set& route);

    int reg_init(int modid, int cmdid);

private:
    int sockfd_[3];   //3个udp socket fd对应agent的3个udp server
    uint32_t seqid_;  //消息的序列号
};
