#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include "tcp_conn.h" 
#include "tcp_server.h" 
#include "message.h"  


//连接的都事件回调
static void conn_rd_callback(event_loop* loop, int fd, void* args)
{
    tcp_conn* conn = (tcp_conn *)args;
    conn->do_read();
}

//连接的都事件回调
static void conn_wt_callback(event_loop* loop, int fd, void* args)
{
    tcp_conn* conn = (tcp_conn *)args;
    conn->do_write();
}


//构造函数，负责初始化tcp_conn
tcp_conn::tcp_conn(int connfd, event_loop* loop)
: connfd_(connfd),
  loop_(loop)
{
    //将connfd_设置成非阻塞
    int flag = fcntl(connfd_, F_GETFL, 0);
    fcntl(connfd_, F_SETFL, O_NONBLOCK|flag);

    //设置TCP_NODELAY，禁用Nagle算法，降低小包延迟
    int op = 1;
    setsockopt(connfd_, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));

    //如果用户设置了连接创建时要调用的hook函数，则调用
    if (tcp_server::conn_start_cb)
    {
        tcp_server::conn_start_cb(this, tcp_server::conn_start_cb_args);
    }

    //将该连接的读事件让loop_监控
    loop_->add_io_event(connfd_, conn_rd_callback, EPOLLIN, this);

    //将该连接集成到对应的tcp_server中
    tcp_server::increase_conn(connfd_, this);
}

//处理读业务
void tcp_conn::do_read()
{
    //从套接字读取数据
    int ret = ibuf.read_data(connfd_);
    if (ret == -1)
    {
        fprintf(stderr, "read data from socket error\n");
        this->clean_conn();
        return;
    }
    else if (ret == 0)
    {
        printf("connection closed by peer\n");
        clean_conn();
        return;
    }

    //解析msg_head数据
    msg_head head;
    //可能一次性有多个包发送过来，因此需要循环读取
    while (ibuf.length() >= MSG_HEAD_LEN)
    {
        memcpy(&head, ibuf.data(), MSG_HEAD_LEN);
        if (head.msglen > MSG_LEN_LIMIT || head.msglen < 0)
        {
            fprintf(stderr, "data format error, need close, msglen = %d\n", head.msglen);
            this->clean_conn();
            break;
        }
        if (ibuf.length() < MSG_HEAD_LEN + head.msglen)
        {
            //缓存中剩余的数据小于实际应该接受的数据长度
            //说明这不是一个完整的包，先不处理，等收完整个包再处理
            break;
        }

        //头部处理完了，往后偏移MSG_HEAD_LEN长度
        ibuf.pop(MSG_HEAD_LEN);

        //处理ibuf.data()业务数据
        // printf("read data: %s\n", ibuf.data());

        //消息路由
        tcp_server::router_.call(ibuf.data(), head.msglen, head.msgid, this);

        //消息体处理完了，往后偏移msglen长度
        ibuf.pop(head.msglen);
    }

    ibuf.adjust();

    return;
}

//处理写业务
void tcp_conn::do_write()
{
    //do_write并不负责组装message，组装message是由send_message负责的

    while (obuf.length())
    {
        int ret = obuf.write2fd(connfd_);
        if (ret == -1)
        {
            fprintf(stderr, "write2fd error, close connection!\n");
            this->clean_conn();
            return;
        }
        if (ret == 0)
        {
            //代表没有空间可以继续写了，不是错误
            break;
        }
    }

    if (obuf.length() == 0)
    {
        //数据全部写完，取消监听connfd_上的写事件
        loop_->del_io_event(connfd_, EPOLLOUT);
    }

    return;
}

//销毁tcp_conn
void tcp_conn::clean_conn()
{
    //如果用户注册了连接销毁时要调用的hook函数，则调用
    if (tcp_server::conn_close_cb)
    {
        tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);
    }

    //=====连接清理工作=========

    //将该连接从tcp_server去除
    tcp_server::decrease_conn(connfd_);

    //将该连接从loop_中去除
    loop_->del_io_event(connfd_);

    //清空buf
    ibuf.clear();
    obuf.clear();

    //关闭原始套接字
    int fd = connfd_;
    connfd_ = -1;
    close(fd);
    
    //========================
}

//实际负责发送消息的方法
int tcp_conn::send_message(const char* data, int msglen, int msgid)
{
    // printf("server send_message: %s, msglen: %d, msgid: %d\n", data, msglen, msgid);
    bool active_epollout = false;
    if (obuf.length() == 0)
    {
        //如果现在obuf中的所有数据都发送完了，那么是一定要激活写事件的
        //如果obuf中还有数据，说明数据还没有完全写到对端，写事件当前是被激活了的，那么没必要再激活
        active_epollout = true;
    }

    //先封装message消息头
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    //写消息头
    int ret = obuf.send_data((const char *)&head, MSG_HEAD_LEN);
    if (ret != 0)
    {
        fprintf(stderr, "send data error\n");
        return -1;
    }

    //写消息体
    ret = obuf.send_data(data, msglen);
    if (ret != 0)
    {
        //如果写消息体失败，那么需要将该消息的消息头也取消
        obuf.pop(MSG_HEAD_LEN);
        return -1;
    }

    if (active_epollout)
    {
        //激活EPOLLOUT写事件
        loop_->add_io_event(connfd_, conn_wt_callback, EPOLLOUT, this);
    }

    return 0;
}

int tcp_conn::get_fd()
{
    return connfd_;
}