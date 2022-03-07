#pragma once

#include "io_buf.h"
#include "buf_pool.h"

class reactor_buf
{
public:
    reactor_buf()
    : buf_(nullptr)
    {

    }
    ~reactor_buf()
    {
        clear();
    }

    //当前的buf中还有多少有效数据
    const int length() const
    {
        return buf_ != nullptr ? buf_->length_ : 0;
    }

    //从buf_中取出数据
    void pop(int len);

    //将buf_放回buf_pool中
    void clear();

protected:  //声明成protected是为了在子类input_buf和output_buf中能访问buf_成员
    io_buf* buf_;
};


//读（输入）缓存buffer
class input_buf : public reactor_buf
{
public:
    //从一个fd中读取数据到reactor_buf中
    int read_data(int fd);

    //取出读到的数据
    const char* data() const
    {
        return buf_ != nullptr ? buf_->data_ + buf_->head_ : nullptr;
    }

    //重置缓冲区
    void adjust()
    {
        if (buf_ != nullptr)
        {
            buf_->adjust();
        }
    }
};


class output_buf : public reactor_buf
{
public:
    //将一段数据写到一个reactor_buf中，但并没有发送出去
    int send_data(const char* data, int datalen);

    //将reactor_buf中的数据写到一个fd中（发送）
    int write2fd(int fd);
};