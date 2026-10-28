// An object of a derived class made into its base (`CodePtr<tag>(label)` of
// JSC's LinkBuffer) is copied as its base part, not given to a constructor
// taking a scalar through a conversion template; a const object's
// condition takes its `explicit operator bool() const`.
#include <stdio.h>
#include <cstddef>
enum class PtrTag : unsigned { None, Handler = 10924 };
enum class Attr : unsigned { None, Other };
class Base { public: void* m_value { nullptr }; enum Tagged { AlreadyTagged }; friend bool operator==(Base, Base) = default; };
template<PtrTag tag, typename, Attr> class FunctionPtr;
template<PtrTag tag, Attr attr = Attr::None> class CodePtr : public Base {
public:
    CodePtr() = default;
    CodePtr(std::nullptr_t) { }
    explicit CodePtr(void* p) { m_value = p; }
    template<typename Out, typename... In> constexpr CodePtr(Out (*ptr)(In...)) { m_value = (void*)ptr; }
    CodePtr(Tagged, void* p) { m_value = p; }
    template<typename Out, typename... In, Attr otherAttr> CodePtr(const FunctionPtr<tag, Out(In...), otherAttr>& other) : CodePtr(AlreadyTagged, other.taggedPtr()) { }
    template<PtrTag otherTag, typename Out, typename... In, Attr otherAttr> CodePtr(const FunctionPtr<otherTag, Out(In...), otherAttr>& other) : CodePtr(AlreadyTagged, other.taggedPtr()) { }
    template<PtrTag newTag> CodePtr<newTag> retagged() const { return CodePtr<newTag>(m_value); }
    explicit operator bool() const { return m_value; }
    template<typename T> operator T() = delete;
};
template<PtrTag tag> class Common : public CodePtr<tag> {
public:
    Common() = default;
    explicit Common(CodePtr<tag> location) : CodePtr<tag>(location) { }
};
template<PtrTag tag> class Label : public Common<tag> {
public:
    Label() { }
    explicit Label(CodePtr<tag> location) : Common<tag>(location) { m_kind = 1; }
    explicit Label(void* location) : Common<tag>(CodePtr<tag>(location)) { m_kind = 2; }
    int m_kind { 0 };
};
template<PtrTag tag> struct Ref { CodePtr<tag> m_code; CodePtr<tag> code() const { return m_code; } };
Ref<PtrTag::Handler> handleCatch(int x) { static int y; Ref<PtrTag::Handler> r; r.m_code = CodePtr<PtrTag::Handler>(&y); return r; }
int takes(CodePtr<PtrTag::Handler> p) { return p.m_value != nullptr; }
template<PtrTag tag> int linkLabel(Label<tag> label) { CodePtr<tag> p = CodePtr<tag>(label); return p.m_value != nullptr; }
int main()
{
    static int y;
    Label<PtrTag::Handler> l { CodePtr<PtrTag::Handler>(&y) };
    const Label<PtrTag::Handler>& c = l;
    int truth = 0;
    if (c)
        truth = 1;
    printf("%d %d %d\n", linkLabel(l), takes(l), truth);
    return 0;
}
