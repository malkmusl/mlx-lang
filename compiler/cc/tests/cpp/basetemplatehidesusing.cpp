// A base's member function template named in a member function hides a
// function template of the name a using-declaration brings to global
// scope (`append("</body>"_s)` in classes derived from WebCore's
// MarkupAccumulator beside `using WTF::append;`).
#include <cstdio>
#include <string>
#include <utility>
namespace W {
template<typename V, typename U> void append(V& v, const U& u) { v.push_back(u); }
}
using W::append;
struct Builder {
    std::string s;
    template<typename... T> void append(T&&... parts) { (s.append(parts), ...); }
};
class Accumulator {
public:
    virtual ~Accumulator() { }
    std::string take() { return m_markup.s; }
protected:
    template<typename ...StringTypes> void append(StringTypes&&... strings) { m_markup.append(std::forward<StringTypes>(strings)...); }
    void appendStartTag(Builder&, int);
    void appendEndTag(Builder&, int);
private:
    Builder m_markup;
};
class Serializer final : public Accumulator {
public:
    Serializer(const std::string& version)
    {
        append("<?xml version=\"", version, "\"?>");
    }
};
class Styled : public Accumulator {
public:
    void wrap() { append("</body>"); }
    using Accumulator::append;
};
int main() {
    Serializer s("1.0");
    Styled t;
    t.wrap();
    t.append("x");
    printf("%s %s\n", s.take().c_str(), t.take().c_str());
}
