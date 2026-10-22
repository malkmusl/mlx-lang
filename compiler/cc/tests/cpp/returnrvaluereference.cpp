// `return params;` of an rvalue reference parameter, for a result of
// another class made from it: moved (C++20's implicit move; WebCore's
// FileSystemWritableFileStreamSink returns a `WriteParams&&` lambda
// parameter as an ExceptionOr<WriteParams>).
#include <cstdio>
#include <memory>
#include <utility>

struct Params {
    std::unique_ptr<int> data;
};

template<typename T> struct Result {
    Result(T&& value) : value(std::move(value)), ok(true) { }
    Result(int error) : ok(false), error(error) { }
    T value;
    bool ok;
    int error { 0 };
};

static Result<Params> check(Params&& params)
{
    if (!params.data)
        return 22;
    return params;
}

int main()
{
    auto good = check(Params { std::make_unique<int>(5) });
    auto bad = check(Params { });
    printf("%d %d %d %d\n", good.ok, *good.value.data, bad.ok, bad.error);
    auto viaLambda = [](Params&& params) -> Result<Params> {
        return params;
    }(Params { std::make_unique<int>(9) });
    printf("%d\n", *viaLambda.value.data);
    return 0;
}
