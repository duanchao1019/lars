#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "tcp_server.h"
#include "tcp_conn.h"
#include "reactor_buf.h"
#include "config_file.h"

//=========消息分发路由管理===========
msg_router tcp_server::router_;

void tcp_server::add_msg_router(int msgid, msg_callback* cb, void* user_data)
{
    router_.register_msg_router(msgid, cb, user_data);
}
//=================================


//===========连接资源管理============
tcp_conn** tcp_server::conns_ = NULL;
int tcp_server::max_conns_ = 0;
int tcp_server::curr_conns_ = 0;
pthread_mutex_t tcp_server::conns_mutex_ = PTHREAD_MUTEX_INITIALIZER;

void tcp_server::increase_conn(int connfd, tcp_conn* conn)
{
    pthread_mutex_lock(&conns_mutex_);
    conns_[connfd] = conn;
    curr_conns_++;
    pthread_mutex_unlock(&conns_mutex_);
}

void tcp_server::decrease_conn(int connfd)
{
    pthread_mutex_lock(&conns_mutex_);
    conns_[connfd] = NULL;
    curr_conns_--;
    pthread_mutex_unlock(&conns_mutex_);
}

void tcp_server::get_conn_num(int* curr_conn)
{
    pthread_mutex_lock(&conns_mutex_);
    *curr_conn = curr_conns_;
    pthread_mutex_unlock(&conns_mutex_);
}
//=======================================


//=======创建连接、销毁连接 Hook 管理========
conn_callback tcp_server::conn_start_cb = NULL;
void* tcp_server::conn_start_cb_args = NULL;
conn_callback tcp_server::conn_close_cb = NULL;
void* tcp_server::conn_close_cb_args = NULL;

void tcp_server::set_conn_start(conn_callback cb, void* args)
{
    conn_start_cb = cb;
    conn_start_cb_args = args;
}

void tcp_server::set_conn_close(conn_callback cb, void* args)
{
    conn_close_cb = cb;
    conn_close_cb_args = args;
}
//=======================================

struct message
{
    char data[m4K];
    char len;
};

struct message msg;

void server_rd_callback(event_loop* loop, int fd, void* args);
void server_wt_callback(event_loop* loop, int fd, void* args);


void accept_callback(event_loop* loop, int fd, void* args)
{
    tcp_server* server = (tcp_server *)args;
    server->do_accept();
}


tcp_server::tcp_server(const char *ip, uint16_t port, event_loop* loop)
: loop_(loop)
{
    //0. 忽略信号
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "ignore signal SIGHUP error\n");
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "ignore signal SIGPIPE error\n");
    }

    //1. 创建套接字
    if ((sockfd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "create sockfd_ error\n");
        exit(1);
    }

    //2. 初始化服务器地址
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    //2.5 设置端口复用
    int opt = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        fprintf(stderr, "setsockopt sockfd_ reuseaddr error\n");
    }

    //3. 绑定端口
    if (bind(sockfd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "bind sockfd_ to server_addr error\n");
        exit(1);
    }

    //4. 监听
    if (listen(sockfd_, 500) == -1)
    {
        fprintf(stderr, "listen sockfd_ error\n");
        exit(1);
    }

    //5. 创建连接管理
    max_conns_ = config_file::instance()->GetNumber("reactor", "maxConn", 100);
    //创建连接信息数组(这里有一个疑问，新到来的连接的fd是从5开始的，这样明显会导致数组越界，为什么却没有发生数组越界呢?)
    conns_ = new tcp_conn*[max_conns_+3]; //3是因为stdin, stdout, stderr已经被占用
    if (conns_ == NULL)
    {
        fprintf(stderr, "new conns_[%d] error\n", max_conns_);
        exit(1);
    }

    //6. 创建线程池
    int thread_num = config_file::instance()->GetNumber("reactor", "threadNum", 3);
    if (thread_num > 0)
    {
        thread_pool_ = new thread_pool(thread_num);
        if (thread_pool_ == NULL)
        {
            fprintf(stderr, "tcp_server new thread_pool error\n");
            exit(1);
        }
    }

    //7. 注册sockfd_读事件
    loop_->add_io_event(sockfd_, accept_callback, EPOLLIN, this);
}

thread_pool* tcp_server::get_pool()
{
    return thread_pool_;
}


void server_rd_callback(event_loop* loop, int fd, void* args)
{
    int ret = 0;

    struct message* msg = (struct message *)args;
    input_buf ibuf;

    ret = ibuf.read_data(fd);
    if (ret == -1)
    {
        fprintf(stderr, "ibuf read_data error\n");
        loop->del_io_event(fd);
        close(fd);
        return;
    }
    if (ret == 0)
    {
        loop->del_io_event(fd);
        close(fd);
        return;
    }

    // printf("ibuf.length() = %d\n", ibuf.length());

    msg->len = ibuf.length();
    bzero(msg->data, msg->len);
    memcpy(msg->data, ibuf.data(), msg->len);

    ibuf.pop(msg->len);
    ibuf.adjust();

    // printf("receive data = %s\n", msg->data);

    //删除读事件，添加写事件
    loop->del_io_event(fd, EPOLLIN);
    loop->add_io_event(fd, server_wt_callback, EPOLLOUT, msg);
}

void server_wt_callback(event_loop* loop, int fd, void* args)
{
    struct message* msg = (struct message *)args;
    output_buf obuf;

    //回显数据
    obuf.send_data(msg->data, msg->len);
    while (obuf.length())
    {
        int write_ret = obuf.write2fd(fd);
        if (write_ret == -1)
        {
            fprintf(stderr, "write connfd error\n");
            return;
        }
        else if (write_ret == 0)
        {
            //不是错误，表示此时不可写
            break;
        }
    }

    //删除写事件，添加读事件
    loop->del_io_event(fd, EPOLLOUT);
    loop->add_io_event(fd, server_rd_callback, EPOLLIN, msg);
}

void tcp_server::do_accept()
{
    int connfd;
    while (true)
    {
        connfd = accept(sockfd_, (struct sockaddr *)&client_addr_, &client_addr_len_);
        if (connfd == -1)
        {
            if (errno == EINTR)        //中断错误，属于正常errno，
                continue;
            else if (errno == EMFILE)  //连接建立过多，文件描述符资源不够用了
            {
                fprintf(stderr, "too many connections\n");
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)  //套接字为非阻塞时产生的正常errno
            {
                printf("would block\n");
                continue;
            }
            else  //发生其他错误，则中止程序
            {
                fprintf(stderr, "accept error\n");
                exit(1);
            }
        }
        else
        {
            //=========accept success!============
            int curr_conns;
            tcp_server::get_conn_num(&curr_conns);
            
            if (curr_conns >= max_conns_)
            {
                //判断连接的数量是否已满，如果满了则不接受该新连接
                fprintf(stderr, "too many connections, max = %d\n", max_conns_);
                close(connfd);
            }
            else
            {
                //将accept到的新连接交给线程池处理
                if (thread_pool_ != NULL) //启动多线程模式
                {
                    //选择一个线程来处理(采用轮询的方式)
                    thread_queue<task_msg>* queue = thread_pool_->get_thread();
                    //创建一个新建连接的消息任务
                    task_msg task;
                    task.type = task_msg::NEW_CONN;
                    task.connfd = connfd;
                    //将消息任务添加到消息队列中
                    queue->send(task);
                }
                else  //启动单线程模式
                {
                    tcp_conn* conn = new tcp_conn(connfd, loop_);
                    if (conn == NULL)
                    {
                        fprintf(stderr, "new tcp_conn error\n");
                        exit(1);
                    }
                    printf("get new connection succ!\n");
                }   
            }
            //======================================
            break;
        }
    }
}

tcp_server::~tcp_server()
{
    close(sockfd_);

    if (conns_ != NULL)
    {
        delete [] conns_;
        conns_ = NULL;
    }
}