#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <numeric>
#include <cmath>
#include <chrono>
#include <format>
#include <future>

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

double parallel_integral_async(double xp, double xk, double dx, int threads_number, int tasks_number) {
    double sub_interval = (xk - xp) / tasks_number;

    std::vector<std::future<double>> futures;

    for (int i = 0; i < tasks_number; ++i) {
        double sub_xp = xp + i * sub_interval;
        double sub_xk = sub_xp + sub_interval;

        IntegralTask task(sub_xp, sub_xk, dx);

        futures.push_back(std::async(std::launch::async, task));
    }

    double total_integral = 0;
    for (auto& f : futures) {
        total_integral += f.get();
    }

    return total_integral;
}

double parallel_integral_threads(double xp, double xk, double dx, int threads_number, int tasks_number) {
    double sub_interval = (xk - xp) / tasks_number;
    std::vector<double> results(tasks_number, 0.0);
    std::atomic<int> task_index{0};

    auto worker = [&results, &task_index, xp, sub_interval, dx, tasks_number]() {
        while (true) {
            int idx = task_index.fetch_add(1);

            if (idx >= tasks_number) {
                break;
            }

            double sub_xp = xp + idx * sub_interval;
            double sub_xk = sub_xp + sub_interval;

            IntegralTask task(sub_xp, sub_xk, dx);
            results[idx] = task.compute();
        }
    };

    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < threads_number; ++i) {
            threads.emplace_back(worker);
        }
    }

    return std::accumulate(results.begin(), results.end(), 0.0);
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
    double result_par = parallel_integral_async(xp, xk, dx, threads_number, tasks_number);
    auto end_par = std::chrono::high_resolution_clock::now();
    auto duration_par = std::chrono::duration<double>(end_par - start_par).count();

    std::cout << std::format("Parallel integral: {}  (Threads number: {}, Time: {} s)\n", result_par, threads_number, duration_par);

    return 0;
}