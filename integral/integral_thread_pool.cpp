#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <cmath>
#include <numeric>
#include <format>

constexpr double PI = 3.14159265358979323846;

struct IntegralTask {
    double xp_;
    double xk_;
    double dx_;
    int N_;

    IntegralTask(double xp, double xk, double dx)
        : xp_(xp), xk_(xk)
    {
        N_ = static_cast<int>(std::ceil((xk_ - xp_) / dx));
        dx_ = (xk_ - xp_) / N_;
    }

    double compute() const {
        double integral = 0;
        for (int i = 0; i < N_; ++i) {
            double x1 = xp_ + i * dx_;
            double x2 = x1 + dx_;
            integral += (sin(x1) + sin(x2)) / 2.0 * dx_;
        }

        return integral;
    }

    double operator()() const { 
        return compute(); 
    }
};


class ThreadPool {
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(int threads) {
        for (int i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(this->mutex_);
                        this->cv_.wait(lock, [this] {
                             return stop_ || !tasks_.empty(); 
                        });

                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::scoped_lock lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    template<typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using R = decltype(f());
        auto task_ptr = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> res = task_ptr->get_future();

        {
            std::scoped_lock lock(mutex_);
            tasks_.emplace([task_ptr]() { (*task_ptr)(); });
        }

        cv_.notify_one();
        return res;
    }
};

double compute_parallel_thread_pool(double xp, double xk, double dx, int threads_number, int tasks_number) {
    ThreadPool pool(threads_number);
    std::vector<std::future<double>> futures;

    double sub_interval = (xk - xp) / tasks_number;

    for (int i = 0; i < tasks_number; ++i) {
        double sub_xp = xp + i * sub_interval;
        double sub_xk = sub_xp + sub_interval;
        IntegralTask task(sub_xp, sub_xk, dx);
        futures.push_back(pool.submit(task));
    }

    double total_integral = 0;
    for (auto& f : futures) {
        total_integral += f.get();
    }

    return total_integral;
}

int main() {
    double xp = 0.0;
    double xk = PI;
    double dx = 0.00001;
    
    int threads_number = std::thread::hardware_concurrency(); 
    int tasks_number = 30;

    // 1. Sequential 
    auto start_seq = std::chrono::high_resolution_clock::now();
    IntegralTask task_seq(xp, xk, dx);
    double result_seq = task_seq.compute();
    auto end_seq = std::chrono::high_resolution_clock::now();
    auto duration_seq = std::chrono::duration<double>(end_seq - start_seq).count();

    std::cout << std::format("Sequential integral: {} (Time: {} s)\n", result_seq, duration_seq);

    // 2. Parallel
    auto start_par = std::chrono::high_resolution_clock::now();
    double total_integral = compute_parallel_thread_pool(xp, xk, dx, threads_number, tasks_number);
    auto end_par = std::chrono::high_resolution_clock::now();
    auto duration_par = std::chrono::duration<double>(end_par - start_par).count();

    std::cout << std::format("Parallel integral: {}  (Threads number: {}, Time: {} s)\n", total_integral, threads_number, duration_par);

    return 0;
}