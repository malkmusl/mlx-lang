// Constant objects holding pointers to globals' bytes (string literals):
// their other members read in constant expressions, their pointers
// relocations to the globals (WebKit's `static constexpr auto
// cekInfoHeader = "..."_s;` sized an array by `cekInfoHeader.length()`
// in PushMessageCrypto).
#include <cstdio>
#include <cstdint>
#include <array>
#include <span>
#include <string>
struct Lit {
    static constexpr Lit fromLiteralUnsafe(const char* s) { return Lit { std::span<const char>(s, std::char_traits<char>::length(s) + 1) }; }
    constexpr size_t length() const { return !chars.empty() ? chars.size() - 1 : 0; }
    constexpr const char* characters() const { return chars.data(); }
    std::span<const char> chars;
};
constexpr Lit operator""_s(const char* c, size_t) { auto r = Lit::fromLiteralUnsafe(c); return r; }
struct Named { const char* name; int value; const char* other; };
constexpr Named table[] = { { "one", 1, nullptr }, { "two", 2, "zwei" } };
int f() {
    static constexpr auto header = "Content-Encoding: aesgcm"_s;
    std::array<uint8_t, header.length() + 1 + 4> info { };
    std::printf("%s %zu\n", header.characters(), header.length());
    return int(info.size());
}
int main() {
    constexpr auto local = "abc"_s;
    static_assert(local.length() == 3);
    constexpr Named n { "named", 7, "x" };
    static_assert(n.value == 7);
    static_assert(table[1].value == 2);
    std::printf("%s %zu %s %d %s\n", local.characters(), local.length(), n.name, n.value, n.other);
    std::printf("%d %s\n", f(), table[1].other);
    return 0;
}
