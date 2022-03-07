#include "buf_pool.h"
#include <assert.h>

//单例对象
buf_pool* buf_pool::instance_ = nullptr;

//用于保证创建单例的init方法全局只执行一次
pthread_once_t buf_pool::once_ = PTHREAD_ONCE_INIT;

pthread_mutex_t buf_pool::mutex_ = PTHREAD_MUTEX_INITIALIZER;

void buf_pool::make_io_buf_list(int cap, int num)
{
    io_buf* prev;

    /*----------开辟num块cap大小的io_buf块的内存池-----------*/
    pool_[cap] = new io_buf(cap);
    if (pool_[cap] == nullptr)
    {
        fprintf(stderr, "new io_buf error\n");
        exit(1);
    }
    prev = pool_[cap];

    //cap大小的io_buf预先开辟num个，总共 cap*num KB供开发者使用
    for (int i = 1; i < num; ++i)
    {
        prev->next = new io_buf(cap);
        if (prev->next == nullptr)
        {
            fprintf(stderr, "new io_buf error\n");
            exit(1);
        }
        prev = prev->next;
    }

    total_mem_ += cap * num;
}


//构造函数私有化
buf_pool::buf_pool()
: total_mem_(0)
{
    make_io_buf_list(m4K, 5000);
    make_io_buf_list(m16K, 1000);
    make_io_buf_list(m64K, 500);
    make_io_buf_list(m256K, 100);
    make_io_buf_list(m1M, 50);
    make_io_buf_list(m4M, 200);
    make_io_buf_list(m8M, 10); 
}

io_buf* buf_pool::alloc_buf(int N)
{
    int index;
    if (N <= m4K)
        index = m4K;
    else if (N <= m16K)
        index = m16K;
    else if (N <= m64K)
        index = m64K;
    else if (N <= m256K)
        index = m256K;
    else if (N <= m1M)
        index = m1M;
    else if (N <= m4M)
        index = m4M;
    else if (N <= m8M)
        index = m8M;
    else
        return nullptr;

    pthread_mutex_lock(&mutex_);

    //如果内存池中存在index大小的内存块
    if (pool_[index] != nullptr)
    {
        io_buf* target = pool_[index];
        pool_[index] = target->next;

        pthread_mutex_unlock(&mutex_);

        target->next = nullptr;
        return target;
    }

    //如果内存池中不存在index大小的内存块
    //当前的开辟空间已经超过内存池容量的最大限制，无法继续开辟空间
    if (total_mem_ + index / 1024 > MEM_LIMIT)
    {
        fprintf(stderr, "already use too many memory!\n");
        exit(1);
    }
    io_buf* new_buf = new io_buf(index);
    if (new_buf == nullptr)
    {
        fprintf(stderr, "new io_buf error\n");
        exit(1);
    }
    total_mem_ += index / 1024;

    pthread_mutex_unlock(&mutex_);

    return new_buf;
}

void buf_pool::revert(io_buf* buffer)
{
    int index = buffer->capacity_;
    buffer->length_ = 0;
    buffer->head_ = 0;

    pthread_mutex_lock(&mutex_);
    assert(pool_.find(index) != pool_.end());
    buffer->next = pool_[index];
    pool_[index] = buffer;
    pthread_mutex_unlock(&mutex_);
}