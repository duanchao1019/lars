#include <stdio.h>
#include <string.h>
#include "tcp_client.h"
#include "echoMessage.pb.h"


struct Qps
{
    Qps()
    {
        last_time = time(NULL);
        succ_cnt = 0;
    }

    long last_time; //最后一次发包的时间 单位为ms
    int succ_cnt;   //成功收到服务器回显的次数
};

//客户端业务
void busy(const char* data, uint32_t len, int msgid, net_connection* conn, void* user_data)
{
    Qps* qps = (Qps *)user_data;  //用户参数

    qps_test::EchoMessage request, response;

    //解析服务端传来的protobuf数据
    if (response.ParseFromArray(data, len) == false)
    {
        printf("server call back data error\n");
        return;
    }

    //判断数据内容是否回显一致
    if (response.content() == "Hello, Lars!!!")
    {
        //服务器的成功响应请求次数加1
        qps->succ_cnt++;
    }

    long curr_time = time(NULL);
    if (curr_time - qps->last_time >= 1)
    {
        //如果当前时间比最后记录时间大于1秒，那么我们进行记录
        printf("---------->qps = %d<----------\n", qps->succ_cnt);
        qps->last_time = curr_time;
        qps->succ_cnt = 0;
    }

    //给服务端发送新的请求
    request.set_id(response.id() + 1);
    request.set_content(response.content());

    std::string requestString;
    request.SerializeToString(&requestString);

    conn->send_message(requestString.c_str(), requestString.size(), msgid);
}

//创建连接成功之后调用的hook函数
void connection_start(net_connection* conn, void* args)
{
    qps_test::EchoMessage request;

    request.set_id(1);
    request.set_content("Hello, Lars!!!");

    std::string requestString;
    request.SerializeToString(&requestString);

    int msgid = 1;  //与server端的消息路由的类型一致
    conn->send_message(requestString.c_str(), requestString.size(), msgid);
}

void* thread_main(void* args)
{
    //给服务端发包
    event_loop loop;

    tcp_client client(&loop, "127.0.0.1", 7777, "qps client");

    Qps qps;

    //设置回调
    client.add_msg_router(1, busy, (void *)&qps);

    //设置连接创建成功之后的hook
    client.set_conn_start(connection_start);

    loop.event_process();

    return NULL;
}


int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("usage: ./client [threadNum]\n");
        return 1;
    }

    //创建N个线程，用来模拟N个客户端
    int thread_num = atoi(argv[1]);
    pthread_t* threads;
    threads = new pthread_t[thread_num];

    for (int i = 0; i < thread_num; i++)
    {
        pthread_create(&threads[i], NULL, thread_main, NULL);

    }
    
    for (int i = 0; i < thread_num; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}