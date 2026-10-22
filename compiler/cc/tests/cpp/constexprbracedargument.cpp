// A constexpr function template's instance first named in a braced
// argument (`broadcast({ Variant { InPlaceIndex<std::to_underlying(e)>,
// data } })`, WebCore's generated FrameTreeSyncClient.cpp) computes the
// template argument: its body is read when it is called.
#include <cstdint>
#include <cstdio>
#include <utility>

enum class SyncDataType : uint8_t { A = 0, B = 1, C = 2 };

template<size_t I> struct in_place_index_t { };
template<size_t I> constexpr in_place_index_t<I> InPlaceIndex { };

template<typename... Ts> struct Variant {
    size_t index;
    template<size_t I, typename U> constexpr Variant(in_place_index_t<I>, U&&) : index(I) { }
};

using SyncVariant = Variant<bool, int, double>;

struct Client {
    void send(const bool& data);
    void broadcast(SyncVariant&& v) { printf("%zu\n", v.index); }
};

void Client::send(const bool& data)
{
    broadcast({ SyncVariant { InPlaceIndex<std::to_underlying(SyncDataType::B)>, data } });
}

int main()
{
    Client c;
    c.send(true);
    return 0;
}
