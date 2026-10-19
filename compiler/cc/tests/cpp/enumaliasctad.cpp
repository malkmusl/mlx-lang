// Enumerators named through an alias of the enum (`using Activity =
// RunLoop::Activity;` in WebCore's RunLoopObserver) in a braced list a
// class template's arguments are deduced from (`static constexpr
// OptionSet defaultActivities = { Activity::BeforeWaiting,
// Activity::Exit };`): no failing name of a type, which made the
// initializer_list guide fail ("no instance of the function template
// '__mlxcc_guide_OptionSet'").
#include <stdio.h>
#include <initializer_list>
#include <optional>
#include <type_traits>

enum class Tag { None, Atomic };
template<typename E, Tag = Tag::None> class Set;
template<typename E, Tag concurrency> class Set {
public:
    using StorageType = std::make_unsigned_t<std::underlying_type_t<E>>;
    constexpr Set() = default;
    constexpr Set(E e) : m_storage(static_cast<StorageType>(e)) { }
    constexpr Set(std::initializer_list<E> list) { for (auto& o : list) m_storage |= static_cast<StorageType>(o); }
    constexpr Set(std::optional<E> optional) : m_storage(optional ? static_cast<StorageType>(*optional) : 0) { }
    constexpr StorageType toRaw() const { return m_storage; }
private:
    StorageType m_storage { 0 };
};

struct RL { enum class Activity : unsigned { Entry = 1, BeforeWaiting = 2, Exit = 4 }; };
class Observer {
public:
    using Activity = RL::Activity;
    static constexpr Set defaultActivities = { Activity::BeforeWaiting, Activity::Exit };
    unsigned schedule(Set<Activity> a = defaultActivities) { return a.toRaw(); }
};

// (A name that is no enumerator still fails: ColorTypes.h's
// HasComponentTypeMember of an enum.)
template<typename, typename = void> inline constexpr bool HasComponentType = false;
template<typename T> inline constexpr bool HasComponentType<T, std::void_t<typename T::ComponentType>> = true;
struct Colored { using ComponentType = float; };

int main(int argc, char **argv)
{
    Observer o;
    constexpr Set local = { Observer::Activity::Entry, Observer::Activity::Exit };
    printf("%u %u\n", o.schedule(), local.toRaw());
    printf("%d %d\n", (int)HasComponentType<RL::Activity>, (int)HasComponentType<Colored>);
    return argc;
}
