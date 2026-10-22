// A class template's instance named in a nested class of another class
// template's instance (`Ref<IsFinished>` in JSDOMAsyncIteratorBase<...>::
// IsFinished::create, WebCore), read apart from the member of the same
// type outside it: the type object named there was left incomplete and
// taken for an aggregate (no constructors) at `return adoptRef(*new
// IsFinished)`. It takes the made instance's description.
#include <cstdio>
#include <type_traits>
template<typename> struct True : std::true_type { };
template<class T> static auto hasRefTest(int) -> True<decltype(static_cast<T*>(nullptr)->ref())>;
template<class> static auto hasRefTest(long) -> std::false_type;
template<class T> struct HasRef : decltype(hasRefTest<T>(0)) { };
template<class T> concept CanUse = HasRef<T>::value;

template<typename T> class Counted {
public:
    void ref() { refs++; }
    void deref() { refs--; }
    int refs { 0 };
protected:
    Counted() = default;
};
template<typename T> struct WeakAndCounted : Counted<T> { };

template<typename T> class Ref {
public:
    enum AdoptTag { Adopt };
    Ref(T& object, AdoptTag) : m_ptr(&object) { }
    Ref(T& object) : m_ptr(&object) { object.ref(); }
    Ref(Ref&& other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ~Ref() { if (m_ptr) m_ptr->deref(); }
    T* operator->() const { return m_ptr; }
    T& get() const { return *m_ptr; }
private:
    T* m_ptr;
};
template<typename T> Ref<T> adoptRef(T&);
template<typename T> requires CanUse<T> Ref<T> protect(T& reference) { return Ref<T>(reference); }
template<typename T> inline Ref<T> adoptRef(T& reference) { return Ref<T>(reference, Ref<T>::Adopt); }

template<typename W> struct Outer {
    class IsFinished : public WeakAndCounted<IsFinished> {
    public:
        static Ref<IsFinished> create() { return adoptRef(*new IsFinished); }
        operator bool() { return m_value; }
        void markAsFinished() { m_value = true; }
    private:
        bool m_value { false };
    };
    Outer() : m_isFinished(IsFinished::create()) { }
    Ref<IsFinished> m_isFinished;
};
int main()
{
    Outer<int> o;
    auto p = protect(o.m_isFinished.get());
    printf("%d %d %d\n", (bool)o.m_isFinished.get(), o.m_isFinished->refs, (int)HasRef<Outer<int>::IsFinished>::value);
    return 3;
}
