#include <algorithm>
#include <atomic>
#include <cassert>
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

#define VERSION_7
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
#elif defined(VERSION_4)
// 原子类型共性知识: 以 atomic_bool 为例
#define Type 3
#if Type == 1
std::atomic_bool b{true};
int main()
{
    // 一般operator=返回引用，可以连等
    std::string s{};
    s = "kk";
    // atomic 的 operator= 不同于通常情况, 赋值操作 b = false 返回一个普通的 bool 值
    b = false;
#if 1
    // 避免多余的加载（load）过程
    bool new_value = (b = true);
#else
    auto &ref = (b = true); // 假如返回 atomic 引用
    bool flag = ref.load();
#endif
}

/*
1: 使用 store 原子的替换当前对象的值，远好于 std::atomic_flag 的 clear()
2: test_and_set() 也可以换为更加通用常见的 exchange，它可以原子的使用新的值替换已经存储的值，并返回旧值
*/

/*
比较/交换: compare_exchange_weak() 和 compare_exchang_strong()
1: compare_exchange_weak: 尝试将原子对象的当前值与预期值进行比较，如果相等则将其更新为新值并返回 true；否则，将原子对象的值加载进 expected（进行加载操作）并返回 false
此操作可能会由于某些硬件的特性而出现假失败[2]，需要在循环中重试
2: compare_exchang_strong: 类似于 compare_exchange_weak，但不会出现假失败，因此不需要重试
*/
#elif Type == 2
int main()
{
    std::atomic<bool> flag{false};
    bool expected = false;
    while (!flag.compare_exchange_weak(expected, true))
        ;
    std::cout << std::boolalpha << flag << std::endl;
}
#else
std::atomic<bool> flag{false};
bool expected = false;

void try_set_flag()
{
    // 尝试将 flag 设置为 true，如果当前值为 false
    if (flag.compare_exchange_strong(expected, true))
    {
        std::osyncstream{std::cout} << "flag 为 false，设为 true。\n";
    }
    else
    {
        std::osyncstream{std::cout} << "flag 为 true, expected 设为 true。\n";
    }
}

int main()
{
    std::jthread t1{try_set_flag};
    std::jthread t2{try_set_flag};
}
#endif

#elif defined(VERSION_5)
// std::atomic<T*>
// 对指针本身的操作是原子，无法保证指针所指向的对象
/*
1: fetch_add：以原子方式增加指针的值, 并返回操作前的指针值）
2: fetch_sub：以原子方式减少指针的值, 并返回操作前的指针值。
3: operator+= 和 operator-=：以原子方式增加或减少指针的值
*/
struct Foo
{
};

int main()
{
    Foo array[5]{};
    std::atomic<Foo *> p{array};

    // p 加 2，并返回原始值
    Foo *x = p.fetch_add(2);
    assert(x == array);
    assert(p.load() == &array[2]);

    // p 减 1
    x = (p -= 1);
    assert(x == &array[1]);
    assert(p.load() == &array[1]);

    // 函数也允许内存序作为给定函数的参数
    p.fetch_add(3, std::memory_order_release);
}

#elif defined(VERSION_6)
// std::atomic<std::shared_ptr>
/*
1: 多个线程能在不同的 shared_ptr 对象上调用所有成员函数[3]（包含复制构造函数与复制赋值）而不附加同步，即使这些实例是同一对象的副本且共享所有权也是如此。
(之所以多个线程通过 shared_ptr 的副本可以调用一切成员函数，甚至包括非 const 的成员函数 operator=、reset，是因为 shared_ptr 的"控制块是线程安全的")
2: 若多个执行线程访问同一 shared_ptr 对象而不同步，且任一线程使用 shared_ptr 的非 const 成员函数，则将出现数据竞争；std::atomic<shared_ptr> 能用于避免数据竞争。
*/

// 显然，std::shared_ptr 并不是完全线程安全的
// 在 C++20 中，原子模板 std::atomic 引入了一个偏特化版本 std::atomic<std::shared_ptr> 允许用户原子地操纵 shared_ptr 对象, 它是原子类型，这意味着它的所有操作都是原子操作
// 若多个执行线程不同步地同时访问同一 std::shared_ptr 对象，且任何这些访问使用了 shared_ptr 的非 const 成员函数，则将出现数据竞争，除非通过 std::atomic<std::shared_ptr> 的实例进行所有访问。

#define Safe
class Data
{
public:
    Data(int value = 0) : value_(value) {}
    int get_value() const { return value_; }
    void set_value(int new_value) { value_ = new_value; }

private:
    int value_;
};

#ifdef UnSafe

auto data = std::make_shared<Data>();

void writer()
{
    for (int i = 0; i < 10; ++i)
    {
        std::shared_ptr<Data> new_data = std::make_shared<Data>(i);
        data.swap(new_data); // 调用非 const 成员函数
        std::this_thread::sleep_for(100ms);
    }
}

void reader()
{
    for (int i = 0; i < 10; ++i)
    {
        if (data)
        {
            std::cout << "读取线程值: " << data->get_value() << std::endl;
        }
        else
        {
            std::cout << "没有读取到数据" << std::endl;
        }
        std::this_thread::sleep_for(100ms);
    }
}

int main()
{
    std::thread writer_thread{writer};
    std::thread reader_thread{reader};

    // 内部实现是两个指针
    std::cout << sizeof(std::shared_ptr<Data>) << std::endl;

    writer_thread.join();
    reader_thread.join();
}
/*
以上这段代码是典型的线程不安全，它满足：
多个线程不同步地同时访问同一 std::shared_ptr 对象
任一线程使用 shared_ptr 的非 const 成员函数
*/

/*
shared_ptr 的通常实现只保有两个指针
1: 指向底层元素的指针（get()) 所返回的指针）
2: 指向“控制块 ”的指针

控制块是一个动态分配的对象，其中包含：
1: 指向被管理对象的指针或被管理对象本身
2: 删除器（类型擦除）
3: 分配器（类型擦除）
4: 持有被管理对象的 shared_ptr 的数量
5: 涉及被管理对象的 weak_ptr 的数量
*/

/*
控制块(指向控制块的指针指向的对象)是线程安全的:
    这意味着多个线程可以安全地操作引用计数和访问管理对象，即使这些 shared_ptr 实例是同一对象的副本且共享所有权也是如此
然而，shared_ptr 对象实例本身并不是线程安全的:
    shared_ptr 对象实例包含一个指向控制块的指针和一个指向底层元素的指针。这两个指针的操作在多个线程中并没有同步机制

因此，如果多个线程同时访问同一个 shared_ptr 对象实例并调用非 const 成员函数（如 reset 或 operator=），这些操作会导致对这些指针的并发修改，进而引发数据竞争。
如果不是同一 shared_ptr 对象，每个线程读写的指针也不是同一个，控制块又是线程安全的，那么自然不存在数据竞争，可以安全的调用所有成员函数。

自我理解：
简而言之: 对 shared_ptr 本身而言，对其内部实现的两个指针没有同步机制，无法保证线程安全，而对于其中指向控制块的指针而言，其指向的 动态分配的空间(堆):控制块 是线程安全的(也就是所指向的对象是线程安全的)
结合 atomic 的理解 : 对于指针的特化，只能保证对指针本身的操作是原子的，而无法保证其所指向的对象。而对于 shared_ptr 只能保证，其中指针指向的对象是线程安全，而无法保证自己本身，所以是线程不安全的，对同一 shared_ptr 对象除非通过 std::atomic<std::shared_ptr> 的实例进行所有访问，才能保证线程安全
*/

#elif defined(Safe)
// https://godbolt.org/z/fbKEhescv
std::atomic<std::shared_ptr<Data>> data = std::make_shared<Data>();

void writer()
{
    for (int i = 0; i < 10; ++i)
    {
        std::shared_ptr<Data> new_data = std::make_shared<Data>(i);
        data.store(new_data); // 原子地替换所保有的值
        // data.load().swap(new_data);
        /*
        没有意义，因为 load() 成员函数返回的是底层 std::shared_ptr 的副本，也就是一个临时对象。
        对这个临时对象调用 swap 并不会改变 data 本身的值，因此这种操作没有实际意义，尽管这不会引发数据竞争（因为是副本）
        */
        std::this_thread::sleep_for(10ms);
    }
}

void reader()
{
    for (int i = 0; i < 10; ++i)
    {
        if (auto sp = data.load())
        {
            std::cout << "读取线程值: " << sp->get_value() << std::endl;
        }
        else
        {
            std::cout << "没有读取到数据" << std::endl;
        }
        std::this_thread::sleep_for(10ms);
    }
}

int main()
{
#if 0
    std::thread writer_thread{writer};
    std::thread reader_thread{reader};

    // 内部实现是两个指针
    std::cout << sizeof(std::shared_ptr<Data>) << std::endl;

    writer_thread.join();
    reader_thread.join();
#endif

    /*
    在使用 std::atomic<std::shared_ptr> 的时要注意:
    切勿将受保护数据的指针或引用传递到互斥量作用域之外，不然保护将形同虚设。
    */
#ifdef UnSafe
    std::atomic<std::shared_ptr<int>> ptr = std::make_shared<int>(10);
    *ptr.load() = 100;
    /*
    1: 调用 load() 成员函数，原子地返回底层共享指针的副本
    2: std::shared_ptr 解引用，等价 *get()，返回了 int&
    3: 直接修改这个引用所指向的资源。
    在第一步时，已经脱离了 std::atomic 的保护，第二步就获取了被保护的数据的引用，第三步进行了修改，这导致了数据竞争
    */
#elif defined(Safe)
    std::atomic<std::shared_ptr<int>> ptr = std::make_shared<int>(10);
    std::atomic_ref<int> ref{*ptr.load()};
    ref = 100; // 原子地赋 100 给被引用的对象
    // 使用 std::atomic_ref 我们得以确保在修改共享资源时保持操作的原子性，从而避免了数据竞争
#endif
}
#endif
#elif defined(VERSION_7)
// C++20 以后任何 atomic 的特化都拥有 wait、notify_one 、notify_all 这些成员函数
// https://godbolt.org/z/z51vaoKj3
std::atomic<std::shared_ptr<int>> ptr = std::make_shared<int>();

void wait_for_wake_up()
{
    std::osyncstream{std::cout}
        << "线程 "
        << std::this_thread::get_id()
        << " 阻塞，等待更新唤醒\n";

    // 等待 ptr 变为其它值
    ptr.wait(ptr.load());

    std::osyncstream{std::cout}
        << "线程 "
        << std::this_thread::get_id()
        << " 已被唤醒\n";
}

void wake_up()
{
    std::this_thread::sleep_for(5s);

    // 更新值并唤醒
    ptr.store(std::make_shared<int>(10));
    ptr.notify_one();
}

int main()
{
    std::jthread t{wait_for_wake_up};
    wake_up();
}
#endif