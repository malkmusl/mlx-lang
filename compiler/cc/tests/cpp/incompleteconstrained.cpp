// Members of a class template's instance constrained by traits of its
// argument, the instance made while the argument's class is not defined
// yet (WTF's WeakHashSet<Frame>::forEach, `requires HasRefPtrMemberFunctions
// <T>::value` and `requires (HasCheckedPtrMemberFunctions<T>::value &&
// !HasRefPtrMemberFunctions<T>::value)`, a member before Frame's
// definition): the constraints are weighed where the member is called,
// on the whole class, and the call chooses between the two of one
// signature.
#include <cstdio>
#include <type_traits>

template<typename> struct True : std::true_type { };
template<class T> static auto hasRefTest(int) -> True<decltype(static_cast<T*>(nullptr)->ref())>;
template<class> static auto hasRefTest(long) -> std::false_type;
template<class T> struct HasRef : decltype(hasRefTest<T>(0)) { };

template<class T> static auto hasCheckedTest(int) -> True<decltype(static_cast<T*>(nullptr)->incrementCheckedPtrCount())>;
template<class> static auto hasCheckedTest(long) -> std::false_type;
template<class T> struct HasChecked : decltype(hasCheckedTest<T>(0)) { };

template<typename T> struct Checked {
    Checked(T* p) : p(p) { p->incrementCheckedPtrCount(); }
    T* p;
};

template<typename T> class Set {
public:
    void set(T& item) { m_item = &item; }
    void forEach(void (*callback)(T&)) requires HasRef<T>::value
    {
        m_item->ref();
        callback(*m_item);
    }
    void forEach(void (*callback)(T&)) requires (HasChecked<T>::value && !HasRef<T>::value)
    {
        Checked<T> checked { m_item };
        callback(*checked.p);
    }
    // One member of the name: its constraint weighed at the call too.
    const char* kind() const requires HasRef<T>::value { return "counted"; }
    const char* kind() const requires (!HasRef<T>::value) { return "checked"; }
    int size() const requires (sizeof(T) > 1) { return 2; }

private:
    T* m_item { nullptr };
};

struct Counted;
struct Plain;
struct Holder {
    Set<Counted> counted;
    Set<Plain> plain;
    void run();
};

template<typename T> struct CheckedBase {
    void incrementCheckedPtrCount() { checks++; }
    int checks { 0 };
};

struct Counted : CheckedBase<Counted> {
    void ref() { refs++; }
    int refs { 0 };
};

struct Plain : CheckedBase<Plain> {
    int value { 7 };
};

void Holder::run()
{
    counted.forEach([](Counted& c) { printf("counted %d %d\n", c.refs, c.checks); });
    plain.forEach([](Plain& p) { printf("plain %d %d\n", p.value, p.checks); });
    printf("%s %s %d\n", counted.kind(), plain.kind(), counted.size());
}

int main()
{
    Counted counted;
    Plain plain;
    Holder holder;
    holder.counted.set(counted);
    holder.plain.set(plain);
    holder.run();
    return 3;
}
