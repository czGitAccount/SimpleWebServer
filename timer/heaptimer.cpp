#include "heaptimer.h"
// 上滤操作
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;  // 找到其父节点
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);     // 父节点的值大于子节点的值，交换父子节点
        i = j;
        j = (i - 1) / 2;     // 继续上滤
    }
}
// 交换节点，并修改其 id 为 vec 对应的 idx
void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i; 
    ref_[heap_[j].id] = j; 
} 
// 下滤操作，确保以当前节点为根的子树满足最小堆的要求
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;  // 左子节点
    while(j < n) {
        // 左子节点大于右子节点，该步骤就是找到左右子节点中最小的那个
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        // 如果该节点的子节点值比它还小, 则交换
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;  // 继续下滤操作
    }
    return i > index;  // true: i 必定非叶子节点，且成功进行下滤
                       // false: 1. 叶子节点  2. 非叶子节点，但无需下滤
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {  // 新节点
        i = heap_.size();
        ref_[id] = i;  // push 到堆尾部，并进行上滤操作，调整到合适的位置
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    } 
    else { // 已有的节点，调整其到合适的位置
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;  
        // 1. 非叶子节点，且成功进行下滤，无须在进行上滤
        // 2. 非叶子节点，没有下滤，需要进行上滤
        // 3. 叶子节点，无下滤，只能上滤
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}
// 删除指定id结点，并触发回调函数 
void HeapTimer::doWork(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();  // 调用回调函数
    del_(i);    // 删除节点
}
// 删除指定位置的结点 
void HeapTimer::del_(size_t index) {   
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        // 将要删除的结点换到队尾，然后调整堆 
        SwapNode_(i, n);
        // 1. 非叶子节点，且成功进行下滤，无须在进行上滤
        // 2. 非叶子节点，没有下滤，需要进行上滤
        // 3. 叶子节点，无下滤，只能上滤
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    // 队尾元素删除 
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}
// 调整指定 id 的结点, 增序调整，只用下滤
void HeapTimer::adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref_[id], heap_.size());
}

// 心搏函数，从头结点开始，清除超时定时器
void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();  // 调用对应的回调函数
        pop();      // pop_front()
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();  // 调用一次心搏函数
    size_t res = -1;
    if(!heap_.empty()) {
        // 设置距离最近超时的差 为 Timeout
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}