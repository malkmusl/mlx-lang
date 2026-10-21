// A static data member declared const in its class and defined constexpr
// outside it, in a header (WebCore's Headroom::FromImage): the definition
// is implicitly inline, one object for every file.
struct Headroom {
    constexpr explicit Headroom(float h) : headroom(h) { }
    float headroom;
    static const Headroom FromImage;
    static const Headroom None;
};
constexpr const Headroom Headroom::FromImage = Headroom { 0 };
constexpr const Headroom Headroom::None = Headroom { 1 };
const Headroom* fromImageInA();
const Headroom* fromImageInB();
