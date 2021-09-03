<<<<<<< HEAD
=======
/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft Apache 2.0
 */ 
// 原作者：mark, 以下为个人学习后进行的复现，增加注释，并进行了部分的修改
>>>>>>> def492361972feedee60578307251dcb6ce473b1
#include <unistd.h>
#include "server/webserver.h"

int main() {
    WebServer server(
        20000, 3, 60000, false,            // 端口 ET模式 timeoutMs 优雅退出  
        6, true, 1, 1024);                 // 线程池数量 日志开关 日志等级 日志异步队列容量 
    server.Start();
} 
  