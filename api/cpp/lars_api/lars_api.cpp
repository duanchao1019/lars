#include "lars_api.h"
#include "lars.pb.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

lars_client::lars_client()
: seqid_(0)
{
    printf("lars_client()\n");

    //1.初始化服务器地址
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &servaddr.sin_addr);

    //2.创建3个UDP socket
    for (int i = 0; i < 3; i++)
    {
        sockfd_[i] = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
        if (sockfd_[i] == -1)
        {
            perror("socket()");
            exit(1);
        }
        //agent的3个端口默认为8888,8889,8890
        servaddr.sin_port = htons(8888 + i);
        int ret = connect(sockfd_[i], (const struct sockaddr *)&servaddr, sizeof(servaddr));
        if (ret == -1)
        {
            perror("connect()");
            exit(1);
        }
        printf("connection agent udp server success!\n");
    }
}

lars_client::~lars_client()
{
    printf("~lars_client()\n");

    for (int i = 0; i < 3; i++)
    {
        close(sockfd_[i]);
    }
}

int lars_client::get_host(int modid, int cmdid, std::string& ip, int& port)
{
    uint32_t seq = seqid_++;

    //1.封装请求消息
    lars::GetHostRequest req;
    req.set_seq(seq);  //序列编号
    req.set_modid(modid);
    req.set_cmdid(cmdid);

    //2.send
    char write_buf[4096], read_buf[20*4096];
    //消息头
    msg_head head;
    head.msglen = req.ByteSizeLong();
    head.msgid = lars::ID_GetHostRequest;
    memcpy(write_buf, &head, MSG_HEAD_LEN);
    //消息体
    req.SerializeToArray(write_buf + MSG_HEAD_LEN, head.msglen);
    //简单的hash来决定发送给哪个udp server
    int index = (modid + cmdid) % 3;
    int ret = sendto(sockfd_[index], write_buf, head.msglen + MSG_HEAD_LEN, 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return lars::RET_SYSTEM_ERROR;
    }

    //3.recv
    int message_len;
    lars::GetHostResponse rsp;
    do {
        message_len = recvfrom(sockfd_[index], read_buf, sizeof(read_buf), 0, NULL, 0);
        if (message_len == -1)
        {
            perror("recvfrom");
            return lars::RET_SYSTEM_ERROR;
        }

        //消息头
        memcpy(&head, read_buf, MSG_HEAD_LEN);
        if (head.msgid != lars::ID_GetHostResponse)
        {
            fprintf(stderr, "message ID error!\n");
            return lars::RET_SYSTEM_ERROR;
        }
        //消息体
        ret = rsp.ParseFromArray(read_buf + MSG_HEAD_LEN, message_len - MSG_HEAD_LEN);
        if (!ret)
        {
            fprintf(stderr, "message format error: head.msglen = %d, message_len = %d, \
                    message_len - MSG_HEAD_LEN = %d, head msgid = %d, ID_GetHostResponse = %d\n", 
                    head.msglen, message_len, message_len - MSG_HEAD_LEN, head.msgid, lars::ID_GetHostResponse);
            return lars::RET_SYSTEM_ERROR;
        }
    } while (rsp.seq() < seq);

    if (rsp.seq() != seq || rsp.modid() != modid || rsp.cmdid() != cmdid)
    {
        fprintf(stderr, "message formate error\n");
        return lars::RET_SYSTEM_ERROR;
    }

    //4.处理消息
    if (rsp.retcode() == lars::RET_SUCC)
    {
        lars::HostInfo host = rsp.host();
        
        struct in_addr inaddr;
        inaddr.s_addr = htonl(host.ip());
        ip = inet_ntoa(inaddr);
        port = host.port();
    }

    return rsp.retcode(); //lars::RET_SUCC
}

void lars_client::report(int modid, int cmdid, const std::string& ip, int port, int retcode)
{
    lars::ReportHostRequest req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    req.set_retcode(retcode);

    lars::HostInfo* hip = req.mutable_host();
    struct in_addr inaddr;
    inet_aton(ip.c_str(), &inaddr);
    int ip_num = inaddr.s_addr;
    hip->set_ip(ip_num);
    hip->set_port(port);

    char write_buf[4096];
    
    msg_head head;
    head.msglen = req.ByteSizeLong();
    head.msgid = lars::ID_ReportHostRequest;
    memcpy(write_buf, &head, MSG_HEAD_LEN);
    req.SerializeToArray(write_buf + MSG_HEAD_LEN, head.msglen);

    int index = (modid + cmdid) % 3;
    int ret = sendto(sockfd_[index], write_buf, head.msglen + MSG_HEAD_LEN, 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
    }
}

int lars_client::get_route(int modid, int cmdid, route_set& route)
{
    lars::GetRouteRequest req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);

    char write_buf[4096], read_buf[20*4096];
    msg_head head;
    head.msglen = req.ByteSizeLong();
    head.msgid = lars::ID_API_GetRouteRequest;
    memcpy(write_buf, &head, MSG_HEAD_LEN);

    req.SerializeToArray(write_buf + MSG_HEAD_LEN, head.msglen);

    int index = (modid + cmdid) % 3;
    int ret = sendto(sockfd_[index], write_buf, head.msglen + MSG_HEAD_LEN, 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return lars::RET_SYSTEM_ERROR;
    }

    lars::GetRouteResponse rsp;

    int msg_len = recvfrom(sockfd_[index], read_buf, sizeof(read_buf), 0, NULL, 0);
    if (msg_len == -1)
    {
        perror("recvfrom");
        return lars::RET_SYSTEM_ERROR;
    }

    memcpy(&head, read_buf, MSG_HEAD_LEN);
    if (head.msgid != lars::ID_API_GetRouteResponse)
    {
        fprintf(stderr, "message ID error!\n");
        return lars::RET_SYSTEM_ERROR;
    }

    ret = rsp.ParseFromArray(read_buf + MSG_HEAD_LEN, msg_len - MSG_HEAD_LEN);
    if (!ret)
    {
        fprintf(stderr, "message format error: head.msglen = %d, message_len = %d, message_len - MSG_HEAD_LEN = %d, head msgid = %d, ID_GetRouteResponse = %d\n",
                head.msglen, msg_len, msg_len - MSG_HEAD_LEN, head.msgid, lars::ID_GetRouteResponse);
        return lars::RET_SYSTEM_ERROR;
    }

    for (int i = 0; i < rsp.host_size(); i++)
    {
        const lars::HostInfo& host = rsp.host(i);
        struct in_addr inaddr;
        inaddr.s_addr = htonl(host.ip());
        std::string ip = inet_ntoa(inaddr);
        int port = host.port();
        route.push_back(ip_port(ip, port));
    }

    return lars::RET_SUCC;
}

int lars_client::reg_init(int modid, int cmdid)
{
    route_set route;

    int retry_cnt = 0;

    while (route.empty() && retry_cnt < 3)
    {
        get_route(modid, cmdid, route);
        if (route.empty())
        {
            usleep(50000);
        }
        else
        {
            break;
        }
        ++retry_cnt;
    }

    if (route.empty())
    {
        return lars::RET_NOEXIST;
    }

    return lars::RET_SUCC;
}