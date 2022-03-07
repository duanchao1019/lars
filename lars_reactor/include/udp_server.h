#pragma once

#include <arpa/inet.h>
#include "net_connection.h"
#include "event_loop.h"
#include "message.h"

class udp_server : public net_connection
{
public:
    udp_server(const char* ip, uint16_t port, event_loop* loop);

    virtual int send_message(const char* data, int msglen, int msgid);

    virtual int get_fd();

    //注册消息路由回调函数
    void add_msg_router(int msgid, msg_callback* cb, void* user_data = NULL);

    ~udp_server();

    //处理消息业务
    void do_read();

private:
    int sockfd_;

    char read_buf_[MSG_LEN_LIMIT];
    char write_buf_[MSG_LEN_LIMIT];

    event_loop* loop_;

    struct sockaddr_in client_addr_;
    socklen_t client_addrlen_;

    msg_router router_; //消息路由分发
};