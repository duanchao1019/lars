#pragma once

#include "event_base.h"

#include <vector>
#include <utility>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>

#define MAXEVENTS 10

typedef std::unordered_map<int, io_event> io_event_map;
typedef io_event_map::iterator io_event_map_it;
typedef std::unordered_set<int> listen_fd_set;

//异步任务(NEW_TASK)回调函数类型
typedef void (*task_func)(event_loop* loop, void* args);

class event_loop
{
public:
    //构造，初始化epoll
    event_loop();

    //阻塞循环监听并处理事件
    void event_process();

    //添加一个io事件到loop中
    void add_io_event(int fd, io_callback* proc, int mask, void* args = nullptr);

    //从loop中删除一个io事件
    void del_io_event(int fd);

    //删除一个io事件的EPOLLIN/EPOLLOUT
    void del_io_event(int fd, int mask);

    //获取全部监听时间的fd集合
    void get_listen_fds(listen_fd_set& fds);

    //======异步任务task模块需要的方法========
    //添加一个任务task到ready_tasks中
    void add_task(task_func func, void* args);
    //执行ready_tasks中的全部task
    void execute_ready_tasks();
    //====================================

private:
    int epfd_;   //epoll fd

    io_event_map io_events_;    //当前event_loop监控的fd和对应事件的关系，并非所有的fd都处在监听，只是由该event_loop负责管理而已
    listen_fd_set listen_fds_;  //当前event_loop一共哪些fd正在监听，通过该容器，服务器可以知道哪些客户端在线，于是可以实现服务器主动向客户端发送消息

    struct epoll_event fired_events_[MAXEVENTS];  //通过epoll_wait返回的被激活事件集合

    
    typedef std::pair<task_func, void*> task_func_pair;
    std::vector<task_func_pair> ready_tasks_;   //需要在该event_loop中执行的任务集合
};