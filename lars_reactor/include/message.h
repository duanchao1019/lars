#pragma once

#include <unordered_map>
#include <utility>
#include "net_connection.h"

//解决tcp粘包问题的消息头
struct msg_head
{
    int msgid;
    int msglen;
};

//消息头的二进制长度
#define MSG_HEAD_LEN 8

//消息体的最大长度限制(消息头+消息体最长为65535个字节)
#define MSG_LEN_LIMIT (65535 - MSG_HEAD_LEN)

typedef void msg_callback(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data);

class msg_router
{
public:
    msg_router()
    {
        printf("msg router init...\n");
    }

    int register_msg_router(int msgid, msg_callback* msg_cb, void* user_data)
    {
        if (router_.find(msgid) != router_.end())
        {
            //该msgid的回调业务已经存在
            return -1;
        }

        // printf("add msg cb msgid = %d\n", msgid);
        router_[msgid].first = msg_cb;
        router_[msgid].second = user_data;

        return 0;
    }

    void call(const char* data, uint32_t msglen, int msgid, net_connection* net_conn)
    {
        // printf("call msgid = %d\n", msgid);
        //判断msgid对应的回调是否存在
        if (router_.find(msgid) == router_.end())
        {
            fprintf(stderr, "msgid %d is not registered...\n", msgid);
            return;
        }

        msg_callback* callback = router_[msgid].first;
        void* user_data = router_[msgid].second;
        callback(data, msglen, msgid, net_conn, user_data);
        // printf("================\n");
    }
private:
    //针对消息类型的路由分发，key为msgid，value为注册的回调业务函数与其参数组成的pair
    std::unordered_map<int, std::pair<msg_callback*, void*>> router_;
};