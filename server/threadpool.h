/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

// 原作者：mark, 以下为个人学习后进行的复现，增加注释，并进行了部分的修改

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            for(size_t i = 0; i < threadCount; i++) {
                // lambda 函数作为线程入口函数
                std::thread([pool = pool_] {  // pool_ 为只能指针，指向一个 pool 结构体
                    std::unique_lock<std::mutex> locker(pool->mtx);  // unique_lock 
                    while(true) {
                        if(!pool->tasks.empty()) {
                            auto task = std::move(pool->tasks.front());
                            pool->tasks.pop();
                            locker.unlock();  // 解锁
                            task();
                            locker.lock();    // 重新加锁
                        } 
                        else if(pool->isClosed) break; // 退出线程，由于线程 detach 所以无需 join
                        else pool->cond.wait(locker);  // 当前正忙，无足够的线程使用，阻塞
                    }
                }).detach();
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all(); // 通知所有线程退出
        }
    }

    template<class F>
    void AddTask(F&& task) { // 右值引用，效率更高，且可以传引用参数
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            // forward 可以保存 task 的左值或者右值的特性
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();  // 通知线程去处理
    }

private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;  // 所有线程共享该 Pool 结构体
};


#endif //THREADPOOL_H