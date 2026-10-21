// An unqualified call of a member function in a lambda (a generic
// one's instance too) takes the class's member templates of the name
// beside its plain functions: `parse(buffer)` in
// ContentSecurityPolicySourceList::parse(const String &)'s lambda calls
// its parse(StringParsingBuffer<CharacterType>).
#include <cstdio>
#include <span>
struct String {
    String(const char* s) : s(s) { }
    bool is8Bit() const { return true; }
    std::span<const unsigned char> span8() const { return { (const unsigned char*)s, 3 }; }
    std::span<const char16_t> span16() const { return { }; }
    const char* s;
};
template<typename T> class Buffer {
public:
    Buffer(std::span<const T> d) : m_data(d) { }
    size_t size() const { return m_data.size(); }
    std::span<const T> m_data;
};
template<typename StringType, typename Function> decltype(auto) readChars(StringType&& string, const Function& functor)
{
    if (string.is8Bit())
        return functor(Buffer { string.span8() });
    return functor(Buffer { string.span16() });
}
class List {
public:
    void parse(const String&);
    int result = 0;
private:
    template<typename C> void parse(Buffer<C>);
};
void List::parse(const String& value)
{
    readChars(value, [&](auto buffer) {
        if (!buffer.size())
            return;
        parse(buffer);
    });
    auto plain = [&](Buffer<char16_t> b) { parse(b); };
    plain(Buffer<char16_t>(std::span<const char16_t>(u"xy", 2)));
}
template<typename C> void List::parse(Buffer<C> buffer)
{
    result = result * 10 + buffer.size() * sizeof(C);
}
int main() {
    List l;
    l.parse(String("abc"));
    printf("%d\n", l.result);
}
