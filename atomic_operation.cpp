#include <algorithm>
#include <atomic>
#include <chrono>
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

// 原子操作即不可分割的操作。系统的所有线程，不可能观察到原子操作完成了一半
// 如果一个线程写入原子对象，同时另一线程从它读取，那么行为有良好定义
// 任何 std::atomic 类型，初始化不是原子操作
// 未定义行为优化(ub优化) : 优化会假设程序中没有未定义行为

#define VERSION_3
#ifdef VERSION_1

#if 0
// 标准原子类型的实现通常包括一个 is_lock_free() 成员函数，允许用户查询特定原子类型的操作是否是通过直接的原子指令实现（返回 true），还是通过锁来实现（返回 false）
// 如果原子类型的内部使用互斥量实现，那么不可能有性能的提升

std::atomic<int> a = 10;
int main()
{
    std::cout << std::boolalpha << a.is_lock_free() << std::endl;
    std::cout << a.load() << std::endl;
}
#else
// 在 C++17 中，所有原子类型都有一个 static constexpr 的数据成员 is_always_lock_free : 如果当前环境上的原子类型 X 是无锁类型，那么 X::is_always_lock_free 将返回 true
// 标准库还提供了一组宏 ATOMIC_xxx_LOCK_FREE ，在编译时对各种整数原子类型是否无锁进行判断
int main()
{
    // 检查 std::atomic<int> 是否总是无锁
    if constexpr (std::atomic<int>::is_always_lock_free)
    {
        std::cout << "当前环境 std::atomic<int> 始终是无锁" << std::endl;
    }
    else
    {
        std::cout << "当前环境 std::atomic<int> 并不总是无锁" << std::endl;
    }

// 使用 ATOMIC_INT_LOCK_FREE 宏进行编译时检查
#if ATOMIC_INT_LOCK_FREE == 2
    std::cout << "int 类型的原子操作一定无锁的。" << std::endl;
#elif ATOMIC_INT_LOCK_FREE == 1
    std::cout << "int 类型的原子操作有时是无锁的。" << std::endl;
#else
    std::cout << "int 类型的原子操作一定有锁的。" << std::endl;
#endif
}
#endif

#elif defined(VERSION_2)
// 通常 std::atomic 对象不可进行复制、移动、赋值。不过可以隐式转换成对应的内置类型，因为它有转换函数
/*
atomic(const atomic&) = delete;
atomic& operator=(const atomic&) = delete;
operator T() const noexcept;
*/

/*
std::atomic 类模板不仅只能使用标准库定义的特化类型，我们也完全可以自定义类型创建对应的原子对象
不过因为是通用模板，操作仅限 load()、store()、exchange()、compare_exchange_weak() 、 compare_exchange_strong()，以及一个转换函数

模板 std::atomic 可用任何满足可复制构造 (CopyConstructible)及可复制赋值 (CopyAssignable)的可平凡复制 (TriviallyCopyable)类型 T 实例化
*/

struct trivial_type
{
    int x{};
    float y{};

    trivial_type() {}

    trivial_type(int a, float b) : x{a}, y{b} {}

    trivial_type(const trivial_type &other) = default;

    trivial_type &operator=(const trivial_type &other) = default;

    ~trivial_type() = default;
};

template <typename T>
void isAtomic()
{
    static_assert(std::is_trivially_copyable<T>::value, "");
    static_assert(std::is_copy_constructible<T>::value, "");
    static_assert(std::is_move_constructible<T>::value, "");
    static_assert(std::is_copy_assignable<T>::value, "");
    static_assert(std::is_move_assignable<T>::value, "");
}

int main()
{
    isAtomic<trivial_type>();
    // 创建一个 std::atomic<trivial_type> 对象
    std::atomic<trivial_type> atomic_my_type{trivial_type{10, 20.5f}};

    // 使用 store 和 load 操作来设置和获取值
    trivial_type new_value{30, 40.5f};
    atomic_my_type.store(new_value);

    std::cout << "x: " << atomic_my_type.load().x << ", y: " << atomic_my_type.load().y << std::endl;

    // 使用 exchange 操作
    trivial_type exchanged_value = atomic_my_type.exchange(trivial_type{50, 60.5f});
    std::cout << "交换前的 x: " << exchanged_value.x
              << ", 交换前的 y: " << exchanged_value.y << std::endl;
    std::cout << "交换后的 x: " << atomic_my_type.load().x
              << ", 交换后的 y: " << atomic_my_type.load().y << std::endl;
}

// 原子类型的每个操作函数，都有一个内存序参数，这个参数可以用来指定执行顺序，操作分为三类：
/*
1: Store 操作（存储操作）：可选的内存序包括 memory_order_relaxed、memory_order_release、memory_order_seq_cst。

2: Load 操作（加载操作）：可选的内存序包括 memory_order_relaxed、memory_order_consume、memory_order_acquire、memory_order_seq_cst。

3: Read-modify-write（读-改-写）操作：可选的内存序包括 memory_order_relaxed、memory_order_consume、memory_order_acquire、memory_order_release、memory_order_acq_rel、memory_order_seq_cst。
*/

#elif defined(VERSION_3)
// std::atomic_flag : 可以在两个状态间切换：设置（true）和清除（false）
// 在 C++20 之前，std::atomic_flag 类型的对象需要以
// 在 C++20 中 std::atomic_flag 的默认构造函数保证对象为“清除”（false）状态，就不再需要使用 ATOMIC_FLAG_INIT

// ATOMIC_FLAG_INIT 在 MSVC STL 它只是一个 {}，在 libstdc++ 与 libc++ 它只是一个 { 0 }

/*
当标志对象已初始化，它只能做三件事情：销毁、清除、设置。这些操作对应的函数分别是：
1: clear() （清除）：将标志对象的状态原子地更改为清除（false）
2: test_and_set（测试并设置）：将标志对象的状态原子地更改为设置（true），并返回它先前保有的值。
3: 销毁：对象的生命周期结束时，自动调用析构函数进行销毁操作。
每个操作都可以指定内存顺序。clear() 是一个“读-改-写”操作，可以应用任何内存顺序。默认的内存顺序是 memory_order_seq_cst
*/

// 有限的特性使得 std::atomic_flag 非常适合用作制作自旋锁
// 自旋锁可以理解为一种忙等锁，因为它在等待锁的过程中不会主动放弃 CPU，而是持续检查锁的状态
// 与此相对，std::mutex 互斥量是一种睡眠锁。当线程请求锁（lock()）而未能获取时，它会放弃 CPU 时间片，让其他线程得以执行，从而有效利用系统资源。
// 从性能上看，自旋锁的响应更快，但是睡眠锁更加节省资源，高效。

class spinlock_mutex
{
    std::atomic_flag flag{};

public:
    spinlock_mutex() noexcept = default;
    void lock() noexcept
    {
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock() noexcept
    {
        flag.clear(std::memory_order_release);
    }
};

spinlock_mutex m;

void f()
{
    std::lock_guard<spinlock_mutex> lc{m};
    std::cout << "😅😅" << "❤️❤️\n";
}

int main()
{
    std::vector<std::thread> vec;
    for (int i = 0; i < 5; ++i)
    {
        vec.emplace_back(f);
    }
    for (auto &t : vec)
    {
        t.join();
    }
}
#endif