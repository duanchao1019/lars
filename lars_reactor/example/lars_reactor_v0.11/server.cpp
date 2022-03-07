#include <string.h>
#include <string>
#include "config_file.h"
#include "tcp_server.h"


void print_lars_task(event_loop* loop, void* args)
{
    printf("=======Active Task Func!============\n");
    listen_fd_set fds;
    loop->get_listen_fds(fds);

    //向所有的fd触发
    for (auto it = fds.begin(); it != fds.end(); ++it)
    {
        int fd = *it;
        tcp_conn* conn = tcp_server::conns_[fd];
        if (conn != NULL)
        {
            int msgid = 101;
            const char* msg = "Hello I am a Task!";
            conn->send_message(msg, strlen(msg), msgid);
        }
    }
}

//回显业务的处理函数
void callback_busy(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data)
{
    // printf("callback_busy...\n");
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
    tcp_server* server = (tcp_server *)args;

    int msgid = 101;
    const char* msg = "welcome! you're online...";

    conn->send_message(msg, strlen(msg), msgid);

    //创建连接成功之后触发异步任务
    server->get_pool()->send_task(print_lars_task);
}

//客户端连接销毁的回调
void on_client_lost(net_connection* conn, void* args)
{
    printf("connection is lost!\n");
}

int main()
{
    config_file::setPath("/home/duanchao/project/lars/conf/serv.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);


    event_loop loop;

    tcp_server server(ip.c_str(), port, &loop);

    //注册消息业务路由
    server.add_msg_router(1, callback_busy);
    server.add_msg_router(2, print_busy);

    //注册连接hook回调
    server.set_conn_start(on_client_build, &server);
    server.set_conn_close(on_client_lost);

    loop.event_process();

    return 0;
}