// A class template instance for a class declared only (a member of
// another class before the class's definition): its members' requires
// clauses are weighed where the members are used, on the defined class.
#include <cstdio>
#include <type_traits>

template<typename T, typename = void> struct HasRef : std::false_type { };
template<typename T> struct HasRef<T, std::void_t<decltype(static_cast<T*>(nullptr)->ref())>> : std::true_type { };

template<typename T> struct Set {
    T* item { nullptr };
    int forEach() const requires HasRef<T>::value { return item ? item->ref() : -1; }
    int size() const { return item ? 1 : 0; }
};

struct Frame;

struct Page {
    Set<Frame> frames;
};

struct Frame {
    int ref() const { return 7; }
};

static_assert(HasRef<Frame>::value);

int main(int argc, char** argv)
{
    Frame frame;
    Page page;
    std::printf("%d %d\n", page.frames.forEach(), page.frames.size());
    page.frames.item = &frame;
    std::printf("%d %d\n", page.frames.forEach(), page.frames.size());
    return 0;
}
