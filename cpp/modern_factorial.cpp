#include <iostream>
#include <chrono>
#include <cstdint>
#include <type_traits>
#include <concepts>

// C++20 Concept: only allow integral types
template<typename T>
concept Integral = std::is_integral_v<T>;

// C++20 Concept: non-negative integer input
template<typename T>
concept NonNegativeIntegral = Integral<T> && requires(T v) {
    { v >= T{0} } -> std::convertible_to<bool>;
};

// constexpr template factorial (compile-time recursion)
template<NonNegativeIntegral T>
constexpr T factorial(T n) {
    if (n <= 1) return T{1};
    return n * factorial(n - T{1});
}

// Compile-time factorial via template specialization (classic TMP)
template<uint64_t N>
struct FactorialTMP {
    static constexpr uint64_t value = N * FactorialTMP<N - 1>::value;
};

template<>
struct FactorialTMP<0> {
    static constexpr uint64_t value = 1;
};

// Constexpr lookup table generated at compile time
template<std::size_t... Is>
constexpr auto make_factorial_table(std::index_sequence<Is...>) {
    return std::array<uint64_t, sizeof...(Is)>{ factorial<uint64_t>(Is)... };
}

constexpr auto FACTORIAL_TABLE = make_factorial_table(std::make_index_sequence<21>{});

int main() {
    const int ITERATIONS = 10'000'000;
    constexpr int N = 20;

    // Verify compile-time result
    static_assert(FactorialTMP<20>::value == 2432902008176640000ULL, "Compile-time factorial is wrong!");
    static_assert(FACTORIAL_TABLE[20] == 2432902008176640000ULL, "Table factorial is wrong!");

    // Benchmark constexpr template (runtime call, but compiler may optimize heavily)
    auto start = std::chrono::high_resolution_clock::now();
    volatile uint64_t result = 0;
    for (int i = 0; i < ITERATIONS; ++i)
        result = factorial<uint64_t>(N);
    auto end = std::chrono::high_resolution_clock::now();
    auto constexpr_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Benchmark TMP compile-time constant (should be near-zero: it's a constant)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
        result = FactorialTMP<N>::value;
    end = std::chrono::high_resolution_clock::now();
    auto tmp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Benchmark lookup table (O(1) array access)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
        result = FACTORIAL_TABLE[N];
    end = std::chrono::high_resolution_clock::now();
    auto table_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "=== Modern C++20 Factorial (Templates + constexpr + Concepts) ===" << std::endl;
    std::cout << "factorial(" << N << ") = " << factorial<uint64_t>(N) << std::endl;
    std::cout << "TMP compile-time value: " << FactorialTMP<N>::value << std::endl;
    std::cout << "Table value:            " << FACTORIAL_TABLE[N] << std::endl;
    std::cout << std::endl;
    std::cout << "constexpr template: " << constexpr_ns / 1'000'000.0 << " ms for " << ITERATIONS << " calls" << std::endl;
    std::cout << "TMP constant:       " << tmp_ns / 1'000'000.0 << " ms for " << ITERATIONS << " calls" << std::endl;
    std::cout << "Lookup table:       " << table_ns / 1'000'000.0 << " ms for " << ITERATIONS << " calls" << std::endl;
    std::cout << "constexpr ns/call: " << (double)constexpr_ns / ITERATIONS << std::endl;
    std::cout << "TMP ns/call:       " << (double)tmp_ns / ITERATIONS << std::endl;
    std::cout << "Table ns/call:     " << (double)table_ns / ITERATIONS << std::endl;

    return 0;
}
