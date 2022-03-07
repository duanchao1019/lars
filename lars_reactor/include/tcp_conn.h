#pragma once


#include "reactor_buf.h"
#include "event_loop.h"
#include "net_connection.h"

class tcp_conn : public net_connection
{
public:
    //构造函数，负责初始化tcp_conn
    tcp_conn(int connfd, event_loop* loop);

    //处理读业务
    void do_read();

    //处理写业务
    void do_write();

    //销毁tcp_conn
    void clean_conn();

    //实际负责发送消息的方法，会将消息打包成TLV格式进行发送
    virtual int send_message(const char* data, int msglen, int msgid);

    virtual int get_fd();

private:
    //当前连接的fd
    int connfd_;
    //该连接归属的event_loop
    event_loop* loop_;
    //输出buf
    output_buf obuf;
    //输入buf
    input_buf ibuf;
};