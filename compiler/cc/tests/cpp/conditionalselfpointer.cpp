// `c ? f() : g()` of a class whose object points into itself (WTF's
// Vector with inline capacity, moved by swapping its inline buffer): each
// call's result made at the expression's result, not moved there by its
// bytes (the moved-from buffer then pointed into the call's; WebCore's
// `auto result = c ? layoutComplexText(...) : layoutSimpleText(...)`, a
// GlyphBuffer cached with its vectors pointing at the stack).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

template<typename T> struct BufferBase {
    T* m_buffer { nullptr };
    unsigned m_capacity { 0 };
    unsigned m_size { 0 };
};
template<typename T, size_t N> struct Buffer : BufferBase<T> {
    Buffer() { this->m_buffer = inlineBuffer(); this->m_capacity = N; }
    T* inlineBuffer() { return reinterpret_cast<T*>(m_inline); }
    bool swap(Buffer& o)
    {
        // (Only an inline buffer each: anything else is the bug.)
        if (this->m_buffer != inlineBuffer() || o.m_buffer != o.inlineBuffer())
            return false;
        unsigned char held[N * sizeof(T)];
        memcpy(held, m_inline, sizeof held);
        memcpy(m_inline, o.m_inline, sizeof held);
        memcpy(o.m_inline, held, sizeof held);
        return true;
    }
    alignas(T) unsigned char m_inline[N * sizeof(T)];
};
template<typename T, size_t N> class Vec : private Buffer<T, N> {
public:
    Vec() { }
    Vec(Vec&&);
    ~Vec() { this->m_size = 0; }
    void append(T v) { this->m_buffer[this->m_size++] = v; }
    T first() const { return this->m_buffer[0]; }
    bool own() { return this->m_buffer == this->inlineBuffer(); }
    bool moved { true };
};
template<typename T, size_t N> Vec<T, N>::Vec(Vec&& other)
{
    moved = this->swap(other);
    std::swap(this->m_size, other.m_size);
}

struct Glyphs { Vec<int, 4> fonts; Vec<int, 4> advances; float initial { 0 }; };
struct Result { float width { 0 }; Glyphs glyphs; };

struct Shaper {
    bool complex;
    Result simple(int n) const { Result r; r.glyphs.fonts.append(n); return r; }
    Result hard(int n) const { Result r; r.glyphs.fonts.append(n * 2); return r; }
};

static Vec<int, 4> make(int n) { Vec<int, 4> v; v.append(n); return v; }
struct Label { const char* text; };

int main()
{
    for (int i = 0; i < 2; ++i) {
        Shaper s { i == 1 };
        auto result = s.complex ? s.hard(3) : s.simple(3);
        printf("%d %d %d\n", result.glyphs.fonts.moved, result.glyphs.fonts.own(), result.glyphs.fonts.first());
        Vec<int, 4> v = i ? make(5) : make(6);
        printf("%d %d %d\n", v.moved, v.own(), v.first());
        Result r2 = (s.complex) ? s.hard(Label { "x" }.text[0]) : s.simple(1);
        printf("%d %d %d\n", r2.glyphs.fonts.moved, r2.glyphs.fonts.own(), r2.glyphs.fonts.first());
    }
    return 0;
}
