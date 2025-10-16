// https://en.cppreference.com/w/cpp/thread/counting_semaphore.html - semaphore

#include <semaphore>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <format>

class Pub {
private:
    static constexpr int MUG_MAX = 100;
    static constexpr int TAP_MAX = 100;

    std::counting_semaphore<MUG_MAX> mugs_;
    std::counting_semaphore<TAP_MAX> taps_;

    const int total_mugs_;
    const int total_taps_;

    std::mutex io_mutex_;

public:
    Pub(int mugs_number, int taps_number)
        : mugs_(mugs_number), taps_(taps_number), total_mugs_(mugs_number), total_taps_(taps_number) {}

    void drink(int customer_id, int drinks_required) {
        for (int i = 0; i < drinks_required; ++i) {
            
            // Step 1
            mugs_.acquire();
            log(std::format("Customer {} takes a mug.", customer_id));
            
            // Step 2
            taps_.acquire();
            log(std::format("Customer {} pours a beer.", customer_id));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            taps_.release();

            // Step 3
            log(std::format("Customer {} is drinking.", customer_id));
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // Step 4
            mugs_.release();
            log(std::format("Customer {} puts down the mug.", customer_id));
        }
        log(std::format("Customer {} leaves the pub.", customer_id));
    }

    void log(const std::string& message) {
        std::scoped_lock lock(io_mutex_);
        std::cout << message << std::endl;
    }

    void close_pub() {
        std::scoped_lock lock(io_mutex_);
        std::cout << "\nThe pub closed!\n";
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
    
    /*std::cout << "Customers number: ";
    std::cin >> customers_number;

    std::cout << "Mugs number: ";
    std::cin >> mugs_number;
    
    std::cout << "Taps number: ";
    std::cin >> taps_number;*/

    std::cout << std::format("Customers: {}, Mugs: {}, Taps: {}\n\n", customers_number, mugs_number, taps_number);

    Pub pub(mugs_number, taps_number);

    std::vector<std::jthread> customers;
    for (int i = 0; i < customers_number; ++i) {
        customers.emplace_back(Customer(i, pub, drinks_per_customer));
    }

    customers.clear();
    pub.close_pub();

    return 0;
}