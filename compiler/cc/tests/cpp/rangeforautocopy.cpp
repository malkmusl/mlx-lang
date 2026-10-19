// `for (auto x : range)` of const elements: x is a copy of its own, not
// const (WebCore's CookieStore moves its copies of a const Vector's
// cookies; WTF::move asserts it is given no const).
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Cookie {
    std::string name;
    std::string value;
};

static int changed(const std::vector<Cookie>& cookies)
{
    int count = 0;
    for (auto cookie : cookies) {
        static_assert(!std::is_const_v<decltype(cookie)>);
        cookie.value = "changed";
        Cookie moved = std::move(cookie);
        count += moved.value == "changed";
    }
    return count;
}

int main()
{
    std::vector<Cookie> cookies { { "a", "1" }, { "b", "2" } };
    printf("%d %s\n", changed(cookies), cookies[0].value.c_str());
    const int numbers[] = { 1, 2, 3 };
    int sum = 0;
    for (auto n : numbers) {
        n *= 2;
        sum += n;
    }
    printf("%d\n", sum);
    return 0;
}
