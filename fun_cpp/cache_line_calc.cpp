/**
 * Cache Line Size Checker
 *
 * Usage:
 *   g++ -o cache_line_test cache_line_calc.cpp
 *   ./cache_line_test
 *   ./cache_line_test --csv results.csv
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <time.h>


static constexpr std::size_t BUFFER_SIZE   = 1024 * 1024 * 16;  // 16 MB
static constexpr std::size_t MIN_STRIDE    = 4;
static constexpr std::size_t MAX_STRIDE    = 256;
static constexpr unsigned    NUM_TRIALS    = 10;

static std::uint64_t now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

// don't inline / don't optimize
__attribute__((noinline))
static void strided_copy(const char* __restrict__ src, char* __restrict__ dst,
                         std::size_t stride)
{
    for (std::size_t i = 0; i < BUFFER_SIZE; i += stride)
        dst[i] = src[i];
}

struct BenchResult {
    double avg_ns; 
    double min_ns;
    double max_ns;
};

static BenchResult benchmark_stride(std::size_t stride)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    double total   = 0.0;
    double best    = std::numeric_limits<double>::max();
    double worst   = 0.0;

    for (unsigned trial = 0; trial < NUM_TRIALS; ++trial)
    {
        char* src = new char[BUFFER_SIZE];
        char* dst = new char[BUFFER_SIZE];

        for (std::size_t i = 0; i < BUFFER_SIZE; ++i)
            src[i] = static_cast<char>(dist(rng));
        std::memset(dst, 0, BUFFER_SIZE);

        std::uint64_t t0 = now_ns();
        for (std::size_t rep = 0; rep < stride; ++rep)
            strided_copy(src, dst, stride);
        std::uint64_t t1 = now_ns();

        double elapsed = static_cast<double>(t1 - t0) / stride; 
        total += elapsed;
        best   = std::min(best, elapsed);
        worst  = std::max(worst, elapsed);

        delete[] src;
        delete[] dst;
    }

    return { total / NUM_TRIALS, best, worst };
}

static std::size_t detect_cache_line(const double* throughputs, std::size_t count)
{
    constexpr double PLATEAU_THRESHOLD = 1.02;

    constexpr int WINDOW = 5;
    int plateau_start = -1;
    int consecutive = 0;

    for (std::size_t i = 1; i < count; ++i)
    {
        double ratio = throughputs[i] / throughputs[i - 1];
        if (ratio < PLATEAU_THRESHOLD)
        {
            if (++consecutive >= WINDOW && plateau_start < 0)
                plateau_start = static_cast<int>(i) - WINDOW + 1;
        }
        else
        {
            consecutive = 0;
            plateau_start = -1;
        }
    }

    if (plateau_start >= 0)
        return static_cast<std::size_t>(plateau_start) + MIN_STRIDE;

    return 0; 
}

int main(int argc, char* argv[])
{
    // std::ofstream csv_file;
    // bool write_csv = false;
    // for (int i = 1; i < argc - 1; ++i)
    // {
    //     if (std::string(argv[i]) == "--csv")
    //     {
    //         csv_file.open(argv[i + 1]);
    //         if (!csv_file)
    //         {
    //             std::cerr << "Error: could not open " << argv[i + 1] << '\n';
    //             return 1;
    //         }
    //         csv_file << "stride,throughput_avg,throughput_min,throughput_max\n";
    //         write_csv = true;
    //     }
    // }

    // const std::size_t num_strides = MAX_STRIDE - MIN_STRIDE + 1;
    // auto* throughputs = new double[num_strides];

    // std::cout << "Cache Line Size Checker\n";
    // std::cout << "Buffer: " << BUFFER_SIZE / (1024 * 1024) << " MB | "
    //           << "Trials: " << NUM_TRIALS << " | "
    //           << "Strides: " << MIN_STRIDE << "-" << MAX_STRIDE << "\n\n";

    // std::cout << std::setw(8)  << "Stride"
    //           << std::setw(16) << "Throughput"
    //           << std::setw(16) << "Best"
    //           << std::setw(16) << "Worst"
    //           << '\n';
    // std::cout << std::setw(8)  << "(bytes)"
    //           << std::setw(16) << "(MB/s avg)"
    //           << std::setw(16) << "(MB/s)"
    //           << std::setw(16) << "(MB/s)"
    //           << '\n';
    // std::cout << std::string(56, '-') << '\n';

    // for (std::size_t stride = MIN_STRIDE; stride <= MAX_STRIDE; ++stride)
    // {
    //     auto [avg_ns, min_ns, max_ns] = benchmark_stride(stride);

    //     double tp_avg = (BUFFER_SIZE * 1000.0) / avg_ns;
    //     double tp_best = (BUFFER_SIZE * 1000.0) / min_ns;
    //     double tp_worst = (BUFFER_SIZE * 1000.0) / max_ns;

    //     throughputs[stride - MIN_STRIDE] = tp_avg;

    //     std::cout << std::fixed << std::setprecision(1);
    //     std::cout << std::setw(8)  << stride
    //               << std::setw(16) << tp_avg
    //               << std::setw(16) << tp_best
    //               << std::setw(16) << tp_worst
    //               << '\n';

    //     if (write_csv)
    //     {
    //         csv_file << std::fixed << std::setprecision(2);
    //         csv_file << stride << ',' << tp_avg << ',' << tp_best << ',' << tp_worst << '\n';
    //     }
    // }

    // std::size_t detected = detect_cache_line(throughputs, num_strides);

    // std::cout << "\n" << std::string(56, '=') << '\n';
    // if (detected > 0)
    // {
    //     std::size_t rounded = 1;
    //     while (rounded < detected) rounded <<= 1;
    //     if (rounded - detected > detected - (rounded >> 1))
    //         rounded >>= 1;

    //     std::cout << "Detected cache line size: " << rounded << " bytes";
    //     if (rounded != detected)
    //         std::cout << "  (plateau started at stride " << detected << ")";
    //     std::cout << '\n';
    // }
    // else
    // {
    //     std::cout << "Could not auto-detect cache line size.\n"
    //               << "Look at the throughput column — it plateaus at the cache line size.\n";
    // }
    // std::cout << std::string(56, '=') << '\n';

    std::ifstream sysfs("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if (sysfs)
    {
       int os_line_size;
       sysfs >> os_line_size;
       std::cout << "OS-reported cache line size: " << os_line_size << " bytes\n";
    }

    // delete[] throughputs;
    return 0;
}
