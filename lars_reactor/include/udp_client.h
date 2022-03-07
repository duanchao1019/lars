#pragma once

#include <arpa/inet.h>
#include "net_connection.h"
#include "message.h"
#include "event_loop.h"


class udp_client : public net_connection
{
public:
    udp_client(const char* ip, uint16_t port, event_loop* loop);
    ~udp_client();

    void add_msg_router(int msgid, msg_callback* cb, void* user_data = NULL);

    virtual int send_message(const char* data, int msglen, int msgid);

    void do_read();

private:
    int sockfd_;

    char read_buf_[MSG_LEN_LIMIT];
    char write_buf_[MSG_LEN_LIMIT];

    event_loop* loop_;

    msg_router router_;
};