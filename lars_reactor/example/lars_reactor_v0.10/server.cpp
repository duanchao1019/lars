#include <string.h>
#include "config_file.h"
#include "udp_server.h"

//回显业务的处理函数
void callback_busy(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data)
{
    printf("callback_busy...\n");
    //直接回显
    net_conn->send_message(data, msglen, msgid);
}


int main()
{
    config_file::setPath("/home/duanchao/project/lars/conf/serv.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);


    event_loop loop;

    udp_server server(ip.c_str(), port, &loop);

    //注册消息业务路由
    server.add_msg_router(1, callback_busy);

    loop.event_process();

    return 0;
}