#include "event_loop.h"
#include <assert.h>


//构造，初始化epoll
event_loop::event_loop()
{
    epfd_ = epoll_create1(0);  //flag=0等价于epoll_create
    if (epfd_ == -1)
    {
        fprintf(stderr, "epoll_create error\n");
        exit(1);
    }
}


//阻塞循环监听并处理事件
void event_loop::event_process()
{
    while (true)
    {
        io_event_map_it ev_it;

        //打印一些调试信息(当前event_loop正在监听的fd集合)
        // printf("wait IN OUT event\n");
        // for (auto it = listen_fds_.begin(); it != listen_fds_.end(); ++it)
        // {
        //     printf("fd %d is listenning by event_loop...\n", *it);
        // }

        int nfds = epoll_wait(epfd_, fired_events_, MAXEVENTS, -1);
        if (nfds < 0)
        {
            fprintf(stderr, "epoll_wait error\n");
            exit(1);
        }
        for (int i = 0; i < nfds; ++i)
        {
            int fired_fd = fired_events_[i].data.fd;
            //通过激活的fd找到对应的事件
            ev_it = io_events_.find(fired_fd);
            assert(ev_it != io_events_.end());

            io_event* ev = &(ev_it->second);

            if (fired_events_[i].events & EPOLLIN)
            {
                //读事件，调用读回调函数
                void* args = ev->rcb_args;
                ev->read_callback(this, fired_fd, args);
            }
            else if (fired_events_[i].events & EPOLLOUT)
            {
                //写事件，调用写回调函数
                void* args = ev->wcb_args;
                ev->write_callback(this, fired_fd, args);
            }
            else if (fired_events_[i].events & (EPOLLHUP | EPOLLERR))
            {
                //在水平触发模式下，如果有事件触发但未处理，可能会出现HUP事件，这是正常的，需要继续处理读写，如果没有监听读写事件则清空
                if (ev->read_callback != nullptr)
                {
                    void* args = ev->rcb_args;
                    ev->read_callback(this, fired_fd, args);
                }
                else if (ev->write_callback != nullptr)
                {
                    void* args = ev->wcb_args;
                    ev->write_callback(this, fired_fd, args);
                }
                else
                {
                    //删除
                    fprintf(stderr, "fd %d get error, delete it from epoll\n", fired_fd);
                    this->del_io_event(fired_fd);
                }
            }
        }

        //每次处理完一组epoll_wait触发的事件后，处理异步任务
        this->execute_ready_tasks();
    }
}


//添加一个io事件到loop中
void event_loop::add_io_event(int fd, io_callback* proc, int mask, void* args)
{
    int final_mask;
    int op;

    //1.找到当前fd是否已经有事件
    io_event_map_it it = io_events_.find(fd);
    if (it == io_events_.end())
    {   //如果没有，操作就是ADD
        final_mask = mask;
        op = EPOLL_CTL_ADD;
    }
    else
    {   //如果有，操作就是MOD
        final_mask = it->second.mask | mask;
        op = EPOLL_CTL_MOD;
    }

    //2.注册回调函数
    if (mask & EPOLLIN)
    {
        io_events_[fd].read_callback = proc;
        io_events_[fd].rcb_args = args;
    }
    else if (mask & EPOLLOUT)
    {
        io_events_[fd].write_callback = proc;
        io_events_[fd].wcb_args = args;
    }

    //3.epoll_ctl将事件添加到epoll中
    io_events_[fd].mask = final_mask;
    struct epoll_event event;
    event.data.fd = fd;
    event.events = final_mask;
    if (epoll_ctl(epfd_, op, fd, &event) == -1)
    {
        fprintf(stderr, "epoll_ctl %d error\n", fd);
        return;
    }

    //4.将fd添加到监听集合中
    listen_fds_.insert(fd);
}


//从loop中删除一个io事件
void event_loop::del_io_event(int fd)
{
    io_events_.erase(fd);
    listen_fds_.erase(fd);
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
}


//删除一个io事件的EPOLLIN/EPOLLOUT
void event_loop::del_io_event(int fd, int mask)
{
    io_event_map_it it = io_events_.find(fd);
    if (it == io_events_.end())
    {
        return;
    }

    int& o_mask = it->second.mask;
    o_mask = o_mask & (~mask);  //删除mask事件

    if (o_mask == 0)
    {
        this->del_io_event(fd);
    }
    else
    {
        struct epoll_event event;
        event.data.fd = fd;
        event.events = o_mask;
        epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event);
    }
}

void event_loop::get_listen_fds(listen_fd_set& fds)
{
    fds = listen_fds_;
}

void event_loop::add_task(task_func func, void* args)
{
    task_func_pair func_pair(func, args);
    ready_tasks_.push_back(func_pair);
}

void event_loop::execute_ready_tasks()
{
    for (auto it = ready_tasks_.begin(); it != ready_tasks_.end(); ++it)
    {
        task_func func = it->first;
        void* args = it->second;

        //执行任务
        func(this, args);
    }

    //全部任务执行完毕后，清空当前的ready_tasks_
    ready_tasks_.clear();
}