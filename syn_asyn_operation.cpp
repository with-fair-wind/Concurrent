#include <algorithm>
#include <barrier>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <latch>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <omp.h>
#include <queue>
#include <random>
#include <semaphore>
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

#define VERSION_17
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
#if 1
    std::this_thread::sleep_for(5s);
    std::cout << "Reset arrived to allow other threads to continue waiting and blocking\n";
    arrived = false;
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
    std::this_thread::sleep_for(5s);
    std::cout << "Reset arrived to allow other threads to continue waiting and blocking\n";
    arrived = false;
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
        // std::this_thread::sleep_for(10ms);
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
// future
/*
1:用于处理线程中需要等待某个事件的情况，线程知道预期结果。等待的同时也可以执行其它的任务
2:独占的 std::future 、共享的 std::shared_future 类似独占/共享智能指针
3:std::future 只能与单个指定事件关联，而 std::shared_future 能关联多个事件
4:它们都是模板，它们的模板类型参数，就是其关联的事件（函数）的返回类型
5:当多个线程需要访问一个独立 future 对象时， 必须使用互斥量或类似同步机制进行保护
6:??? 而多个线程访问同一共享状态，若每个线程都是通过其自身的 shared_future 对象副本进行访问，则是安全的
*/

// 创建异步任务获取返回值
// 假设需要执行一个耗时任务并获取其返回值，但是并不急切的需要它。那么就可以启动新线程计算
// 然而 std::thread 没提供直接从线程获取返回值的机制。所以可以使用 std::async 函数模板。
/*
1:使用 std::async 启动一个异步任务，会返回一个 std::future 对象，这个对象和任务关联，将持有最终计算出来的结果。
2:当需要任务执行完的结果的时候，只需要调用 get() 成员函数，就会阻塞直到 future 为就绪为止（即任务执行完毕），返回执行结果。
3:valid() 成员函数检查 future 当前是否关联共享状态，即是否当前关联任务。还未关联，或者任务已经执行完（调用了 get()、set()），都会返回 false
*/
#define Strategy
#ifdef Init
int task(int n)
{
    std::cout << "异步任务 ID: " << std::this_thread::get_id() << "\n";
    return n * n;
}

int main()
{
    std::future<int> future = std::async(task, 10);
    std::cout << "main: " << std::this_thread::get_id() << '\n';
    std::cout << std::boolalpha << future.valid() << '\n'; // true
    std::cout << future.get() << '\n';
    std::cout << std::boolalpha << future.valid() << '\n'; // false
}

#elif defined(Params)
// std::async支持所有可调用(Callable)对象，并且也是默认按值复制，必须使用 std::ref 才能传递引用。
// 和 std::thread 一样，内部会将保有的参数副本转换为右值表达式进行传递，这是为了那些只支持移动的类型

struct X
{
    int operator()(int n) const
    {
        return n * n;
    }
};
struct Y
{
    int f(int n) const
    {
        return n * n;
    }
};
void f(int &p) { std::cout << &p << '\n'; }

int main()
{
    Y y;
    int n = 0;
    auto t1 = std::async(X{}, 10);
    auto t2 = std::async(&Y::f, &y, 10);
    auto t3 = std::async([] {});
    auto t4 = std::async(f, std::ref(n)); //
    std::cout << &n << '\n';
}

#elif defined(MoveOnly)
struct move_only
{
    move_only() { std::puts("默认构造"); }
    move_only(move_only &&) noexcept { std::puts("移动构造"); }
    move_only &operator=(move_only &&) noexcept
    {
        std::puts("移动赋值");
        return *this;
    }
    move_only(const move_only &) = delete;
};

move_only task(move_only x)
{
    std::cout << "异步任务 ID: " << std::this_thread::get_id() << '\n';
    return x;
}

int main()
{
    move_only x;
    std::future<move_only> future = std::async(task, std::move(x));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "main\n";
    move_only result = future.get(); // 等待异步任务执行完毕
}

#elif defined(Strategy)
/*
1:std::launch::async 在不同线程上执行异步任务。
2:std::launch::deferred 惰性求值，不创建线程，等待 future 对象调用 wait 或 get 成员函数的时候执行任务。
*/
/*
std::async 函数模板有两个重载，不给出执行策略就是以：std::launch::async | std::launch::deferred 调用另一个重载版本
此策略表示由实现选择到底是否创建线程执行异步任务。
如果系统资源充足，并且异步任务的执行不会导致性能问题，那么系统可能会选择在新线程中执行任务。
如果系统资源有限，或者延迟执行可以提高性能或节省资源，那么系统可能会选择延迟执行。
而在MSVC中 只要不是 launch::deferred 策略，那么 MSVC STL 实现中都是必然在线程中执行任务。因为是线程池，所以执行新任务是否创建新线程，任务执行完毕线程是否立即销毁，不确定
*/

void f()
{
    std::cout << std::this_thread::get_id() << '\n';
}

void t1()
{
    std::this_thread::sleep_for(3s);
    std::cout << "t1 end!\n";
}

void t2()
{
    std::cout << "wait fot t1 end!\n";
}

int main()
{
    std::cout << std::this_thread::get_id() << '\n';
    auto f1 = std::async(std::launch::deferred, f);
    f1.wait();                                                           // 在 wait() 或 get() 调用时执行，不创建线程
    auto f2 = std::async(std::launch::async, f);                         // 创建线程执行异步任务
    auto f3 = std::async(std::launch::deferred | std::launch::async, f); // 实现选择的执行方式
    // 如果从 std::async 获得的 std::future 没有被移动或绑定到引用，那么在完整表达式结尾， std::future 的**析构函数将阻塞，直到到异步任务完成**。因为临时对象的生存期就在这一行，而对象生存期结束就会调用调用析构函数。
    // 这并不能创建异步任务，它会阻塞，然后逐个执行
    std::async(std::launch::async, t1); // 临时量的析构函数等待 t1()
    std::async(std::launch::async, t2); // t1() 完成前不开始
}
// 被移动的 std::future 没有所有权，失去共享状态，不能调用 get、wait 成员函数。
#endif
#elif defined(VERSION_4)
// future 与 std::packaged_task
// 类模板 std::packaged_task 包装任何可调用(Callable)目标（函数、lambda 表达式、bind 表达式或其它函数对象），使得能异步调用它
// 其返回值或所抛异常被存储于能通过 std::future 对象访问的共享状态中
// 通常它会和 std::future 一起使用，不过也可以单独使用
// std::packaged_task 只能移动，不能复制

#if 0
int main()
{
#if 0
    std::packaged_task<double(int, int)> task([](int a, int b)
                                              {
        std::cout << "task ID: " << std::this_thread::get_id() << "\n"; 
        return std::pow(a, b); });
    std::future<double> future = task.get_future(); // 和 future 关联
    task(10, 2);                                    // 此处执行任务
    std::cout << future.get() << "\n";              // 不阻塞，此处获取返回值
#else
    // 在线程中执行异步任务，然后再获取返回值，可以这么做
    std::packaged_task<double(int, int)> task([](int a, int b)
                                              { return std::pow(a, b); });
    std::future<double> future = task.get_future();
    std::thread t{std::move(task), 10, 2}; // 任务在线程中执行
    // todo.. 幻想还有许多耗时的代码
    t.join();
    std::cout << future.get() << '\n'; // 并不阻塞，获取任务返回值罢了
#endif
}
#else
// std::packaged_task 也可以在线程中传递，在需要的时候获取返回值，而非像上面那样将它自己作为可调用对象
template <typename R, typename... Ts, typename... Args>
    requires std::invocable<std::packaged_task<R(Ts...)> &, Args...>
void async_task(std::packaged_task<R(Ts...)> &task, Args &&...args)
{
    // todo..
    task(std::forward<Args>(args)...);
    std::this_thread::sleep_for(2s);
}

int main()
{
    std::packaged_task<int(int, int)> task([](int a, int b)
                                           { std::this_thread::sleep_for(2s);
                                            return a + b; });
    int value = 50;
    std::future<int> future = task.get_future();
    // 创建一个线程来执行异步任务
    // 套了一个 lambda，这是因为函数模板不是函数，它并非具体类型，没办法直接被那样传递使用，只能包一层
    std::thread t{[&]
                  { async_task(task, value, value); }};
    std::cout << future.get() << '\n'; // 会阻塞直至任务执行完毕
    std::cout << "end!" << std::endl;
    t.join();
}
#endif
#elif defined(VERSION_5)
// 改写sum : std::package_task + std::future 的形式
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

        std::vector<std::future<value_type>> futures{num_threads};
        std::vector<std::packaged_task<value_type()>> tasks;

        std::vector<std::thread> threads;

        auto start = first;
        for (std::size_t i = 0; i < num_threads; ++i)
        {
            auto end = std::next(start, chunk_size + (i < remainder ? 1 : 0));
            tasks.emplace_back([start, end]
                               { return std::accumulate(start, end, value_type{}); });
            futures[i] = tasks[i].get_future();
            threads.emplace_back(std::move(tasks[i]));
            start = end;
        }

        for (auto &thread : threads)
            thread.join();

        value_type total_sum{};
        for (auto &future : futures)
            total_sum += future.get();
        return total_sum;
    }
    return std::accumulate(first, second, value_type{});
}

int main()
{
    unsigned int n = std::thread::hardware_concurrency();
    std::cout << "支持 " << n << " 个并发线程。\n";

    std::vector<std::string> vecs{"1", "2", "3", "4"};
    auto result = sum(vecs.begin(), vecs.end());
    std::cout << result << '\n';

    vecs.clear();
    for (std::size_t i = 0; i <= 102u; ++i)
        vecs.push_back(std::to_string(i));

    result = sum(vecs.begin(), vecs.end());
    // std::cout << result << '\n';
}

#elif defined(VERSION_6)
// promise
// 类模板 std::promise 用于存储一个值或一个异常，之后通过 std::promise 对象所创建的 std::future 对象异步获得
// 只能移动不可复制，多用作函数形参

#define ReSet
#ifdef Normal
void calculate_square(std::promise<int> promiseObj, int num)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // 计算平方并设置值到 promise 中
    promiseObj.set_value(num * num);

    std::this_thread::sleep_for(2s);
}

int main()
{
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    std::thread t{calculate_square, std::move(promise), 5};

    /*
    *** 区别于 std::packaged_task 阻塞至 std::packaged_task 任务执行完，promise 关联的 future 只阻塞到set_value/set_exception
    */

    std::cout << future.get() << std::endl; // 阻塞直至结果可用
    std::cout << "end!" << std::endl;
    t.join();
}
#elif defined(Exception)
// set_exception() 接受一个 std::exception_ptr 类型的参数，这个参数通常通过 std::current_exception() 获取，用于指示当前线程中抛出的异常
// std::future 对象通过 get() 函数获取这个异常，如果 promise 所在的函数有异常被抛出，则 std::future 对象会重新抛出这个异常，从而允许主线程捕获并处理它
// 编译器不知道为啥不行
// https://godbolt.org/z/xs3x178vE
void throw_function(std::promise<int> prom)
{
    try
    {
        throw std::runtime_error("一个异常");
    }
    catch (...)
    {
        prom.set_exception(std::current_exception());
    }
}

int main()
{
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    std::jthread t(throw_function, std::move(prom));

    try
    {
        std::cout << "等待线程执行，抛出异常并设置\n";
        throw fut.get();
    }
    catch (std::exception &e)
    {
        std::cerr << "来自线程的异常: " << e.what() << '\n';
    }
}
#elif defined(ReSet)
// 共享状态的 promise 已经存储值或者异常，再次调用 set_value（set_exception） 会抛出 std::future_error 异常，将错误码设置为 promise_already_satisfied
void throw_function(std::promise<int> prom)
{
    prom.set_value(100);
    try
    {
        throw std::runtime_error("一个异常");
    }
    catch (...)
    {
        try
        {
            // 共享状态的 promise 已存储值，调用 set_exception/set_value 产生异常
            prom.set_exception(std::current_exception());
            // prom.set_value(50);
        }
        catch (std::exception &e)
        {
            std::cerr << "来自 set_exception 的异常: " << e.what() << '\n';
        }
    }
}

int main()
{
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    std::thread t(throw_function, std::move(prom));

    std::cout << "等待线程执行，抛出异常并设置\n";
    std::cout << "值：" << fut.get() << '\n'; // 100

    t.join();
}
#endif

#elif defined(VERSION_7)
// future 是一次性的，需要注意移动。且调用 get 函数后，future 对象也会失去共享状态
/*
1:移动语义：移动操作标志着所有权的转移，意味着 future 不再拥有共享状态（如之前所提到）。get 和 wait 函数要求 future 对象拥有共享状态，否则会抛出异常。
2:共享状态失效：调用 get 成员函数时，future 对象必须拥有共享状态，但调用完成后，它就会失去共享状态，不能再次调用 get
*/

// 在 get 函数中: future _Local{_STD move(*this)}; 将当前对象的共享状态转移给了这个局部对象，而局部对象在函数结束时析构。这意味着当前对象失去共享状态，并且状态被完全销毁。
int main()
{
    std::future<void> future = std::async([] {});
    std::cout << std::boolalpha << future.valid() << '\n'; // true
    future.get();
    std::cout << std::boolalpha << future.valid() << '\n'; // false
    try
    {
        future.get(); // 抛出 future_errc::no_state 异常
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

#elif defined(VERSION_8)
// 多个线程的等待 std::shared_future
// std::future 有一个局限：future 是一次性的，它的结果只能被一个线程获取。get() 成员函数只能调用一次，当结果被某个线程获取后，std::future 就无法再用于其他线程

#if 0
// 可能有多个线程都需要耗时的异步任务的返回值
int task()
{
    // todo...
    return 10;
}

void thread_functio1(std::shared_future<int> fut)
// void thread_functio1(std::shared_future<int> &fut)
// void thread_functio1(std::future<int> &fut)
{
    // todo...
    try
    {
        int res = fut.get();
        std::cout << "func1 need: " << res << "\n";
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }
    // todo...
}

void thread_functio2(std::shared_future<int> fut)
// void thread_functio2(std::shared_future<int> &fut)
// void thread_functio2(std::future<int> &fut)
{
    // todo...
    try
    {
        int res = fut.get();
        std::cout << "func2 need: " << res << "\n";
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }
    // todo...
}

int main()
{
    std::shared_future<int> future = std::async(task);
    std::jthread t1{thread_functio1, future};
    std::jthread t2{thread_functio2, future};
    // std::jthread t1{thread_functio1, std::ref(future)};
    // std::jthread t2{thread_functio2, std::ref(future)};
}
#else
// 此时就需要使用 std::shared_future 来替代 std::future 了。
// std::future 与 std::shared_future 的区别就如同 std::unique_ptr、std::shared_ptr 一样
// std::future 是只能移动的，其所有权可以在不同的对象中互相传递，但只有一个对象可以获得特定的同步结果
// 而 std::shared_future 是可复制的，多个对象可以指代同一个共享状态

/*
***
在多个线程中对"同一个" std::shared_future 对象进行操作时（如果没有进行同步保护）存在条件竞争。
而从多个线程访问同一共享状态，若每个线程都是通过其自身的 shared_future 对象副本进行访问，则是安全的
***
*/
std::string fetch_data()
{
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟耗时操作
    return "从网络获取的数据！";
}

int main()
{
    std::future<std::string> future_data = std::async(std::launch::async, fetch_data);

    // // 转移共享状态，原来的 future 被清空  valid() == false
    std::shared_future<std::string> shared_future_data = future_data.share();
#if 0
    // 多个线程持有一个 shared_future对象并操作
    // 这段代码存在数据竞争, 它并没有提供线程安全的方式, lambda 是按引用传递，也就是“同一个”进行操作了。

    // 第一个线程等待结果并访问数据
    std::thread thread1([&shared_future_data]
                        {
        std::cout << "线程1:等待数据中..." << std::endl;
        shared_future_data.wait();
        std::cout << "线程1:收到数据:" << shared_future_data.get() << std::endl; });

    // 第二个线程等待结果并访问数据
    std::thread thread2([&shared_future_data] // 多个线程持有一个 shared_future对象并操作
                        {
        std::cout << "线程2:等待数据中..." << std::endl;
        shared_future_data.wait();
        std::cout << "线程2:收到数据:" << shared_future_data.get() << std::endl; });
#else
    // 第一个线程等待结果并访问数据
    std::thread thread1([shared_future_data]
                        {
        std::cout << "线程1:等待数据中..." << std::endl;
        shared_future_data.wait();
        std::cout << "线程1:收到数据:" << shared_future_data.get() << std::endl; });

    // 第二个线程等待结果并访问数据
    std::thread thread2([shared_future_data] // 多个线程持有一个 shared_future对象并操作
                        {
        std::cout << "线程2:等待数据中..." << std::endl;
        shared_future_data.wait();
        std::cout << "线程2:收到数据:" << shared_future_data.get() << std::endl; });
#endif
    thread1.join();
    thread2.join();
}
#endif

#elif defined(VERSION_9)
// 返回类型是一个枚举类 std::future_status ，三个枚举项分别表示三种 future 状态。
/*
deferred 共享状态持有的函数正在延迟运行，结果将仅在明确请求时计算
ready 共享状态就绪
timeout 共享状态在经过指定的等待时间内仍未就绪
*/

int main()
{
#define TimeOut
#ifdef Deferred
    auto future = std::async(std::launch::deferred, []
                             { std::cout << "deferred\n"; });
    if (future.wait_for(35ms) == std::future_status::deferred)
        std::cout << "std::launch::deferred" << '\n';
    future.wait(); // 在 wait() 或 get() 调用时执行，不创建线程
#elif defined(Ready)
    std::future<int> future = std::async([]
                                         { 
                                            std::this_thread::sleep_for(34ms);
                                            return 6; });
    if (future.wait_for(35ms) == std::future_status::ready)
        std::cout << future.get() << '\n';
    else
        std::cout << "not ready\n";
#elif defined(TimeOut)
    std::future<int> future = std::async([]
                                         { 
                                            std::this_thread::sleep_for(36ms);
                                            return 6; });
    if (future.wait_for(35ms) == std::future_status::timeout)
        std::cout << "任务还未执行完毕\n";
    else
        std::cout << "任务执行完毕\n"
                  << future.get() << std::endl;
#endif
}
#elif defined(VERSION_10)
// Windows 内核中的时间间隔计时器默认每隔 15.6 毫秒触发一次中断。因此，如果你使用基于系统时钟的计时方法，默认情况下精度约为 15.6 毫秒。不可能达到纳秒级别。
#ifdef _MSC_VER
#pragma comment(lib, "mylib.lib")
#endif
#include "Windows.h"
int main()
{
    // timeBeginPeriod(1);
    auto start = std::chrono::system_clock::now();
    std::this_thread::sleep_for(1s);
    auto end = std::chrono::system_clock::now();
    auto res = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1000>>>(end - start);
    std::cout << res.count() << std::endl;
    // timeEndPeriod(1);
}

#elif defined(VERSION_11)
std::condition_variable cv;
bool done{};
std::mutex m;

bool wait_loop()
{
    const auto timeout = std::chrono::steady_clock::now() + 500ms;
    std::unique_lock<std::mutex> lk{m};
#if 0
    while (!done)
    {
        if (cv.wait_until(lk, timeout) == std::cv_status::timeout)
        {
            std::cout << "超时 500ms\n";
            return false;
        }
    }
#else
    if (!cv.wait_until(lk, timeout, []
                       { return done; }))
    {
        std::cout << "超时 500ms\n";
        return false;
    }
#endif
    return true;
}

int main()
{
    std::thread t{wait_loop};
    std::this_thread::sleep_for(600ms);
    done = true;
    cv.notify_one();
    t.join();
}

#elif defined(VERSION_12)
// C++20 引入了信号量 信号量源自操作系统，是一个古老而广泛应用的同步设施，在各种编程语言中都有自己的抽象实现
// 信号量是一个非常轻量简单的同步设施，它维护一个计数，这个计数不能小于 0
/*
信号量提供两种基本操作：释放（增加计数）和等待（减少计数）。
如果当前信号量的计数值为 0，那么执行“等待”操作的线程将会一直阻塞，直到计数大于 0，也就是其它线程执行了“释放”操作。

C++ 提供了两个信号量类型：std::counting_semaphore 与 std::binary_semaphore，定义在 <semaphore> 中
binary_semaphore 只是 counting_semaphore 的一个特化别名 : using binary_semaphore = counting_semaphore<1>;

acquire 函数就是“等待”（原子地减少计数），release 函数就是"释放"（原子地增加计数）
*/

/*
1:counting_semaphore 是一个轻量同步原语，能控制对共享资源的访问。
2:不同于 std::mutex，counting_semaphore 允许同一资源进行多个并发的访问，至少允许 LeastMaxValue 个同时访问者
3:binary_semaphore 是 std::counting_semaphore 的特化的别名，其 LeastMaxValue 为 1
4:LeastMaxValue 是我们设置的非类型模板参数，意思是信号量维护的计数最大值
5:如其名所示，LeastMaxValue 是最小 的最大值，而非实际 最大值。静态成员函数 max()可能产生大于 LeastMaxValue 的值。
*/

// 全局二元信号量对象
// 设置对象初始计数为 0
#if 0
std::binary_semaphore smph_signal_main_to_thread{0};
std::binary_semaphore smph_signal_thread_to_main{0};

void thread_proc()
{
    smph_signal_main_to_thread.acquire();
    std::cout << "[线程] 获得信号" << std::endl;

    std::this_thread::sleep_for(3s);

    std::cout << "[线程] 发送信号\n";
    smph_signal_thread_to_main.release();
}

int main()
{
    std::jthread thr_worker{thread_proc};

    std::cout << "[主] 发送信号\n";
    smph_signal_main_to_thread.release();

    smph_signal_thread_to_main.acquire();
    std::cout << "[主] 获得信号\n";
}
#else
// 定义一个信号量，最大并发数为 3
std::counting_semaphore<3> semaphore{3};

void handle_request(int request_id)
{
    // 请求到达，尝试获取信号量
    std::cout << "进入 handle_request 尝试获取信号量\n";

    semaphore.acquire();

    std::cout << "成功获取信号量\n";

    // 此处延时三秒可以方便测试，会看到先输出 3 个“成功获取信号量”，因为只有三个线程能成功调用 acquire，剩余的会被阻塞
    std::this_thread::sleep_for(3s);

    // 模拟处理时间
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<> dis(1, 5);
    int processing_time = dis(gen);
    std::this_thread::sleep_for(std::chrono::seconds(processing_time));

    std::cout << std::format("请求 {} 已被处理\n", request_id);

    semaphore.release();
}

int main()
{
    // 模拟 10 个并发请求
    std::vector<std::jthread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(handle_request, i);
    }
}
#endif

#elif defined(VERSION_13)
// 自己的理解
/*
std::binary_semaphore 可以近似当作互斥量使用
std::condition_variable 条件变量因为需要结合 unique_lock 即使被 notify_all 也只有一个线程可以抢到锁
std::counting_semaphore 信号量允许同一资源进行多个并发的访问，此时若涉及到对共享资源的操作，还是需要进行互斥量保护
*/

#elif defined(VERSION_14)
// 闩 (latch) 与屏障 (barrier) 是线程协调机制
// 允许任何数量的线程阻塞直至期待数量的线程到达
// 闩不能重复使用，而屏障则可以, 它们定义在标头 <latch> 与 <barrier>
// std::latch “闩” : 单次使用的线程屏障
/*
latch 类维护着一个 std::ptrdiff_t 类型的计数，且只能减少计数，无法增加计数
在创建对象的时候初始化计数器的值。线程可以阻塞，直到 latch 对象的计数减少到零
由于无法增加计数，这使得 latch 成为一种单次使用的屏障
*/

#if 0
std::latch work_start{3};

void work()
{
    std::cout << "等待其它线程执行\n";
    work_start.wait(); // 等待计数为 0
    std::cout << "任务开始执行\n";
}

int main()
{
    std::jthread thread{work};
    std::this_thread::sleep_for(3s);
    std::cout << "休眠结束\n";
    work_start.count_down();  // 默认值是 1 减少计数 1
    work_start.count_down(2); // 传递参数 2 减少计数 2
}
#else
// 由于 latch 的计数不可增加，它的使用通常非常简单，可以用来划分任务执行的工作区间
std::latch latch{10};
// arrive_and_wait 函数等价于：count_down(n); wait(); 也就是减少计数 + 等待
// 必须等待所有线程执行到 latch.arrive_and_wait(); 将 latch 的计数减少至 0 才能继续往下执行。
void f(int id)
{
    // todo.. 脑补任务
    std::this_thread::sleep_for(1s);
    std::cout << std::format("线程 {} 执行完任务，开始等待其它线程执行到此处\n", id);
    latch.arrive_and_wait();
    std::cout << std::format("线程 {} 彻底退出函数\n", id);
}

int main()
{
    std::vector<std::jthread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(f, i);
    }
}
#endif
#elif defined(VERSION_15)
// std::barrier 和 std::latch 最大的不同是，前者可以在阶段完成之后将计数重置为构造时传递的值，而后者只能减少计数
#if 0
std::barrier barrier{10,
                     [n = 1]() mutable noexcept
                     { std::cout << "\t第" << n++ << "轮结束\n"; }};
// 一个过程完成后，重置计数，然后调用传入的 lambda 表达式

void f(int start, int end)
{
    for (int i = start; i <= end; ++i)
    {
        std::osyncstream{std::cout} << i << ' ';
        barrier.arrive_and_wait(); // 减少计数并等待 解除阻塞时就重置计数并调用函数对象

        std::this_thread::sleep_for(300ms);
    }
}
// arrive_and_wait 等价于 wait(arrive());原子地将期待计数减少 1，然后在当前阶段的同步点阻塞直至运行当前阶段的阶段完成步骤
// arrive_and_wait() 会在期待计数减少至 0 时调用我们构造 barrier 对象时传入的 lambda 表达式，并解除所有在阶段同步点上阻塞的线程。之后重置期待计数为构造中指定的值。屏障的一个阶段就完成了。
//  std::osyncstream C++20 确保输出流在多线程环境中同步，避免除数据竞争，而且将不以任何方式穿插或截断

int main()
{
    std::vector<std::jthread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(f, i * 10 + 1, (i + 1) * 10);
    }
}
#else
//  arrive_and_drop 可以改变重置的计数值：它将所有后继阶段的初始期待计数减少一，当前阶段的期待计数也减少一, 用来控制在需要的时候，让一些线程退出同步
std::atomic_int active_threads{4};
std::barrier barrier{4,
                     [n = 1]() mutable noexcept
                     {
                         std::cout << "\t第" << n++ << "轮结束，活跃线程数: " << active_threads << '\n';
                     }};
// lambda 表达式必须声明为 noexcept ，因为 std::barrier 要求其函数对象类型必须是不抛出异常的
// 即要求 std::is_nothrow_invocable_v<_Completion_function&> 为 true

void f(int thread_id)
{
    for (int i = 1; i <= 5; ++i)
    {
        std::osyncstream{std::cout} << "线程 " << thread_id << " 输出: " << i << '\n';
        if (i == 3 && thread_id == 2)
        { // 假设线程 ID 为 2 的线程在完成第三轮同步后退出
            std::osyncstream{std::cout} << "线程 " << thread_id << " 完成并退出\n";
            --active_threads;          // 减少活跃线程数
            barrier.arrive_and_drop(); // 减少当前计数 1，并减少重置计数 1
            return;
        }
        barrier.arrive_and_wait(); // 减少计数并等待，解除阻塞时重置计数并调用函数对象
    }
}

int main()
{
    std::vector<std::jthread> threads;
    for (int i = 1; i <= 4; ++i)
    {
        threads.emplace_back(f, i);
    }
}
#endif
#elif defined(VERSION_16)
// [OpenMP](https://learn.microsoft.com/zh-cn/cpp/parallel/openmp/2-directives?view=msvc-170#263-barrier-directive)
void f(int start, int end, int thread_id)
{
    for (int i = start; i <= end; ++i)
    {
        // 输出当前线程的数字
        std::cout << std::to_string(i) + " ";

        // 等待所有线程同步到达 barrier 也就是等待都输出完数字
#pragma omp barrier

// 每个线程输出完一句后，主线程输出轮次信息
#pragma omp master
        {
            static int round_number = 1;
            std::cout << "\t第" << round_number++ << "轮结束\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 再次同步 等待所有线程（包括主线程）到达此处、避免其它线程继续执行打断主线程的输出
#pragma omp barrier
    }
}

int main()
{
    constexpr int num_threads = 10;
    omp_set_num_threads(num_threads);

#pragma omp parallel
    {
        const int thread_id = omp_get_thread_num();
        f(thread_id * 10 + 1, (thread_id + 1) * 10, thread_id);
    }
}
// https://godbolt.org/z/fabqhbx3P

#elif defined(VERSION_17)
// 启动线程时可能存在其它数据成员还未初始化，导致未定义行为
/*
初始化的实际顺序如下：
1: 果构造函数是最终派生类的，那么按基类声明的深度优先、从左到右的遍历中的出现顺序（从左到右指的>是基说明符列表中所呈现的顺序），初始化各个虚基类。
2: 然后，以在此类的基类说明符列表中出现的从左到右顺序，初始化各个直接基类。
3: 然后，以类定义中的声明顺序，初始化各个非静态成员。
4: 最后，执行构造函数体。
*/
struct X
{
    X()
    {
        // 假设 X 的初始化没那么快
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::puts("X");
        v.resize(10, 6);
    }
    std::vector<int> v;
};

#define Type1
struct Test
{

#ifdef Type1
#define Type1_2
#ifdef Type1_1
    Test() : t{&Test::f, this} // 线程已经开始执行
    {
    }
#elif defined(Type1_2)
    Test()
    {
        t = std::thread{&Test::f, this};
    }
#endif
#elif defined(Type2)
    Test() {}
    void start() { t = std::thread{&Test::f, this}; }
#endif

    ~Test()
    {
        if (t.joinable())
            t.join();
    }
    void f() const
    { // 如果在函数执行的线程 f 中使用 x 则会存在问题。使用了未初始化的数据成员 ub
        std::cout << "f\n";
        std::cout << x.v[5] << std::endl;
    }

#ifdef Type1
#ifdef Type1_1
    X x;
    std::thread t;
#elif defined(Type1_2)
    std::thread t;
    X x;
#endif
#elif defined(Type2)
    std::thread t; // 声明顺序决定了初始化顺序，优先初始化 t
    X x;
#endif
};

int main()
{
#ifdef Type1
    Test t;
#elif defined(Type2)
    Test t;
    t.start(); // 此方式不用管初始化顺序，因为此时初始化肯定已经全部完成了
#endif
}

#endif