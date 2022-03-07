#include "subscribe.h"

extern tcp_server* server;

SubscribeList* SubscribeList::instance_ = NULL;
pthread_once_t SubscribeList::once_ = PTHREAD_ONCE_INIT;

SubscribeList::SubscribeList()
{
    pthread_mutex_init(&book_list_lock_, NULL);
    pthread_mutex_init(&push_list_lock_, NULL);
}

void SubscribeList::subscribe(uint64_t mod, int fd)
{   //将mod->fd的关系加入到book_list_中
    pthread_mutex_lock(&book_list_lock_);
    book_list_[mod].insert(fd);
    pthread_mutex_unlock(&book_list_lock_);
}

void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
    pthread_mutex_lock(&book_list_lock_);
    if (book_list_.find(mod) != book_list_.end())
    {
        book_list_[mod].erase(fd);
        if (book_list_[mod].empty())
        {
            book_list_.erase(mod);
        }
    }
    pthread_mutex_unlock(&book_list_lock_);    
}

void push_change_task(event_loop* loop, void* args)
{
    SubscribeList* subscribe = (SubscribeList*)args;

    //1.获取全部的在线客户端fd
    listen_fd_set online_fds;
    loop->get_listen_fds(online_fds);

    //2.从subscribe的push_list_中找到于online_fds集合匹配，放在一个新的publish_map里
    publish_map need_publish;
    subscribe->make_publish_map(online_fds, need_publish);

    //3.依次从need_publish取出数据，发送给对应客户端连接
    for (auto it = need_publish.begin(); it != need_publish.end(); ++it)
    {
        int fd = it->first;
        //遍历fd对应的modid/cmdid集合
        for (auto st = it->second.begin(); st != it->second.end(); ++st)
        {   //一个modid/cmdid
            int modid = int((*st) >> 32);
            int cmdid = int(*st);

            //组装protobuf消息，发送给客户端
            lars::GetRouteResponse rsp;
            rsp.set_modid(modid);
            rsp.set_cmdid(cmdid);

            //通过route查询对应的host的ip/port信息，进行组装
            host_set hosts = Route::instance()->get_hosts(modid, cmdid);
            for (auto hit = hosts.begin(); hit != hosts.end(); ++hit)
            {
                uint64_t ip_port_pair = *hit;
                lars::HostInfo host_info;
                host_info.set_ip((uint32_t)(ip_port_pair >> 32));
                host_info.set_port((uint32_t)ip_port_pair);

                //添加到rsp中
                rsp.add_host()->CopyFrom(host_info);
            }

            //给当前fd发送一个更新消息
            std::string responseString;
            rsp.SerializeToString(&responseString);

            //通过fd取出链接信息
            net_connection* conn = tcp_server::conns_[fd];
            if (conn != NULL)
            {
                conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
            }
        }
    }
}

void SubscribeList::make_publish_map(listen_fd_set& online_fds, publish_map& need_publish)
{
    pthread_mutex_lock(&push_list_lock_);
    //遍历push_list_找到online_fds匹配的数据，放到need_publish中，然后从push_list_中删除
    for (auto it = push_list_.begin(); it != push_list_.end(); ++it)
    {
        //it->first是fd，it->second是modid/cmdid的集合
        if (online_fds.find(it->first) != online_fds.end())
        { 
            need_publish[it->first] = push_list_[it->first];
            push_list_.erase(it);
        }
    }
    pthread_mutex_unlock(&push_list_lock_);
}

void SubscribeList::publish(std::vector<uint64_t>& change_mods)
{
    //将change_mods(经过修改的modid/cmdid)放到发布清单push_list_中
    pthread_mutex_lock(&book_list_lock_);
    pthread_mutex_lock(&push_list_lock_);

    for (auto it = change_mods.begin(); it != change_mods.end(); ++it)
    {
        uint64_t mod = *it;
        if (book_list_.find(mod) != book_list_.end())
        {   //只有当修改的modid/cmdid被订阅了才需要发布
            for (auto fds_it = book_list_[mod].begin(); fds_it != book_list_[mod].end(); ++fds_it)
            {
                int fd = *fds_it;
                push_list_[fd].insert(mod);
            }

        }
    }

    pthread_mutex_unlock(&push_list_lock_);
    pthread_mutex_unlock(&book_list_lock_);

    //通知各个线程去执行推送任务
    server->get_pool()->send_task(push_change_task, this);
}