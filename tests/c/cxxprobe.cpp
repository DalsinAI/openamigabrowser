#include <atomic>
#include <cstdio>
#include <expected>
#include <mutex>
#include <pthread.h>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <cmath>
#include <limits>

static std::atomic<int> counter{0};
static std::mutex m;
static void* worker(void*) { for (int i = 0; i < 1000; ++i) { std::lock_guard l(m); counter++; } return nullptr; }

static std::expected<int, std::string> half(int x) { if (x & 1) return std::unexpected("odd"); return x / 2; }

int main()
{
    std::printf("CXXPROBE_START\n");
    std::vector<int> v{1, 2, 3, 4};
    std::span<int> s(v);
    std::printf("cxx23 span sum=%d expected=%d/%s\n", s[0] + s[3], *half(8), half(3).error().c_str());
    pthread_t t[4];
    for (auto& th : t) pthread_create(&th, nullptr, worker, nullptr);
    for (auto& th : t) pthread_join(th, nullptr);
    std::printf("pthreads counter=%d (want 4000)\n", counter.load());
    try { throw std::runtime_error("boom"); } catch (const std::exception& e) { std::printf("exception caught: %s\n", e.what()); }
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::printf("fp sqrt2=%.6f nan=%d sin1=%.6f\n", std::sqrt(2.0), std::isnan(nan), std::sin(1.0));
    std::atomic<long long> a64{1}; a64.fetch_add(41);
    std::printf("atomic64=%lld\n", a64.load());
    std::printf("CXXPROBE_OK\n");
    return 0;
}
