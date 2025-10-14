/*

Tematy:
    Pub w C++
    Całkowanie z labów tak jak w java z pulą wątków
    Zmienne warunkowe problem czytelników
    Mnożenie macierz wektor
    Pytanie o skalowalność obliczeń
    Teoria współbieżnośći w C++


Książki:
    “C++ Concurrency in Action” – Anthony Williams
    “Programming with C++20: Concepts, Ranges, and Coroutines” – Andreas Fertig

Plan:
    Pub C++:    
        Cel: implementacja prostego systemu publikacja/subskrypcja wątków.
        Mechanizmy użyte: std::mutex, std::condition_variable, std::queue.
        Możliwość wykorzystania std::jthread do automatycznego zarządzania wątkami.
        Przykład: wątki subskrybentów odbierają komunikaty od wydawcy. 

    Całkowanie z pulą wątków:
        Cel: równoległe obliczanie całek (tak jak w labach w Javie).
        Mechanizm:
            Thread pool (implementacja własna lub std::async)
            Podział zadania na podcałki wykonywane równolegle
        Analiza wydajności: porównanie liczby wątków vs czas wykonania.

    Problem czytelników:
        Cel: synchronizacja wielu czytelników i pisarzy do wspólnego zasobu.
        Mechanizmy:
            std::shared_mutex (wielu czytelników, jeden pisarz)
            std::condition_variable do oczekiwania wątków
            Omówienie możliwych deadlocków i race conditions

    Mnożenie macierz wektor:
        Cel: równoległe mnożenie macierzy przez wektor.
        Implementacja:
            Podział macierzy na bloki lub wiersze dla wątków
            Thread pool lub std::async do równoległego wykonania
            Analiza skalowalności: wpływ liczby wątków i rozmiaru danych na czas obliczeń.

    Skalowalność obliczeń:
        Porównanie:
            Wykonanie sekwencyjne vs równoległe
            Liczba wątków vs przyspieszenie (speedup)
        Omówienie wąskich gardeł (synchronizacja, pamięć, cache).
        Wnioski praktyczne dla projektowania algorytmów równoległych.

    Teoria współbieżności w C++
        Mechanizmy standardowe:
            std::thread, std::jthread (C++20)
            std::mutex, std::shared_mutex, std::recursive_mutex
            std::atomic<T>
            std::condition_variable
        Nowe mechanizmy w C++20:
            std::stop_token, std::latch, std::barrier
            Semafory: std::counting_semaphore, std::binary_semaphore
            Równoległe algorytmy: std::for_each(std::execution::par, ...)
*/

#include <iostream>
#include <thread>

int main() {
    std::thread t([]{ std::cout << "Wątek działa!\n"; });
    t.join();
    std::cout << "C++20 działa w WSL!" << std::endl;
}