#pragma once

#include <stdint.h>

/* 被代理的主机的基本信息 */
struct host_info
{
    host_info(uint32_t ip, int port, uint32_t init_vsucc)
    : ip(ip),
      port(port),
      vsucc(init_vsucc),
      verr(0),
      rsucc(0),
      rerr(0),
      contin_succ(0),
      contin_err(0),
      overload(false)
    {
        //host_info初始化构造函数
    }

    void set_idle();

    void set_overload();

    bool check_window();

    uint32_t ip;
    int port;

    uint32_t vsucc;        //虚拟成功次数
    uint32_t verr;         //虚拟失败次数
    uint32_t rsucc;        //真实成功次数
    uint32_t rerr;         //真实失败次数
    uint32_t contin_succ;  //连续成功次数
    uint32_t contin_err;  //连续失败次数

    long idle_ts;  //当主机更变为idle状态的时间
    long overload_ts;  //当主机更变为overload状态的时间
    bool overload;  //是否过载
};