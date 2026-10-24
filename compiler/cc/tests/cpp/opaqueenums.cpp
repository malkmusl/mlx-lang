// An opaque enum declaration (`enum class E : T;`) declares E in its own
// scope even when an outer one has a class of the name (WebCore::Style's
// ImageOrientation beside WebCore::ImageOrientation).
#include <cstdio>

namespace WebCore {
struct ImageOrientation { int value = 3; };
namespace Style {
enum class ImageOrientation : bool;
enum class Kind : unsigned char;
struct Uses { ImageOrientation orientation; WebCore::ImageOrientation outer; };
enum class ImageOrientation : bool { FromImage, None };
enum class Kind : unsigned char { A, B };
}
enum class Kind : int { C = 7 };
}

int main()
{
    WebCore::Style::Uses u { WebCore::Style::ImageOrientation::None, { } };
    std::printf("%d %d %d %d\n", (int)u.orientation, u.outer.value, (int)WebCore::Style::Kind::B, (int)WebCore::Kind::C);
    return 0;
}
