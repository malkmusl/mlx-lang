// WTF forms: variable template partials over is_same_v, members of typedef'd instances, chrono sums, operator templates chosen late
#include <stdio.h>
#include <type_traits>
#include <ctime>
#include <stdint.h>
#include <array>
#include <chrono>
#include <filesystem>

namespace p_vt1 {
struct CA { static std::time_t to_time_t(int) { return 7; } };
struct CB { };
struct FA { using clock = CA; };
struct FB { using clock = CB; };
template<typename, typename = void> inline constexpr bool HasT = false;
template<typename C> inline constexpr bool HasT<C, std::void_t<std::enable_if_t<std::is_same_v<std::time_t, decltype(C::to_time_t(0))>>>> = true;
template <typename F> int f(F) { if constexpr (HasT<typename F::clock>) return 1; else return 2; }
void run() { printf("%d %d\n", f(FA{}), f(FB{})); }
}

namespace p_vt3 {
struct CA { static std::time_t to_time_t(int) { return 7; } };
struct CB { };
struct FA { using clock = CA; };
struct FB { using clock = CB; };
template<typename, typename = void> inline constexpr bool HasT = false;
template<typename C> inline constexpr bool HasT<C, std::enable_if_t<std::is_same_v<C, CA>>> = true;
template <typename F> int f(F) { if constexpr (HasT<typename F::clock>) return 1; else return 2; }
void run() { printf("%d %d\n", f(FA{}), f(FB{})); }
}

namespace p_dg1 {
class SHA { public: static constexpr size_t hashSize = 20; typedef std::array<uint8_t, hashSize> Digest; void computeHash(Digest&); uint32_t m_hash[5] = {1, 2, 3, 4, 5}; };
void SHA::computeHash(Digest& digest) {
    for (size_t i = 0; i < 5; ++i) { uint32_t hashValue = m_hash[i]; for (int j = 0; j < 4; ++j) { digest[4 * i + (3 - j)] = hashValue & 0xFF; hashValue >>= 8; } }
}
void run() { SHA s; SHA::Digest d; s.computeHash(d); printf("%d %d\n", d[3], d[19]); }
}

namespace p_dg2 {
typedef std::array<uint8_t, 20> D;
void f(D& d) { d[3] = 7; }
void run() { D d; f(d); printf("%d\n", d[3]); }
}

namespace p_ch1 {
void run() {
  auto ft = std::filesystem::file_time_type();
  auto d = ft - decltype(ft)::clock::now();
  auto tp = d + std::chrono::system_clock::now();
  auto c = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
  printf("%d\n", std::chrono::system_clock::to_time_t(c) != 1);
}
}

namespace p_fs1 {
namespace W {
template<typename, typename = void> inline constexpr bool HasToTimeT = false;
template<typename ClockType> inline constexpr bool HasToTimeT<ClockType, std::void_t<
    std::enable_if_t<std::is_same_v<std::time_t, decltype(ClockType::to_time_t(std::filesystem::file_time_type()))>>
>> = true;
template <typename FileTimeType>
typename std::time_t toTimeT(FileTimeType fileTime)
{
    if constexpr (HasToTimeT<typename FileTimeType::clock>)
        return decltype(fileTime)::clock::to_time_t(fileTime);
    else
        return std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime - decltype(fileTime)::clock::now() + std::chrono::system_clock::now()));
}
long get(std::filesystem::file_time_type t) { return (long)toTimeT(t); }
}
void run() { printf("%d\n", W::get(std::filesystem::file_time_type()) != 12345); }
}

int main() {
    p_vt1::run();
    p_vt3::run();
    p_dg1::run();
    p_dg2::run();
    p_ch1::run();
    p_fs1::run();
    return 0;
}
