#pragma once

#include <unordered_map>

#include "io_buf.h"

typedef std::unordered_map<int, io_buf*> pool_t;

//内存池容量的最大限制，单位是KB，所以该限制是5G
#define MEM_LIMIT (5U * 1024 * 1024)

//内存刻度
enum MEM_CAP
{
    m4K = 4096,
    m16K = 16384,
    m64K = 65536,
    m256K = 262144,
    m1M = 1048576,
    m4M = 4194304,
    m8M = 8388608
};

class buf_pool
{
public:
    //初始化单例对象
    static void init()
    {
        instance_ = new buf_pool();
    }
    //获取单例对象
    static buf_pool* instance()
    {
        pthread_once(&once_, init);
        return instance_;
    }

    //从内存池中申请一块内存
    io_buf* alloc_buf(int N);
    io_buf* alloc_buf() { return alloc_buf(m4K); }

    //重置一个io_buf，放回pool中
    void revert(io_buf* buffer);

private:
    void make_io_buf_list(int cap, int num);  //在构造函数中调用，用来初始化内存池中某种大小的io_buf链表

private:
    buf_pool();
    buf_pool(const buf_pool&);
    const buf_pool& operator=(const buf_pool&);
    
    static buf_pool* instance_;  //单例对象
    static pthread_once_t once_; //用于保证创建单例的init方法只执行一次的锁

    pool_t pool_;          //内存池

    uint64_t total_mem_;  //内存池的总内存大小，单位为KB

    static pthread_mutex_t mutex_;  //用户保护内存池链表的互斥锁
};