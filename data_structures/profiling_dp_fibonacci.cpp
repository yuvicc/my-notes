#include <iostream>
#include <vector>
#include <chrono>
#include <print>
#include <array>

template<typename T, std::size_t N>
constexpr std::array<T, N> make_filled_array(T value) {
    std::array<T, N> arr{};
    arr.fill(value);
    return arr;
}

std::array<long long, 501> dp = make_filled_array<long long, 501>(-1);

long long fibonacci(long long n)
{
    if (n <= 2) return 1;

    if(dp[n] != -1) return dp[n];

    dp[n] = fibonacci(n - 1) + fibonacci(n - 2);
    return dp[n];
}

int main()
{
    long long n = 500;
    
    auto start = std::chrono::high_resolution_clock::now();    

    long long result = fibonacci(n);

    auto stop = std::chrono::high_resolution_clock::now();

    std::cout << "Answer: " << result << std::endl;
    std::cout << "Duration {}" << (stop - start) << std::endl;

    return 0;
}
