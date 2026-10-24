// A constructor's list naming the members of an anonymous struct inside
// an anonymous union (ANGLE's DeviceQueueIndex: `union { struct {
// int8_t mFamilyIndex; int8_t mQueueIndex; }; uint16_t mValue; }`): each
// at its own offset, in constant evaluation and at run time; the
// struct's members left out take their default initializers.
#include <stdint.h>
#include <stdio.h>

class Index final {
  public:
    constexpr Index() : mFamily(-1), mQueue(-1) {}
    constexpr Index(uint32_t family) : mFamily((int8_t)family), mQueue(-1) {}
    Index(uint32_t family, uint32_t queue) : mFamily((int8_t)family), mQueue((int8_t)queue) {}
    constexpr uint32_t family() const { return mFamily; }
    constexpr uint32_t queue() const { return mQueue; }
    uint16_t value() const { return mValue; }
  private:
    union {
        struct { int8_t mFamily; int8_t mQueue; };
        uint16_t mValue;
    };
};

struct Defaults {
    Defaults(int a) : first(a) {}
    union {
        struct { int first; int second = 7; };
        long both;
    };
};

static constexpr Index kForeign = Index(~2U);
static_assert(kForeign.family() == ~2U, "family");
static_assert(kForeign.queue() == ~0U, "queue");

int main() {
    Index runtime(3, 5);
    Defaults d(4);
    printf("%x %x %x %u %u %d %d\n", kForeign.family(), kForeign.queue(), runtime.value(), runtime.family(), runtime.queue(), d.first, d.second);
    return 0;
}
