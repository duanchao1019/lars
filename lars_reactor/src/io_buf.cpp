#include <string.h>
#include <assert.h>

#include "io_buf.h"


io_buf::io_buf(int size)
: capacity_(size),
  head_(0),
  length_(0),
  data_(nullptr),
  next(nullptr)
{
    data_ = new char[size];
    assert(data_ != nullptr);
}

void io_buf::clear()
{
    head_ = length_ = 0;
}

void io_buf::adjust()
{
    if (head_ != 0)
    {
        if (length_ != 0)
        {
            memmove(data_, data_ + head_, length_);
        }
        head_ = 0;
    }
}

void io_buf::copy(const io_buf* other)
{
    memcpy(data_, other->data_ + other->head_, other->length_);
    head_ = 0;
    length_ = other->length_;
}

void io_buf::pop(int len)
{
    head_ += len;
    length_ -= len;
}