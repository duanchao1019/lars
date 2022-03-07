#include "dns_route.h"
#include "subscribe.h"
#include <string>
#include <string.h>
using std::string;

Route* Route::instance_ = NULL;
pthread_once_t Route::once_ = PTHREAD_ONCE_INIT;


Route::Route()
{
    pthread_rwlock_init(&map_lock_, NULL);

    data_pointer_ = new route_map();
    temp_pointer_ = new route_map();

    this->connect_db();

    this->build_maps();
}

void Route::connect_db()
{
    //mysql数据库配置
    string db_host = config_file::instance()->GetString("mysql", "db_host", "127.0.0.1");
    uint16_t db_port = config_file::instance()->GetNumber("mysql", "db_port", 3306);
    string db_user = config_file::instance()->GetString("mysql", "db_user", "root");
    string db_passwd = config_file::instance()->GetString("mysql", "db_passwd", "971019");
    string db_name = config_file::instance()->GetString("mysql", "db_name", "lars_dns");

    if (mysql_init(&db_conn_) == NULL)
    {
        fprintf(stderr, "mysql_init error\n");
        exit(1);
    }

    //超时断开
    mysql_options(&db_conn_, MYSQL_OPT_CONNECT_TIMEOUT, "30");
    //设置mysql连接断开后自动重连
    char reconnect = 1;
    mysql_options(&db_conn_, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(&db_conn_, db_host.c_str(), db_user.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0))
    {
        fprintf(stderr, "Failed to connect mysql, error: %s\n", mysql_error(&db_conn_));
        exit(1);
    }   
}

void Route::build_maps()
{
    int ret = 0;

    snprintf(sql_, 1000, "SELECT * FROM RouteData;");
    ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "Failed to find any data, error %s\n", mysql_error(&db_conn_));
        exit(1);
    }

    //得到结果集
    MYSQL_RES* result = mysql_store_result(&db_conn_);

    //得到行数
    uint32_t line_num = mysql_num_rows(result);

    MYSQL_ROW row;
    for (uint32_t i = 0; i < line_num; i++)
    {
        row = mysql_fetch_row(result);
        int modId = atoi(row[1]);
        int cmdId = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        //组装map的key，由modId/cmdId组合
        uint64_t key = ((uint64_t)modId << 32) + cmdId;
        uint64_t value = ((uint64_t)ip << 32) + port;

        printf("modId = %d, cmdId = %d, ip = %u, port = %d\n", modId, cmdId, ip, port);

        //插入到RouteDataMap_A中
        (*data_pointer_)[key].insert(value);
    }

    mysql_free_result(result); //释放结果集
}

/*
* return 0  : 加载成功，version没有改变
* return 1  : 加载成功，version有改变
* return -1 : 加载失败
*/
int Route::load_version()
{
    //这里面只会有一条数据
    snprintf(sql_, 1000, "SELECT version FROM RouteVersion WHERE id = 1;");

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "load version error: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(&db_conn_);

    long line_num = mysql_num_rows(result);
    if (line_num == 0)
    {
        fprintf(stderr, "No version in table RouteVersion: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    //得到version
    long new_version = atol(row[0]);
    if (new_version == this->version_)
    {
        //加载成功但是没有修改
        return 0;
    }
    this->version_ = new_version;
    printf("now route version is %ld\n", this->version_);

    mysql_free_result(result);

    return 1;
}

int Route::load_route_data()
{
    temp_pointer_->clear();

    snprintf(sql_, 1000, "SELECT * FROM RouteData;");

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "load version error: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(&db_conn_);
    if (!result)
    {
        fprintf(stderr, "mysql_store_result: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    long line_num = mysql_num_rows(result);
    MYSQL_ROW row;
    for (long i = 0; i < line_num; i++)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[1]);
        int cmdid = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        uint64_t value = ((uint64_t)ip << 32) + port;

        (*temp_pointer_)[key].insert(value);
    }
    
    printf("load data to temp_pointer_ success! size = %lu\n", temp_pointer_->size());

    mysql_free_result(result);

    return 0;
}

void Route::swap()
{
    pthread_rwlock_wrlock(&map_lock_);
    route_map* temp = data_pointer_;
    data_pointer_ = temp_pointer_;
    temp_pointer_ = temp;
    pthread_rwlock_unlock(&map_lock_);
}

void Route::load_changes(std::vector<uint64_t>& change_list)
{
    //读取当前版本之前的全部修改
    snprintf(sql_, 1000, "SELECT modid,cmdid FROM RouteChange WHERE version <= %ld;", version_);

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "mysql_real_query: %s\n", mysql_error(&db_conn_));
        return ;
    }

    MYSQL_RES* result = mysql_store_result(&db_conn_);
    if (!result)
    {
        fprintf(stderr, "mysql_store_result: %s\n", mysql_error(&db_conn_));
        return ;
    }

    long line_num = mysql_num_rows(result);
    if (line_num == 0)
    {
        fprintf(stderr, "No version in table ChangeLog: %s\n", mysql_error(&db_conn_));
        return ;
    }

    MYSQL_ROW row;
    for (long i = 0; i < line_num; i++)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[0]);
        int cmdid = atoi(row[1]);
        uint64_t key = (((uint64_t)modid) << 32) + cmdid;
        change_list.push_back(key);
    }
    mysql_free_result(result);
}

void Route::remove_changes(bool remove_all)
{
    if (!remove_all)
    {
        snprintf(sql_, 1000, "DELETE FROM RouteChange WHERE version <= %ld;", version_);
    }
    else
    {
        snprintf(sql_, 1000, "DELETE FROM RouteChange;");
    }
    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "delete RouteChange: %s\n", mysql_error(&db_conn_));
        return ;
    }

    return ;
}

host_set Route::get_hosts(int modid, int cmdid)
{
    host_set hosts;

    //组装key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    
    //加读锁
    pthread_rwlock_rdlock(&map_lock_);
    if (data_pointer_->find(key) != data_pointer_->end())
    {
        //找到了对应的ip+host对
        hosts = (*data_pointer_)[key];
    }
    pthread_rwlock_unlock(&map_lock_);

    return hosts;
}

//周期性后端检查db的route信息的更新变化
//由backend thread完成
void* check_route_changes(void* args)
{
    int wait_time = 10;  //10s自动修改一次，也可以从配置文件中读取
    long last_load_time = time(NULL);

    //清空全部的RouteChange
    Route::instance()->remove_changes(true);

    while (true)
    {
        sleep(1);
        long curr_time = time(NULL);

        //加载RouteVersion得到当前版本号
        int ret = Route::instance()->load_version();
        if (ret == 1)
        {
            //version有改版，有modid/cmdid修改
            if (Route::instance()->load_route_data() == 0)
            {
                Route::instance()->swap();
                last_load_time = curr_time;  //更新最后加载时间
            }

            //获取被修改的modid/cmdid对应的订阅客户端，进行推送
            std::vector<uint64_t> changes;
            Route::instance()->load_changes(changes);

            //推送
            SubscribeList::instance()->publish(changes);

            //删除当前版本之前的修改记录
            Route::instance()->remove_changes();
        }
        else
        {
            //version没有改版
            if (curr_time - last_load_time >= wait_time)
            {
                //超时，也需要加载最新的数据到temp_pointer_
                if (Route::instance()->load_route_data() == 0)
                {
                    Route::instance()->swap();
                    last_load_time = curr_time;
                }
            }
        }
    }
}