#include <semaphore>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <format>


class Pub {
private:
    const int total_mugs_; 
    const int total_taps_; 

    std::vector<std::unique_ptr<std::binary_semaphore>> taps_; 
    std::atomic<int> current_mugs_available_;
    static constexpr int mug_max_ = 100; 
    std::counting_semaphore<mug_max_> mugs_;
    std::vector<bool> tap_in_use_;
    std::mutex io_mutex_;

public:
    Pub(int mugs_number, int taps_number)
        : mugs_(mugs_number),
          total_mugs_(mugs_number), 
          total_taps_(taps_number),
          current_mugs_available_(mugs_number) 
    {
        taps_.reserve(total_taps_);
        tap_in_use_.resize(total_taps_, false);
        for (int i = 0; i < total_taps_; ++i) {
            taps_.push_back(std::make_unique<std::binary_semaphore>(1));
        }
    }

    void drink(int customer_id, int drinks_required) {
        for (int i = 0; i < drinks_required; ++i) {
            mugs_.acquire();
            --current_mugs_available_;
      
            log(std::format("Customer {} takes a mug.", customer_id));
            
            int used_tap = -1;
            while (used_tap == -1) {
                
                for (int j = 0; j < total_taps_; ++j) {
                    if (taps_[j]->try_acquire()) {
                        used_tap = j;

                        tap_in_use_[used_tap] = true;

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

            log(std::format("Customer {} is drinking.", customer_id));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            mugs_.release();
            ++current_mugs_available_;
            tap_in_use_[used_tap] = false;
            log(std::format("Customer {} puts down the mug.", customer_id));
        }

        log(std::format("Customer {} leaves the pub.", customer_id));
    }

    void log(const std::string& message) {
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

    void verify_and_close_pub(int initial_mugs_number, int final_mugs_number) {    
        if (final_mugs_number == initial_mugs_number) {
            log(std::format("\nAll mugs returned properly! Start: {}, End: {}.", initial_mugs_number, final_mugs_number));
        } else {
            log(std::format("\nMug count mismatch! Start: {}, End: {}", initial_mugs_number, final_mugs_number));
        }

        for (int i = 0; i < total_taps_; ++i) {
            if (!tap_in_use_[i]) {
                log(std::format("Tap {} was used correctly.", i));
            } else {
                log(std::format("Tap {} usage error detected!", i));
            }
        }
    }

    int mugs_remaining() const {
        return current_mugs_available_.load();
    }

    int total_mugs() const {
        return total_mugs_;
    }

    void simulate(int customers_number, int drinks_per_customer); 
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

void Pub::simulate(int customers_number, int drinks_per_customer) {
    int initial_mugs_number = total_mugs_;

    {
        std::vector<std::jthread> customer_threads;
        for (int i = 0; i < customers_number; ++i) {
            customer_threads.emplace_back(Customer(i, *this, drinks_per_customer));
        }
    }

    int final_mugs_number = mugs_remaining();
    verify_and_close_pub(initial_mugs_number, final_mugs_number);
}


int main() {

    const int customers_number = 12;   
    const int mugs_number = 4;      
    const int taps_number = 2;        
    const int drinks_per_customer = 3;  

    Pub pub(mugs_number, taps_number);  

    std::cout << std::format("Customers: {}, Mugs: {}, Taps: {}\n\n", customers_number, mugs_number, taps_number);

    pub.simulate(customers_number, drinks_per_customer);

    return 0;
}