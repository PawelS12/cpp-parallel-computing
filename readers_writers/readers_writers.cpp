#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <random>
#include <format>
#include <deque>

class Library;

class Reader {
private:
    int id_; 
    Library& library_; 

public:
    Reader(int id, Library& library); 
    void operator()(std::stop_token stop); 
};

class Writer {
private:
    int id_;
    Library& library_; 

public:
    Writer(int id, Library& library); 
    void operator()(std::stop_token stop); 
};

class Library {
private:
    int readers_ = 0;     
    int writers_ = 0;        
    int waiting_writers_ = 0;  
    int waiting_readers_ = 0;
    std::condition_variable cond_readers_; 
    std::condition_variable cond_writers_;  
    std::mutex mutex_; 
    std::mutex io_mutex_; 
    std::deque<int> writers_queue_;

public:
    void start_read(int id) {
        std::unique_lock lock(mutex_);
        waiting_readers_++;

        cond_readers_.wait(lock, [this] {
            return writers_ == 0 && waiting_writers_ == 0; 
        });

        waiting_readers_--;
        readers_++;

        if (waiting_readers_ > 0) {
            cond_readers_.notify_one(); 
        }

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
        writers_queue_.push_back(id); 
        log(std::format("Writer {} is waiting to write", id));

        cond_writers_.wait(lock, [this, id] {
            return readers_ == 0 && writers_ == 0 && !writers_queue_.empty() && writers_queue_.front() == id;
        });

        waiting_writers_--;
        writers_++;
        log(std::format("Writer {} starts writing (writers = {}, readers = {})", id, writers_, readers_));
    }

    void end_write(int id) {
        std::unique_lock lock(mutex_);
        writers_--;

        log(std::format("Writer {} finished writing (writers = {}, readers = {})", id, writers_, readers_));

        if (!writers_queue_.empty() && writers_queue_.front() == id) {
            writers_queue_.pop_front();
        }
        
        if (waiting_readers_ > 0 && writers_queue_.empty()) {
            cond_readers_.notify_all();
        } else if (!writers_queue_.empty()) {
            cond_writers_.notify_all();
        }
    }

    void reading(int id) { 
        log(std::format("Reader {} is reading", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    void writing(int id) { 
        log(std::format("Writer {} is writing", id));
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    void log(const std::string& message) {
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

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

        std::this_thread::sleep_for(std::chrono::seconds(duration_time));

        for (auto& t : readers_threads) {
            t.request_stop();
        }

        for (auto& t : writers_threads) {
            t.request_stop();
        }
    }
};

Reader::Reader(int id, Library& library) : id_(id), library_(library) {}

void Reader::operator()(std::stop_token stop) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(200, 500);

    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
        library_.start_read(id_);
        library_.reading(id_);
        library_.end_read(id_);
    }
}

Writer::Writer(int id, Library& library) : id_(id), library_(library) {}

void Writer::operator()(std::stop_token stop) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(2000, 3000);

    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
        library_.start_write(id_);
        library_.writing(id_);
        library_.end_write(id_);
    }
}

int main() {

    Library library; 
    int readers_number = 3; 
    int writers_number = 9;
    int duration_time = 15; 
    
    library.simulate(readers_number, writers_number, duration_time); 
    
    library.summary();
    return 0;
}