// An instance whose argument is an instance named only over a class
// declared but not defined yet is named only too: WebCore's SharedWorker
// declares `create(..., std::optional<Variant<String, WorkerOptions>>&&)`
// before WorkerOptions is defined, and the optional made the Variant,
// whose storage takes sizeof of each alternative (workers-9).
#include <optional>
#include <stdio.h>
#include <utility>

template<class... T> struct Variant {
    static constexpr unsigned long size = (sizeof(T) + ...);
    char storage[size];
    int index = 0;
};
template<class T> struct Box { T value; };

struct Options;
struct Worker {
    static int create(std::optional<Variant<int, Options>> &&options);
    static int count(Box<Box<Variant<Options, char>>> *boxes) { return boxes ? 1 : 0; }
};
struct Options { int x; long y; };

int Worker::create(std::optional<Variant<int, Options>> &&options) { return options ? (int)sizeof(*options) : -1; }

int main(int argc, char **)
{
    std::optional<Variant<int, Options>> none;
    std::optional<Variant<int, Options>> some = Variant<int, Options> {};
    printf("%d %d %zu\n", Worker::create(std::move(none)), Worker::create(std::move(some)), sizeof(Variant<int, Options>));
    Box<Box<Variant<Options, char>>> box {};
    printf("%d %zu\n", Worker::count(&box), sizeof box);
    return argc;
}
