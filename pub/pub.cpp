#include <semaphore>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <format>

class Pub {
private:
    static constexpr int mug_max_ = 100;

    std::counting_semaphore<mug_max_> mugs_;
    std::vector<std::unique_ptr<std::binary_semaphore>> taps_;

    const int total_mugs_;
    const int total_taps_;

    std::atomic<int> current_mugs_available_;
    std::mutex io_mutex_;

public:
    Pub(int mugs_number, int taps_number)
        : mugs_(mugs_number),
          total_mugs_(mugs_number), 
          total_taps_(taps_number),
          current_mugs_available_(mugs_number) 
    {
        taps_.reserve(total_taps_);
        for (int i = 0; i < total_taps_; ++i) {
            taps_.push_back(std::make_unique<std::binary_semaphore>(1));
        }
    }

    void drink(int customer_id, int drinks_required) {
        for (int i = 0; i < drinks_required; ++i) {
            
            // Step 1
            mugs_.acquire();
            --current_mugs_available_;
            log(std::format("Customer {} takes a mug.", customer_id));
            
            // Step 2
            int used_tap = -1;
            while (used_tap == -1) {
                
                for (int j = 0; j < total_taps_; ++j) {
                    if (taps_[j]->try_acquire()) {
                        used_tap = j;
                        break;
                    }
                }

                if (used_tap == -1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            log(std::format("Customer {} pours a beer from tap #{}", customer_id, used_tap));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            taps_[used_tap]->release();

            // Step 3
            log(std::format("Customer {} is drinking.", customer_id));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // Step 4
            mugs_.release();
            ++current_mugs_available_;
            log(std::format("Customer {} puts down the mug.", customer_id));
        }
        log(std::format("Customer {} leaves the pub.", customer_id));
    }

    void log(const std::string& message) {
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

    void verify_and_close_pub(int initial_mugs, int final_mugs) {    
        if (final_mugs != initial_mugs) {
            log(std::format("\nERROR: Mug count mismatch! Start: {}, End: {}\n", initial_mugs, final_mugs));
        } else {
            log(std::format("\nSUCCESS: All mugs returned properly! Start: {}, End: {}.\n", initial_mugs, final_mugs));
        }
    }

    int mugs_remaining() const {
        return current_mugs_available_.load();
    }

    int total_mugs() const {
        return total_mugs_;
    }
};


class Customer {
private:
    int customer_id_;
    Pub& pub_;
    int drinks_required_;

public:
    Customer(int id, Pub& pub, int drinks_required) 
        : customer_id_(id), pub_(pub), drinks_required_(drinks_required) {}

    void operator()() {
        pub_.drink(customer_id_, drinks_required_);
    }
};


int main() {
    const int customers_number = 12;
    const int mugs_number = 4;
    const int taps_number = 2;
    const int drinks_per_customer = 3;

    Pub pub(mugs_number, taps_number);

    int initial_mugs = pub.total_mugs();

    std::cout << std::format("Customers: {}, Mugs: {}, Taps: {}\n\n", customers_number, mugs_number, taps_number);

    {
        std::vector<std::jthread> customer_threads;
        for (int i = 0; i < customers_number; ++i) {
            customer_threads.emplace_back(Customer(i, pub, drinks_per_customer));
        }
    }

    int final_mugs = pub.mugs_remaining();
    pub.verify_and_close_pub(initial_mugs, final_mugs);

    return 0;
}