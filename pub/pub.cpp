#include <semaphore>
#include <iostream>

class Pub {
private:
    static constexpr int MAX_KUFLE = 100;
    static constexpr int MAX_KRANY = 100;

    std::counting_semaphore<MAX_KUFLE> kufle_;
    std::counting_semaphore<MAX_KRANY> krany_;

public:
    Pub(int liczba_kufli, int liczba_kranow) : kufle_(liczba_kufli), krany_(liczba_kranow) {}

    void pij(int id_klient, int ile_musze_wypic) {
        for (int i = 0; i < ile_musze_wypic; ++i) {
            kufle_.acquire();
            std::
        }
    }


};
