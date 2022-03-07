#include <strings.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "tcp_client.h"

static void read_callback(event_loop* loop, int fd, void* args)
{
    tcp_client* cli = (tcp_client *)args;
    cli->do_read();
}

static void write_callback(event_loop* loop, int fd, void* args)
{
    tcp_client* cli = (tcp_client *)args;
    cli->do_write();
}

static void connection_delay(event_loop* loop, int fd, void* args)
{
    tcp_client* cli = (tcp_client *)args;
    loop->del_io_event(fd);

    int result = 0;
    socklen_t result_len = sizeof(result);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);
    if (result == 0)
    {
        cli->connected = true;

        printf("connect %s:%d succ!\n", inet_ntoa(cli->server_addr_.sin_addr), ntohs(cli->server_addr_.sin_port));

        //调用开发者注册的连接创建成功时调用的hook函数
        if (cli->conn_start_cb != NULL)
        {
            cli->conn_start_cb(cli, cli->conn_start_cb_args);
        }

        loop->add_io_event(fd, read_callback, EPOLLIN, cli);
        if (cli->obuf.length_ != 0)
        {
            //输出缓冲区有数据可写
            loop->add_io_event(fd, write_callback, EPOLLOUT, cli);
        }
    }
    else
    {
        fprintf(stderr, "connection %s:%d error\n", inet_ntoa(cli->server_addr_.sin_addr), ntohs(cli->server_addr_.sin_port));
    }
}


//=========创建连接、销毁连接 Hook 部分================
conn_callback tcp_client::conn_start_cb = NULL;
void* tcp_client::conn_start_cb_args = NULL;
conn_callback tcp_client::conn_close_cb = NULL;
void* tcp_client::conn_close_cb_args = NULL;

void tcp_client::set_conn_start(conn_callback cb, void* args)
{
    conn_start_cb = cb;
    conn_start_cb_args = args;
}
void tcp_client::set_conn_close(conn_callback cb, void* args)
{
    conn_close_cb = cb;
    conn_close_cb_args = args;
}
//====================================================


tcp_client::tcp_client(event_loop* loop, const char* ip, uint16_t port, const char* name)
: sockfd_(-1),
  name_(name),
  loop_(loop),
  ibuf(4194304),
  obuf(4194304)
{
    bzero(&server_addr_, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr_.sin_addr);

    addrlen_ = sizeof(server_addr_);

    this->do_connect();
}

void tcp_client::do_connect()
{
    if (sockfd_ == -1)
    {
        close(sockfd_);
    }
    //创建套接字
    sockfd_ = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
    if (sockfd_ == -1)
    {
        fprintf(stderr, "create tcp client socket error\n");
        exit(1);
    }

    int ret = connect(sockfd_, (const struct sockaddr *)&server_addr_, addrlen_);
    if (ret == 0)
    {
        //如果用户注册了连接创建成功时要调用的hook函数，则调用
        if (conn_start_cb != NULL)
        {
            conn_start_cb(this, conn_start_cb_args);
        }

        //连接创建成功
        connected = true;

        printf("connect %s:%d succ!\n", inet_ntoa(server_addr_.sin_addr), ntohs(server_addr_.sin_port));

        //注册读回调
        loop_->add_io_event(sockfd_, read_callback, EPOLLIN, this);
        //如果写缓冲中有数据，那么也需要注册写回调
        if (this->obuf.length_ != 0)
        {
            loop_->add_io_event(sockfd_, write_callback, EPOLLOUT, this);
        }
    }
    else
    {
        if (errno == EINPROGRESS)
        {
            //当fd是非阻塞的时候，可能会出现这个错误，但是并不代表连接创建失败
            //如果fd是可写状态，那么连接是创建成功的
            fprintf(stderr, "do_connect EINPROGRESS\n");

            //让loop_去触发一个判断连接是否创建成功的业务，用EPOLLOUT事件立刻触发
            loop_->add_io_event(sockfd_, connection_delay, EPOLLOUT, this);
        }
        else
        {
            fprintf(stderr, "connection error\n");
            exit(1);
        }
    }
}

int tcp_client::do_read()
{
    assert(connected);

    int need_read = 0;
    if (ioctl(sockfd_, FIONREAD, &need_read) == -1)
    {
        fprintf(stderr, "ioctl FIONREAD error");
        return -1;
    }
    //确保ibuf中可以容纳可读数据
    assert(need_read <= ibuf.capacity_ - ibuf.length_);

    int ret;
    do {
        ret = read(sockfd_, ibuf.data_ + ibuf.length_, need_read);
    } while (ret == -1 && errno == EINTR);

    if (ret == 0)
    {
        //对端关闭
        if (name_ != nullptr)
        {
            printf("%s client: connection close by peer!\n", name_);
        }
        else
        {
            printf("client: connection close by peer!\n");
        }

        clean_conn();
        return -1;
    }
    else if (ret == -1)
    {
        fprintf(stderr, "client: do_read(), error\n");
        clean_conn();
        return -1;
    }

    assert(ret == need_read);
    ibuf.length_ += ret;

    //解包
    msg_head head;
    int msgid, length;
    while (ibuf.length_ >= MSG_HEAD_LEN)
    {
        memcpy(&head, ibuf.data_ + ibuf.head_, MSG_HEAD_LEN);
        msgid = head.msgid;
        length = head.msglen;

        ibuf.pop(MSG_HEAD_LEN);

        this->router_.call(ibuf.data_ + ibuf.head_, length, msgid, this);

        ibuf.pop(length);
    }

    ibuf.adjust();

    return 0;
}

int tcp_client::do_write()
{
    //数据有长度且数据头部索引是起始位置
    assert(obuf.head_ == 0 && obuf.length_);

    int ret;

    while(obuf.length_)
    {
        do {
            ret = write(sockfd_, obuf.data_, obuf.length_);
        } while (ret == -1 && errno == EINTR);

        if (ret > 0)
        {
            obuf.pop(ret);
            obuf.adjust();
        }
        else if (ret == -1 && errno != EAGAIN)
        {
            fprintf(stderr, "tcp client write error\n");
            this->clean_conn();
        }
        else
        {
            //出错，不能再继续写
            break;
        }
    }

    if (obuf.length_ == 0)
    {
        //已经写完，删除写事件
        // printf("do write over, del EPOLLOUT\n");
        this->loop_->del_io_event(sockfd_, EPOLLOUT);
    }

    return 0;
}

//释放连接资源，重置连接
void tcp_client::clean_conn()
{
    if (sockfd_ != -1)
    {
        printf("clean conn, del socket!\n");
        loop_->del_io_event(sockfd_);
        close(sockfd_);
    }
    connected = false;

    //调用开发者注册的销毁连接时调用的hook函数
    if (conn_close_cb != NULL)
    {
        conn_close_cb(this, conn_close_cb_args);
    }
    
    //重置连接
    this->do_connect();
}

void tcp_client::add_msg_router(int msgid, msg_callback* cb, void* user_data)
{
    router_.register_msg_router(msgid, cb, user_data);
}

tcp_client::~tcp_client()
{
    if (sockfd_ != -1)
    {
        loop_->del_io_event(sockfd_);
        close(sockfd_);
    }
    connected = false;
}


int tcp_client::send_message(const char* data, int msglen, int msgid)
{
    if (connected == false)
    {
        fprintf(stderr, "no connected, send message stop!\n");
        return -1;
    }

    bool need_add_event = (obuf.length_ == 0) ? true : false;
    if (msglen + MSG_HEAD_LEN > this->obuf.capacity_ - obuf.length_)
    {
        fprintf(stderr, "No more space to write socket!\n");
        return -1;
    }

    //封装消息头
    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    memcpy(obuf.data_ + obuf.length_, &head, MSG_HEAD_LEN);
    obuf.length_ += MSG_HEAD_LEN;

    memcpy(obuf.data_ + obuf.length_, data, msglen);
    obuf.length_ += msglen;

    if (need_add_event)
    {
        loop_->add_io_event(sockfd_, write_callback, EPOLLOUT, this);
    }

    return 0;
}

int tcp_client::get_fd()
{
    return sockfd_;
}