// <charconv>, <system_error>, <chrono>, <thread>, <mutex> and
// <condition_variable>: conversions, error codes, durations and clocks,
// threads counting under a lock, a predicate wait and a timed wait.
#include <stdio.h>
#include <string.h>
#include <charconv>
#include <system_error>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
struct Counter { std::mutex m; std::condition_variable cv; int value = 0; bool ready = false; };
void work(Counter *c, int n) {
    for (int i = 0; i < n; i++) { std::lock_guard<std::mutex> g(c->m); c->value++; }
}
int main() {
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof buf, -12345);
    *r.ptr = 0; printf("%s %d\n", buf, (int)(r.ec == std::errc()));
    r = std::to_chars(buf, buf + sizeof buf, 255u, 16); *r.ptr = 0; printf("%s\n", buf);
    r = std::to_chars(buf, buf + sizeof buf, 0.1); *r.ptr = 0; printf("%s\n", buf);
    r = std::to_chars(buf, buf + sizeof buf, 1e21); *r.ptr = 0; printf("%s\n", buf);
    r = std::to_chars(buf, buf + sizeof buf, 2.5, std::chars_format::fixed, 3); *r.ptr = 0; printf("%s\n", buf);
    r = std::to_chars(buf, buf + 2, 123456); printf("%d\n", (int)(r.ec == std::errc::value_too_large));
    int v = 0; const char *t = "4711xyz";
    auto f = std::from_chars(t, t + strlen(t), v); printf("%d %d\n", v, (int)(f.ptr - t));
    unsigned char small = 0; const char *big = "300";
    f = std::from_chars(big, big + 3, small); printf("%d\n", (int)(f.ec == std::errc::result_out_of_range));
    double d = 0; const char *ds = "3.25e2rest";
    f = std::from_chars(ds, ds + strlen(ds), d); printf("%g %d\n", d, (int)(f.ptr - ds));
    std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
    printf("%d %d %s %s\n", (int)(bool)ec, ec.value(), ec.category().name(), ec.message().c_str());
    std::error_code none;
    printf("%d %d\n", (int)(bool)none, (int)(ec == std::errc::no_such_file_or_directory));
    using namespace std::chrono;
    milliseconds ms(1500);
    seconds s = duration_cast<seconds>(ms);
    microseconds us = ms;
    printf("%lld %lld %lld\n", (long long)ms.count(), (long long)s.count(), (long long)us.count());
    auto sum = ms + seconds(2);
    printf("%lld %d %d\n", (long long)sum.count(), (int)(ms < seconds(2)), (int)(minutes(1) == seconds(60)));
    using namespace std::chrono_literals;
    auto lit = 250ms + 1s;
    printf("%lld\n", (long long)lit.count());
    auto t0 = steady_clock::now();
    std::this_thread::sleep_for(milliseconds(20));
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    printf("slept %d\n", (int)(elapsed >= 20 && elapsed < 2000));
    printf("epoch %d\n", (int)(system_clock::to_time_t(system_clock::now()) > 1600000000));
    Counter c;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) threads.push_back(std::thread(work, &c, 1000));
    for (auto &th : threads) th.join();
    printf("value %d\n", c.value);
    std::thread waiter([&c] { std::unique_lock<std::mutex> lock(c.m); Counter *p = &c; c.cv.wait(lock, [p] { return p->ready; }); c.value = -1; });
    { std::lock_guard<std::mutex> g(c.m); c.ready = true; }
    c.cv.notify_all();
    waiter.join();
    printf("after %d %d\n", c.value, (int)(std::this_thread::get_id() != waiter.get_id()));
    std::unique_lock<std::mutex> lock(c.m);
    auto st = c.cv.wait_for(lock, milliseconds(10));
    printf("timeout %d hw %d\n", (int)(st == std::cv_status::timeout), (int)(std::thread::hardware_concurrency() > 0));
    return 0;
}
