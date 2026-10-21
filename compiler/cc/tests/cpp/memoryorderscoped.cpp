// C++20's std::memory_order is scoped (`std::memory_order::relaxed` in
// WebCore's WebGLExtension.h: "no such name in the namespace"), the old
// names its values, as defaults and as arguments.
#include <stdio.h>
#include <atomic>

std::atomic<int *> pointer;
int load(std::memory_order order = std::memory_order_seq_cst) { return *pointer.load(order); }

int main(int argc, char **argv)
{
    int value = 7;
    pointer.store(&value, std::memory_order::release);
    std::atomic<int> count { 3 };
    count.fetch_add(2, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order::seq_cst);
    printf("%d %d %d %d\n", *pointer.load(std::memory_order::relaxed), load(), count.load(std::memory_order::acquire), std::memory_order_acquire == std::memory_order::acquire);
    return argc;
}
