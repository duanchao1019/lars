#include <string.h>
#include "tcp_server.h"

//回显业务的处理函数
void callback_busy(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data)
{
    printf("callback_busy...\n");
    //直接回显
    net_conn->send_message(data, msglen, msgid);
}

//打印信息的处理函数
void print_busy(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data)
{
    printf("recv client: [%s]\n", data);
    printf("msgid: [%d]\n", msgid);
    printf("msglen: [%d]\n", msglen);
}

//新客户端创建连接的回调
void on_client_build(net_connection* conn, void* args)
{
    int msgid = 101;
    const char* msg = "welcome! you're online...";

    conn->send_message(msg, strlen(msg), msgid);
}

//客户端连接销毁的回调
void on_client_lost(net_connection* conn, void* args)
{
    printf("connection is lost!\n");
}

int main()
{
    event_loop loop;

    tcp_server server("127.0.0.1", 7777, &loop);

    //注册消息业务路由
    server.add_msg_router(1, callback_busy);
    server.add_msg_router(2, print_busy);

    //注册连接hook回调
    server.set_conn_start(on_client_build);
    server.set_conn_close(on_client_lost);

    loop.event_process();

    return 0;
}