#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

#define VERSION_15

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

#elif defined(VERSION_7)
// 传递参数
struct X
{
    operator int() const
    {
        std::cout << __func__ << std::endl;
        return 0;
    }
};

void f(const int &a)
{
    std::cout << "&a: " << &a << std::endl;
}

int main()
{
    int n = 1;
    std::cout << "&n: " << &n << std::endl;
    // 传参会复制到新线程的内存空间中，即使函数中的参数是引用，依然实际是复制
    // 内部实现:会先进行 decay 衰退，通过复制/移动构造传递给tuple，最后通过std::move传递给 invoke
    std::thread t{f, n};

    // void f(int &) 如果不使用 std::ref 会产生编译错误
    // std::thread 内部会将保有的参数副本转换为右值表达式进行传递，这是为了那些只支持移动的类型，左值引用没办法引用右值表达式，产生编译错误

    // std::ref 返回一个包装类 std::reference_wrapper
    // 这个类就是包装引用对象类模板，将对象包装，可以隐式转换为被包装对象的引用。
    // 该隐式转换由 std::reference_wrapper 内部定义的转换函数实现
    std::thread t1{f, std::ref(n)};
    t.join();
    t1.join();

    std::reference_wrapper<int> r = std::ref(n);
    int &p = r; // r 隐式转换为 n 的引用 此时 p 引用的就是 n

    std::reference_wrapper<const int> cr = std::cref(n);
    const int &cp = cr; // r 隐式转换为 n 的 const 的引用 此时 p 引用的就是 n

    std::cout << "&p: " << &p << "\t&r: " << &r << std::endl;
    std::cout << "&cp: " << &cp << "\t&cr: " << &cr << std::endl;

    X x;
    int a = x; // 自动调用定义的转化函数
}
#elif defined(VERSION_8)
// 只支持移动的类型
struct move_only
{
    move_only() { std::puts("默认构造"); }
    move_only(const move_only &) = delete;
    move_only(move_only &&) noexcept
    {
        std::puts("移动构造");
    }
};

void f1(move_only obj) { std::cout << &obj << std::endl; }
void f2(move_only &&obj) { std::cout << &obj << std::endl; }
void f3(const move_only &obj) { std::cout << &obj << std::endl; }

int main()
{
    move_only obj;
    std::cout << &obj << std::endl;
    std::cout << "\n";

    // std::thread t{f1, obj};
    // error 左值传递给 tuple 时，无法调用复制构造(只能移动的类型)

    std::thread t1{f1, std::move(obj)};
    t1.join();
    std::cout << "\n";
    // 两次打印移动构造均为std::thread内部实现，第一次是移动构造给 tuple, 第二次是 invoke 传递给f进行调用

    std::thread t2{f2, std::move(obj)};
    t2.join();
    std::cout << "\n";
    std::thread t3{f3, std::move(obj)};
    t3.join();
}
#elif defined(VERSION_9)
// 成员函数、lambda表达式作为可调用对象
struct X
{
    void task_run(int &a) const { std::cout << &a << std::endl; }
};

int main()
{
    X x;
    int n = 0;
    std::cout << &n << std::endl;
    // 成员指针必须和对象一起使用，这是唯一标准用法，成员指针不可以转换到函数指针单独使用，即使是非静态成员函数没有使用任何数据成员
    // 传入成员函数指针、与其配合使用的对象、调用成员函数的参数，构造线程对象 t，启动线程
    // std::thread t1{&X::task_run, &x, n};
    // &X::task_run 是一个整体，它们构成了成员指针，&类名::非静态成员
    // t1.join();

    // std::bind 也是默认按值复制的，即使我们的成员函数形参类型为引用
    std::thread t2{std::bind(&X::task_run, &x, n)};
    t2.join();

    std::thread t3{std::bind(&X::task_run, &x, std::ref(n))};
    t3.join();

    std::thread t4{[](auto i)
                   { std::cout << i << std::endl; }, "kk"}; // C++14 泛型lambda
    t4.join();
}
#elif defined(VERSION_10)
// 传参中的悬空引用问题
void f(const std::string &) {}
// std::thread 的构造函数中调用了创建线程的函数（windows 下可能为 _beginthreadex）
// 它将我们传入的参数，f、buffer ，传递给这个函数，在新线程中执行函数 f
// 也就是说，调用和执行 f(buffer) 并不是说要在 std::thread 的构造函数中，而是在创建的新线程中，具体什么时候执行，取决于操作系统的调度
// 所以完全有可能函数 test 先执行完，而新线程此时还没有进行 f(buffer) 的调用，转换为std::string，那么 buffer 指针就悬空了，会导致问题
void test()
{
    char buffer[1024]{};
    // todo.. code
    // buffer 是一个数组对象，作为 std::thread 构造参数的传递的时候会decay-copy （确保实参在按值传递时会退化） 隐式转换为了指向这个数组的指针
    std::thread t{f, buffer}; // “启动线程”，而不是调用传递的可调用对象
    t.detach();
}
// 解决方案: 1: 将 detach() 替换为 join() 2: 显式将 buffer 转换为 std::string : std::thread t{ f,std::string(buffer) }

int main()
{
    // A的引用只能引用A，或者以任何形式转换到A
    double a = 1;
    const int &p = a; // a 隐式转换到了 int 类型，此转换为右值表达式
    const std::string &str = "123";
}
#elif defined(VERSION_11)
// std::this_thread
int main()
{
    std::cout << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(1s);
    auto current_time = std::chrono::system_clock::now();
    std::cout << "-----" << std::endl;
    std::this_thread::sleep_until(current_time + 2s);
    std::this_thread::yield();
    /*
    // 线程需要等待某个操作完成，如果直接用一个循环不断判断这个操作是否完成就会使得这个线程占满 CPU 时间，这会造成资源浪费。
    // 此时可以判断操作是否完成，如果还没完成就调用 yield 交出 CPU 时间片让其他线程执行，过一会儿再来判断是否完成，这样这个线程占用 CPU 时间会大大减少
    while (!isDone()) { // 忙等，一直占用CPU时间片
        // std::this_thread::yield();
        std::this_thread::sleep_for(1us); // 不是忙等， 或者让线程休眠，睡一会儿
        // to do...
    }
    */
}
#elif defined(VERSION_12)
// std::thread 转移所有权
// 线程对象管理线程资源
// std::thread 不可复制。两个 std::thread 对象不可表示一个线程，std::thread 对线程资源是独占所有权
// 移动操作可以将一个 std::thread 对象的线程资源所有权转移给另一个 std::thread 对象

std::thread f()
{
    std::thread t{[] {}};
    return t; // 自动调用了移动构造，重载决议 会选择到移动构造
    // return t 先调用了 t 的移动构造，将资源转移给函数返回值的 std::thread 对象
    // 此时 t 没有了线程资源，才开始正常析构
}

// 函数调用传参 : 本质上时初始化(构造)形参的对象
void g(std::thread t)
{
    t.join();
}

int main()
{
#if 0
    // 移动构造
    std::thread t{[]
                  {
                      std::this_thread::sleep_for(500ms);
                      std::cout << std::this_thread::get_id() << '\n';
                  }};
    std::cout << std::boolalpha << t.joinable() << '\n'; // 线程对象 t 当前关联了活跃线程 打印 true
    std::thread t2{std::move(t)};                        // 将 t 的线程资源的所有权移交给 t2
    std::cout << std::boolalpha << t.joinable() << '\n'; // 线程对象 t 当前没有关联活跃线程 打印 false
    // t.join(); // Error! t 没有线程资源
    t2.join(); // t2 当前持有线程资源
    std::cout << std::boolalpha << t2.joinable() << '\n';
#else
    // 移动赋值
    std::thread t;                                       // 默认构造，没有关联活跃线程
    std::cout << std::boolalpha << t.joinable() << '\n'; // false
    std::thread t2{[] {}};
    t = std::move(t2);                                   // 转移线程资源的所有权到 t
    std::cout << std::boolalpha << t.joinable() << '\n'; // true
    t.join();

    t2 = std::thread([] {}); // 临时对象是右值表达式，不用调用 std::move
    t2.join();
#endif

    // return t 重载决议选择到了移动构造，将 t 线程资源的所有权转移给函数调用 f() 返回的临时 std::thread 对象中
    // 这个临时对象再用来初始化 rt ，临时对象是右值表达式，这里一样选择到移动构造，将临时对象的线程资源所有权移交给 rt
    // 此时 rt 具有线程资源的所有权，由它调用 join() 正常析构
    // 如果标准达到 C++17，强制的复制消除（RVO）保证这里少一次移动构造的开销（临时对象初始化 rt 的这次）
    // 经自己测试后，发现两次移动的开销都被省略
    std::thread rt = f();
    rt.join();

    std::thread tmp{[] {}};
    g(std::move(tmp));
    g(std::thread{[] {}});
}
#elif defined(VERSION_13)
// std::thread 源码 见 md 源码解析
struct X
{
    X(X &&x) noexcept {}
    template <class Fn, class... Args>
    X(Fn &&f, Args &&...args) {}
    X(const X &) = delete;

    // 添加约束，使其无法选择到有参构造函数
    // template <class Fn, class... Args, std::enable_if_t<!std::is_same_v<std::remove_cvref_t<Fn>, X>, int> = 0>
    // X(Fn &&f, Args &&...args) {}
};

int main()
{
    std::thread t{[] {}};
    std::cout << sizeof(std::thread) << std::endl;
    t.join();

    X x{[] {}};
    X x2{x}; // 正常属于拷贝构造，而拷贝构造被弃置
             // 但此时选择到了有参构造函数，不导致编译错误
}
#elif defined(VERSION_14)
// 包装 std::thread 这个类和 std::thread 的区别就是析构函数会自动 join
class joining_thread
{
    std::thread t;

public:
    joining_thread() noexcept = default;
    template <typename Callable, typename... Args>
    explicit joining_thread(Callable &&func, Args &&...args) : t{std::forward<Callable>(func), std::forward<Args>(args)...} {}
    explicit joining_thread(std::thread t_) noexcept : t{std::move(t_)} {}
    joining_thread(joining_thread &&other) noexcept : t{std::move(other.t)} {}

    joining_thread &operator=(std::thread &&other) noexcept
    {
        if (joinable())
        { // 如果当前有活跃线程，那就先执行完
            join();
        }
        t = std::move(other);
        return *this;
    }
    ~joining_thread()
    {
        if (joinable())
            join();
    }
    void swap(joining_thread &other) noexcept
    {
        t.swap(other.t);
    }
    std::thread::id get_id() const noexcept
    {
        return t.get_id();
    }
    bool joinable() const noexcept
    {
        return t.joinable();
    }
    void join()
    {
        t.join();
    }
    void detach()
    {
        t.detach();
    }
    std::thread &data() noexcept
    {
        return t;
    }
    const std::thread &data() const noexcept
    {
        return t;
    }
};

int main()
{
    auto func = []
    { std::osyncstream{std::cout} << std::this_thread::get_id() << std::endl; };
    std::vector<joining_thread> vec;
    for (int i = 0; i < 10; ++i)
        vec.emplace_back(func);
}
#elif defined(VERSION_15)

/*
C++20 std::jthread 相比于 C++ 11 引入的 std::thread，只是多了两个功能：
1: RAII 管理：在析构时自动调用 join()
2: 线程停止功能：线程的取消/停止
*/
// std::jthread 的通常实现就是单纯的保有 std::thread + std::stop_source(通常为8字节) 这两个数据成员

// std::jthread 所谓的线程停止只是一种基于用户代码的控制机制，而不是一种与操作系统系统有关系的线程终止
// 使用 std::stop_source 和 std::stop_token 提供了一种优雅地请求线程停止的方式，但实际上停止的决定和实现都由用户代码来完成

void f(std::stop_token stop_token, int value)
{
    while (!stop_token.stop_requested())
    { // 检查是否已经收到停止请求
        std::cout << value++ << ' ' << std::flush;
        std::this_thread::sleep_for(200ms);
    }
    std::cout << std::endl;
}
/*
1: get_stop_source：返回与 jthread 对象关联的 std::stop_source，允许从外部请求线程停止。
2: get_stop_token：返回与 jthread 对象停止状态关联的 std::stop_token，允许检查是否有停止请求。
3: request_stop：请求线程停止。
*/

/* !!!
“停止状态”指的是由 std::stop_source 和 std::stop_token 管理的一种标志，用于通知线程应该停止执行。
这种机制不是强制性的终止线程，而是提供一种线程内外都能检查和响应的信号(!!!)。
*/

int main()
{
#if 0
    std::jthread thread{f, 1}; // 打印 1..15 大约 3 秒
    std::this_thread::sleep_for(3s);
    // jthread 的析构函数调用 request_stop() 和 join()
#else
    std::jthread thread{f, 1}; // 打印 1..15 大约 3 秒
    // 隐式传递的参数为 与成员变量 std::stop_source(_Ssource) 相关联的 std::stop_token(_Ssource.get_token())
    std::this_thread::sleep_for(3s);
    thread.request_stop(); // 发送信息，线程终止
    std::this_thread::sleep_for(1s);
    std::cout << "end!\n";
#endif
}

/*
std::stop_source：
这是一个可以发出停止请求的类型。当调用 stop_source 的 request_stop() 方法时，它会设置内部的停止状态为“已请求停止”
任何持有与这个 stop_source 关联的 std::stop_token 对象都能检查到这个停止请求

std::stop_token：
这是一个可以检查停止请求的类型。线程内部可以定期检查 stop_token 是否收到了停止请求
通过调用 stop_token.stop_requested()，线程可以检测到停止状态是否已被设置为“已请求停止”
*/

#endif