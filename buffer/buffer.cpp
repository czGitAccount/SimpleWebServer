#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 一般情况下 Buffer 内容
// [0                readPos_           writePos_          end]
// [Prependable ↑   |     Readable ↑   |      Writeable ↑     ]

// 返回目前缓存区中还未读取内容的长度
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}
// 返回目前可以往缓存区中存入的大小
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}
// 返回目前 Buffer 中预留空间的大小 (该内容中)
size_t Buffer::PrependableBytes() const {
    return readPos_;
}
// 返回当前 readPos 地址
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}
// 从缓存中读取数据后，需要将 readPos 移动对应的长度
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}
// 将所有 [readPos_, end] 的缓存空间都归还到预留空间中
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}
// 缓存区重置
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}
// 读取缓存，并存入 str (该读写操作为最后读取操作，即读取结束后清空缓存)
// 1. str = str[readPos_, readPos_ + len]
// 2. 清空缓存
// 3. 返回 str
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}
// 返回可以写入缓存的位置 (cosnt)
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}
// 返回可以写入缓存的位置 
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}
// 每次数据写入缓存区，对应的 writePos_ 移动到对应的位置
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

// Append 有四个重载版本：
// 1. void Append(const string& str)   内部会调用 3
// 2. void Append(const void* data, size_t len)  内部调用 3
// 3. void Append(const char* str, size_t, len)
// 4. void Append(const Buffer& buff)  内部调用 3

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);  // 确保缓存区中可写大小足够，如果不够利用预留空间位置，或者扩展缓存区
    std::copy(str, str + len, BeginWrite()); // 往缓存区中写入内容
    HasWritten(len);    // 移动 writePos_ 到对应的位置
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}
// 确保缓存区中可写大小足够，如果不够利用预留空间位置，或者扩展缓存区
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {  // 空间不足
        MakeSpace_(len);         // 利用预留空间位置，或者扩展缓存区
    }
    assert(WritableBytes() >= len); // 足够的话，无操作
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535]; 
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    struct iovec iov[2]; // 将两个不连续的缓存组成 iov
    // iov[0] 为 Buffer，iov[1] 为 buff
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);  // 分散读写
    if(len < 0) {  // 更改错误码
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        // writePos_ += len;  
        HasWritten(len); // Buffer 缓存区足够了, 那就没有 buff 什么事了
    }
    else {
        // 如果 Buffer 大小不够存， len - buffer_.size() 的内容被存在 buff 里面了
        writePos_ = buffer_.size();
        // 此时只需要将 buff 中的内容，重新写入 Buffer 中即可 (中间会扩容)
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);  // 使用 write 读取 [readPos_, WritePos_] 内内容即可
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    // readPos_ += len;  
    Retrieve(len);  // 统一使用 Retrieve
    return len;
}
// 返回指向 buffer_ (vector) 的头指针
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}
// 返回指向 buffer_ (vector) 的头指针 (const)
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}
// 1. 利用预留空间位置  2. 扩展缓存区
void Buffer::MakeSpace_(size_t len) {
    // 2. 如果 剩下的写空间 + 预留空间 都不够，就扩展缓存到合适的位置
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } 
    // 1. 预留空间足够
    else {
        size_t readable = ReadableBytes();
        // 将 [readPos_, writePos_) copy 到 [0, writePos_ - readPos)
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}