#include "reactor_buf.h"
#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


void reactor_buf::pop(int len)
{
    assert(buf_ != nullptr && buf_->length_ >= len);

    buf_->pop(len);

    //如果此时buf_的可用数据长度为0
    if (buf_->length_ == 0)
    {  
        this->clear();
    }
}

//将buf_放回buf_pool中
void reactor_buf::clear()
{
    if (buf_ != nullptr)
    {
        buf_pool::instance()->revert(buf_);
        buf_ = nullptr;  
    }
}


//从一个fd中读取数据到reactor_buf中
int input_buf::read_data(int fd)
{
    int need_read;  //硬件有多少数据可以读

    //一次性读出所有数据，但在此之前，需要知道一共有多少数据是可读的
    //需要给fd设置FIONREAD，得到read缓冲区中的可读数据字节数
    if (ioctl(fd, FIONREAD, &need_read) == -1)
    {
        fprintf(stderr, "ioctl FIONREAD error\n");
        return -1;
    }

    if (buf_ == nullptr)
    {
        //如果buf_为空，从内存池申请
        buf_ = buf_pool::instance()->alloc_buf(need_read);
        if (buf_ == nullptr)
        {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    }
    else
    {
        assert(buf_->head_ == 0);
        //如果buf_可用，判断容量是否够存
        if (buf_->capacity_ - buf_->length_ < (int)need_read)
        {
            //不够存，从内存池重新申请容量更大的一块io_buf
            io_buf* new_buf = buf_pool::instance()->alloc_buf(need_read + buf_->length_);
            if (new_buf == nullptr)
            {
                fprintf(stderr, "no idle buf for alloc\n");
                return -1;
            }
            //将之前的buf_的数据拷贝到新申请的new_buf中
            new_buf->copy(buf_);
            //将之前的buf_放回内存池中
            buf_pool::instance()->revert(buf_);
            //将新申请的new_buf成为当前的buf_
            buf_ = new_buf;
        }
    }

    //读取数据
    int already_read = 0;
    do {
        if (need_read == 0) 
        {   //如果fd是阻塞的，那么对方未写数据的情况下，ioctl会将need_read设置为0，此时read会阻塞
            already_read = read(fd, buf_->data_ + buf_->length_, m4K);
        }
        else
        {
            already_read = read(fd, buf_->data_ + buf_->length_, need_read);
        }
    } while (already_read == -1 && errno == EINTR);

    if (already_read > 0)
    {
        if (need_read != 0)
        {
            assert(already_read == need_read);
        }
        buf_->length_ += already_read;
    }

    return already_read;
}


int output_buf::send_data(const char* data, int datalen)
{
    if (buf_ == nullptr)
    {
        buf_ = buf_pool::instance()->alloc_buf(datalen);
        if (buf_ == nullptr)
        {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    }
    else
    {
        assert(buf_->head_ == 0);
        if (buf_->capacity_ - buf_->length_ < datalen)
        {
            io_buf* new_buf = buf_pool::instance()->alloc_buf(datalen + buf_->length_);
            if (new_buf == nullptr)
            {
                fprintf(stderr, "no idle buf for alloc\n");
                return -1;
            }
            new_buf->copy(buf_);
            buf_pool::instance()->revert(buf_);
            buf_ = new_buf;
        }
    }

    //将data数据拷贝到io_buf中，拼接到后面
    memcpy(buf_->data_ + buf_->length_, data, datalen);
    buf_->length_ += datalen;

    return 0;
}

int output_buf::write2fd(int fd)
{
    assert(buf_ != nullptr && buf_->head_ == 0);

    int already_write = 0;

    do {
        already_write = write(fd, buf_->data_, buf_->length_);
    } while (already_write == -1 && errno == EINTR);

    if (already_write > 0)
    {   //将已经处理的数据清空
        buf_->pop(already_write);
        buf_->adjust();
    }
    //如果fd非阻塞，可能会得到EAGAIN, EWOULDBLOCK错误
    if (already_write == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        already_write = 0;
    }

    return already_write;
}