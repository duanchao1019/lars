#pragma once


class event_loop;


//IO事件触发的回调函数
typedef void io_callback(event_loop* loop, int fd, void* args);


//IO事件封装
struct io_event
{
    io_event()
    : mask(0),
      read_callback(nullptr),
      write_callback(nullptr),
      rcb_args(nullptr),
      wcb_args(nullptr)
    {
        
    }

    int mask;   //事件类型 EPOLLIN EPOLLOUT
    io_callback* read_callback;  //EPOLLIN事件触发的回调
    io_callback* write_callback; //EPOLLOUT事件触发的回调
    void* rcb_args;  //read_callback的回调函数参数
    void* wcb_args;  //write_callback的回调函数参数
};