// std::call_once: threads that come while another runs the function wait
// for it to finish (they see what it made).
#include <mutex>
#include <thread>
#include <stdio.h>
#include <unistd.h>
static std::once_flag flag;
static int value;
static void work(int* seen) { std::call_once(flag, [] { usleep(20000); value = 42; }); *seen = value; }
int main() {
    int a = 0, b = 0, c = 0;
    std::thread t1(work, &a), t2(work, &b), t3(work, &c);
    t1.join(); t2.join(); t3.join();
    printf("%d %d %d\n", a, b, c);
    std::call_once(flag, [] { value = 7; });
    printf("%d\n", value);
    return 0;
}
