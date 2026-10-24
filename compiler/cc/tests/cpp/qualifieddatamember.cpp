// A data member named through a base's typedef (`base::s`, WTF's
// Expected::swap) in a function body read while a generic lambda's
// result type is deduced (mpark::variant's generic_assign visiting an
// Expected): no missing name failing the lambda's call ("no instance of
// the function template 'invoke'", Variant.h:1340).
#include <stdio.h>
#include <utility>

namespace det {
template<class T> struct base {
    T s;
    base() : s() { }
    base(T v) : s(v) { }
};
}
template<class T> struct expected : det::base<T> {
    typedef det::base<T> base;
    typedef expected<T> type;
    expected() = default;
    expected(T v) : base(v) { }
    expected(expected&&) = default;
    expected& operator=(expected&& e) { type(std::move(e)).swap(*this); return *this; }
    void swap(expected& o) { std::swap(base::s, o.s); }
};

namespace mp {
template <typename F, typename... Args>
constexpr auto inv(F &&f, Args &&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...))) -> decltype(std::forward<F>(f)(std::forward<Args>(args)...)) { return std::forward<F>(f)(std::forward<Args>(args)...); }
template<class T> struct base {
    T value;
    template<class That> void generic_assign(That&& that)
    {
        inv([this](auto& mine, auto&& theirs) { mine = std::forward<decltype(theirs)>(theirs); }, value, std::forward<That>(that).value);
    }
};
}

int main(int argc, char **argv)
{
    mp::base<expected<int>> a { expected<int>(1) }, b { expected<int>(5) };
    a.generic_assign(std::move(b));
    printf("%d\n", a.value.s);
    return argc;
}
