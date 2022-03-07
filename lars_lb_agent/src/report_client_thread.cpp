#include "lars_reactor.h"
#include "main_server.h"
#include <pthread.h>
#include <string>

//只要thread_queue有数据，loop就会触发此回调函数来处理业务
void new_report_request(event_loop* loop, int fd, void* args)
{
    tcp_client* client = (tcp_client*)args;

    //1.将请求数据从thread_queue中取出
    std::queue<lars::ReportStatusRequest> msgs;

    //2.将数据放在queue队列中
    report_queue->recv(msgs);

    //3.遍历队列，通过client依次将每个msg发送给reporter service
    while (!msgs.empty())
    {
        lars::ReportStatusRequest req = msgs.front();
        msgs.pop();

        std::string requestString;
        req.SerializeToString(&requestString);

        client->send_message(requestString.c_str(), requestString.size(), lars::ID_ReportStatusRequest);
    }
}


void* report_client_thread(void* args)
{
    printf("report client thread start\n");

    event_loop loop;

    std::string ip = config_file::instance()->GetString("reporter", "ip", "");
    short port = config_file::instance()->GetNumber("reporter", "port", 0);

    tcp_client client(&loop, ip.c_str(), port, "reporter client");

    report_queue->set_loop(&loop);
    report_queue->set_callback(new_report_request, &client);

    loop.event_process();

    return NULL;

}

void start_report_client()
{
    pthread_t tid;

    int ret = pthread_create(&tid, NULL, report_client_thread, NULL);
    if (ret == -1)
    {
        perror("pthread_create");
        exit(1);
    }

    pthread_detach(tid);
}