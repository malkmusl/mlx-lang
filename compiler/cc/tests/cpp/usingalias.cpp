// A member using-declaration naming a base through an alias template
// (WTF's USING_CAN_MAKE_WEAKPTR: `using CanMakeWeakPtr<T, Eager>::weakImpl;`
// with another base that has a weakImpl of its own), a conditional with
// an lvalue of a class on one side and a class converting to a reference
// of it on the other (`c ? *origin : SecurityOrigin::create(url)` into a
// deduced Ref), and std::unordered_map's node handles (extract, a key
// changed, insert).
#include <stdio.h>
#include <unordered_map>
#include <utility>

struct ImplA { int tag; };
struct ImplB { int tag; };
enum class Init { Lazy, Eager };
template <class T, class Impl> struct Factory {
    using ObjectType = T;
    using WeakPtrImplType = Impl;
    Impl* impl() const { return m; }
    Impl* m;
};
template <class F, Init init = Init::Lazy> struct CanMakeWeakBase {
    using WeakValueType = typename F::ObjectType;
    using WeakPtrImplType = typename F::WeakPtrImplType;
    WeakPtrImplType& weakImpl() const { return *m_factory.impl(); }
    unsigned weakCount() const { return 0; }
    F m_factory;
};
template <class T, Init init = Init::Lazy, class Impl = ImplA> using CanMakeWeak = CanMakeWeakBase<Factory<T, Impl>, init>;
struct EventTarget : CanMakeWeak<EventTarget, Init::Lazy, ImplB> { virtual ~EventTarget() { } };
#define SINGLE_ARG(...) __VA_ARGS__
#define USING_CAN_MAKE_WEAKPTR(BASE) using BASE::weakImpl; using BASE::weakCount; using BASE::WeakPtrImplType;
class Media : public EventTarget, public CanMakeWeak<Media, Init::Eager> {
public:
    USING_CAN_MAKE_WEAKPTR(SINGLE_ARG(CanMakeWeak<Media, Init::Eager>));
};
template <class T, class Impl = ImplA> struct WeakRef {
    WeakRef(const T& object) : impl(&object.weakImpl()) { }
    Impl* impl;
};

struct Origin { int id; };
template <class T> struct Ref {
    T* p;
    Ref(T& t) : p(&t) { }
    Ref(Ref&& o) : p(o.p) { o.p = nullptr; }
    operator T&() const { return *p; }
};
Origin fresh { 2 };
Ref<Origin> create() { return Ref<Origin>(fresh); }
int chosen(bool useContext, const Origin* context)
{
    Ref origin = useContext ? *context : create();
    return origin.p->id;
}

int main()
{
    ImplA a { 10 };
    ImplB b { 20 };
    Media m;
    static_cast<CanMakeWeak<Media, Init::Eager>&>(m).m_factory.m = &a;
    static_cast<CanMakeWeak<EventTarget, Init::Lazy, ImplB>&>(m).m_factory.m = &b;
    WeakRef<Media> r(m);
    printf("weak %d\n", r.impl->tag);

    Origin context { 1 };
    printf("cond %d %d\n", chosen(true, &context), chosen(false, &context));

    std::unordered_map<unsigned long, int> map;
    map[3] = 30;
    map[4] = 40;
    auto node = map.extract(3ul);
    printf("extract %d %d %zu\n", node ? 1 : 0, node.mapped(), map.size());
    node.key() = 5;
    auto result = map.insert(std::move(node));
    printf("insert %d %d %zu %d\n", result.inserted ? 1 : 0, result.position->second, map.size(), map.count(3ul) ? 1 : 0);
    auto none = map.extract(9ul);
    printf("none %d\n", none.empty() ? 1 : 0);
    auto again = map.insert(std::move(none));
    printf("again %d %zu\n", again.inserted ? 1 : 0, map.size());
    return 0;
}
