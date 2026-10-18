// Conditional expressions of Skia's: a pointer and nullptr into `auto`
// then tested; a std::function tested in `&&`; a function pointer and a
// captureless lambda (`proc ? proc : [](void *) {}`).
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

struct Task {
    int fId;
    int proxy() const { return fId; }
};

using ReleaseProc = void (*)(void *);
static int released = 0;
static void release(void *) { ++released; }

static bool pick(const std::vector<std::unique_ptr<Task>> &tasks, const std::function<bool(int)> &refs) {
    auto current = (!tasks.empty()) ? tasks.back().get() : nullptr;
    if (current && refs && refs(current->proxy())) return true;
    return false;
}

int main(int argc, char **argv) {
    std::vector<std::unique_ptr<Task>> tasks;
    tasks.push_back(std::make_unique<Task>(Task{4}));
    std::function<bool(int)> refs = [](int id) { return id == 4; };
    std::function<bool(int)> none;
    ReleaseProc proc = nullptr;
    proc = proc ? proc : [](void *) {};
    proc(nullptr);
    ReleaseProc real = release;
    real = real ? real : [](void *) {};
    real(nullptr);
    printf("%d %d %d %d\n", pick(tasks, refs), pick(tasks, none), pick({}, refs), released);
    return argc;
}
