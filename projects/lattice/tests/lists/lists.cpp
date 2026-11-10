// The translator's test of: see tools/check_lattice_emit.sh (lists).
#include <initializer_list>
#include <utility>
#include <vector>
#include <map>
namespace lists {
struct Entry { unsigned id; const char *name; };
static unsigned sumOf(std::initializer_list<Entry> entries)
{
    unsigned total = 0;
    for (const Entry &entry : entries) total += entry.id * 3 + (unsigned char)entry.name[0];
    return total;
}
static unsigned firstOf(const Entry *entries, unsigned count)
{
    unsigned total = 0;
    for (unsigned i = 0; i < count; i++) total += entries[i].id;
    return total;
}
static unsigned rows(const unsigned char table[][4], unsigned count)
{
    unsigned total = 0;
    for (unsigned i = 0; i < count; i++) total += table[i][0] + table[i][3] * 2;
    return total;
}
template <typename T, unsigned N>
static unsigned lengthOf(const T (&text)[N]) { return N * 10 + (unsigned char)text[0]; }
struct Named
{
    template <unsigned N>
    Named(unsigned id, const char (&text)[N]) : mId(id), mName(text), mLength(N) {}
    unsigned mId;
    const char *mName;
    unsigned mLength;
};
unsigned run()
{
    unsigned total = sumOf({{1, "one"}, {2, "two"}, {3, "three"}});
    Entry local[3] = {{10, "a"}, {20, "b"}, {30, "c"}};
    total += firstOf(local, 3) + firstOf(&local[1], 2);
    std::vector<std::pair<unsigned, const char *>> pairs = {{7, "seven"}, {8, "eight"}};
    for (const auto &pair : pairs) total += pair.first + (unsigned char)pair.second[1];
    std::map<unsigned, unsigned> squares = {{2, 4}, {3, 9}, {4, 16}};
    total += squares[3] + (unsigned)squares.size();
    unsigned char grid[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    total += rows(grid, 3);
    total += lengthOf("hello") + lengthOf("x");
    Named named(5, "label");
    total += named.mId + named.mLength + (unsigned char)named.mName[2];
    return total;
}
}
