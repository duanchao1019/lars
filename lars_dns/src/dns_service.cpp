#include <unordered_set>
#include "lars_reactor.h"
#include "mysql.h"
#include "dns_route.h"
#include "subscribe.h"
#include "lars.pb.h"

tcp_server* server;

extern void* check_route_changes(void* args);

typedef std::unordered_set<uint64_t> client_sub_mod_list;

void get_route(const char* data, uint32_t len, int msgid, net_connection* net_conn, void* user_data)
{
    //1.解析proto文件
    lars::GetRouteRequest req;

    req.ParseFromArray(data, len);

    //2.得到modid和cmdid
    int modid, cmdid;
    modid = req.modid();
    cmdid = req.cmdid();

    //2.5 如果之前没有订阅过modid/cmdid，则订阅
    uint64_t mod = (((uint64_t)modid) << 32) + cmdid;
    client_sub_mod_list* sub_list = (client_sub_mod_list*)net_conn->param;
    if (sub_list == NULL)
    {
        fprintf(stderr, "sub_list = NULL\n");
    }
    else if (sub_list->find(mod) == sub_list->end())
    {
        //说明当前mod是没有被订阅的，需要订阅
        sub_list->insert(mod);
        SubscribeList::instance()->subscribe(mod, net_conn->get_fd());
        printf("fd %d subscribe modid = %d, cmdid = %d\n", net_conn->get_fd(), modid, cmdid);
    }

    //3.根据modid/cmdid获取host信息
    host_set hosts = Route::instance()->get_hosts(modid, cmdid);

    //4.将host信息数据打包成protobuf
    lars::GetRouteResponse rsp;

    rsp.set_modid(modid);
    rsp.set_cmdid(cmdid);

    for (auto it = hosts.begin(); it != hosts.end(); ++it)
    {
        uint64_t ip_port = *it;
        lars::HostInfo host;
        host.set_ip((uint32_t)(ip_port >> 32));  //  ( ) 运算符比 >> 运算符优先级要高
        host.set_port((int)ip_port);
        rsp.add_host()->CopyFrom(host);
    }

    //5.发送给客户端
    std::string responseString;
    rsp.SerializeToString(&responseString);
    net_conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
}

//每个新客户端创建成功之后，执行该函数
void create_subscribe(net_connection* conn, void* args)
{   
    //给当前的conn绑定一个订阅的mod的一个set集合
    conn->param = new client_sub_mod_list;
}

//每个连接销毁前调用
void clear_subscribe(net_connection* conn, void* args)
{
    client_sub_mod_list* sub_list = (client_sub_mod_list*)conn->param;

    for (auto it = sub_list->begin(); it != sub_list->end(); ++it)
    {
        //取消DNS订阅
        uint64_t mod = *it;
        SubscribeList::instance()->unsubscribe(mod, conn->get_fd());
    }

    delete sub_list;
    conn->param = NULL;
}


int main(int argc, char **argv)
{
    event_loop loop;

    //加载配置文件
    config_file::setPath("conf/lars_dns.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 7778);


    //创建tcp服务器
    server = new tcp_server(ip.c_str(), port, &loop);

    //注册创建/销毁连接的HOOK函数
    server->set_conn_start(create_subscribe);
    server->set_conn_close(clear_subscribe);

    //注册回调业务
    server->add_msg_router(lars::ID_GetRouteRequest, get_route);

    //开辟一个线程定期发布已经更变的mod集合
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, check_route_changes, NULL);
    if (ret == -1)
    {
        perror("pthread_create backend thread");
        exit(1);
    }
    pthread_detach(tid);

    //开始事件监听
    printf("lars dns service...\n");    
    loop.event_process();

    return 0;
}