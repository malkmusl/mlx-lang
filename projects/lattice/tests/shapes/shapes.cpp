// The translator's test of: see tools/check_lattice_emit.sh (shapes).
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
// (A class outside every namespace: its name is written as it is.)
struct label
{
    unsigned count;
    unsigned twice() const { return count * 2; }
};
namespace shapes {
// Operators outside a class, a class copied by its bytes assigned a
// value through its converting constructor.
template <typename T> class Checked
{
  public:
    constexpr Checked() = default;
    template <typename Src> constexpr Checked(Src value) : mValue(static_cast<T>(value)), mValid(true) {}
    bool valid() const { return mValid; }
    T value() const { return mValue; }

  private:
    T mValue = 0;
    bool mValid = false;
};
template <typename T> Checked<T> operator+(Checked<T> a, Checked<T> b) { return Checked<T>(a.value() + b.value()); }
template <typename T> Checked<T> operator/(Checked<T> a, Checked<T> b) { return b.value() == 0 ? Checked<T>() : Checked<T>(a.value() / b.value()); }
static unsigned blocks(unsigned width, unsigned block, unsigned least)
{
    Checked<unsigned> checkedWidth(width);
    Checked<unsigned> checkedBlock(block);
    Checked<unsigned> count = (checkedWidth + checkedBlock) / checkedBlock;
    if (count.valid() && count.value() < least)
    {
        count = least;
    }
    return count.valid() ? count.value() : 99;
}
// What one side of `&&`, `||` or `?:` makes runs only when C++ runs it.
static unsigned guarded(unsigned divisor)
{
    unsigned result = 0;
    if (divisor != 0 && Checked<unsigned>(100 / divisor).value() > 3)
    {
        result += 1;
    }
    if (divisor == 0 || Checked<unsigned>(100 / divisor).value() > 30)
    {
        result += 2;
    }
    return result;
}
// A parameter named as a class the unit writes.
static unsigned labelled(unsigned label)
{
    struct label made;
    made.count = label;
    return made.twice() + label;
}
// The address of one of two things: strings, arrays, variables.
static const int kDefault[4] = {1, 2, 3, 4};
static const int kOpaque[4] = {5, 6, 7, 8};
static unsigned chosen(bool opaque, bool append)
{
    const char *mode = append ? "ab" : "wb";
    const int *table = opaque ? kOpaque : kDefault;
    return (unsigned)mode[0] + (unsigned)table[2];
}
static void push(std::vector<const char *> *names, const char *const &name) { names->push_back(name); }
static unsigned pushed(bool hasFirst)
{
    std::vector<const char *> names;
    const char *first = "first";
    const char *other = "another";
    push(&names, hasFirst ? first : other);
    return (unsigned)names[0][0];
}
// One object's address indexed, an array of arrays as its bytes.
struct Pixel
{
    unsigned char R, G, B, A;
};
static unsigned channels(const Pixel &pixel)
{
    unsigned sum = 0;
    for (int ch = 0; ch < 3; ch++)
    {
        sum += (&pixel.R)[ch] * (unsigned)(ch + 1);
    }
    return sum;
}
static unsigned total(const unsigned char *bytes, unsigned count)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < count; i++)
    {
        sum += bytes[i];
    }
    return sum;
}
static unsigned grid()
{
    unsigned char values[4][4];
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            values[y][x] = (unsigned char)(y * 4 + x);
        }
    }
    return total(reinterpret_cast<unsigned char *>(values), 16);
}
// const dropped below the first pointer.
static unsigned names(const char *const *list, unsigned count)
{
    char **writable = const_cast<char **>(list);
    void *raw = reinterpret_cast<void *>(writable);
    char **again = static_cast<char **>(raw);
    unsigned sum = 0;
    for (unsigned i = 0; i < count; i++)
    {
        sum += (unsigned)again[i][0];
    }
    return sum;
}
// The library's math of an integer.
static unsigned bits(uint64_t precision) { return (unsigned)std::log2(precision); }
// A function pointer that takes a function pointer.
typedef void (*Write)(const unsigned char *source, unsigned char *dest);
typedef void (*Convert)(const Pixel &pixel, Write write, unsigned char *dest);
static void writeRed(const unsigned char *source, unsigned char *dest) { dest[0] = source[0]; }
static void convert(const Pixel &pixel, Write write, unsigned char *dest) { write(&pixel.R, dest); }
static unsigned converted(const Pixel &pixel)
{
    Convert function = convert;
    Write write = writeRed;
    unsigned char out[2] = {0, 0};
    function(pixel, write, out);
    return out[0];
}
// A temporary moved into an element, an iterator made of a pointer
// member in a const method, a vector moved out of a pointer.
template <class Iter> struct WrapIter
{
    Iter it;
    WrapIter(const Iter &iter) : it(iter) {}
    WrapIter &operator++()
    {
        ++it;
        return *this;
    }
    auto &operator*() const { return *it; }
};
template <class T, size_t N> struct Fast
{
    T *mData = mFixed.data();
    size_t mSize = 0;
    std::array<T, N> mFixed;
    WrapIter<const T *> begin() const { return WrapIter<const T *>(mData); }
    template <typename... Args> void emplace_back(Args &&...args) { mData[mSize++] = std::move(T(std::forward<Args>(args)...)); }
};
struct Sub
{
    int a;
    int b;
};
static unsigned moved(std::vector<uint32_t> *out)
{
    std::vector<uint32_t> taken = std::move(*out);
    return (unsigned)taken.size();
}
static unsigned fast()
{
    Fast<int, 4> numbers;
    int seven = 7;
    numbers.emplace_back(seven);
    numbers.emplace_back(5);
    Fast<Sub, 2> records;
    Sub sub{3, 4};
    records.emplace_back(sub);
    std::vector<uint32_t> list;
    list.push_back(1);
    list.push_back(2);
    unsigned count = moved(&list);
    auto it = numbers.begin();
    ++it;
    return count * 1000 + (unsigned)*it * 100 + (unsigned)records.mData[0].b * 10 + (unsigned)numbers.mData[0];
}
// A base class's part returned by reference, a constant pointer taken by
// reference, a number wider than an int where an int is taken, a pointer
// made of an integer tested and read, a signed char as a plain one, a
// call through a variable of a function pointer type.
struct Sink
{
    int n = 4;
};
struct Tag
{
    int t = 1;
};
struct Wide : Tag, Sink
{
    int m = 2;
};
struct Log
{
    Wide mWide;
    Sink &sink() { return mWide; }
};
static const char *const kMessage = "seven!!";
static unsigned length(const char *const &text)
{
    unsigned n = 0;
    while (text[n] != 0)
    {
        n++;
    }
    return n;
}
static int index(int i) { return i; }
typedef unsigned (*Handler)(unsigned);
static unsigned twice(unsigned n) { return n * 2; }
static unsigned through(uintptr_t raw, unsigned n)
{
    if (reinterpret_cast<void *>(raw) == nullptr)
    {
        return 0;
    }
    if (!reinterpret_cast<unsigned char *>(raw))
    {
        return 1;
    }
    return reinterpret_cast<const unsigned *>(raw)[0] + n;
}
static signed char narrow(signed char c) { return c; }
static unsigned text(signed char c)
{
    char plain = static_cast<char>(narrow(c));
    return (unsigned char)plain;
}
static unsigned more()
{
    unsigned sum = length(kMessage);
    sum += (unsigned)index(0xFFFFFFFF) + 10;
    unsigned value = 30;
    sum += through(reinterpret_cast<uintptr_t>(&value), 5);
    sum += through(0, 5);
    sum += text(-2);
    Log log;
    Sink &out = log.sink();
    out.n += 3;
    sum += (unsigned)log.mWide.n * 1000;
    Handler handler = twice;
    sum += handler(3);
    return sum;
}
// A constant made when the program starts and one made at its first use
// are variables; `*rows` of an array of arrays is its first row; a
// pointer to an array is stepped over the arrays.
static unsigned calls = 0;
static bool answer()
{
    calls++;
    return true;
}
static unsigned count()
{
    calls++;
    return 40;
}
static const unsigned kStart = count();
static bool once()
{
    static const bool cached = answer();
    return cached;
}
struct Image
{
    int id;
};
static unsigned firstOf(const unsigned char *row) { return row[0] + row[3]; }
static unsigned rows()
{
    unsigned char pixels[3][4];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            pixels[i][j] = (unsigned char)(i * 10 + j);
        }
    }
    unsigned sum = firstOf(*pixels);
    Image images[2][3];
    int next = 0;
    for (auto &line : images)
    {
        for (auto &image : line)
        {
            image.id = next++;
        }
    }
    Image(*line)[3] = images;
    ++line;
    sum += (unsigned)(*line)[1].id * 100;
    for (Image(*it)[3] = images; it != images + 2; ++it)
    {
        sum += (unsigned)(*it)[2].id;
    }
    return sum;
}
static unsigned made()
{
    unsigned sum = kStart;
    sum += once() ? 1 : 0;
    sum += once() ? 1 : 0;
    return (sum * 10 + calls) * 1000 + rows();
}
unsigned run()
{
    unsigned sum = more() + made();
    sum += blocks(10, 4, 2);
    sum += blocks(1, 4, 5);
    sum += blocks(8, 0, 1);
    const char *const words[3] = {"one", "three", "seven"};
    sum += guarded(0) + guarded(5) * 10 + guarded(2) * 100;
    sum += labelled(11);
    sum += chosen(false, true);
    sum += chosen(true, false);
    sum += pushed(true);
    sum += pushed(false);
    Pixel pixel{10, 20, 30, 40};
    sum += channels(pixel);
    sum += grid();
    sum += names(words, 3);
    sum += bits(4096);
    sum += converted(pixel);
    sum += fast();
    return sum;
}
}
