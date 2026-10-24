// An explicit specialization of a class template's plain conversion
// function for one instance (`template <> constexpr
// BoxedUint32<IdRefHelper>::operator uint32_t() const`, ANGLE's
// spirv_types.h): the instance's member, the others' the template's.
#include <stdint.h>
#include <stdio.h>

template <typename Helper>
class Boxed {
  public:
    explicit constexpr Boxed(uint32_t value) : mValue{value} {}
    constexpr operator uint32_t() const { return mValue.value; }
    uint32_t twice() const { return mValue.value * 2; }
  private:
    Helper mValue;
};
struct IdHelper { uint32_t value; };
struct LiteralHelper { uint32_t value; };

template <>
inline constexpr Boxed<IdHelper>::operator uint32_t() const { return mValue.value + 100; }
template <>
inline uint32_t Boxed<LiteralHelper>::twice() const { return mValue.value * 3; }

int main() {
    Boxed<IdHelper> id(1);
    Boxed<LiteralHelper> literal(2);
    uint32_t a = id, b = literal;
    printf("%u %u %u %u\n", a, b, id.twice(), literal.twice());
    return 0;
}
