#include "log.h"

using namespace std;
// 构造函数
Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}
// 析构函数
Log::~Log() {
    // writeThread 线程存在，且可以 joinable 确保它并不在 detach 状态
    if(writeThread_ && writeThread_->joinable()) {
        // 先刷一遍 pop push
        while(!deque_->empty()) {
            deque_->flush();  // 会有往 deque 中 push 的操作, 一直没有 push 操作
        };
        // 关闭 deque
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}
// 注意，以下两个操作，都是对队列的操作，所以需要
// 获取 Log 等级
int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}
// 设置 Log 等级
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {
        isAsync_ = true;
        if(!deque_) {
            // 创建输出 BlockDeque
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);
            // 创建 写线程 (只有一个写线程)
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    // 按照格式将时间信息写入文件，并将其作为文件名
    // 例如 bin/log/2021_07_01.log
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_) { 
            flush();
            fclose(fp_); 
        }
        // 创建打开文件，并且可利用文件指针追加内容到文件
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    /* 日志日期 日志行数 */
    // 如果发现日志日期不对，或者日志行数超过了规定的行数，就重新创建一个新的文件来存日志信息
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        // 天数不一样，改一下后面的日期
        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        // 行数达到最大上限，就将文件名后多加一个 lineCount_  / MAX_LINES
        // 例子 bin/log/2021_07_01-1.log
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        // 输出年月日 时间 
        // 例子：2021-07-01 11:01:33.310956
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);  // 添加日志头
        // 可变参数输入（Log 真实内容）
        va_start(vaList, format); 
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2); // 加回车

        // 看使用同步，还是异步写 Log
        if(isAsync_ && deque_ && !deque_->full()) {
            // 将 buff 中 read buff 区域内容 push 到 队列中，之后处理（异步方式）
            deque_->push_back(buff_.RetrieveAllToStr()); 
        } else {
            // 否则直接写入文件（同步方式）
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();  // 缓冲区重置
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); // 异步方式，通知消费者 pop
    }
    fflush(fp_);         // 清空缓存
}

void Log::AsyncWrite_() {  // 线程入口函数
    string str = "";
    while(deque_->pop(str)) {     // pop 会在阻塞状态，等待 deque_->flush 通知
        lock_guard<mutex> locker(mtx_);  // 必须要加锁，str 为临界资源，可能在 fputs 的过程中 被 pop 操作修改
        fputs(str.c_str(), fp_);  // 将 str 写入 文件
    }
}

Log* Log::Instance() {
    static Log inst;  // 静态单例
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}