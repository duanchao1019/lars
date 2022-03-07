#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include "udp_server.h"


void read_callback(event_loop* loop, int fd, void* args)
{
    udp_server* server = (udp_server*)args;
    server->do_read();
}

void udp_server::do_read()
{
    while (true)
    {
        int pkg_len = recvfrom(sockfd_, read_buf_, sizeof(read_buf_), 0, (struct sockaddr *)&client_addr_, &client_addrlen_);
        if (pkg_len == -1)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
            {
                perror("recvfrom\n");
                break;
            }
        }

        //处理数据
        msg_head head;
        memcpy(&head, read_buf_, MSG_HEAD_LEN);
        if (head.msglen > MSG_LEN_LIMIT || head.msglen < 0 || head.msglen + MSG_HEAD_LEN != pkg_len)
        {
            //报文格式有问题
            fprintf(stderr, "do_read, data format error, msgid = %d, msglen = %d, pkg_len = %d\n", 
                head.msgid, head.msglen, pkg_len);
            continue;
        }

        //调用注册的路由业务
        router_.call(read_buf_ + MSG_HEAD_LEN, head.msglen, head.msgid, this);
    }
}

udp_server::udp_server(const char* ip, uint16_t port, event_loop* loop)
: loop_(loop)
{
    //1. 忽略一些信号
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        perror("signal ignore SIGHUP");
        exit(1);
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        perror("signal ignore SIGPIPE");
        exit(1);
    }
    
    //2. 创建套接字
    sockfd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if (sockfd_ == -1)
    {
        perror("create udp socket");
        exit(1);
    }

    //3. 设置 ip+端口
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    //4. 绑定
    bind(sockfd_, (struct sockaddr *)&servaddr, sizeof(servaddr));

    bzero(&client_addr_, sizeof(client_addr_));
    client_addrlen_ = sizeof(client_addr_);

    printf("server on %s:%d is running...\n", ip, port);

    //5. 添加读业务事件
    loop_->add_io_event(sockfd_, read_callback, EPOLLIN, this);
}

int udp_server::send_message(const char* data, int msglen, int msgid)
{
    if (msglen > MSG_LEN_LIMIT)
    {
        fprintf(stderr, "too large message to send\n");
        return -1;
    }

    msg_head head;
    head.msgid = msgid;
    head.msglen = msglen;

    memcpy(write_buf_, &head, MSG_HEAD_LEN);
    memcpy(write_buf_ + MSG_HEAD_LEN, data, msglen);

    int ret = sendto(sockfd_, write_buf_, msglen + MSG_HEAD_LEN, 0, (struct sockaddr *)&client_addr_, client_addrlen_);
    if (ret == -1)
    {
        perror("sendto()...");
        return -1;
    }

    return ret;
}

int udp_server::get_fd()
{
    return sockfd_;
}

void udp_server::add_msg_router(int msgid, msg_callback* cb, void* user_data)
{
    router_.register_msg_router(msgid, cb, user_data);
}

udp_server::~udp_server()
{
    loop_->del_io_event(sockfd_);
    close(sockfd_);
}