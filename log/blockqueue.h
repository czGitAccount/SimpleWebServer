#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>  // 模板类
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();
    void Close();

    bool empty();
    bool full();

    size_t size();
    size_t capacity();

    T front();
    T back();

    void push_back(const T &item);
    void push_front(const T &item);

    bool pop(T &item);
    bool pop(T &item, int timeout);

    void flush();

private:    
    bool isClose_;  

    std::deque<T> deq_;  // blockqueue 本质由 deque 来实现

    size_t capacity_;

    std::mutex mtx_;

    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};

// 构造函数
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}
// 析构函数
template<class T> 
BlockDeque<T>::~BlockDeque() { Close(); };
// 关闭队列
template<class T>
// close 函数的意义就是，在执行 close 后的 push 全部都失效，之前的 pop 全部输出！！
void BlockDeque<T>::Close() {
    {   // 对队列的操作都需要加锁
        // 此时让该段代码在同一作用域内，
        // 使用 lock_guard 加锁，如果离开作用域析构函数会进行解锁
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();    
        isClose_ = true;
    }
    // condProducer 先进行唤醒操作，condConsumer 后进行唤醒操作
    // 但是由于 condProducer 后可能进行 condConsumer.notify_one()
    // 所以整体 condProducer 先完成，condConsumer 后完成
    // 内核优先唤醒 condProducer 除非 condConsumer.notify_one() 才有可能有后者被唤醒
    condProducer_.notify_all();  // 通知所有 wait 阻塞的, push 操作还是会执行的
    condConsumer_.notify_all();  // 通知所有 wait 阻塞的, 此时 isClose 已经为 true, pop 会直接 return false
    // 经历以上步骤后 其实 deq_ 可能还会有内容存在的 ！！！
    // 但是无所谓，deq 是一个 deque 所以会有自己的析构函数，释放内存！
    
    // 但是代码中应用在析构函数内，并且在 close 之前先循环执行了 flush 操作
    // 保证了进入 close 时，该输出到缓冲区的内容已经全部完成，deq_ 为空
    // 但是不确定，进入 close 后，会不会恰好 push，所以还需要再进行一次 争锁操作，clear 可能 push 的内容
    // 之后 push 的操作，都会存入到 deq_ 中，但是 pop 操作不会进行，会直接 return false, 结束写线程
};
// 通知一个消费者取
template<class T>  
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();   
};
 // 手动 clear
template<class T> 
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}
// 返回队首
template<class T>  
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}
// 返回队尾
template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}
// 返回队列大小
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}
// 返回 deq 最大长度
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}
// 往队列里面 push 
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    // 注意此时使用的时 unique_lock !!!
    std::unique_lock<std::mutex> locker(mtx_);
    // 如果当前队列都满了，让生产者别
    while(deq_.size() >= capacity_) {
        // 此时 wait 第二参数 默认为 false
        // 会解掉 unique_lock 的锁，并阻塞在该行，等待 notify_one 或者 notify_all
        // 一旦该 wait 在 notify 中竞争锁成功，会重新加锁，并继续进行下面的步骤
        // 如果此时 deq_.size() < capacity 则会跳出 while 
        // 否者，重新放掉自己的锁，并阻塞休眠，等待 notify
        condProducer_.wait(locker);  
    }
    deq_.push_back(item);
    condConsumer_.notify_one();  // 通知消费者取
}
// 往队列里面 push 
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}
// 判断队列是否为空
template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}
// 判断队列是否已经满了
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}
// 从队列里面 pop
template<class T>
bool BlockDeque<T>::pop(T &item) {
    // unique_lock, wait 的操作，与 push 操作完全相同
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){  // 此时最好考虑 deq 为空的情况下，是否存在关闭 deq 的可能
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();  // 通知生产者生产
    return true;
}
// 从队列里面 pop (有时限的阻塞模式)
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H