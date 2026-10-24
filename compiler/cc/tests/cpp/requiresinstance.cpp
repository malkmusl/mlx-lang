// A class template instance whose member functions differ by their
// requires clauses (WTF::WeakHashSet's two forEach), made for an
// instance of another template not made yet (RunLoop's
// WeakHashSet<Observer<void(Event, const String&)>>): that instance is
// made, the clauses weighed on it, and only the forEach whose clause
// holds is there; the other's body (a CheckedPtr of a class without
// checked-pointer functions) is never made.
#include <stdio.h>
#include <type_traits>

namespace detail {
template<typename> struct True : std::true_type { };
template<class T> static auto refTest(int) -> True<decltype(static_cast<T*>(nullptr)->ref())>;
template<class> static auto refTest(...) -> std::false_type;
template<class T> static auto checkedTest(int) -> True<decltype(static_cast<T*>(nullptr)->incrementCheckedPtrCount())>;
template<class> static auto checkedTest(...) -> std::false_type;
}
template<class T> struct HasRef : decltype(detail::refTest<T>(0)) { };
template<class T> struct HasChecked : decltype(detail::checkedTest<T>(0)) { };

template<typename T> struct Checked {
    T* p;
    Checked(T* q) : p(q) { p->incrementCheckedPtrCount(); }
};

template<typename T> struct Set {
    T* item = nullptr;
    int forEach() requires HasRef<T>::value { item->ref(); return 1; }
    int forEach() requires (HasChecked<T>::value && !HasRef<T>::value) { Checked<T> c { item }; return 2; }
};

template<typename> class Observer;
template<typename Out, typename... In> class Observer<Out(In...)> final {
public:
    int refs = 0;
    void ref() { refs++; }
};

// (Named by an alias first: the alias leaves the instance unmade.)
using LoopObserver = Observer<void(int, const char*)>;

struct Loop {
    Set<LoopObserver> observers;
};

int main(int argc, char **argv)
{
    Observer<void(int, const char*)> o;
    Loop loop;
    loop.observers.item = &o;
    int which = loop.observers.forEach();
    printf("forEach %d refs %d\n", which, o.refs);
    return argc - 1;
}
