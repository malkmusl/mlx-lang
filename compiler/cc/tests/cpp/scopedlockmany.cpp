// std::scoped_lock of several mutexes (deduced: JSC's BufferMemoryHandle
// locks two), std::lock and std::try_lock: locked without deadlock in
// either order.
#include <stdio.h>
#include <mutex>
#include <thread>
std::mutex a, b;
int counter = 0;
void work(bool flip)
{
    for (int i = 0; i < 20000; ++i) {
        if (flip) {
            std::scoped_lock l(b, a);
            ++counter;
        } else {
            std::scoped_lock l(a, b);
            ++counter;
        }
    }
}
int main()
{
    { std::scoped_lock one(a); ++counter; }
    std::thread t1(work, false), t2(work, true);
    t1.join();
    t2.join();
    std::recursive_mutex r;
    { std::scoped_lock three(a, b, r); ++counter; }
    printf("%d %d\n", counter, std::try_lock(a, b));
    a.unlock();
    b.unlock();
    return 0;
}
