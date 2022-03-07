#pragma once

#include "event_loop.h"

struct task_msg
{
    enum TASK_TYPE {
        NEW_CONN,  //新建连接任务
        NEW_TASK,  //一般任务
    };

    TASK_TYPE type;  //任务类型

    union {
        //针对NEW_CONN新建连接任务，需要传递connfd
        int connfd;
        //针对一般任务，可以给任务提供一个回调函数(暂时用不上)
        struct {
            task_func task_cb;
            void* args;
        };
    };
};