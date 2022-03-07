#pragma once

#include <queue>
#include <unistd.h>
#include <sys/eventfd.h>

#include "event_loop.h"


template <typename T>
class thread_queue
{
public:
    thread_queue()
    : loop_(NULL)
    {
        pthread_mutex_init(&queue_mutex_, NULL);
        evfd_ = eventfd(0, EFD_NONBLOCK);
        if (evfd_ == -1)
        {
            perror("eventfd(0, EFD_NONBLOCK)");
            exit(1);
        }
    }

    ~thread_queue()
    {
        pthread_mutex_destroy(&queue_mutex_);
        close(evfd_);
    }

    //向队列中添加一个任务
    void send(const T& task)
    {
        //触发消息事件的占位传输内容
        unsigned long long idle_num = 1;

        pthread_mutex_lock(&queue_mutex_);
        //将任务添加到队列
        queue_.push(task);

        //向evfd_写，触发EPOLLIN事件，来处理该任务
        int ret = write(evfd_, &idle_num, sizeof(unsigned long long));
        if (ret == -1)
        {
            perror("write evfd_");
        }

        pthread_mutex_unlock(&queue_mutex_);
    }

    //获取含有任务的队列
    void recv(std::queue<T>& new_queue)
    {
        unsigned long long idle_num;

        pthread_mutex_lock(&queue_mutex_);
        
        //把占位的数据读出来，确保底层缓冲区中没有数据存留
        int ret = read(evfd_, &idle_num, sizeof(unsigned long long));
        if (ret == -1)
        {
            perror("read evfd_");
        }

        //把当前含有任务的队列拷贝出去，用一个空队列换回当前队列，同时清空自身队列，需要确保new_queue为一个空队列
        std::swap(queue_, new_queue);

        pthread_mutex_unlock(&queue_mutex_);
    }

    //设置当前消息队列是被哪个event_loop所监控
    void set_loop(event_loop* loop)
    {
        this->loop_ = loop;
    }

    //设置当前消息队列的每个任务触发的回调业务
    void set_callback(io_callback* cb, void* args = NULL)
    {
        if (loop_ != NULL)
        {
            loop_->add_io_event(evfd_, cb, EPOLLIN, args);
        }
    }

    event_loop* get_loop()
    {
        return loop_;
    }

private:
    int evfd_;   //触发读取消息队列中的任务的fd
    event_loop* loop_;  //当前消息队列所绑定在哪个event_loop事件循环中
    std::queue<T> queue_;  //任务队列
    pthread_mutex_t queue_mutex_;  //添加任务，读取任务的保护锁
};