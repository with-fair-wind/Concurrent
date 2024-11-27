#include <algorithm>
#include <condition_variable>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <shared_mutex>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

/*
"同步操作"
"同步操作"是指在计算机科学和信息技术中的一种操作方式，其中不同的任务或操作按顺序执行，一个操作完成后才能开始下一个操作。
在多线程编程中，各个任务通常需要通过**同步设施**进行相互**协调和等待**，以确保数据的**一致性**和**正确性**
*/

#define VERSION_3
#ifdef VERSION_1
/*
等待事件及条件
详见 md
1: 忙等待(自旋)
2: 加延时
3: 条件变量
*/

// 忙等和加延时中:
// 忙等待
#define Condition
#ifdef BusyWait
bool flag = false;
std::mutex m;
void wait_for_flag()
{
    // 循环中，函数对互斥量解锁，再再对互斥量上锁，另外的线程很难有机会获取锁并设置标识（因为修改函数和等待函数共用一个互斥量）对cpu的占用很高
    std::unique_lock<std::mutex> lk{m};
    while (!flag)
    {
        lk.unlock(); // 1 解锁互斥量
        lk.lock();   // 2 上锁互斥量
    }
    std::cout << "end\n";
}

int main()
{
    std::jthread t1{wait_for_flag};
    std::jthread t2{[]
                    { std::this_thread::sleep_for(5s);
        std::unique_lock<std::mutex> lk{m};
        flag = true; }};
}

// 加延时
#elif defined(Delay)
bool flag = false;
std::mutex m;

// 很难确定正确的休眠时间, 在需要快速响应的程序中就意味着丢帧或错过了一个时间片
void wait_for_flag()
{
    // 循环中，休眠②前函数对互斥量解锁①，再休眠结束后再对互斥量上锁，让另外的线程有机会获取锁并设置标识（因为修改函数和等待函数共用一个互斥量）
    std::unique_lock<std::mutex> lk{m};
    while (!flag)
    {
        lk.unlock(); // 1 解锁互斥量
        std::this_thread::sleep_for(10ms);
        lk.lock(); // 2 上锁互斥量
    }
    std::cout << "end\n";
}

#elif defined(Condition)
// 通过另一线程触发等待事件的机制是最基本的唤醒方式，这种机制就称为“条件变量”
// std::condition_variable std::condition_variable_any
// condition_variable_any 是 condition_variable 的泛化
// 相对于只在 std::unique_lock<std::mutex> 上工作的  std::condition_variable
// condition_variable_any 能在任何满足可基本锁定(lock/unlock)(BasicLockable)要求的锁上工作
// any 版更加通用但是却有更多的性能开销。所以通常首选 std::condition_variable
std::mutex mtx;
std::condition_variable cv;
bool arrived = false;

void wait_for_arrival()
{
    std::unique_lock<std::mutex> lck(mtx);
    // 会释放锁, 等到被唤醒，以及条件满足时，会去抢互斥锁
    cv.wait(lck, []
            { return arrived; }); // 等待 arrived 变为 true
    std::cout << "到达目的地，可以下车了！" << std::endl;
#if 0
    arrived = false;
    std::this_thread::sleep_for(5s);
    std::cout << "reset arrived to satisfy condition\n";
    arrived = true;
#endif
}

void race_arrival()
{
    std::unique_lock<std::mutex> lck(mtx);
    // 会释放锁, 等到被唤醒，以及条件满足时，会去抢互斥锁
    cv.wait(lck, []
            { return arrived; }); // 等待 arrived 变为 true
    std::cout << "race_arrival" << std::endl;
#if 0
    arrived = false;
    std::this_thread::sleep_for(5s);
    std::cout << "reset arrived to satisfy condition\n";
    arrived = true;
#endif
}

void simulate_arrival()
{
    std::this_thread::sleep_for(std::chrono::seconds(5)); // 模拟地铁到站，假设5秒后到达目的地
    {
        std::lock_guard<std::mutex> lck(mtx);
        arrived = true; // 设置条件变量为 true，表示到达目的地
    }
    // cv.notify_one(); // 通知等待的线程
    cv.notify_all();
}

int main()
{
    std::jthread t1{wait_for_arrival};
    std::jthread t3{race_arrival};
    std::jthread t2{simulate_arrival};
}

// 条件变量的 wait 成员函数有两个版本，以上代码使用的就是第二个版本，传入了一个谓词
/*
void wait(std::unique_lock<std::mutex>& lock);                 // 1

template<class Predicate>
void wait(std::unique_lock<std::mutex>& lock, Predicate pred); // 2

②等价于：
while (!pred())
    wait(lock);

第二个版本只是对第一个版本的包装，等待并判断谓词，会调用第一个版本的重载。这可以避免“虚假唤醒（spurious wakeup）” (如果不是 while 判断而是 if 判断 即为虚假唤醒)
*/
#endif

#elif defined(VERSION_2)
template <typename T>
class threadsafe_queue
{
    mutable std::mutex m;              // 互斥量，用于保护队列操作的独占访问
    std::condition_variable data_cond; // 条件变量，用于在队列为空时等待
    std::queue<T> data_queue;          // 实际存储数据的队列
public:
    threadsafe_queue() {}
    void push(T new_value)
    {
        {
            std::lock_guard<std::mutex> lk{m};
            std::cout << "push:" << new_value << std::endl;
            data_queue.push(new_value);
            std::this_thread::sleep_for(2s);
        }
        data_cond.notify_one();
    }
    // 从队列中弹出元素（阻塞直到队列不为空）
    void pop(T &value)
    {
        std::unique_lock<std::mutex> lk{m};
        data_cond.wait(lk, [this]
                       { return !data_queue.empty(); });
        value = data_queue.front();
        std::cout << "pop:" << value << std::endl;
        data_queue.pop();
    }
    // 从队列中弹出元素（阻塞直到队列不为空），并返回一个指向弹出元素的 shared_ptr
    std::shared_ptr<T> pop()
    {
        std::unique_lock<std::mutex> lk{m};
        data_cond.wait(lk, [this]
                       { return !data_queue.empty(); });
        std::shared_ptr<T> res{std::make_shared<T>(data_queue.front())};
        data_queue.pop();
        return res;
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(m);
        return data_queue.empty();
    }
};

void producer(threadsafe_queue<int> &q)
{
    for (int i = 0; i < 5; ++i)
        q.push(i);
}
void consumer(threadsafe_queue<int> &q)
{
    for (int i = 0; i < 5; ++i)
    {
        int value{};
        q.pop(value);
    }
}

int main()
{
    threadsafe_queue<int> q;
    std::jthread t1{producer, std::ref(q)};
    std::jthread t2{consumer, std::ref(q)};
}

#elif defined(VERSION_3)
// 模拟

#endif
