// A member alias of a scoped enum names it before `::` in the body of a
// member defined outside the class, where an enum of the same name is
// declared around (WebCore's EventTrackingRegions::eventName: its
// `using EventType = EventTrackingRegionsEventType;` and the namespace's
// `enum class EventType : uint16_t;`).
#include <cstdio>

namespace N {

enum class TrackedType : unsigned char { Gesture, Mouse, Wheel };

struct Regions {
    using EventType = TrackedType;
    static const char* name(EventType);
    static EventType first();
};

enum class EventType : unsigned short;

const char* Regions::name(EventType type)
{
    switch (type) {
    case EventType::Gesture:
        return "gesture";
    case EventType::Mouse:
        return "mouse";
    case EventType::Wheel:
        return "wheel";
    }
    return "?";
}

Regions::EventType Regions::first()
{
    return EventType::Gesture;
}

enum class EventType : unsigned short { Click = 7, Key };

}

int main()
{
    printf("%s %s %s\n", N::Regions::name(N::TrackedType::Gesture), N::Regions::name(N::TrackedType::Mouse), N::Regions::name(N::TrackedType::Wheel));
    printf("first: %s\n", N::Regions::name(N::Regions::first()));
    printf("namespace's: %d\n", static_cast<int>(N::EventType::Key));
    return 0;
}
