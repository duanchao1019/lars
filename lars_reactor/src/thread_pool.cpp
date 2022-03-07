#include "thread_pool.h"
#include "tcp_conn.h"

//处理任务消息的主流程
//只要有人调用thread_queue::send()方法就会触发该函数
void deal_task_message(event_loop* loop, int fd, void* args)
{
    //得到是哪个消息队列触发的
    thread_queue<task_msg>* queue = (thread_queue<task_msg>*)args;

    //将queue中的全部任务取出来
    std::queue<task_msg> tasks;
    queue->recv(tasks);

    while (!tasks.empty())
    {
        task_msg task = tasks.front();
        tasks.pop();

        if (task.type == task_msg::NEW_CONN)
        {
            tcp_conn* conn = new tcp_conn(task.connfd, loop);
            if (conn == NULL)
            {
                fprintf(stderr, "in thread new tcp_conn error\n");
                exit(1);
            }
            printf("[thread]: get new connection succ!\n");
        }
        else if (task.type == task_msg::NEW_TASK)
        {
            //是一个新的普通任务

            //当前的loop就是一个thread的事件监控loop，让当前的loop去触发task任务的回调
            loop->add_task(task.task_cb, task.args);
        }
        else
        {
            fprintf(stderr, "unknown task!\n");
        }
    }
}

//一个线程的主业务函数
void* thread_main(void* args)
{
    thread_queue<task_msg>* queue = (thread_queue<task_msg>*)args;

    //每个线程都应该有一个event_loop来监控客户端连接的读写事件
    event_loop* loop = new event_loop();
    if (loop == NULL)
    {
        fprintf(stderr, "new event_loop error\n");
        exit(1);
    }

    queue->set_loop(loop);
    queue->set_callback(deal_task_message, queue);

    loop->event_process();

    return NULL;
}

thread_pool::thread_pool(int thread_num)
: queues_(NULL), 
  thread_num_(thread_num),
  tids_(NULL),
  index_(0)
{
    if (thread_num <= 0)
    {
        fprintf(stderr, "thread_num <= 0\n");
        exit(1);
    }

    //任务队列的个数和线程的个数一致
    queues_ = new thread_queue<task_msg>*[thread_num];
    tids_ = new pthread_t[thread_num];

    int ret;
    for (int i = 0; i < thread_num; i++)
    {
        //创建一个线程
        printf("create %d thread\n", i);
        //给这个线程创建一个任务消息队列
        queues_[i] = new thread_queue<task_msg>();
        ret = pthread_create(&tids_[i], NULL, thread_main, queues_[i]);
        if (ret == -1)
        {
            perror("thread_pool, create thread");
            exit(1);
        }

        //将线程脱离
        pthread_detach(tids_[i]);
    }
}

thread_queue<task_msg>* thread_pool::get_thread()
{
    if (index_ == thread_num_)
    {
        index_ = 0;
    }

    return queues_[index_];
}

void thread_pool::send_task(task_func func, void* args)
{
    task_msg task;
    task.type = task_msg::NEW_TASK;
    task.task_cb = func;
    task.args = args;

    for (int i = 0; i < thread_num_; i++)
    {   //向每一个线程的任务队列中发送这个任务
        thread_queue<task_msg>* queue = queues_[i];
        queue->send(task);
    }
}