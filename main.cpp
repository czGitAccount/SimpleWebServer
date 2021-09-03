#include <unistd.h>
#include "server/webserver.h"

int main() {
    WebServer server(
        20000, 3, 60000, false,            // 端口 ET模式 timeoutMs 优雅退出  
        6, true, 1, 1024);                 // 线程池数量 日志开关 日志等级 日志异步队列容量 
    server.Start();
} 
  