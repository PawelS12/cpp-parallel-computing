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

constexpr double PI = 3.14159265358979323846;


// ---------------------------------------------------------------------
// Klasa IntegralTask - zadanie obliczenia całki na podprzedziale

class IntegralTask {
private:
    double xp_; // początek przedziału
    double xk_; // koniec przedziału
    double dx_; // krok całkowania
    int N_; // liczba podprzedziałów

public:
    IntegralTask(double xp, double xk, double dx) 
        : xp_(xp), xk_(xk)
    {
        // obliczenie liczby podprzedziałów oraz dopasowanie kroku
        double range = xk_ - xp_;
        N_ = static_cast<int>(std::ceil(range / dx));
        dx_ = range / N_;
    }   
    
    // obliczenie całki metodą trapezów
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


// ---------------------------------------------------------------------
// Klasa BaseIntegrator - abstrakcyjna klasa z wirtualną metodą compute()

class BaseIntegrator {
protected:
    double xp_;
    double xk_;
    double dx_;
    int tasks_number_; // ilośc zadań

public:
    BaseIntegrator(double xp, double xk, double dx, int tasks_number) 
        : xp_(xp), xk_(xk), dx_(dx), tasks_number_(tasks_number) {}

    // destruktor
    virtual ~BaseIntegrator() = default;

    // czysto wirtualna metoda dla klas dziedziczących
    virtual double compute() = 0;
};


// ---------------------------------------------------------------------
// Klasa AsyncIntegrator - obliczanie całki za pomocą std::async i std::future
// System sam decyduje, czy uruchomić nowe wątki, czy wykorzystać istniejące.
// Nie ma ręcznego zarządzania pulą 

class AsyncIntegrator : public BaseIntegrator {
public:
    AsyncIntegrator(double xp, double xk, double dx, int tasks_number)
        : BaseIntegrator(xp, xk, dx, tasks_number) {}

    double compute() override {
        double sub_interval = (xk_ - xp_) / tasks_number_;  // długość podzadania

        // wektor obiektów future do odczytania wyników obliczeń wątków
        std::vector<std::future<double>> futures;

        for (int i = 0; i < tasks_number_; ++i) {
            double sub_xp = xp_ + i * sub_interval; // początek podzadania
            double sub_xk = sub_xp + sub_interval; // koniec podzadania

            IntegralTask task(sub_xp, sub_xk, dx_); // stworzenie obiektu IntegralTask

            // uruchomienie podzadania asynchronicznie w osobnym wątku dzieki std::launch::async
            // zwraca std::future do odczytania wyniku
            futures.push_back(std::async(std::launch::async, task));
        }

        
        double total_integral = 0.0;

        // pobranie wyników z vectora obiektów future i zsumowanie ich 
        for (auto& f : futures) {
            total_integral += f.get();
        }

        return total_integral;
    }
};


// ---------------------------------------------------------------------
// Klasa ThreadIntegrator - obliczanie całki za pomocą std::jthread

class ThreadIntegrator : public BaseIntegrator {
private:
    int threads_number_; // liczba wątków

public:
    ThreadIntegrator(double xp, double xk, double dx, int tasks_number, int threads_number)
        : BaseIntegrator(xp, xk, dx, tasks_number), threads_number_(threads_number) {}

    double compute() override {
        double sub_interval = (xk_ - xp_) / tasks_number_; // długość podzadania
        std::vector<double> results(tasks_number_, 0.0); // vector do wyników podzadań
        std::atomic<int> task_index{0}; // zmienna atomowa, licznik podzadań

        // funkcja anonimowa lambda wykonywana przez każdy wątek
        auto compute_subintervals = [&]() {
            while (true) {
                // pobranie indeksu podzadania
                int idx = task_index.fetch_add(1);

                // zakończenie działania, jeśli wszystkie podzadania zostały przydzielone
                if (idx >= tasks_number_) {
                    break;
                }

                double sub_xp = xp_ + idx * sub_interval; // początek podzadania
                double sub_xk = sub_xp + sub_interval; // koniec podzadania

                IntegralTask task(sub_xp, sub_xk, dx_); // stworzenie obiektu IntegralTask
                results[idx] = task.compute(); // zapisanie wyniku w vectorze
            }
        };

        {
            std::vector<std::jthread> threads;
            for (int i = 0; i < threads_number_; ++i) {
                threads.emplace_back(compute_subintervals);
            }
        }

        // zsumowanie wyników z vectora
        return std::accumulate(results.begin(), results.end(), 0.0);
    }
};


// ---------------------------------------------------------------------
// Klasa ThreadPool - własna implementacja puli wątków
// twprzona jest określona liczba wątków, które cały czas działają i pobierają zadania z kolejki
// istnieje gotowa implementacja np. w zewnętrznej bibliotece boost, ale nie jest to oficjalna część języka c++

class ThreadPool {
private:
    std::vector<std::jthread> threads_; // vector wątków
    std::queue<std::function<void()>> tasks_; // kolejka zadań dla wątków
    std::mutex mutex_; // mutex do synchronizacji kolejki
    std::condition_variable cv_; // zmienna warunkowa do powiadamiania o dostepnych zadaniacj
    bool flag_stop_ = false; // flaga do zatrzymywania puli wątków

public:
    ThreadPool(int threads_number) {
        for (int i = 0; i < threads_number; ++i) {
            threads_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    // sekcja krytyczna
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this]{ return flag_stop_ || !tasks_.empty(); });

                        // jesli brak zadań oraz flaga = true, wątek kończy działanie
                        if (flag_stop_ && tasks_.empty()) {
                            return;
                        }

                        // pobranie zadania z kolejki
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    // wykonanie zadania
                    task();
                }
            });
        }
    }

    // destruktor, kończy wszystkie wątki
    ~ThreadPool() {
        {
            std::scoped_lock lock(mutex_);
            flag_stop_ = true; // ustawienie flagi zatrzymania
        }

        cv_.notify_all(); // powiadomienie wątków aby zakończyły działanie
    }

    // submit() dodaje nowe zadanie do kolejki
    template<typename F>
    // F&& - forwarding reference, przyjmuje funkcje lub lambde
    // std::future<decltype(f())> - typ zwracany przez funkcje, inny rodzaj zapisu używany przy szablonach
    auto submit(F&& f) -> std::future<decltype(f())> { 
        using R = decltype(f()); // typ zwracany przez funkcję f()

        //wskaźnik do packaged_task w kopii shared_ptr, aby bezpiecznie przekazać go między wątkami.
        auto task_ptr = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> result = task_ptr->get_future(); // stworzenie obiektu future

        {
            std::scoped_lock lock(mutex_); 

            // opakowanie w funkcję void(), która wywoła packaged_task
            tasks_.emplace([task_ptr]() {
                 (*task_ptr)(); 
            }); 
        }

        cv_.notify_one(); // powiadomienie pojedynczego wątku o nowym zadaniu 

        return result;       
    }
};


// ---------------------------------------------------------------------
// Klasa ThreadPoolIntegrator - obliczanie całki za pomocą własnej puli wątków

class ThreadPoolIntegrator : public BaseIntegrator {
private:
    int threads_number_;

public:
    ThreadPoolIntegrator(double xp, double xk, double dx, int tasks_number, int threads_number)
        : BaseIntegrator(xp, xk, dx, tasks_number), threads_number_(threads_number) {}

    double compute() override {
        ThreadPool pool(threads_number_);
        std::vector<std::future<double>> futures;

        double sub_interval = (xk_ - xp_) / tasks_number_;

        for (int i = 0; i < tasks_number_; ++i) {
            double sub_xp = xp_ + i * sub_interval;
            double sub_xk = sub_xp + sub_interval;

            IntegralTask task(sub_xp, sub_xk, dx_);
            futures.push_back(pool.submit(task));
        }

        double total_integral = 0.0;
        for (auto& f : futures) {
            total_integral += f.get();
        }

        return total_integral;
    }
};

class Benchmark {
public:
    // funkcja szablonowa, przyjmuje dowolny typ
    template<typename F>
    static double measure(F&& func, const std::string& name) { // metoda static - nie trzeba tworzyć obiektu klasy
        auto start = std::chrono::high_resolution_clock::now(); // zegar wysokiej rozdzielczości
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

    // sekwencyjne
    Benchmark::measure([&]() {
        IntegralTask task(xp, xk, dx);
        return task.compute();
    }, "Sequential integral");

    // równolegle std::async
    Benchmark::measure([&]() {
        AsyncIntegrator integrator(xp, xk, dx, tasks_number);
        return integrator.compute();
    }, "Parallel integral async/future");

    // równolegle std::jthread
    Benchmark::measure([&]() {
        ThreadIntegrator integrator(xp, xk, dx, tasks_number, threads_number);
        return integrator.compute();
    }, "Parallel integral jthread");

    // 4. Równolegle własna pula wątków
    Benchmark::measure([&]() {
        ThreadPoolIntegrator integrator(xp, xk, dx, tasks_number, threads_number);
        return integrator.compute();
    }, "Parallel integral thread pool");

    return 0;
}