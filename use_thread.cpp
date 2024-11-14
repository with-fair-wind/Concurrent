#include <algorithm>
#include <exception>
#include <iostream>
#include <iterator>
#include <numeric>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

#define VERSION_6

#ifdef VERSION_1
void hello()
{
    std::cout << "Hello World" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

int main()
{
#if 0
    std::thread t{hello};
    std::cout << std::boolalpha << t.joinable() << std::endl; // true 当前线程对象关联了活跃线程
    // t.join(); 等待线程对象 `t` 关联的线程执行完毕，否则将一直阻塞。这里的调用是必须的，否则 `std::thread` 的析构函数将调用 std::terminate() 无法正确析构。
    t.join();                                                 // 有两件事情  1:阻塞，让线程执行完毕 2:修改对象的状态,joinable返回值
    std::cout << std::boolalpha << t.joinable() << std::endl; // false 当前线程对象已经没有关联活跃线程了
#else
    std::thread t; // 默认构造，构造不关联线程的对象
    std::cout << std::boolalpha << t.joinable() << std::endl;
#endif
}

#elif defined(VERSION_2)
template <typename ForwardIt>
auto sum(ForwardIt first, ForwardIt second)
// typename std::iterator_traits<ForwardIt>::value_type sum(ForwardIt first, ForwardIt second)  // C++11
{
    using value_type = std::iter_value_t<ForwardIt>; // C++20
    // using value_type = typename std::iterator_traits<ForwardIt>::value_type // C++11
    std::size_t num_threads = std::thread::hardware_concurrency();
    std::ptrdiff_t distance = std::distance(first, second);

    if (distance > 1024000)
    {
        std::size_t chunk_size = distance / num_threads;
        std::size_t remainder = distance % num_threads;

        std::vector<value_type> results{num_threads};
        std::vector<std::thread> threads;

        auto start = first;
        for (std::size_t i = 0; i < num_threads; ++i)
        {
            auto end = std::next(start, chunk_size + (i < remainder ? 1 : 0));
            threads.emplace_back([start, end, &results, i]
                                 { results[i] = std::accumulate(start, end, value_type{}); });
            start = end;
        }

        for (auto &thread : threads)
            thread.join();

        return std::accumulate(results.begin(), results.end(), value_type{});
    }
    return std::accumulate(first, second, value_type{});
}

// 如果需要多线程求和，可以使用 C++17 引入的求和算法 std::reduce 并指明执行策略。它的效率接近我们实现的 sum 的两倍，当前环境核心越多数据越多，和单线程效率差距越明显。

int main()
{
    unsigned int n = std::thread::hardware_concurrency();
    std::cout << "支持 " << n << " 个并发线程。\n";

    std::vector<std::string> vecs{"1", "2", "3", "4"};
    auto result = sum(vecs.begin(), vecs.end());
    std::cout << result << '\n';

    vecs.clear();
    for (std::size_t i = 0; i <= 1024001u; ++i)
        vecs.push_back(std::to_string(i));

    result = sum(vecs.begin(), vecs.end());
    // std::cout << result << '\n';
}

#elif defined(VERSION_3)
// 在 C++ 标准库中，没有直接管理线程的机制，只能通过对象关联线程后，通过该对象来管理线程。
// 类 std::thread 的对象就是指代线程的对象，而我们本节说的“线程管理”，其实也就是指管理 std::thread 对象。
struct Task
{
    void operator()() const
    {
        std::cout << "void operator()() const" << std::endl;
    }
};

// 在确定每个形参的类型后，类型是 “T 的数组”或某个函数类型 T 的形参会调整为具有类型“指向 T 的指针”
std::thread f(Task());     // 声明  形参:函数类型
std::thread f(Task (*p)()) // 定义  形参:函数指针类型
{
    return {};
}

struct X : decltype([]
                    { std::cout << "crazy" << std::endl; })
{
};

int main()
{
#if 0
    Task task;
    task();
    std::thread t{task};
    t.join();
#else
    std::thread t{Task{}};
    // std::thread t(Task());
    // 这被编译器解析为函数声明，是一个返回类型为 std::thread，函数名为 t，接受一个返回 Task 的空参的函数指针类型，也就是 Task(*)()
    t.join();

    f(nullptr);

    X x;
    x();
#endif
}

#elif defined(VERSION_4)
struct func
{
    int &m_i;
    func(int &i) : m_i{i} {}
    void operator()(int n) const
    {
        for (int i = 0; i <= n; ++i)
            m_i += i; // 可能悬空引用
        std::cout << m_i << std::endl;
    }
};

// 必须是 std::thread 的 joinable() 为 true 即线程对象有活跃线程，才能调用 join() 和 detach()

int main()
{
#if 0
    std::thread thread{
        []
        { std::cout << "hello world" << std::endl; }};
    thread.detach(); // 不阻塞
    // 在单线程的代码中，对象销毁之后再去访问，会产生未定义行为，多线程增加了这个问题发生的几率
    // 比如函数结束，那么函数局部对象的生存期都已经结束了，都被销毁了，此时线程函数还持有函数局部对象的指针或引用
    // 线程对象放弃了对线程资源的所有权，不再管理此线程，也就是线程对象没有关联活跃线程了，此时 joinable 为 false
    // 允许此线程独立的运行，在线程退出时释放所有分配的资源。
    std::cout << std::boolalpha << thread.joinable() << std::endl;
    std::this_thread::sleep_for(10ms);
#else
    int n = 0;
    std::thread my_thread{func{n}, 100};
    my_thread.detach(); // 分离，不等待线程结束
    // 分离的线程可能还在运行
    // 一定要确保在主线程结束之前(或同时) 被detach的线程，要执行完毕
    // std::this_thread::sleep_for(10ms);
#endif
}
#elif defined(VERSION_5)
struct func
{
    int &m_i;
    func(int &i) : m_i{i} {}
    void operator()(int n) const
    {
        for (int i = 0; i <= n; ++i)
            m_i += i; // 可能悬空引用
        std::this_thread::sleep_for(1s);
        std::cout << m_i << std::endl;
    }
};

void f2() { throw std::runtime_error("test f2()"); }

void f()
{
    int n = 0;
    std::thread t{func{n}, 10};
    try
    {
        // todo.. 一些当前线程可能抛出异常的代码
        f2();
    }
    catch (...) // 此处捕获异常只是确保函数 f 中创建的线程正常执行完成，其局部对象正常析构释放 不掩盖错误
    {
        t.join(); // 执行 join() 确保线程正常执行完成，线程对象可以正常析构
        throw;    // 1: 如果不抛出，还得执行一个 t.join() 显然逻辑不对，自然抛出
                  // 2: 函数产生的异常，由调用方进行处理
    }
    t.join(); // f2()函数未抛出异常则使线程正常执行完成，线程对象可以正常析构
}

int main()
{
    try
    {
        f();
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "Caught an exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Caught an unknown exception!" << std::endl;
    }
}

#elif defined(VERSION_6)
// RAII
struct func
{
    int &m_i;
    func(int &i) : m_i{i} {}
    void operator()(int n) const
    {
        for (int i = 0; i <= n; ++i)
            m_i += i; // 可能悬空引用
        std::this_thread::sleep_for(1s);
        std::cout << m_i << std::endl;
    }
};

void f2() { throw std::runtime_error("test f2()"); }

// 严格来说其实thread_guard也不算 RAII，因为 thread_guard 的构造函数其实并没有申请资源，只是保有了线程对象的引用，在析构的时候进行了 join() 。
class thread_guard
{
public:
    explicit thread_guard(std::thread &t) : thread_(t) {}
    ~thread_guard()
    {
        std::puts("析构");
        if (thread_.joinable())
            thread_.join();
    }

    thread_guard &operator=(const thread_guard &) = delete;
    thread_guard(const thread_guard &) = delete;

private:
    std::thread &thread_;
};

// 函数 f 执行完毕，局部对象就要逆序销毁了。因此，thread_guard 对象 g 是第一个被销毁的，调用析构函数。
// 即使函数 f2() 抛出了一个异常，这个销毁依然会发生 !!!（前提是你捕获了这个异常）
// 这确保了线程对象 t 所关联的线程正常的执行完毕以及线程对象的正常析构
void f()
{
    int n = 0;
    std::thread t{func(n), 10};
    thread_guard g(t);
    f2();
}

int main()
{
#if 0
    f(); // 如果异常被抛出但未被捕获那么就会调用 std::terminate。是否对未捕获的异常进行任何栈回溯由实现定义。（简单的说就是不一定会调用析构）
#else
    try
    {
        f();
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "Caught an exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Caught an unknown exception!" << std::endl;
    }
#endif
}

#endif