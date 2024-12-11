#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <syncstream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;

// qt、boost 库的多线程见 md

inline std::size_t default_thread_pool_size() noexcept
{
    std::size_t num_threads = std::thread::hardware_concurrency();
    num_threads = num_threads == 0 ? 2 : num_threads;
    return num_threads;
}

// 构造函数：初始化线程池并启动线程
// 析构函数：停止线程池并等待所有线程结束(而非任务结束)

class Thread_Pool
{
public:
    using Task = std::function<void()>;
    Thread_Pool(const Thread_Pool &) = delete;
    Thread_Pool &operator=(const Thread_Pool &) = delete;

    Thread_Pool(std::size_t num_thread = default_thread_pool_size()) : stop_{false}, num_thread_{num_thread}
    {
        start();
    }

    ~Thread_Pool()
    {
        stop();
    }

    template <typename F, typename... Args>
    std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> submit(F &&f, Args &&...args)
    {
        using RetType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        if (stop_)
            throw std::runtime_error("ThreadPool is stopped");
        auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<RetType> ret = task->get_future();

        {
            std::lock_guard<std::mutex> lock{mutex_};
            tasks_.emplace([task]
                           { (*task)(); });
        }
        cv_.notify_one();

        return ret;
    }

    void start()
    {
        for (std::size_t i = 0; i < num_thread_; ++i)
        {
            pool_.emplace_back([this]
                               {
                while(!stop_)
                {
                    std::unique_lock<std::mutex> lock{mutex_};
                    cv_.wait(lock, [this]
                             { return stop_ || !tasks_.empty(); });
                    if(tasks_.empty())
                        return;
                    Task task = std::move(tasks_.front());
                    tasks_.pop();
                    task();
                } });
        }
    }

    void stop()
    {
        stop_ = true;
        cv_.notify_all();
        for (auto &thread : pool_)
        {
            if (thread.joinable())
                thread.join();
        }
        pool_.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic_bool stop_;
    std::size_t num_thread_;
    std::queue<Task> tasks_;
    std::vector<std::thread> pool_;
};

#if 1

int print_task(int n)
{
    std::osyncstream{std::cout} << "Task " << n << " is running on thr: " << std::this_thread::get_id() << '\n';
    return n;
}
int print_task2(int n)
{
    std::osyncstream{std::cout} << "🐢🐢🐢 " << n << " 🐉🐉🐉" << std::endl;
    return n;
}

struct X
{
    // void f(int &&n) const
    // {
    //     std::osyncstream{std::cout} << &n << '\n';
    // }
    void f(std::reference_wrapper<int> &&n) const
    {
        std::osyncstream{std::cout} << &(n.get()) << '\n';
    }
};

int main()
{
#if 1
    Thread_Pool pool{4};                   // 创建一个有 4 个线程的线程池
    std::vector<std::future<int>> futures; // future 集合，获取返回值

    for (int i = 0; i < 10; ++i)
    {
        futures.emplace_back(pool.submit(print_task, i));
    }

    for (int i = 0; i < 10; ++i)
    {
        futures.emplace_back(pool.submit(print_task2, i));
    }

    // int sum = 0;
    // for (auto &future : futures)
    // {
    //     sum += future.get(); // get() 成员函数 阻塞到任务执行完毕，获取返回值
    // }
    // std::cout << "sum: " << sum << '\n';
    // std::this_thread::sleep_for(1us); // 线程池中的线程还未执行到 while(stop_), 此时已经迎来了析构，stop_为true，线程直接退出，并不执行tasks_中的任务
    std::this_thread::sleep_for(1ms); // 线程池中的所有线程，都执行完了所有任务后，析构才调用 stop_为true 线程退出
#else
    Thread_Pool pool{4}; // 创建一个有 4 个线程的线程池

    X x;
    int n = 6;
    std::cout << &n << '\n';
    // 是因为Args && 被识别为int&后紧接着衰退为int，再通过std forward被转发为了右值吗，所以不能和f中的int&绑定？
    // auto t = pool.submit(&X::f, &x, n); // 默认复制，地址不同  衰退使得按值传递
    auto t2 = pool.submit(&X::f, &x, std::ref(n));
    // t.wait();
    t2.wait();
#endif
} // 析构自动 stop()自动 stop()

#endif