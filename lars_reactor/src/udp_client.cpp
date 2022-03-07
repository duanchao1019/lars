#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "udp_client.h"

void read_callback(event_loop* loop, int fd, void* args)
{
    udp_client* client = (udp_client *)args;
    client->do_read();
}

void udp_client::do_read()
{
    while (true)
    {
        int pkt_len = recvfrom(sockfd_, read_buf_, sizeof(read_buf_), 0, NULL, NULL);
        if (pkt_len == -1)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
            {
                perror("recvfrom()");
                break;
            }
        }

        msg_head head;
        memcpy(&head, read_buf_, MSG_HEAD_LEN);
        if (head.msglen > MSG_LEN_LIMIT || head.msglen < 0 || head.msglen + MSG_HEAD_LEN != pkt_len)
        {
            fprintf(stderr, "do read, data format error, msgid = %d, msglen = %d, pkt_len = %d\n", 
                    head.msgid, head.msglen, pkt_len);
            continue;
        }

        router_.call(read_buf_ + MSG_HEAD_LEN, head.msglen, head.msgid, this);
    }
}

udp_client::udp_client(const char* ip, uint16_t port, event_loop* loop)
: loop_(loop)
{
    sockfd_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if (sockfd_ == -1)
    {
        perror("create socket error");
        exit(1);
    }

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    int ret = connect(sockfd_, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret == -1)
    {
        perror("connect");
        exit(1);
    }

    loop_->add_io_event(sockfd_, read_callback, EPOLLIN, this);
}

udp_client::~udp_client()
{
    loop_->del_io_event(sockfd_);
    close(sockfd_);
}

void udp_client::add_msg_router(int msgid, msg_callback* cb, void* user_data)
{
    router_.register_msg_router(msgid, cb, user_data);
}

int udp_client::send_message(const char* data, int msglen, int msgid)
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

    int ret = sendto(sockfd_, write_buf_, MSG_HEAD_LEN + msglen, 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto()...");
        return -1;
    }

    return ret;
}