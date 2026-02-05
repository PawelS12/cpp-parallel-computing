# Modern C++ Parallel Programming

## Overview

This repository contains code written in modern C++ (C++17 and C++20), developed as part of an engineering project focused on parallel and concurrent programming.

The project demonstrates practical usage of multiple concurrency models and synchronization mechanisms available in modern C++, along with selected external libraries.

## Technologies and Mechanisms Used

- **Thread management**
  - std::jthread
  - std::async
  - std::future

- **Synchronization**
  - std::mutex
  - std::atomic
  - std::unique_lock
  - std::scoped_lock
  - std::condition_variable
  - std::binary_semaphore
  - std::counting_semaphore

- **Parallel execution models**
  - std::execution::par
  - std::execution::par_unseq
  - OpenMP

- **External libraries**
  - Boost.Asio (thread_pool)

## Implemented Problems and Examples

- Producer–Consumer problem
- Readers–Writers problem
- Parallel numerical integration
- Loop decomposition strategies for matrix–vector multiplication

## Project Goals

- Compare different approaches to parallel and concurrent programming in modern C++
- Analyze performance and scalability of selected solutions
- Demonstrate practical usage of synchronization mechanisms and execution policies

