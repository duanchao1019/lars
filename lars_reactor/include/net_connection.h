#pragma once


class net_connection
{
public:
    net_connection() {}

    virtual int send_message(const char* data, int msglen, int msgid) = 0;

    virtual int get_fd() = 0;

    void* param; //传递一些自定义的参数，可以使得一个连接的不同业务间能够通过该参数进行通信
};

//创建连接、销毁连接要触发的回调函数原型
typedef void (*conn_callback)(net_connection* conn, void* args);