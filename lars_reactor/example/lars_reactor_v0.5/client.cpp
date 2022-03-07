#include "tcp_client.h"
#include <stdio.h>
#include <string.h>

//客户端业务
void busy(const char* data, uint32_t len, int msgid, net_connection* conn, void* user_data)
{
    //得到服务器回显的数据
    printf("recv server: [%s]\n", data);
    printf("msgid: [%d]\n", msgid);
    printf("len: [%d]\n", len);

    conn->send_message(data, len, msgid);
}

int main()
{
    event_loop loop;

    tcp_client client(&loop, "127.0.0.1", 7777, "clientv0.5");

    client.set_msg_callback(busy);

    loop.event_process();

    return 0;
}