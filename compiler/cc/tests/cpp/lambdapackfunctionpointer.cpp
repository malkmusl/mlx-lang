// A captureless lambda taking a template's pack (`[](const void *buf,
// Args... args) { ... }` of Skia's GrGLFunction) converted to a pointer
// to a function: its invoker calls the call operator with its
// parameters, the pack's elements among them, by names of their own.
#include <cstdio>
#include <cstring>
#include <utility>

template<typename T> class Function;

template<typename R, typename... Args> class Function<R(Args...)> {
public:
    using Fn = R(Args...);
    Function(Fn* pointer)
    {
        memcpy(m_buffer, &pointer, sizeof(pointer));
        m_call = [](const void* buffer, Args... args) {
            return (*(Fn* const*)buffer)(std::forward<Args>(args)...);
        };
    }
    R operator()(Args... args) const { return m_call(m_buffer, std::forward<Args>(args)...); }

private:
    R (*m_call)(const void*, Args...) = nullptr;
    char m_buffer[16];
};

static unsigned char isEnabled(unsigned int capability) { return capability == 3; }
static int sum(int a, long b, short) { return a + (int)b; }

int main()
{
    Function<unsigned char(unsigned int)> enabled(isEnabled);
    Function<int(int, long, short)> add(sum);
    printf("%d %d %d\n", enabled(3), enabled(4), add(5, 6, 7));
    return 0;
}
