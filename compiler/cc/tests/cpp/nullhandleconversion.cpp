// A class of handles compared with, assigned from and constructed from an
// object whose template conversion operator gives any pointer (the
// conformance suite's NULL_HANDLE): == and != between a handle class and
// it use the class's operators through its constructor, between a
// pointer and it the built-in ones through the conversion.
#include <stdio.h>
#include <stdint.h>
#include <vector>

struct NullHandle
{
    template <typename T>
    operator T *()
    {
        return nullptr;
    }
};
#define NULL_HANDLE \
    NullHandle      \
    {               \
    }

template <int Type>
class Handle
{
public:
    Handle(void) : m_internal(5)
    {
    }
    Handle(NullHandle) : m_internal(0)
    {
    }
    template <typename T>
    Handle(const T *obj) : m_internal(reinterpret_cast<uintptr_t>(obj))
    {
    }
    Handle &operator=(NullHandle)
    {
        m_internal = 0;
        return *this;
    }
    bool operator==(const Handle<Type> &other) const
    {
        return m_internal == other.m_internal;
    }
    bool operator!=(const Handle<Type> &other) const
    {
        return m_internal != other.m_internal;
    }
    uint64_t m_internal;
};

typedef Handle<3> Buffer;
typedef struct Device_s *Device;

static Buffer getBuffer(int n)
{
    Buffer b;
    if (n == 0)
        b = NULL_HANDLE;
    return b;
}

template <typename T>
static int countSet(const std::vector<T> &handles)
{
    int n = 0;
    for (size_t i = 0; i < handles.size(); ++i)
        if (handles[i] != NULL_HANDLE)
            n++;
    return n;
}

int main()
{
    int a = getBuffer(0) != NULL_HANDLE;
    int b = getBuffer(1) != NULL_HANDLE;
    std::vector<Device> devices(3);
    devices[1] = (Device)&a;
    int c = devices[1] != NULL_HANDLE;
    int d = devices[0] != NULL_HANDLE;
    int e = devices[0] == NULL_HANDLE;
    std::vector<Buffer> buffers(2);
    buffers[0] = NULL_HANDLE;
    printf("%d %d %d %d %d %d %d\n", a, b, c, d, e, countSet(devices), countSet(buffers));
    return 0;
}
