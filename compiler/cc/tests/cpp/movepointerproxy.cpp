// An owning pointer moved through a proxy struct, as the conformance
// suite's de::MovePtr does (an auto_ptr_ref): a temporary converted to the
// proxy by its conversion operator, then taken by the operator= or the
// constructor that takes the proxy by value. The object is deleted once:
// a bitwise copy of the temporary would delete it twice.
#include <stdio.h>

template <typename T>
struct Data
{
    explicit Data(T *p) : ptr(p)
    {
    }
    T *ptr;
};

template <typename T>
class MovePtr
{
public:
    MovePtr(void) : m_ptr(nullptr)
    {
    }
    explicit MovePtr(T *ptr) : m_ptr(ptr)
    {
    }
    MovePtr(MovePtr<T> &other) : m_ptr(other.release())
    {
    }
    MovePtr(Data<T> data) : m_ptr(data.ptr)
    {
    }
    ~MovePtr(void)
    {
        delete m_ptr;
    }
    MovePtr &operator=(MovePtr<T> &other)
    {
        reset(other.release());
        return *this;
    }
    MovePtr &operator=(Data<T> data)
    {
        reset(data.ptr);
        return *this;
    }
    operator Data<T>(void)
    {
        return Data<T>(release());
    }
    T *release(void)
    {
        T *p  = m_ptr;
        m_ptr = nullptr;
        return p;
    }
    void reset(T *p)
    {
        delete m_ptr;
        m_ptr = p;
    }
    T *operator->(void) const
    {
        return m_ptr;
    }

private:
    T *m_ptr;
};

struct Thing
{
    Thing(int v) : value(v)
    {
        printf("make %d\n", v);
    }
    ~Thing(void)
    {
        printf("free %d\n", value);
    }
    int value;
};

static MovePtr<Thing> make(int v)
{
    return MovePtr<Thing>(new Thing(v));
}

static int take(MovePtr<Thing> p)
{
    return p->value;
}

int main()
{
    MovePtr<Thing> a = make(1);
    MovePtr<Thing> b(make(2));
    MovePtr<Thing> c;
    c = make(3);
    c = make(4);
    int t = take(make(5));
    printf("%d %d %d %d\n", a->value, b->value, c->value, t);
    return 0;
}
