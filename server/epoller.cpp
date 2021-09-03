/*  该部分集成了
    1. Epoller 构造函数，析构函数
    2. AddFd(增) ModFd(改) DelFd(删) 使用 epoll_ctl 函数
       分别使用 EPOLL_CTL_ADD，EPOLL_CTL_ADD，EPOLL_CTL_ADD 字段
    3. Wait 为 epoll_wait, 默认 -1 即永久阻塞
    4. 提供了 GetEventFd GetEvents 用来获得 epoll_wait 返回的事件 Fd 和 Event
*/

#include "epoller.h"

// 给内核推荐 epoll 大小 512
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};  // 初始化
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs) { // &events_[0] 表示 epoll_event 数组的首地址，即数组指针
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
    // 返回值为事件数
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}