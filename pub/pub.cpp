#include <semaphore>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <format>


class Pub {
private:
    const int total_mugs_; // całkowita ilość kufli w pubie
    const int total_taps_; // całkowita ilość kranów w pubie 

    // binary_semaphore (c++ 20) - przyjmuje dwa stany: 1(wolny) lub 0(zajęty), nie można przechowywac go w vectorze jako obiekt
    // unique_ptr automatycznie zarządza pamięcią 
    // dzięki binary_semaphore wiemy dokładnie który kran jest używany przez klienta, jeśli nie chcemy tego wiedzieć można użyć counting_semaphore jak przy kuflach 
    std::vector<std::unique_ptr<std::binary_semaphore>> taps_; 

    // liczba aktualnie dostępnych kufli
    // atomic - operacjie: ++ -- load() są atomowe, bardziej wydajny niż mutex
    std::atomic<int> current_mugs_available_;

    static constexpr int mug_max_ = 100; // limit kufli wymagany do counting_semaphore

    // counting_semaphore (c++ 20) - licznik kufli, kontroluje wiele zasobów jednocześnie
    // metody semafora: acquire() zmniejsza licznik, release() zwieksza, try_acquire() próbuje zmniejszyć bez blokowania
    // automatycznie blokuje wątki gdy liczba zasobu jest równa 0
    std::counting_semaphore<mug_max_> mugs_;

    // mutex do logów
    std::mutex io_mutex_;

public:
    // Konstruktor
    Pub(int mugs_number, int taps_number)
        : mugs_(mugs_number),
          total_mugs_(mugs_number), 
          total_taps_(taps_number),
          current_mugs_available_(mugs_number) 
    {
        // reserve() - zarezerwowanie pamięci dla kranów
        // w pętli tworzone są semafory (stan 1 - wolny) dla każdego kranu 
        taps_.reserve(total_taps_);
        for (int i = 0; i < total_taps_; ++i) {
            taps_.push_back(std::make_unique<std::binary_semaphore>(1));
        }
    }

    // Kolejność kroków metody drink():
    //   1. klient pobiera kufel za pomocą acquire()
    //   2. kilent znajduje kran (stan 1: wolny) i nalewa piwo
    //   3. klient pije piwo
    //   4. klient oddaje kufel za pomocą release()
    void drink(int customer_id, int drinks_required) {
        for (int i = 0; i < drinks_required; ++i) {
            
            // Krok 1
            mugs_.acquire();
            --current_mugs_available_;
            // std::format (c++ 20) tworzy string, który wypisuje funkcja log()
            log(std::format("Customer {} takes a mug.", customer_id));
            
            // Krok 2
            int used_tap = -1;
            while (used_tap == -1) {
                
                for (int j = 0; j < total_taps_; ++j) {

                    // klient próbuje zablokować semafor kranu
                    if (taps_[j]->try_acquire()) {
                        used_tap = j;
                        break;
                    }
                }

                if (used_tap == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            log(std::format("Customer {} pours a beer from tap {}", customer_id, used_tap));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            taps_[used_tap]->release();

            // Krok 3
            log(std::format("Customer {} is drinking.", customer_id));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // Krok 4
            mugs_.release();
            ++current_mugs_available_;
            log(std::format("Customer {} puts down the mug.", customer_id));
        }

        log(std::format("Customer {} leaves the pub.", customer_id));
    }

    void log(const std::string& message) {
        // scoped_lock (c++ 17) automatycznie blokuje i zwalnia mutex (mutex zwalnia się po zakończeniu funkcji)
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

    // porównanie ilości kufli po symulacji z ilością przed symulacją
    void verify_and_close_pub(int initial_mugs_number, int final_mugs_number) {    
        if (final_mugs_number == initial_mugs_number) {
            log(std::format("\nSUCCESS: All mugs returned properly! Start: {}, End: {}.\n", initial_mugs_number, final_mugs_number));
        } else {
            log(std::format("\nERROR: Mug count mismatch! Start: {}, End: {}\n", initial_mugs_number, final_mugs_number));
        }
    }

    // load() odczytuje aktualną wartość zmiennej atomowej
    int mugs_remaining() const {
        return current_mugs_available_.load();
    }

    int total_mugs() const {
        return total_mugs_;
    }

    // deklaracja funkcji symulacji pubu
    void simulate(int customers_number, int drinks_per_customer); 
};


class Customer {
private:
    int customer_id_; // id kilenta
    Pub& pub_; // referencja do pubu
    int drinks_required_; // liczba kufli, które klient musi wypić

public:

    // Konstruktor
    Customer(int id, Pub& pub, int drinks_required) 
        : customer_id_(id), pub_(pub), drinks_required_(drinks_required) {}

    // przeciążenie operatora (), który teraz wywołuje metodę drink()
    void operator()() {
        pub_.drink(customer_id_, drinks_required_);
    }
};

void Pub::simulate(int customers_number, int drinks_per_customer) {
    int initial_mugs_number = total_mugs_;

    // Po wyjściu z bloku {} wektor zostanie zniszczony, a destruktory std::jthread automatycznie wykonają join().
    // std::jthread (c++ 20) joining thread:
    //   - automatyczny join() w destruktorze
    //   - możliwośc wywołania stop_token, który powoduje zatrzymanie wątku w dowolnym momencie
    {
        std::vector<std::jthread> customer_threads;
        for (int i = 0; i < customers_number; ++i) {
            customer_threads.emplace_back(Customer(i, *this, drinks_per_customer));
        }
    }

    // sprawdzenie czy symulacja się powiodła
    int final_mugs_number = mugs_remaining();
    verify_and_close_pub(initial_mugs_number, final_mugs_number);
}


int main() {
    const int customers_number = 12;    // liczba klientów w pubie
    const int mugs_number = 4;          // liczba kufli 
    const int taps_number = 2;          // liczba kranów
    const int drinks_per_customer = 3;  // ilość kufli które musi wypić klient

    Pub pub(mugs_number, taps_number);  // obiekt pubu

    // początkowy stan pubu
    std::cout << std::format("Customers: {}, Mugs: {}, Taps: {}\n\n", customers_number, mugs_number, taps_number);

    // symulacja
    pub.simulate(customers_number, drinks_per_customer);

    return 0;
}