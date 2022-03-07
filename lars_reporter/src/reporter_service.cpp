#include "lars_reactor.h"
#include "lars.pb.h"
#include "store_report.h"
#include <string>

thread_queue<lars::ReportStatusRequest>** reporter_queues = NULL;
int thread_cnt = 0;

extern void* store_main(void* args);


void get_report_status(const char* data, uint32_t len, int msgid, net_connection* conn, void* user_data)
{
    lars::ReportStatusRequest req;
    req.ParseFromArray(data, len);

    //将上报数据存储到db
    // StoreReport sr;
    // sr.store(req);

    //消息轮询发送到各个线程的消息队列中
    static int index = 0;
    reporter_queues[index]->send(req);
    index++;
    index = index % thread_cnt;
}

void create_reportdb_threads()
{
    thread_cnt = config_file::instance()->GetNumber("reporter", "db_thread_cnt", 3);
    
    //开启线程池的消息队列
    reporter_queues = new thread_queue<lars::ReportStatusRequest>*[thread_cnt];

    if (reporter_queues == NULL)
    {
        fprintf(stderr, "create thread_queue<lars::ReportStatusRequest>*[%d] error", thread_cnt);
        exit(1);
    }

    for (int i = 0; i < thread_cnt; i++)
    {
        //给当前线程创建一个消息队列
        reporter_queues[i] = new thread_queue<lars::ReportStatusRequest>();
        if (reporter_queues[i] == NULL)
        {
            fprintf(stderr, "create thread_queue %d error\n", i);
            exit(1);
        }

        pthread_t tid;
        int ret = pthread_create(&tid, NULL, store_main, reporter_queues[i]);
        if (ret == -1)
        {
            perror("pthread_create");
            exit(1);
        }

        pthread_detach(tid);
    }
}

int main(int argc, char* argv[])
{
    event_loop loop;

    //加载配置文件
    config_file::setPath("../conf/lars_reporter.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "127.0.0.1");
    short port = config_file::instance()->GetNumber("reactor", "port", 7779);

    //创建tcp_server
    tcp_server server(ip.c_str(), port, &loop);

    //添加数据上报请求处理的消息分发处理业务
    server.add_msg_router(lars::ID_ReportStatusRequest, get_report_status);

    //为了防止在业务中出现IO阻塞，那么需要启动一个线程池对IO进行操作，接收业务的请求存储消息
    create_reportdb_threads();

    //启动事件监听
    loop.event_process();

    return 0;
}