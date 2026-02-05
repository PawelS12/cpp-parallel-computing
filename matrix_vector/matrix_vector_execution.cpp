#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <algorithm>
#include <execution>
#include <thread>

class MatrixVector {
private:
    int n_; 
    int total_size_; 
    std::vector<double> a_; 
    std::vector<double> x_; 
    std::vector<double> y_; 
    std::vector<double> z_; 

public:
    explicit MatrixVector(int n)
        : n_(n), total_size_(n * n), a_(total_size_), x_(n), y_(n), z_(n)
    {
        for (int i = 0; i < total_size_; ++i) {
            a_[i] = 1.0001 * i;
        }
        for (int i = 0; i < n_; ++i) {
            x_[i] = static_cast<double>(n_ - i);
        }
    }

    void multiply_row_sequential() {
        std::fill(y_.begin(), y_.end(), 0.0);
        for (int i = 0; i < n_; ++i) {
            for (int j = 0; j < n_; ++j) {
                y_[i] += a_[n_ * i + j] * x_[j];
            }
        }
    }

    void multiply_row_sequential_stdtransform() {
        for (int i = 0; i < n_; ++i) {
            y_[i] = std::transform_reduce( 
                std::execution::seq,
                a_.begin() + n_ * i,
                a_.begin() + n_ * (i + 1),
                x_.begin(),
                0.0
            );
        }
    }

    void multiply_col_sequential() {
        std::fill(y_.begin(), y_.end(), 0.0);
        for (int j = 0; j < n_; ++j) {
            for (int i = 0; i < n_; ++i) {
                y_[i] += a_[i + j * n_] * x_[j];
            }
        }
    }

    void mat_vec_row_row_decomp() {
        std::for_each(std::execution::par, y_.begin(), y_.end(),
            [&](double &yi) {
                int i = &yi - &y_[0];
                double sum = 0.0;
                for (int j = 0; j < n_; ++j) {
                    sum += a_[n_ * i + j] * x_[j];
                }
                yi = sum;
            });
    }

    void mat_vec_row_row_decomp_stdtransform() {
        std::for_each(std::execution::par, std::begin(y_), std::end(y_),
            [&](double &yi) {
                int i = static_cast<int>(&yi - &y_[0]);
                yi = std::transform_reduce(
                    std::execution::par_unseq,
                    a_.begin() + n_ * i,
                    a_.begin() + n_ * (i + 1),
                    x_.begin(),
                    0.0
                );
            });
    }

    void mat_vec_row_col_jthread() {
        std::fill(y_.begin(), y_.end(), 0.0);
        int num_threads = std::thread::hardware_concurrency();
        std::vector<std::jthread> threads;
        std::vector<std::vector<double>> local_results(num_threads, std::vector<double>(n_, 0.0));
        
        int cols_per_thread = (n_ + num_threads - 1) / num_threads;

        for (int t = 0; t < num_threads; ++t) {
            int start_col = t * cols_per_thread;
            int end_col = std::min(start_col + cols_per_thread, n_);
            int thread_id = t;

            threads.emplace_back([=, &local_results, this](std::stop_token){
                std::vector<double>& y_local = local_results[thread_id];
                for (int j = start_col; j < end_col; ++j) {
                    for (int i = 0; i < n_; ++i) {
                        y_local[i] += a_[n_ * i + j] * x_[j];
                    }
                }
            });
        }

        for (auto& th : threads) th.join();

        for (int i = 0; i < n_; ++i) {
            for (int t = 0; t < num_threads; ++t) {
                y_[i] += local_results[t][i];
            }
        }
    }

    void mat_vec_col_row_block_jthread() {
        int block_size = 64;
        std::fill(y_.begin(), y_.end(), 0.0);

        int num_threads = std::thread::hardware_concurrency();
        std::vector<std::jthread> threads;
        std::vector<std::vector<double>> local_results(num_threads, std::vector<double>(n_, 0.0));

        int rows_per_thread = (n_ + num_threads - 1) / num_threads;

        for (int t = 0; t < num_threads; ++t) {
            int start_row = t * rows_per_thread;
            int end_row = std::min(start_row + rows_per_thread, n_);
            int thread_id = t; 

            threads.emplace_back([=, &local_results, this](std::stop_token){
                std::vector<double> &y_local = local_results[thread_id];
                for (int col_block = 0; col_block < n_; col_block += block_size) {
                    int end_col = std::min(col_block + block_size, n_);
                    for (int j = col_block; j < end_col; ++j)
                        for (int i = start_row; i < end_row; ++i)
                            y_local[i] += a_[i + j*n_] * x_[j];
                }
            });
        }

        for (auto &th : threads) th.join(); 

        for (int i = 0; i < n_; ++i)
            for (int t = 0; t < num_threads; ++t)
                y_[i] += local_results[t][i];
    }

    void mat_vec_col_col_jthread() {
        std::fill(y_.begin(), y_.end(), 0.0);

        int num_threads = std::thread::hardware_concurrency();
        std::vector<std::jthread> threads;
        std::vector<std::vector<double>> local_results(num_threads, std::vector<double>(n_, 0.0));
        int cols_per_thread = (n_ + num_threads - 1) / num_threads;

        for (int t = 0; t < num_threads; ++t) {
            int start_col = t * cols_per_thread;
            int end_col = std::min(start_col + cols_per_thread, n_);
            int thread_id = t;

            threads.emplace_back([=, &local_results, this](std::stop_token){
                std::vector<double> &y_local = local_results[thread_id];
                for (int j = start_col; j < end_col; ++j)
                    for (int i = 0; i < n_; ++i)
                        y_local[i] += a_[i + j*n_] * x_[j];
            });
        }

        for (auto &th : threads) th.join();

        for (int i = 0; i < n_; ++i)
            for (int t = 0; t < num_threads; ++t)
                y_[i] += local_results[t][i];
    }


    void set_reference(bool column_major = false) {
        if (column_major) {
            multiply_col_sequential();
        } else {
            multiply_row_sequential();
        }
        z_ = y_;
    }

    bool check_result() const {
        for (int i = 0; i < n_; ++i) {
            if (std::fabs(y_[i] - z_[i]) > 1e-9 * std::fabs(z_[i])) {
                return false;
            }
        }
        return true;
    }

    void benchmark(const std::string& name, void (MatrixVector::*func)(), bool column_major_ref = false, int repetitions = 10) {
        set_reference(column_major_ref);

        double total_time = 0.0;

        for (int i = 0; i < repetitions; ++i) {
            std::fill(y_.begin(), y_.end(), 0.0); 
            auto t1 = std::chrono::high_resolution_clock::now();
            (this->*func)();
            auto t2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = t2 - t1;
            total_time += elapsed.count();
        }

        double avg_time = total_time / repetitions;

        double ops = 2.0 * n_ * n_;
        double bytes = 8.0 * (n_ * n_ + 2.0 * n_);
        double gflops = ops / avg_time * 1e-9;
        double gbs = bytes / avg_time * 1e-9;

        std::cout << std::format("{} | avg time: {:.6f} s | {:.6f} GFLOP/s | {:.6f} GB/s ", 
                                name, avg_time, gflops, gbs);

        if (!check_result()) {
            std::cout << " benchmark- wrong result";
        } else {
            std::cout << "  benchmark - correct result";
        }

        std::cout << "\n";
    }
};

int main() {

    const int N = 15000; 
    MatrixVector mv(N); 

    std::cout << "\n--- CZAS SEKWENCYJNY ---\n";
    mv.benchmark("Row major sequential", &MatrixVector::multiply_row_sequential);
    mv.benchmark("Col major sequential", &MatrixVector::multiply_col_sequential, true);

    std::cout << "\nROW MAJOR:\n";
    mv.benchmark("Row-row decomposition", &MatrixVector::mat_vec_row_row_decomp);
    mv.benchmark("Row-col decomposition", &MatrixVector::mat_vec_row_col_jthread);

    std::cout << "\nROW MAJOR (std::transform_reduce):\n";
    mv.benchmark("Row-row decomposition (std::transform)", &MatrixVector::mat_vec_row_row_decomp_stdtransform);

    std::cout << "\nCOLUMN MAJOR:\n";
    mv.benchmark("Col-row decomposition", &MatrixVector::mat_vec_col_row_block_jthread, true);
    mv.benchmark("Col-col decomposition", &MatrixVector::mat_vec_col_col_jthread, true);

    return 0;
}
