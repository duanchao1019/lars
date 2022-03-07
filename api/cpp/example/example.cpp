#include "lars_api.h"
#include <iostream>

void usage()
{
    printf("usage: ./example [modid] [cmdid]\n");
}

int main(int argc, char* argv[])
{
    int ret;

    if (argc != 3)
    {
        usage();
        return 1;
    }

    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);
    lars_client api;

    std::string ip;
    int port;

    ret = api.reg_init(modid, cmdid);
    if (ret != 0)
    {
        std::cout << "modid " << modid << ", cmdid " << cmdid 
                  << " stioo not exist host, after register, ret = " << ret << std::endl;
    }

    route_set route;
    ret = api.get_route(modid, cmdid, route);
    if (ret == 0)
    {
        std::cout << "get route success!" << std::endl;
        for (auto it = route.begin(); it != route.end(); ++it)
        {
            std::cout << "ip = " << (*it).first << ", port = " << (*it).second << std::endl;
        }
    }

    ret = api.get_host(modid, cmdid, ip, port);
    if (ret == 0)
    {
        std::cout << "host is " << ip << ":" << port << std::endl;
        //上报调用结果
        api.report(modid, cmdid, ip, port, 0);
    }
    else if (ret == 3)
    {
        printf("no exist!\n");
    }

    return 0;
}