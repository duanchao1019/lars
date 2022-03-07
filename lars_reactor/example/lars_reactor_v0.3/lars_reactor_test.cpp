#include "tcp_server.h"
#include "reactor_buf.h"


int main()
{
    event_loop loop;
    
    tcp_server server("127.0.0.1", 7777, &loop);

    loop.event_process();

    return 0;
}