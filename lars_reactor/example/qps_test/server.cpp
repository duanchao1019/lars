#include <string.h>
#include <string>
#include "config_file.h"
#include "tcp_server.h"
#include "echoMessage.pb.h"

//回显业务的处理函数
void callback_busy(const char* data, uint32_t msglen, int msgid, net_connection* net_conn, void* user_data)
{
    qps_test::EchoMessage request, response;

    //解包
    request.ParseFromArray(data, msglen);

    //设置新protobuf包
    response.set_id(request.id());
    response.set_content(request.content());

    //序列化
    std::string responseString;
    response.SerializeToString(&responseString);

    net_conn->send_message(responseString.c_str(), responseString.size(), msgid);
}


int main()
{
    config_file::setPath("/home/duanchao/project/lars/conf/serv.conf");
    std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_file::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);

    event_loop loop;

    tcp_server server(ip.c_str(), port, &loop);

    //注册消息业务路由
    server.add_msg_router(1, callback_busy);

    loop.event_process();

    return 0;
}