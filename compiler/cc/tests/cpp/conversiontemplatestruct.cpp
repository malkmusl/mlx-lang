// A value whose template conversion operator makes any struct (the
// conformance suite's initVulkanStructure): converted to the struct an
// argument, a return value, a member, a variable and a vector's element
// want.
#include <stdio.h>
#include <string.h>
#include <vector>

struct Info {
    int sType;
    void *pNext;
    int value;
};

template <class T> int typeOf() { return 42; }

struct initB {
    initB(void *next = nullptr) : m_next(next) {}
    template <class S> operator S() {
        S result;
        memset(&result, 0, sizeof(S));
        result.sType = typeOf<S>();
        result.pNext = m_next;
        return result;
    }
    void *m_next;
};

static int take(Info i) { return i.sType + 1; }
static Info give() { return initB(); }

struct Holder {
    Holder() : info(initB()) {}
    Info info;
};

int main() {
    int x = 5;
    printf("%d\n", take(initB()));
    printf("%d\n", give().sType);
    Holder h;
    printf("%d\n", h.info.sType);
    Info chained = initB(&x);
    printf("%d %d\n", chained.sType, chained.pNext == &x);
    std::vector<Info> v;
    v.push_back(initB());
    printf("%d\n", v[0].sType);
    return 0;
}
