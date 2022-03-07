#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <pthread.h>
#include "lars_reactor.h"
#include "lars.pb.h"
#include "dns_route.h"

typedef std::unordered_map<uint64_t, std::unordered_set<int>> subscribe_map;
typedef std::unordered_map<int, std::unordered_set<uint64_t>> publish_map;


class SubscribeList
{
public:
    static void init()
    {
        instance_ = new SubscribeList();
    }

    static SubscribeList* instance()
    {
        pthread_once(&once_, init);
        return instance_;
    }

    //订阅
    void subscribe(uint64_t mod, int fd);

    //取消订阅
    void unsubscribe(uint64_t mod, int fd);

    //发布
    void publish(std::vector<uint64_t>& change_mods);

    //根据在线用户fd得到需要发布的列表
    void make_publish_map(listen_fd_set& online_fds, publish_map& need_publish);

private:
    SubscribeList();
    SubscribeList(const SubscribeList&);
    SubscribeList& operator=(const SubscribeList&);

private:
    static SubscribeList* instance_;
    static pthread_once_t once_;

    subscribe_map book_list_;  //订阅清单
    pthread_mutex_t book_list_lock_;

    publish_map push_list_;   //发布清单
    pthread_mutex_t push_list_lock_;
};