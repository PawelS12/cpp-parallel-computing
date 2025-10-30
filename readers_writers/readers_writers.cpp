#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <random>
#include <format>

class Library {
private:
    std::mutex mutex_;
    std::mutex io_mutex_; 
    std::condition_variable cond_readers_;
    std::condition_variable cond_writers_;
    int readers_ = 0;          
    int writers_ = 0;          
    int waiting_writers_ = 0;  

public:
    void start_read(int id) {
        std::unique_lock lock(mutex_);
        while (writers_ > 0 || waiting_writers_ > 0) {
            cond_readers_.wait(lock);
        }
        readers_++;
        log(std::format("Reader {} starts reading (readers = {}, writers = {})", id, readers_, writers_));
    }

    void end_read(int id) {
        std::unique_lock lock(mutex_);
        readers_--;
        log(std::format("Reader {} finished reading (readers = {}, writers = {})", id, readers_, writers_));

        if (readers_ == 0) {
            cond_writers_.notify_one(); 
        }
    }

    void start_write(int id) {
        std::unique_lock lock(mutex_);
        waiting_writers_++;

        while (readers_ > 0 || writers_ > 0) { 
            cond_writers_.wait(lock);
        }

        waiting_writers_--;
        writers_++;
        log(std::format("Writer {} starts writing (writers = {}, readers = {})", id, writers_, readers_));
    }

    void end_write(int id) {
        std::unique_lock lock(mutex_);
        writers_--;
        log(std::format("Writer {} finished writing (writers = {}, readers = {})", id, writers_, readers_));

        if (waiting_writers_ > 0) {
            cond_writers_.notify_one(); 
        } else {
            cond_readers_.notify_all(); 
        }
    }

    void reading(int id) { 
        log(std::format("Reader {} is reading", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void writing(int id) { 
        log(std::format("Writer {} is writing", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    void log(const std::string& message) {
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }
};


class Reader {
private:
    int id_;
    Library& library_;

public:
    Reader(int id, Library& library) : id_(id), library_(library) {}

    void operator()(std::stop_token stop) {
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dist(100, 1000);

        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
            library_.start_read(id_);
            library_.reading(id_);
            library_.end_read(id_);
        }
    }
};


class Writer {
private:
    int id_;
    Library& library_;

public:
    Writer(int id, Library& library) : id_(id), library_(library) {}

    void operator()(std::stop_token stop) {
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dist(500, 1500);

        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
            library_.start_write(id_);
            library_.writing(id_);
            library_.end_write(id_);
        }
    }
};


int main() {
    Library library;
    int readers_number = 5;
    int writers_number = 2;

    {
        std::vector<std::jthread> readers_threads;
        std::vector<std::jthread> writers_threads;

        for (int i = 0; i < readers_number; ++i) {
            readers_threads.emplace_back(Reader(i, library));
        }

        for (int i = 0; i < writers_number; ++i) {
            writers_threads.emplace_back(Writer(i, library));
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));

        for (auto& t : readers_threads) {
            t.request_stop();
        }

        for (auto& t : writers_threads) {
            t.request_stop();
        }
    }

    std::cout << "--- Simulation completed ---\n";

    return 0;
}