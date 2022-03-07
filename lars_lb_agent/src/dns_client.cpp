#include "lars_reactor.h"
#include "main_server.h"
#include <pthread.h>

static void conn_init(net_connection* conn, void* args)
{
    for (int i = 0; i < 3; i++)
    {
        r_lb[i]->reset_lb_status();
    }
}

void new_dns_request(event_loop* loop, int fd, void* args)
{
    tcp_client* client = (tcp_client*)args;

    std::queue<lars::GetRouteRequest> msgs;

    dns_queue->recv(msgs);

    while (!msgs.empty())
    {
        lars::GetRouteRequest req = msgs.front();
        msgs.pop();

        std::string requestString;
        req.SerializeToString(&requestString);

        client->send_message(requestString.c_str(), requestString.size(), lars::ID_GetRouteRequest);
    }
}

void deal_recv_route(const char* data, uint32_t len, int msgid, net_connection* conn, void* user_data)
{
    lars::GetRouteResponse rsp;

    rsp.ParseFromArray(data, len);

    int modid = rsp.modid();
    int cmdid = rsp.cmdid();
    int index = (modid + cmdid) % 3;

    r_lb[index]->update_host(modid, cmdid, rsp);
}

void* dns_client_thread(void* args)
{
    printf("dns client thread start\n");

    event_loop loop;

    std::string ip = config_file::instance()->GetString("dnsserver", "ip", "");
    short port = config_file::instance()->GetNumber("dnsserver", "port", 0);

    tcp_client client(&loop, ip.c_str(), port, "dns client");

    dns_queue->set_loop(&loop);
    dns_queue->set_callback(new_dns_request, &client);

    client.add_msg_router(lars::ID_GetRouteResponse, deal_recv_route);

    client.set_conn_start(conn_init);

    loop.event_process();

    return NULL;
}

void start_dns_client()
{
    pthread_t tid;

    int ret = pthread_create(&tid, NULL, dns_client_thread, NULL);
    if (ret == -1)
    {
        perror("pthread_create");
        exit(1);
    }

    pthread_detach(tid);
}