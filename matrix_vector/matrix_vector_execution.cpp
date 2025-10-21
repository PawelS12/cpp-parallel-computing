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
    std::vector<double> a_, x_, y_, z_;

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
    

    // ---------------------------------------------------------------------
    // sekwencyjnie row-major 

    void multiply_row_sequential() {
        for (int i = 0; i < n_; ++i) {
            y_[i] = 0.0;
            for (int j = 0; j < n_; ++j) {
                y_[i] += a_[n_ * i + j] * x_[j];
            }
        }
    }

    // alternatywna wersja z std::transform_reduce
    void multiply_row_sequential_stdtransform() {
        for (int i = 0; i < n_; ++i) {
            y_[i] = std::transform_reduce(
                std::execution::unseq,
                a_.begin() + n_ * i,
                a_.begin() + n_ * (i + 1),
                x_.begin(),
                0.0
            );
        }
    }

    
    // ---------------------------------------------------------------------
    // sekwencyjnie column-major 

    void multiply_col_sequential() {
        std::fill(y_.begin(), y_.end(), 0.0);
        for (int j = 0; j < n_; ++j) {
            for (int i = 0; i < n_; ++i) {
                y_[i] += a_[i + j * n_] * x_[j];
            }
        }
    }


    // ---------------------------------------------------------------------
    // równolegle row-major, dekompozycja wierszowa
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

    // alternatywna wersja z std::transform_reduce
    void mat_vec_row_row_decomp_stdtransform() {
        std::for_each(std::execution::par, std::begin(y_), std::end(y_),
            [&](double &yi) {
                int i = static_cast<int>(&yi - &y_[0]);
                yi = std::transform_reduce(
                    std::execution::unseq,
                    a_.begin() + n_ * i,
                    a_.begin() + n_ * (i + 1),
                    x_.begin(),
                    0.0
                );
            });
    }


    // ---------------------------------------------------------------------
    // równolegle row-major, dekompozycja kolumnowa

    void mat_vec_row_col_decomp() {
        std::fill(y_.begin(), y_.end(), 0.0);

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

    // alternatywna wersja z std::transform_reduce
    void mat_vec_row_col_decomp_stdtransform() {
        std::fill(y_.begin(), y_.end(), 0.0);
        std::for_each(std::execution::par, std::begin(y_), std::end(y_),
            [&](double &yi) {
                int i = static_cast<int>(&yi - &y_[0]);
                yi = std::transform_reduce(
                    std::execution::unseq,
                    a_.begin() + n_ * i,
                    a_.begin() + n_ * (i + 1),
                    x_.begin(),
                    0.0
                );
            });
    }


    // ---------------------------------------------------------------------
    // równolegle column-major, dekompozycja wierszowa

    void mat_vec_col_row_decomp() {
        std::for_each(std::execution::par, y_.begin(), y_.end(),
            [&](double &yi) {
                int i = &yi - &y_[0];
                double sum = 0.0;
                for (int j = 0; j < n_; ++j) {
                    sum += a_[i + j * n_] * x_[j];
                }
                yi = sum;
            });
    }


    // ---------------------------------------------------------------------
    // równolegle column-major, dekompozycja kolumnowa

    void mat_vec_col_col_decomp() {
        std::fill(y_.begin(), y_.end(), 0.0);

        std::for_each(std::execution::par, y_.begin(), y_.end(),
            [&](double &yi) {
                int i = &yi - &y_[0];
                double sum = 0.0;
                for (int j = 0; j < n_; ++j) {
                    sum += a_[i + j * n_] * x_[j];
                }
                yi = sum;
            });
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

    void benchmark(const std::string& name, void (MatrixVector::*func)(), bool column_major_ref = false) {
        set_reference(column_major_ref);

        auto t1 = std::chrono::high_resolution_clock::now();
        (this->*func)();
        auto t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = t2 - t1;

        double ops = 2.0 * n_ * n_;
        double bytes = 8.0 * n_ * n_;
        double gflops = ops / elapsed.count() * 1e-9;
        double gbs = bytes / elapsed.count() * 1e-9;

        std::cout << std::format("{} | time: {:.6f} s | {:.6f} GFLOP/s | {:.6f} GB/s ", name, elapsed.count(), gflops, gbs);

        if (!check_result()) {
            std::cout << " benchmark- wrong result";
        } else {
            std::cout << "  benchmark - correct result";
        }

        std::cout << "\n";
    }
};

int main() {

    // g++ -std=c++20 -O3 matrix_vector_execution.cpp -ltbb -o mv_ex

    const int N = 2000;
    MatrixVector mv(N);

    std::cout << "\nROW MAJOR:\n";
    mv.benchmark("Row-row decomposition", &MatrixVector::mat_vec_row_row_decomp);
    mv.benchmark("Row-col decomposition", &MatrixVector::mat_vec_row_col_decomp);

    std::cout << "\nROW MAJOR (std::transform_reduce):\n";
    mv.benchmark("Row-row decomposition (std::transform)", &MatrixVector::mat_vec_row_row_decomp_stdtransform);
    mv.benchmark("Row-col decomposition (std::transform)", &MatrixVector::mat_vec_row_col_decomp_stdtransform);

    std::cout << "\nCOLUMN MAJOR:\n";
    mv.benchmark("Col-col decomposition", &MatrixVector::mat_vec_col_col_decomp, true);
    mv.benchmark("Col-row decomposition", &MatrixVector::mat_vec_col_row_decomp, true);

    return 0;
}
