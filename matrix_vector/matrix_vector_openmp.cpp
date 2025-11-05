#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <omp.h>
#include <format>

class MatrixVector {
private:
    int n_; // rozmiar macierzy 
    int total_size_; // liczba elementów macierzy
    std::vector<double> a_; // wektor macierzy
    std::vector<double> x_; // wektor wejściowy
    std::vector<double> y_; // wektor wynikowy
    std::vector<double> z_; // wektor do wyników

public:
    explicit MatrixVector(int n)
        : n_(n), total_size_(n * n), a_(total_size_), x_(n), y_(n), z_(n)
    {
        // wypełnienie macierzy wartościami
        for (int i = 0; i < total_size_; ++i) {
            a_[i] = 1.0001 * i;
        }

        for (int i = 0; i < n_; ++i) {
            x_[i] = static_cast<double>(n_ - i);
        }
    }


    // ---------------------------------------------------------------------
    // sekwencyjnie, wierszowo

    void multiply_row_sequential() {
        for (int i = 0; i < n_; ++i) {
            y_[i] = 0.0;
            for (int j = 0; j < n_; ++j) {
                y_[i] += a_[n_ * i + j] * x_[j];
            }
        }
    }


    // ---------------------------------------------------------------------
    // równolegle, dekompozycja wiersz-wiersz

    void mat_vec_row_row_decomp() {
        #pragma omp parallel for
        for (int i = 0; i < n_; ++i) {
            double sum = 0.0;
            for (int j = 0; j < n_; ++j) {
                sum += a_[n_ * i + j] * x_[j];
            }
            y_[i] = sum;
        }
    }


    // ---------------------------------------------------------------------
    // równolegle, dekompozycja wiersz-kolumna

    void mat_vec_row_col_decomp() {
        std::fill(y_.begin(), y_.end(), 0.0);

        #pragma omp parallel
        {
            std::vector<double> y_local(n_, 0.0);

            #pragma omp for
            for (int j = 0; j < n_; ++j) {
                for (int i = 0; i < n_; ++i) {
                    y_local[i] += a_[n_ * i + j] * x_[j];
                }
            }

            #pragma omp critical
            {
                for (int i = 0; i < n_; ++i)
                    y_[i] += y_local[i];
            }
        }
    }


    // ---------------------------------------------------------------------
    // sekwencyjnie, kolumnowo

    void multiply_col_sequential() {
        for (int i = 0; i < n_; ++i) {
            y_[i] = 0.0;
        }
        for (int j = 0; j < n_; ++j) {
            for (int i = 0; i < n_; ++i) {
                y_[i] += a_[i + j * n_] * x_[j];
            }
        }
    }


    // ---------------------------------------------------------------------
    // równolegle, dekompozycja kolumna-kolumna

    void mat_vec_col_col_decomp() {
        std::fill(y_.begin(), y_.end(), 0.0);

        #pragma omp parallel
        {
            std::vector<double> y_local(n_, 0.0);

            #pragma omp for
            for (int j = 0; j < n_; ++j) {
                for (int i = 0; i < n_; ++i) {
                    y_local[i] += a_[i + j * n_] * x_[j];
                }
            }

            #pragma omp critical
            {
                for (int i = 0; i < n_; ++i) {
                    y_[i] += y_local[i];
                }
            }
        }
    }


    // ---------------------------------------------------------------------
    // równolegle, dekompozycja kolumna-wiersz

    void mat_vec_col_row_decomp() {
        #pragma omp parallel for
        for (int i = 0; i < n_; ++i) {
            double sum = 0.0;
            for (int j = 0; j < n_; ++j) {
                sum += a_[i + j * n_] * x_[j];
            }
            y_[i] = sum;
        }
    }


    // funkcja do ustawienia referencji do sprawdzenia poprawności
    void set_reference(bool column_major = false) {
        if (column_major) {
            multiply_col_sequential();
        } else {
            multiply_row_sequential();
        }
        z_ = y_;
    }


    // funkcja do sprawdzenia poprawności
    bool check_result() const {
        for (int i = 0; i < n_; ++i) {
            if (std::fabs(y_[i] - z_[i]) > 1.e-9 * std::fabs(z_[i])) {
                return false;
            }
        }
        return true;
    }

    
    // sprawdzenie czasu wykonania i przepustowości
    void benchmark(const std::string& name, void (MatrixVector::*func)(), bool column_major_ref = false) {
        set_reference(column_major_ref);

        auto t1 = std::chrono::high_resolution_clock::now();
        (this->*func)();
        auto t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = t2 - t1;

        double ops = 2.0 * n_ * n_;
        double bytes = 8.0 * (n_ * n_ + 2.0 * n_);
        double gflops = ops / elapsed.count() * 1e-9;
        double gbs = bytes / elapsed.count() * 1e-9;

        std::cout << std::format("{} | time: {:.6f} s | {:.6f} GFLOP/s | {:.6f} GB/s ", name, elapsed.count(), gflops, gbs);

        if (!check_result()) {
            std::cout << " benchmark- wrong result";
        } else {
            std::cout << "  benchmark- correct result";
        }

        std::cout << "\n";
    }
};


int main() {

    // g++ -std=c++23 -fopenmp matrix_vector_openmp.cpp -o mv

    const int N = 2000; // rozmiar macierzy
    MatrixVector mv(N); // obiekt klasy MatrixVector

    std::cout << std::format("OpenMP threads available: {}\n", omp_get_max_threads());

    std::cout << "\nRow Major:\n";
    mv.benchmark("Row-row decomposition", &MatrixVector::mat_vec_row_row_decomp);
    mv.benchmark("Row-col decomposition", &MatrixVector::mat_vec_row_col_decomp);

    std::cout << "\nColumn Major:\n";
    mv.benchmark("Col-col decomposition", &MatrixVector::mat_vec_col_col_decomp, true);
    mv.benchmark("Col-row decomposition", &MatrixVector::mat_vec_col_row_decomp, true);

    return 0;
}