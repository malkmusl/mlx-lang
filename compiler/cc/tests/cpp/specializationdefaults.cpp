// An explicit specialization takes the template's default arguments:
// `template <> template <> R Info<float>::closest(Axis, ..., Method)
// const;` (WebCore's ScrollSnapOffsetsInfo) and `template <> int
// scaled<long>(long, int)` are called without their last arguments.
#include <stdio.h>
#include <optional>
#include <utility>
enum class Axis { X, Y };
enum class Method { Closest, Directional };
struct Size { float w; };
struct Point { float x; };
template<typename UnitType> struct Info {
    UnitType base;
    template<typename SizeType, typename PointType>
    std::pair<UnitType, std::optional<unsigned>> closest(Axis, const SizeType& viewport, PointType offset, float velocity, std::optional<UnitType> original = std::nullopt, Method = Method::Closest) const;
};
using FloatInfo = Info<float>;
template <> template <>
std::pair<float, std::optional<unsigned>> FloatInfo::closest(Axis, const Size& viewport, Point offset, float velocity, std::optional<float> original, Method) const;

std::optional<unsigned> pick(const FloatInfo& info)
{
    Size s { 3 };
    Point p { 4 };
    return info.closest(Axis::X, s, p, 0).second;
}

template <> template <>
std::pair<float, std::optional<unsigned>> FloatInfo::closest(Axis, const Size& viewport, Point offset, float velocity, std::optional<float> original, Method method) const
{
    return { base + viewport.w + offset.x + velocity, original ? 9u : (unsigned)method + 1 };
}

template<class T> int scaled(T value, int factor = 3) { return (int)value * factor; }
template<> int scaled<long>(long value, int factor) { return (int)value * factor + 1000; }

int main(int argc, char**)
{
    FloatInfo info { 1 };
    printf("%u %d %d %d\n", *pick(info), scaled(2), scaled(2L), scaled(2L, 5));
    return argc;
}
