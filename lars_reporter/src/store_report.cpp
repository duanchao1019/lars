#include "store_report.h"
#include "lars_reactor.h"
#include <string>
#include <unistd.h>

StoreReport::StoreReport()
{
    //1.初始化
    //1.1多线程使用mysql需要先调用mysql_library_init
    mysql_library_init(0, NULL, NULL);

    //1.2初始化连接，设置超时时间
    mysql_init(&db_conn_);
    mysql_options(&db_conn_, MYSQL_OPT_CONNECT_TIMEOUT, "30");
    my_bool reconnect;
    mysql_options(&db_conn_, MYSQL_OPT_RECONNECT, &reconnect);

    //2.加载配置
    std::string db_host = config_file::instance()->GetString("mysql", "db_host", "127.0.0.1");
    short db_port = config_file::instance()->GetNumber("mysql", "db_port", 3306);
    std::string db_user = config_file::instance()->GetString("mysql", "db_usr", "root");
    std::string db_passwd = config_file::instance()->GetString("mysql", "db_passwd", "971019");
    std::string db_name = config_file::instance()->GetString("mysql", "db_name", "lars_dns");

    //3.连接数据库
    if (mysql_real_connect(&db_conn_, db_host.c_str(), db_user.c_str(), 
        db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0) == NULL)
    {
        fprintf(stderr, "mysql real connect error\n");
        exit(1);
    }
}

void StoreReport::store(lars::ReportStatusRequest req)
{
    for (int i = 0; i < req.results_size(); i++)
    {
        //一条report调用记录
        const lars::HostCallResult& result = req.results(i);
        int overload = result.overload() ? 1 : 0;
        char sql[1204];

        snprintf(sql, 1024, "INSERT INTO ServerCallStatus"
                 "(modid, cmdid, ip, port, caller, succ_cnt, err_cnt, ts, overload) "
                 "VALUES(%d, %d, %u, %u, %u, %u, %u, %u, %d) ON DUPLICATE KEY "
                 "UPDATE succ_cnt = %u, err_cnt = %u, ts = %u, overload = %d", 
                 req.modid(), req.cmdid(), result.ip(), result.port(), req.caller(), 
                 result.succ(), result.err(), req.ts(), overload,
                 result.succ(), result.err(), req.ts(), overload);

        mysql_ping(&db_conn_);  //ping测试一下，防止连接断开，会触发重新建立连接

        if (mysql_real_query(&db_conn_, sql, strlen(sql)) != 0)
        {
            fprintf(stderr, "Fail to insert into ServerCallStatus %s\n", 
                    mysql_error(&db_conn_));
        }
    }
}
