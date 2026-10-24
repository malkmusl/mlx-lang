// Class template argument deduction by the guides written, beside a
// constructor template whose own pack names none of the class's
// parameters (GCGLSpanTuple { firsts, counts, size } of WebGL's multi-draw).
#include <stdio.h>
#include <stddef.h>
#include <tuple>
#include <vector>
template <typename... Types>
struct SpanTuple {
    SpanTuple(Types*... pointers, size_t size) : size { size }, data { pointers... } { }
    template <typename... Vectors>
    SpanTuple(const std::vector<Vectors>&... vectors) : size(vectors.size()...), data { vectors.data()... } { }
    template <unsigned I> auto at() const { return std::get<I>(data); }
    const size_t size;
    std::tuple<Types*...> data;
};
template <typename T0, typename T1> SpanTuple(T0*, T1*, size_t) -> SpanTuple<T0, T1>;
template <typename T0> SpanTuple(const std::vector<T0>&) -> SpanTuple<const T0>;
template <typename T0, typename T1, typename T2> SpanTuple(T0*, T1*, T2*, size_t) -> SpanTuple<T0, T1, T2>;
static int sum(SpanTuple<const int, const int> t) { return t.at<0>()[0] + t.at<1>()[t.size - 1]; }
int main() {
    int a[2] = { 1, 2 };
    int b[2] = { 30, 40 };
    printf("%d\n", sum(SpanTuple { (const int *)a, (const int *)b, (size_t)2 }));
    SpanTuple three { a, b, a, 2 };
    printf("%zu %d\n", three.size, three.at<2>()[1]);
}
