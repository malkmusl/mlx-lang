// A class deleting its copy constructor and assignment, with templates
// taking an rvalue of any of its instances (WTF's Bag): an rvalue of the
// class itself is moved by the templates.
#include <stdio.h>
#include <utility>
struct Raw { };
template<typename T, typename P = Raw>
class Bag final {
    Bag(const Bag&) = delete;
    Bag& operator=(const Bag&) = delete;
public:
    Bag() = default;
    template<typename U> Bag(Bag<T, U>&& other) : m_head(other.m_head) { other.m_head = 0; }
    template<typename U> Bag& operator=(Bag<T, U>&& other) { m_head = other.m_head; other.m_head = 0; return *this; }
    int m_head { 0 };
};
struct Holder {
    Bag<int> m_a;
    void adopt(Holder& other) { m_a = std::move(other.m_a); }
};
int main() { Holder a, b; b.m_a.m_head = 42; a.adopt(b); Bag<int> c(std::move(a.m_a)); printf("%d %d %d\n", a.m_a.m_head, b.m_a.m_head, c.m_head); return 0; }
