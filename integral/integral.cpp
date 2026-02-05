#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <numeric>
#include <cmath>
#include <chrono>
#include <format>
#include <future>
#include <queue>

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>


constexpr double PI = 3.14159265358979323846;


class IntegralTask {
private:
    double xp_; 
    double xk_; 
    double dx_; 
    int N_;

public:
    IntegralTask(double xp, double xk, double dx) 
        : xp_(xp), xk_(xk)
    {
        double range = xk_ - xp_;
        N_ = static_cast<int>(std::ceil(range / dx));
        dx_ = range / N_;
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

class BaseIntegrator {
protected:
    double xp_;
    double xk_;
    double dx_;
    int tasks_number_; 

public:
    BaseIntegrator(double xp, double xk, double dx, int tasks_number) 
        : xp_(xp), xk_(xk), dx_(dx), tasks_number_(tasks_number) {}

    virtual ~BaseIntegrator() = default;
    virtual double compute() = 0;
};

class AsyncIntegrator : public BaseIntegrator {
public:
    AsyncIntegrator(double xp, double xk, double dx, int tasks_number)
        : BaseIntegrator(xp, xk, dx, tasks_number) {}

    double compute() override {
        double sub_interval = (xk_ - xp_) / tasks_number_;  

        std::vector<std::future<double>> futures;

        for (int i = 0; i < tasks_number_; ++i) {
            double sub_xp = xp_ + i * sub_interval;
            double sub_xk = sub_xp + sub_interval; 

            IntegralTask task(sub_xp, sub_xk, dx_); 

            futures.push_back(std::async(std::launch::async, task));
        }

        double total_integral = 0.0;

        for (auto& f : futures) {
            total_integral += f.get();
        }

        return total_integral;
    }
};

class ThreadIntegrator : public BaseIntegrator {
private:
    int threads_number_; 

public:
    ThreadIntegrator(double xp, double xk, double dx, int tasks_number, int threads_number)
        : BaseIntegrator(xp, xk, dx, tasks_number), threads_number_(threads_number) {}

    double compute() override {
        double sub_interval = (xk_ - xp_) / tasks_number_; 
        std::vector<double> results(tasks_number_, 0.0); 
        std::atomic<int> task_index{0}; 

        auto compute_subintervals = [&]() {
            while (true) {
                int index = task_index.fetch_add(1);

                if (index >= tasks_number_) {
                    break;
                }

                double sub_xp = xp_ + index * sub_interval; 
                double sub_xk = sub_xp + sub_interval; 

                IntegralTask task(sub_xp, sub_xk, dx_); 
                results[index] = task.compute(); 
            }
        };

        {
            std::vector<std::jthread> threads;
            for (int i = 0; i < threads_number_; ++i) {
                threads.emplace_back(compute_subintervals);
            }
        }

        return std::accumulate(results.begin(), results.end(), 0.0);
    }
};

class BoostThreadPoolIntegrator : public BaseIntegrator {
private:
    int threads_number_;

public:
    BoostThreadPoolIntegrator(double xp, double xk, double dx, int tasks_number, int threads_number)
        : BaseIntegrator(xp, xk, dx, tasks_number), threads_number_(threads_number) {}

    double compute() override {
        boost::asio::thread_pool pool(threads_number_);
        std::vector<std::future<double>> futures;

        double sub_interval = (xk_ - xp_) / tasks_number_;

        for (int i = 0; i < tasks_number_; ++i) {
            double sub_xp = xp_ + i * sub_interval;
            double sub_xk = sub_xp + sub_interval;

            std::packaged_task<double()> task([=] {
                IntegralTask integral(sub_xp, sub_xk, dx_);
                return integral.compute();
            });

            futures.push_back(task.get_future());

            boost::asio::post(pool, [t = std::move(task)]() mutable {
                t();
            });
        }

        pool.join(); 

        double total_integral = 0.0;
        for (auto& f : futures) {
            total_integral += f.get();
        }
        return total_integral;
    }
};

class Benchmark {
public:
    template<typename F>
    static double measure(F&& func, const std::string& name) { 
        auto start = std::chrono::high_resolution_clock::now();
        double result = func();
        auto end = std::chrono::high_resolution_clock::now();

        double duration = std::chrono::duration<double>(end - start).count();
        std::cout << std::format("{}: {} (Time: {} s)\n", name, result, duration);

        return duration;
    }
};


int main() {

    double xp = 0.0;
    double xk = PI;
    double dx = 0.00001;

    int threads_number = std::thread::hardware_concurrency(); 
    int tasks_number = 30; 

    Benchmark::measure([&]() {
        IntegralTask task(xp, xk, dx);
        return task.compute();
    }, "Sequential integral");

    Benchmark::measure([&]() {
        AsyncIntegrator integrator(xp, xk, dx, tasks_number);
        return integrator.compute();
    }, "Parallel integral async/future");

    Benchmark::measure([&]() {
        ThreadIntegrator integrator(xp, xk, dx, tasks_number, threads_number);
        return integrator.compute();
    }, "Parallel integral jthread");

    Benchmark::measure([&]() {
        BoostThreadPoolIntegrator integrator(xp, xk, dx, tasks_number, threads_number);
        return integrator.compute();
    }, "Parallel integral Boost thread pool");

    return 0;
}