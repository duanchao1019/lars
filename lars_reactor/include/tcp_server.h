#pragma once

#include <netinet/in.h>
#include "event_loop.h"
#include "tcp_conn.h"
#include "message.h"
#include "thread_pool.h"


class tcp_server
{
public:
    tcp_server(const char *ip, uint16_t port, event_loop* loop);

    void do_accept();

    ~tcp_server();

    thread_pool* get_pool();  //获取当前server的线程池

private:
    int sockfd_;   //listenfd;
    struct sockaddr_in client_addr_;
    socklen_t client_addr_len_;
    event_loop* loop_;
    thread_pool* thread_pool_;

public:
    //========客户端连接管理部分===============
    static void increase_conn(int connfd, tcp_conn* conn); //新增一个新建的连接
    static void decrease_conn(int connfd);  //减少一个销毁的连接
    static void get_conn_num(int* curr_conn);  //获取当前管理的连接数量

    static tcp_conn** conns_;   //管理连接的数组


    //========消息路由分发部分================
    void add_msg_router(int msgid, msg_callback* cb, void* user_data = NULL); //注册消息路由回调函数

    static msg_router router_;  //消息分发路由


    //=========创建连接、销毁连接 Hook 部分================
    static void set_conn_start(conn_callback cb, void* args = NULL); //设置连接创建时要调用的hook函数
    static void set_conn_close(conn_callback cb, void* args = NULL); //设置连接销毁时要调用的hook函数

    static conn_callback conn_start_cb; //创建连接之后要触发的回调
    static void* conn_start_cb_args;    //创建连接之后要触发的回调函数的参数

    static conn_callback conn_close_cb; //销毁连接之后要触发的回调
    static void* conn_close_cb_args;    //销毁连接之后要触发的回调函数的参数

private:
    static int max_conns_;  //最大的允许的连接数量
    static int curr_conns_; //当前管理的连接数量
    static pthread_mutex_t conns_mutex_;  //保护curr_conns_修改的的锁
};