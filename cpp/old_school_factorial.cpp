#include <iostream>
#include <chrono>
#include <cstdint>

// Normal iterative factorial
uint64_t factorial(int n) {
    if (n < 0) return 0;
    uint64_t result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

// Normal recursive factorial
uint64_t factorial_recursive(int n) {
    if (n <= 1) return 1;
    return n * factorial_recursive(n - 1);
}

int main() {
    const int ITERATIONS = 10'000'000;
    const int N = 20;

    // Benchmark iterative
    auto start = std::chrono::high_resolution_clock::now();
    volatile uint64_t result = 0;
    for (int i = 0; i < ITERATIONS; ++i)
        result = factorial(N);
    auto end = std::chrono::high_resolution_clock::now();
    auto iterative_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Benchmark recursive
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
        result = factorial_recursive(N);
    end = std::chrono::high_resolution_clock::now();
    auto recursive_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "=== Normal C++ Factorial ===" << std::endl;
    std::cout << "factorial(" << N << ") = " << factorial(N) << std::endl;
    std::cout << "Iterative: " << iterative_ns / 1'000'000.0 << " ms for " << ITERATIONS << " calls" << std::endl;
    std::cout << "Recursive: " << recursive_ns / 1'000'000.0 << " ms for " << ITERATIONS << " calls" << std::endl;
    std::cout << "Iterative ns/call: " << (double)iterative_ns / ITERATIONS << std::endl;
    std::cout << "Recursive ns/call: " << (double)recursive_ns / ITERATIONS << std::endl;

    return 0;
}
