#pragma once


#include <arpa/inet.h>
#include "event_loop.h"
#include "io_buf.h"
#include "message.h"


class tcp_client : public net_connection
{
public:
    tcp_client(event_loop* loop, const char* ip, uint16_t port, const char* name);

    virtual int send_message(const char* data, int msglen, int msgid);

    virtual int get_fd();

    void do_connect();

    int do_read();

    int do_write();

    void clean_conn();

    void add_msg_router(int msgid, msg_callback* cb, void* user_data = NULL);

    ~tcp_client();

public:
    //=========创建连接、销毁连接 Hook 部分================
    static void set_conn_start(conn_callback cb, void* args = NULL); //设置连接创建时要调用的hook函数
    static void set_conn_close(conn_callback cb, void* args = NULL); //设置连接销毁时要调用的hook函数

    static conn_callback conn_start_cb; //创建连接之后要触发的回调
    static void* conn_start_cb_args;    //创建连接之后要触发的回调函数的参数

    static conn_callback conn_close_cb; //销毁连接之后要触发的回调
    static void* conn_close_cb_args;    //销毁连接之后要触发的回调函数的参数

public:
    bool connected;
    struct sockaddr_in server_addr_;
    io_buf ibuf;
    io_buf obuf;

private:
    int sockfd_;
    socklen_t addrlen_;

    //客户端的事件处理机制
    event_loop* loop_;

    //当前客户端的名称，用于记录日志
    const char* name_;

    msg_router router_; //消息路由分发
};