// A class whose filling constructor takes a tag first (WTF's
// Vector(FillWith, size, value)): `Fill<unsigned char>(16, 0)` is no
// call of it, and the message says the class and the arguments' types.
struct FillWith { };
template<typename T> struct Fill {
    explicit Fill(unsigned long size) : size(size) { }
    Fill(FillWith, unsigned long size, const T& value) : size(size), value(value) { }
    template<typename F> requires requires(F f) { f(0ul); } Fill(unsigned long size, const F&) : size(size) { }
    unsigned long size;
    T value { };
};
int main() { Fill<unsigned char> filled(16, 0); return filled.size; }
