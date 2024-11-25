#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

#define VERSION_13

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

// 有的时候即使固定锁顺序，依旧会产生问题
struct X
{
    X(const std::string &str) : object{str} {}
    void print() const { std::puts(object.c_str()); }
    void address() const { std::cout << "object in add: " << static_cast<const void *>(object.data()) << std::endl; }

    friend void swap(X &lhs, X &rhs);

private:
    std::string object;
    std::mutex m;
};

void swap(X &lhs, X &rhs)
{
    if (&lhs == &rhs)
        return;
#define SocpedLock
#ifdef DeadLock
    std::lock_guard<std::mutex> lock1{lhs.m};
    std::this_thread::sleep_for(5ms);
    std::lock_guard<std::mutex> lock2{rhs.m};
    // C++ 标准库有很多办法解决这个问题，可以使用 std::lock ，它能一次性锁住多个互斥量，并且没有死锁风险。
#elif defined(STDLock)
    std::lock(lhs.m, rhs.m);
    std::lock_guard<std::mutex> lock1{lhs.m, std::adopt_lock};
    std::this_thread::sleep_for(5ms);
    std::lock_guard<std::mutex> lock2{rhs.m, std::adopt_lock};
// 因为前面已经使用了 std::lock 上锁，所以后的 std::lock_guard 构造都额外传递了一个 std::adopt_lock 参数，让其选择到不上锁的构造函数。函数退出也能正常解锁
#elif defined(SocpedLock)
    // C++17 新增了 std::scoped_lock ，提供此函数的 RAII 包装，通常它比裸调用 std::lock 更好
    //  使用 std::scoped_lock 可以将所有 std::lock 替换掉，减少错误发生
    std::scoped_lock guard{lhs.m, rhs.m};
#endif
    std::swap(lhs.object, rhs.object);
}

int main()
{
#if 0
    std::jthread t1{f1};
    std::jthread t2{f2};
#else
    // 考虑用户调用的时候将参数交换，就会产生死锁：
    X a{"🤣"}, b{"😅"};
    a.address();
    b.address();
    std::jthread t1{[&]
                    { swap(a, b); a.address(); b.address(); }}; // 1

    std::jthread t2{[&]
                    { swap(b, a); a.address(); b.address(); }}; // 2
#endif
}

#elif defined(VERSION_6)
// unique_lock
struct X
{
    X(const std::string &str) : object{str} {}
    void print() const { std::puts(object.c_str()); }
    void address() const { std::cout << "object in add: " << static_cast<const void *>(object.data()) << std::endl; }

    friend void swap(X &lhs, X &rhs);

private:
    std::string object;
    std::mutex m;
};

void swap(X &lhs, X &rhs)
{
    if (&lhs == &rhs)
        return;
    // std::unique_lock 私有数据成员 _Owns -> bool, _Pmtx -> _Mutex *
    // std::defer_lock 是“不获得互斥体的所有权” _Owns初始化为false, 没有所有权自然构造函数就不会上锁
    // std::unique_lock 是有 lock() 、try_lock() 、unlock() 成员函数的，所以可以直接传递给 std::lock
    // lock() 函数中会将_Owns置为true, 即表示当前对象拥有互斥量的所有权, 析构函数中if(_Owns) _Pmtx->unlock();
    //  必须得是当前对象拥有互斥量的所有权析构函数才会调用 unlock() 解锁互斥量。
    // std::unique_lock 要想调用 lock() 成员函数，必须是当前没有所有权
    // 正常的用法是，先上锁了互斥量，然后传递 std::adopt_lock 构造 std::unique_lock 对象表示拥有互斥量的所有权，即可在析构的时候正常解锁。

    /*
    简而言之：
    1: 使用 std::defer_lock 构造函数不上锁，要求构造之后上锁
    2: 使用 std::adopt_lock 构造函数不上锁，要求在构造之前互斥量上锁
    3: 默认构造会上锁，要求构造函数之前和构造函数之后都不能再次上锁
    */

#define Default
#ifdef Defer_lock
    // 此场景不会死锁
    std::unique_lock<std::mutex> loc1{lhs.m, std::defer_lock};
    std::unique_lock<std::mutex> loc2{rhs.m, std::defer_lock};
    std::lock(loc1, loc2);
    swap(lhs.object, rhs.object);
#elif defined(Adopt_lock)
#if 1
    // 此场景不会死锁
    std::lock(lhs.m, rhs.m);
    std::unique_lock<std::mutex> loc1{lhs.m, std::adopt_lock};
    std::unique_lock<std::mutex> loc2{rhs.m, std::adopt_lock};
#else
    // 此场景会死锁
    std::unique_lock<std::mutex> loc1{lhs.m, std::adopt_lock};
    std::unique_lock<std::mutex> loc2{rhs.m, std::adopt_lock};
    // 可以有这种写法，但是不推荐
    loc1.mutex()->lock();
    std::this_thread::sleep_for(5ms);
    loc2.mutex()->lock();
#endif
    swap(lhs.object, rhs.object);
#elif defined(Default)
    // 此场景会死锁
    std::unique_lock<std::mutex> loc1{lhs.m};
    std::this_thread::sleep_for(5ms);
    std::unique_lock<std::mutex> loc2{rhs.m};
    swap(lhs.object, rhs.object);
#endif
}

int main()
{
    X a{"🤣"}, b{"😅"};
    std::jthread t1{[&]
                    { swap(a, b); }};
    std::jthread t2{[&]
                    { swap(b, a); }};
}

#elif defined(VERSION_7)
// 不同作用域传递互斥量
// 互斥量满足互斥体 (Mutex)的要求，不可复制不可移动!!!
// 所谓的在不同作用域传递互斥量，其实只是传递了它们的指针或者引用罢了。可以利用各种类来进行传递，比如前面提到的 std::unique_lock

// 此时 X 也为不可复制不可移动
struct X
{
    // std::mutex m;
    // 解决方法:
    // static inline std::mutex x;
    // std::mutex *m;
    // std::shared_ptr<std::mutex> m;
};

// std::unique_lock 是只能移动不可复制的类，它移动即标志其管理的互斥量的所有权转移了
// 有些时候，这种转移（就是调用移动构造）是自动发生的，比如当函数返回 std::unique_lock 对象。另一种情况就是得显式使用 std::move。

// 一种可能的使用是允许函数去锁住一个互斥量，并将互斥量的所有权转移到调用者上，所以调用者可以在这个锁保护的范围内执行代码

std::mutex some_mutex;

std::unique_lock<std::mutex> get_lock()
{
    extern std::mutex some_mutex;
    std::unique_lock<std::mutex> lk{some_mutex}; // 默认构造上锁
    return lk;                                   // 选择到 unique_lock 的移动构造，转移所有权
    // 转移所有权后 _Owns == false, 析构不会解锁，在调用方控制或析构解锁
}
void process_data()
{
    // 转移到主函数的 lk 中
    std::unique_lock<std::mutex> lk{get_lock()};
    // 执行一些任务...
} // 最后才会析构解锁

int main()
{
    // X x;
    // X x2{x};
    process_data();
}

#elif defined(VERSION_8)
// 保护共享数据的"初始化"过程
// 对于共享数据的"初始化"过程的保护: 通常就不会用互斥量，这会造成很多的额外开销
/*
1: 双检锁（错误）需使用原子变量，参考设计模式中单例模式中的懒汉模式
2: 使用 std::call_once
3: 静态局部变量初始化从 C++11 开始是线程安全
*/

// 标准库提供了 std::call_once 和 std::once_flag
// 比起锁住互斥量并显式检查指针，每个线程只需要使用 std::call_once 就可以
struct some
{
    void do_something() {}
};

std::shared_ptr<some> ptr;
std::once_flag resource_flag;

void init_resource()
{
    ptr.reset(new some);
}

void foo()
{
    std::call_once(resource_flag, init_resource); // 线程安全的一次初始化
    ptr->do_something();
}

void test()
{
    std::call_once(resource_flag, []
                   { std::cout << "f init\n"; });
}

// “初始化”，那自然是只有一次。但是 std::call_once 也有一些例外情况（比如异常）会让传入的可调用对象被多次调用，即“多次”初始化：
std::once_flag flag;
int n = 0;

void f()
{
    std::call_once(flag, []
                   {
        ++n;
        std::cout << "第" << n << "次调用\n";
        throw std::runtime_error("异常"); });
}

class my_class
{
};
inline my_class &get_my_class_instance()
{
    static my_class instance = (std::cout << "get_my_class_instance\n", my_class{}); // 线程安全的初始化过程 初始化严格发生一次
    return instance;
}

int main()
{
// 编译器有问题
#if 0
#if 0
// https://godbolt.org/z/rrndqYsbc
    test();
    test();
    test();
#else
    // https://godbolt.org/z/Gbd6Kfsnq
    try
    {
        f();
    }
    catch (std::exception &)
    {
    }

    try
    {
        f();
    }
    catch (std::exception &)
    {
    }
#endif
#endif
    get_my_class_instance();
    get_my_class_instance();
    get_my_class_instance();
}

#elif defined(VERSION_9)
// 保护不常更新的数据结构
// 读写锁: std::shared_timed_mutex（C++14）、 std::shared_mutex（C++17）
// 它们的区别简单来说，前者支持更多的操作方式，后者有更高的性能优势。

// std::shared_mutex 同样支持 std::lock_guard、std::unique_lock。和 std::mutex 做的一样，保证写线程的独占访问
// 而那些无需修改数据结构的读线程，可以使用 std::shared_lock<std::shared_mutex> 获取访问权，多个线程可以一起读取

class Settings
{
private:
    std::map<std::string, std::string> data_;
    mutable std::shared_mutex mutex_; // “M&M 规则”：mutable 与 mutex 一起出现

public:
    void set(const std::string &key, const std::string &value)
    {
        std::lock_guard<std::shared_mutex> lock{mutex_};
        data_[key] = value;
    }

    std::string get(const std::string &key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : ""; // 如果没有找到键返回空字符串
    }
};

#elif defined(VERSION_10)
// std::recursive_mutex

// 线程对已经上锁的 std::mutex 再次上锁是错误的，这是未定义行为
// 在某些情况下，一个线程会尝试在释放一个互斥量前多次获取，所以提供了std::recursive_mutex

/*
std::recursive_mutex 允许同一线程多次锁定同一个互斥量，而不会造成死锁
当同一线程多次对同一个 std::recursive_mutex 进行锁定时，只有在解锁与锁定次数相匹配时，互斥量才会真正释放 lock 与 unlock 调用次数必须相同
但它并不影响不同线程对同一个互斥量进行锁定的情况。不同线程对同一个互斥量进行锁定时，会按照互斥量的规则进行阻塞
*/
std::recursive_mutex mtx;

void recursive_function(int count)
{
#if 0
    // 递归函数，每次递归都会锁定互斥量
    mtx.lock();
    std::cout << "Locked by thread: " << std::this_thread::get_id() << ", count: " << count << std::endl;
    if (count > 0)
        recursive_function(count - 1); // 递归调用
    mtx.unlock(); // 解锁互斥量
#else
    // 同样也可以使用 std::lock_guard、std::unique_lock 帮助管理 std::recursive_mutex，而非显式调用 lock 与 unlock
    std::lock_guard<std::recursive_mutex> lc{mtx};
    std::cout << "Locked by thread: " << std::this_thread::get_id() << ", count: " << count << std::endl;
    if (count > 0)
        recursive_function(count - 1);
#endif
}

int main()
{
    std::jthread t1(recursive_function, 3);
    std::jthread t2(recursive_function, 2);
}

#elif defined(VERSION_11)
// new、delete 是线程安全的吗？
// C++ 只保证了 operator new、operator delete 这两个方面即内存的申请与释放的线程安全（不包括用户定义的）
/*
new 表达式线程安全要考虑三方面：operator new、构造函数、修改指针
delete 表达式线程安全考虑两方面：operator delete、析构函数
*/
// 详见 md

#elif defined(VERSION_12)
// 线程存储期
int global_counter = 0;
thread_local int thread_local_counter = 0;

void print_counters()
{
    std::cout << "global: " << global_counter++ << '\n';
    std::cout << "thread_local: " << thread_local_counter++ << '\n';
    std::cout << "&global: " << &global_counter << '\n';
    std::cout << "&thread_local: " << &thread_local_counter << '\n';
}

int main()
{
    // 地址复用
    std::thread{print_counters}.join();
    std::thread{print_counters}.join();
}

#elif defined(VERSION_13)
// 线程变量 拥有线程存储期。它的存储在线程开始时分配，并在线程结束时解分配。每个线程拥有它自身的对象实例。
// 只有声明为 thread_local 的对象拥有此存储期（不考虑非标准用法）。
/*
它的初始化需要考虑局部与非局部两种情况：
1: 非局部变量：所有具有线程局部存储期的非局部变量的初始化，会作为线程启动的一部分进行，并按顺序早于线程函数的执行开始。
2: 静态局部变量(同普通静态局部对象)：控制流首次经过它的声明时才会被初始化（除非它被零初始化或常量初始化）。在其后所有的调用中，声明都会被跳过
*/

// gcc 与 clang 不打印, msvc会打印
// https://godbolt.org/z/Pr1PodPrK
thread_local int n = (std::cout << "thread_local init\n", 0);
void f1() { std::cout << "f\n"; }
void f2() { thread_local static int n = (std::cout << "f2 init\n", 0); }

int main()
{
    std::cout << "main\n";
    std::thread{f1}.join();
    f2();
    f2();
    f2();
}

#endif