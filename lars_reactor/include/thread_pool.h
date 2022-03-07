#pragma once

#include <pthread.h>
#include "task_msg.h"
#include "thread_queue.h"


class thread_pool
{
public:
    //构造，初始化线程池，开辟thread_num_个线程
    thread_pool(int thread_num);

    //通过轮询的方式获取一个线程的thread_queue
    thread_queue<task_msg>* get_thread();

    //发送一个task给thread_pool里的全部thread
    void send_task(task_func func, void* args = NULL);

private:
    //queues_是一个数组指针，其指向的数组中的每个元素都是一个thread_queue<task_msg>*(即一个指向消息队列的指针)
    thread_queue<task_msg>** queues_;   //每个线程对应一个thread_queue

    int thread_num_;  //线程池中的线程个数

    pthread_t* tids_;  //已经启动的全部线程的编号(实际这个就是线程池)

    int index_;  //当前选中的线程队列的下标，用来向各个线程间轮询发送任务
};