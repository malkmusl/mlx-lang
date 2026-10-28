// An object of a class without a copy constructor of its own is copied
// (moved) member by member, not by a constructor taking another type
// through a deleted conversion template (JSC's CodeLocationLabel(void *)
// and CodePtr's `template <class T> operator T() = delete`).
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
struct Info { int type; Label<PtrTag::Handler> nativeCode; };
int main()
{
    static int y;
    Info a { 3, Label<PtrTag::Handler>(CodePtr<PtrTag::Handler>(&y)) };
    Info b(static_cast<Info&&>(a));
    Info c(b);
    Label<PtrTag::Handler> d(static_cast<Label<PtrTag::Handler>&&>(b.nativeCode));
    printf("%d %d %d %d\n", b.nativeCode.m_kind, c.nativeCode.m_kind, d.m_kind, b.nativeCode.m_value == &y && c.nativeCode.m_value == &y && d.m_value == &y);
    return 0;
}
