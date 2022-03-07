#pragma once


//内存块结构
class io_buf
{
public:
    io_buf(int size);

    void clear();

    void adjust();

    void copy(const io_buf* other);

    void pop(int len);

    int capacity_;
    int head_;
    int length_;
    char* data_;
    io_buf* next;
};