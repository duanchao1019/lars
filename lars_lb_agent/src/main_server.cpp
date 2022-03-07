#include "main_server.h"
#include "route_lb.h"
#include <netdb.h>

thread_queue<lars::ReportStatusRequest>* report_queue = NULL;
thread_queue<lars::GetRouteRequest>* dns_queue = NULL;

route_lb* r_lb[3];

struct load_balance_config lb_config;

static void create_route_lb()
{
    for (int i = 0; i < 3; i++)
    {
        int id = i + 1;
        r_lb[i] = new route_lb(id);
        if (r_lb[i] == NULL)
        {
            fprintf(stderr, "no more space to new route_lb\n");
            exit(1);
        }
    }
}

static void init_lb_agent()
{
    //加载配置文件
    config_file::setPath("../conf/lars_lb_agent.conf");
    lb_config.probe_nums = config_file::instance()->GetNumber("loadbalance", "probe_num", 10);
    lb_config.init_succ_cnt = config_file::instance()->GetNumber("loadbalance", "init_succ_cnt", 180);
    lb_config.init_err_cnt = config_file::instance()->GetNumber("loadbalance", "init_err_cnt", 5);
    lb_config.err_rate = config_file::instance()->GetFloat("loadbalance", "err_rate", 0.1);
    lb_config.succ_rate = config_file::instance()->GetFloat("loadbalance", "succ_rate", 0.95);
    lb_config.contin_err_limit = config_file::instance()->GetNumber("loadbalance", "contin_err_limit", 10);
    lb_config.contin_succ_limit = config_file::instance()->GetNumber("loadbalance", "contin_succ_limit", 10);
    lb_config.window_err_rate = config_file::instance()->GetFloat("loadbalance", "window_err_rate", 0.7);
    lb_config.idle_timeout = config_file::instance()->GetNumber("loadbalance", "idle_timeout", 15);
    lb_config.overload_timeout = config_file::instance()->GetNumber("loadbalance", "overload_timeout", 15);
    lb_config.update_timeout = config_file::instance()->GetNumber("loadbalance", "update_timeout", 15);
    
    //初始化3个route_lb模块
    create_route_lb();

    char my_host_name[1024];
    if (gethostname(my_host_name, 1024) == 0)
    {
        struct hostent* hd = gethostbyname(my_host_name);
        if (hd)
        {
            struct sockaddr_in myaddr;
            myaddr.sin_addr = *(struct in_addr*)hd->h_addr_list[0];
            lb_config.local_ip = ntohl(myaddr.sin_addr.s_addr);
        }
    }
    if (!lb_config.local_ip)
    {
        struct in_addr inaddr;
        inet_aton("127.0.0.1", &inaddr);
        lb_config.local_ip = ntohl(inaddr.s_addr);
    }
}

int main(int argc, char* argv[])
{
    //1.加载配置文件
    init_lb_agent();

    //1.5初始化LoadBalance的负载均衡器
    
    //2.启动udp server服务，用来接收业务层（调用者/使用者）的消息
    start_UDP_servers();

    //3.启动lars_reporter client线程
    report_queue = new thread_queue<lars::ReportStatusRequest>();
    if (report_queue == NULL)
    {
        fprintf(stderr, "create report_queue error!\n");
        exit(1);
    }
    start_report_client();

    //4.启动lars_dns client线程
    dns_queue = new thread_queue<lars::GetRouteRequest>();
    if (dns_queue == NULL)
    {
        fprintf(stderr, "create dns_queue error!\n");
        exit(1);
    }
    start_dns_client();

    std::cout << "done!" << std::endl;
    while (1)
    {
        sleep(10);
    }

    return 0;
}