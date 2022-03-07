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

int main()
{
    event_loop loop;

    tcp_server server("127.0.0.1", 7777, &loop);

    server.add_msg_router(1, callback_busy);
    server.add_msg_router(2, print_busy);

    loop.event_process();

    return 0;
}