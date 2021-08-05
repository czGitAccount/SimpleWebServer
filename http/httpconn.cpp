/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

// 原作者：mark, 以下为个人学习后进行的复现，增加注释，并进行了部分的修改

#include "httpconn.h"
using namespace std;
// 这三个数据都是类内静态数据，属于类，而不属于对象
const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;  // 是否 ET 模式
// 构造函数
HttpConn::HttpConn() {  
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { Close(); }; // 析构函数

// httpConn 初始化
void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;  // 原子操作
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();  // 重置读缓存
    readBuff_.RetrieveAll();   // 重置写缓存
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}
// 连接关闭
void HttpConn::Close() {
    response_.UnmapFile();  // response 清空共享内存
    if(isClose_ == false){  // 如果由于非主动原因关闭，isClose == false
        isClose_ = true; 
        userCount--;  // 原子操作
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

// 获取 Fd
int HttpConn::GetFd() const {
    return fd_;
};
// 获取 addr
struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}
// 获取 IP
const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}
// 获取 port
int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);  // ET 模式需要用户处理全部读完数据
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        // 分散发送
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        // 发送缓存中已经没有数据，表示数据已经传输完成
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } 
        // 发送数据长度如果大于 Buffer 缓存(iov_[0])，则需要借助 iov_[1] 缓存
        // 所以此时需要更新 iov_[1] 缓存目前所在的位置，以及长度信息
        // 注意 iov_[1] 使用的内容并不会重置，而是会随着使用越来越少
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            // Buffer 缓存 (iov_[0]) 则重置
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        // 如果发送长度 Buffer 缓存足够，使用它即可
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240); // ET 模式，也两个缓冲区大小 > 10240
    return len;
}
// HttpConn 处理流程
bool HttpConn::process() {
    request_.Init();  // request 操作初始化
    // 看是否读入 request
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    // 解析 request 请求，并且解析成功
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        // 按照 request 解析结果，初始化 response 消息
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        // 初始化 response 消息（bad request 消息）
        response_.Init(srcDir, request_.path(), false, 400);
    }
    // 根据 request 结果，拼接相应的 response 结果，放入 writeBuff_ 中
    response_.MakeResponse(writeBuff_);
    // response 头部信息：stateLine、Header 存入 iov_[0] 中
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    // content 中文件内容存入 iov_[1]
    // content 文件内容起初存在一个共享内存中
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
