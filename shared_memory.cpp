#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <mutex>
#include <numeric>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

#define VERSION_5

#ifdef VERSION_1
// 条件竞争
// std::cout 的 operator<< 调用是线程安全的，不会被打断。
// 即：同步的 C++ 流保证是线程安全的（从多个线程输出的单独字符可能交错，但无数据竞争）
void f() { std::cout << "❤️\n"; }
void f2() { std::cout << "😢\n"; }

int main()
{
    std::thread t{f};
    std::thread t2{f2};
    t.join();
    t2.join();
}
/*
当某个表达式的求值写入某个内存位置，而另一求值读或修改同一内存位置时，称这些表达式冲突。
拥有两个冲突的求值的程序就有数据竞争，除非:
1: 两个求值都在同一线程上，或者在同一信号处理函数中执行
2: 两个冲突的求值都是原子操作（见 std::atomic）
3: 一个冲突的求值发生早于 另一个（见 std::memory_order）
如果出现数据竞争，那么程序的行为未定义。
// 标量类型等都同理
*/
#elif defined(VERSION_2)
// std::lock_guard
std::mutex m;

// “粒度”通常用于描述锁定的范围大小，较小的粒度意味着锁定的范围更小，因此有更好的性能和更少的竞争。
void f()
{
#if 0
    m.lock();
    std::cout << std::this_thread::get_id() << '\n';
    m.unlock();
#else
    // code...
    {
        // C++17 添加了一个新的特性，类模板实参推导 std::lock_guard 可以根据传入的参数自行推导，而不需要写明模板类型参数
        // std::lock_guard<std::mutex> lc{m};
        std::lock_guard lc{m};
        std::cout << std::this_thread::get_id() << '\n';
    } // 控制锁的粒度
    // code...
#endif
}

void add_to_list(int n, std::list<int> &list)
{
    std::vector<int> numbers(n + 1);
    std::iota(numbers.begin(), numbers.end(), 0);
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
    {
        std::lock_guard<std::mutex> lc{m};
        list.push_back(sum);
    }
}
void print_list(const std::list<int> &list)
{
    std::lock_guard<std::mutex> lc{m};
    for (const auto &i : list)
        std::cout << i << ' ';
    std::cout << '\n';
}

// C++17 还引入了一个新的“管理类”：std::scoped_lock
// 相较于 lock_guard的区别在于，它可以管理多个互斥量。不过对于处理一个互斥量的情况，它和 lock_guard 几乎完全相同。

int main()
{
#if 0
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < 10; ++i)
        threads.emplace_back(f);

    for (auto &thread : threads)
        thread.join();
#else
    std::list<int> list;
    std::thread t1{add_to_list, 10, std::ref(list)};
    std::thread t2{add_to_list, 10, std::ref(list)};
    std::thread t3{print_list, std::cref(list)};
    std::thread t4{print_list, std::cref(list)};
    t1.join();
    t2.join();
    t3.join();
    t4.join();
#endif
}
#elif defined(VERSION_3)
// try_lock
std::mutex mtx;

void thread_function(int id)
{
    // 尝试加锁
    if (mtx.try_lock())
    {
        std::osyncstream{std::cout} << "线程：" << id << " 获得锁" << std::endl;
        // 临界区代码
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟临界区操作
        mtx.unlock();                                                // 解锁
        std::osyncstream{std::cout} << "线程：" << id << " 释放锁" << std::endl;
    }
    else
    {
        std::osyncstream{std::cout} << "线程：" << id << " 获取锁失败 处理步骤" << std::endl;
    }
}

int main()
{
    std::thread t1(thread_function, 1);
    std::thread t2(thread_function, 2);

    t1.join();
    t2.join();
}

#elif defined(VERSION_4)
// 切勿将受保护数据的指针或引用传递到互斥量作用域之外，不然保护将形同虚设
class Data
{
    int a{};
    std::string b{};

public:
    void do_something()
    {
        // 修改数据成员等...
    }
};

class Data_wrapper
{
    Data data;
    std::mutex m;

public:
    template <class Func>
    void process_data(Func func)
    {
        std::lock_guard<std::mutex> lc{m};
        func(data); // 受保护数据传递给函数
    }
};

Data *p = nullptr;

void malicious_function(Data &protected_data)
{
    p = &protected_data; // 受保护的数据被传递到外部
}

Data_wrapper d;

void foo()
{
    d.process_data(malicious_function); // 传递了一个恶意的函数
    p->do_something();                  // 在无保护的情况下访问保护数据
}
#elif defined(VERSION_5)
// 死锁
std::mutex m1, m2;
std::size_t n{};
void f1()
{
    std::lock_guard<std::mutex> lc1{m1};
    std::this_thread::sleep_for(5ms);
    std::lock_guard<std::mutex> lc2{m2};
    ++n;
}
void f2()
{
    std::lock_guard<std::mutex> lc1{m2};
    std::this_thread::sleep_for(5ms);
    std::lock_guard<std::mutex> lc2{m1};
    ++n;
}

struct X
{
    X(const std::string &str) : object{str} {}

    friend void swap(X &lhs, X &rhs);

private:
    std::string object;
    std::mutex m;
};

void swap(X &lhs, X &rhs)
{
    if (&lhs == &rhs)
        return;
    std::lock_guard<std::mutex> lock1{lhs.m};
    std::this_thread::sleep_for(5ms);
    std::lock_guard<std::mutex> lock2{rhs.m};
    swap(lhs.object, rhs.object);
}

int main()
{
#if 0
    std::jthread t1{f1};
    std::jthread t2{f2};
#else
    X a{"🤣"}, b{"😅"};
    std::jthread t1{[&]
                    { swap(a, b); }}; // 1
    std::jthread t2{[&]
                    { swap(b, a); }}; // 2
#endif
}
#endif