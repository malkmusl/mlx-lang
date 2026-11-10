// The translator's test of: see tools/check_lattice_emit.sh (casts2).
#include <array>
#include <string.h>
#include <utility>
namespace casts2 {
typedef unsigned (*Callback)(unsigned);
struct Registry
{
    const void *userData = nullptr;
    const void *callback = nullptr;
    void set(Callback function, const void *data) { callback = reinterpret_cast<const void *>(function); userData = data; }
    unsigned call(unsigned n) const { return reinterpret_cast<Callback>(callback)(n) + *static_cast<const unsigned *>(userData); }
    void *mutableData() const { return const_cast<void *>(userData); }
};
static unsigned twice(unsigned n) { return n * 2; }
static unsigned fill(char *out, const char *text)
{
    char *writable = const_cast<char *>(text);
    unsigned length = (unsigned)strlen(writable);
    memcpy(out, writable, length + 1);
    return length;
}
typedef std::array<unsigned char, 20> Key;
static unsigned sumKey(const Key &key)
{
    unsigned total = 0;
    for (unsigned char byte : key) total += byte;
    return total;
}
static unsigned keyed(const std::array<unsigned char, 20> &key, unsigned scale) { return sumKey(key) * scale; }
struct Owner;
template <typename T, typename... Args>
static unsigned build(T &&first, Args &&...rest) { return first->weigh(std::forward<Args>(rest)...); }
struct Owner
{
    unsigned base = 7;
    unsigned weigh(unsigned a, const unsigned &b) { return base + a * b; }
    unsigned viaTemplate(unsigned a)
    {
        unsigned b = 3;
        return build(this, a, b);
    }
};
unsigned run()
{
    Registry registry;
    unsigned data = 40;
    registry.set(twice, &data);
    unsigned total = registry.call(5);
    *static_cast<unsigned *>(registry.mutableData()) += 2;
    total += registry.call(1);
    char buffer[16];
    total += fill(buffer, "hello");
    total += (unsigned char)buffer[4];
    Key key;
    for (unsigned i = 0; i < key.size(); i++) key[i] = (unsigned char)(i * 3);
    total += keyed(key, 2);
    Owner owner;
    total += owner.viaTemplate(4);
    return total;
}
}
