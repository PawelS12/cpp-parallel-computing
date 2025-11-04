#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <random>
#include <format>

class Library;

class Reader {
private:
    int id_; // id czytelnika
    Library& library_; // referencja do obiektu biblioteki

public:
    Reader(int id, Library& library); // konstruktor
    void operator()(std::stop_token stop); // przeciążenie operatora ()
};

class Writer {
private:
    int id_; // id pisarza
    Library& library_; // referencja do obeiktu biblioteki

public:
    Writer(int id, Library& library); // konstruktor
    void operator()(std::stop_token stop); // przeciążenie operatora ()
};

class Library {
private:
    std::mutex mutex_; // główny mutez, synchronizujący dostęp do bibioteki
    std::mutex io_mutex_; // mutex do 
    
    // condition_variable (C++11) – mechanizm synchronizacji wątków
    // Umożliwia wstrzymanie (wait) wątku do momentu spełnienia określonego warunku
    // Metody: wait(lock, lambda) – usypia wątek, aż lambda zwróci true, notify_one() – budzi jeden oczekujący wątek, notify_all() – budzi wszystkie oczekujące wątki
    std::condition_variable cond_readers_; 
    std::condition_variable cond_writers_; 

    int readers_ = 0; // liczba czytających     
    int writers_ = 0; // liczba pisarzy         
    int waiting_writers_ = 0; // liczba czekających pisarzy   

public:
    void start_read(int id) {
        // std::unique_lock blokuje przekazany mutex przy tworzeniu obiektu
        // tylko unique_lock działa z funkcją wait(), ponieważ musi mieć możliwość tymczasowego zwolnienia i ponownego zablokowania mutexa
        std::unique_lock lock(mutex_);

        // Funkcja cond_readers_.wait(lock, lambda):
        //   - wstrzymuje wykonanie bieżącego wątku (czytelnika)
        //   - automatycznie zwalnia mutex podczas oczekiwania
        //   - po przebudzeniu ponownie blokuje mutex
        //   - gdy lambda zwraca true czytelnik przechodzi dalej
        // lambda chroni przed spurious wakeup (fałszywym przebudzeniem).
        cond_readers_.wait(lock, [this] {
            return writers_ == 0 && waiting_writers_ == 0;
        });

        /* Można też z pętlą while ale to jest bardziej nowoczesne

        while (!(writers_ == 0 && waiting_writers_ == 0)) {
            cond_readers_.wait(lock);
        } 
        */

        readers_++;
        log(std::format("Reader {} starts reading (readers = {}, writers = {})", id, readers_, writers_));
    }

    void end_read(int id) {
        std::unique_lock lock(mutex_);
        readers_--;

        log(std::format("Reader {} finished reading (readers = {}, writers = {})", id, readers_, writers_));

        if (readers_ == 0) {
            // notify_one() (C++11) - budzi jeden wątek oczekujący na cond_writers_
            cond_writers_.notify_one(); 
        }
    }

    void start_write(int id) {
        std::unique_lock lock(mutex_);
        waiting_writers_++;

        cond_writers_.wait(lock, [this] {
            return readers_ == 0 && writers_ == 0;
        });

        waiting_writers_--;
        writers_++;
        log(std::format("Writer {} starts writing (writers = {}, readers = {})", id, writers_, readers_));
    }

    void end_write(int id) {
        std::unique_lock lock(mutex_);
        writers_--;
        log(std::format("Writer {} finished writing (writers = {}, readers = {})", id, writers_, readers_));

        if (waiting_writers_ > 0) {
            // notify_one() - budzi jeden czekający wątek pisarza
            cond_writers_.notify_one(); 
        } else {
            // notify_all() - budzi wszystkie czekające wątki czytelników
            cond_readers_.notify_all(); 
        }
    }

    // Funkcja symuluje proces czytania przez czytelnika.
    void reading(int id) { 
        log(std::format("Reader {} is reading", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Funkcja symuluje proces pisania przez pisarza.
    void writing(int id) { 
        log(std::format("Writer {} is writing", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    void log(const std::string& message) {
        // scoped_lock (c++ 17) automatycznie blokuje i zwalnia mutex (mutex zwalnia się po zakończeniu funkcji)
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

    // podsumowanie po zakończeniu symulacji
    void summary() {
        log("\n--- Simulation completed ---");
        log("Final state:");
        log(std::format("  Active readers: {}", readers_));
        log(std::format("  Active writers: {}", writers_));
        log(std::format("  Waiting writers: {}", waiting_writers_));

        if (readers_ == 0 && writers_ == 0) {
            log("All threads have terminated.");
        } else {
            log("Some threads are still active.");
        }
    }

    void simulate(int readers_number, int writers_number, int duration_time) {
        std::vector<std::jthread> readers_threads;
        std::vector<std::jthread> writers_threads;

        for (int i = 0; i < readers_number; ++i) {
            readers_threads.emplace_back(Reader(i, *this));
        }
        
        for (int i = 0; i < writers_number; ++i) {
            writers_threads.emplace_back(Writer(i, *this));
        }

        // czas działania wątków
        std::this_thread::sleep_for(std::chrono::seconds(duration_time));


        // std::jthread (C++20) używa wewnętrznego std::stop_source / std::stop_token.
        // request_stop() ustawia flagę zakończenia pracy wątku, wątek sprawdza ją poprzez stop.stop_requested()

        // Pętla wysyłająca sygnał zatrzymania do wszystkich wątków czytelników
        for (auto& t : readers_threads) {
            t.request_stop();
        }

        // Pętla wysyłająca sygnał zatrzymania do wszystkich wątków pisarzy
        for (auto& t : writers_threads) {
            t.request_stop();
        }

        /* można też użyć wspólnego tokena stopu, który przekazuje sie do wszystkich wątków, ale w tym przypadku jest to nadmiarowe

        std::stop_source stop_source;
        std::stop_token token = stop_source.get_token();

        for (int i = 0; i < readers_number; ++i) {
            readers_threads.emplace_back(Reader(i, library), token);
        }

        for (int i = 0; i < writers_number; ++i) {
            writers_threads.emplace_back(Writer(i, library), token);
        } */

        // wywołanie podsumowania
        summary();
    }
};

Reader::Reader(int id, Library& library) : id_(id), library_(library) {}

void Reader::operator()(std::stop_token stop) {
    // generator losowości do czasów czytania
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(100, 1000);

    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
        library_.start_read(id_);
        library_.reading(id_);
        library_.end_read(id_);
    }
}

Writer::Writer(int id, Library& library) : id_(id), library_(library) {}

void Writer::operator()(std::stop_token stop) {
    // generator losowości do czasów przerw między pisaniem
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(500, 1500);

    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
        library_.start_write(id_);
        library_.writing(id_);
        library_.end_write(id_);
    }
}

int main() {
    Library library; // obiekt biblioteki
    int readers_number = 5; // liczba czytelników
    int writers_number = 2; // ;iczba pisarzy
    int duration_time = 10; // czas trwania wątków
    
    // symulacja biblioteki
    library.simulate(readers_number, writers_number, duration_time); 
    
    return 0;
}