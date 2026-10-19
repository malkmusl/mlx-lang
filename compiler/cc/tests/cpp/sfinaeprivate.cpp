// A substitution naming a member function that is not public fails
// (WTF's HasRefPtrMemberFunctions, which makeUnique asserts false of
// WebCore's StyleSheetCSSRuleList: its ref() and deref() are private
// overrides of its base's public ones), while the class's own members
// still call them, also through a substitution in a lambda of theirs
// (mpark's variant visiting its alternatives).
#include <cstdio>
#include <type_traits>

template<typename> struct True : std::true_type { };

template<class T> static auto test(int) -> True<decltype(static_cast<T*>(nullptr)->ref(), static_cast<T*>(nullptr)->deref())>;
template<class> static auto test(...) -> std::false_type;

template<class T> struct HasRef : decltype(test<T>(0)) { };

struct List {
    virtual ~List() = default;
    virtual void ref() const { }
    virtual void deref() const { }
};

class Shared : public List {
public:
    int count() const { ref(); return m_count; }

private:
    void ref() const final { ++m_count; }
    void deref() const final { --m_count; }
    mutable int m_count { 0 };
};

class Counted {
public:
    void ref() const { }
    void deref() const { }
};

class Hidden {
protected:
    void ref() const { }
public:
    void deref() const { }
};

struct Plain {
    int value;
};

template<class F, class A> auto apply(F f, A a) -> decltype(f(a)) { return f(a); }

class Scaler {
public:
    int twice() { auto f = [this](auto x) { return this->scale(x); }; return apply(f, 4); }

private:
    int scale(int x) { return x * 2; }
};

int main()
{
    printf("List %d, Shared %d, Counted %d, Hidden %d, Plain %d\n", HasRef<List>::value, HasRef<Shared>::value, HasRef<Counted>::value, HasRef<Hidden>::value, HasRef<Plain>::value);
    Shared shared;
    printf("count %d\n", shared.count());
    Scaler scaler;
    printf("twice %d\n", scaler.twice());
    return 0;
}
