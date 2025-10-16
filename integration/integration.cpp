#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <cmath>
#include <format>
#include <atomic>
#include <numeric>
#include <chrono>

constexpr double PI = 3.14159265358979323846;

double f_sin(double x) {
    return std::sin(x);
}

struct IntegralTask {
    double xp, xk, dx;
    int N;

    IntegralTask(double start, double end, double delta)
        : xp(start), xk(end), dx(delta)
    {
        N = static_cast<int>(std::ceil((xk - xp) / dx));
        dx = (xk - xp) / N;
        std::cout << std::format("Creating task: xp = {} xk = {}, N = {}, dx = {}\n", xp, xk, N, dx);
    }

    double compute() const {
        double sum = 0;
        for (int i = 0; i < N; ++i) {
            double x1 = xp + i * dx;
            double x2 = x1 + dx;
            sum += (f_sin(x1) + f_sin(x2)) / 2.0 * dx;
        }
        std::cout << std::format("Partial integral: {}\n", sum);
        return sum;
    }

    double operator()() const { return compute(); }
};

double parallelIntegral(double xp, double xk, double dx, int numThreads, int numTasks) {
    double subInterval = (xk - xp) / numTasks;
    std::vector<double> results(numTasks, 0.0);
    std::atomic<int> taskIndex{0};

    auto worker = [&]() {
        while (true) {
            int idx = taskIndex.fetch_add(1);
            if (idx >= numTasks) break;
            double subXp = xp + idx * subInterval;
            double subXk = subXp + subInterval;
            IntegralTask task(subXp, subXk, dx);
            results[idx] = task.compute();
        }
    };

    std::vector<std::jthread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker);
    }

    return std::accumulate(results.begin(), results.end(), 0.0);
}

int main() {
    double xp = 0.0;
    double xk = PI;
    double dx = 0.00001;

    int numThreads = std::thread::hardware_concurrency(); 
    int numTasks = 30; 

    auto startSeq = std::chrono::high_resolution_clock::now();
    IntegralTask seqTask(xp, xk, dx);
    double seqResult = seqTask.compute();
    auto endSeq = std::chrono::high_resolution_clock::now();
    auto durationSeq = std::chrono::duration_cast<std::chrono::milliseconds>(endSeq - startSeq).count();
    std::cout << std::format("Sequential integral: {} (Time: {} ms)\n\n", seqResult, durationSeq);

    auto startPar = std::chrono::high_resolution_clock::now();
    double parallelResult = parallelIntegral(xp, xk, dx, numThreads, numTasks);
    auto endPar = std::chrono::high_resolution_clock::now();
    auto durationPar = std::chrono::duration_cast<std::chrono::milliseconds>(endPar - startPar).count();
    std::cout << std::format("Parallel integral: {} (Time: {} ms)\n", parallelResult, durationPar);

    return 0;
}
