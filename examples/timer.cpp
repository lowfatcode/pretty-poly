#include <chrono>

static auto start_time = std::chrono::high_resolution_clock::now();

uint64_t get_time_ns() {
    return std::chrono::nanoseconds(std::chrono::high_resolution_clock::now() - start_time).count();
}