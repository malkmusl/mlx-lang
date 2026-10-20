// Class template argument deduction in a new-expression, `new
// DiscreteWrapper(property, &Style::appearance, &Style::setAppearance)`
// through the class's deduction guides, as a declaration deduces
// (WebCore's StyleInterpolationWrapperMap: a std::array of wrappers
// made so, in a constructor's initializer).
#include <cstdio>
#include <array>
#include <type_traits>
enum class Prop : unsigned short { Invalid, Custom, Appearance, MathDepth, Ruby, Count };
constexpr unsigned short propCount = 5;
struct Style {
    int appearance() const { return a; }
    void setAppearance(int v) { a = v; }
    const char* name() const { return n; }
    void setName(const char* v) { n = v; }
    int a { 1 };
    const char* n { "x" };
};
class WrapperBase {
public:
    WrapperBase(Prop p) : m_property(p) {}
    virtual ~WrapperBase() = default;
    virtual int kind() const = 0;
    Prop m_property;
};
template<typename T, typename GetterType = T> class WrapperWithGetter : public WrapperBase {
public:
    WrapperWithGetter(Prop p, GetterType (Style::*getter)() const) : WrapperBase(p), m_getter(getter) {}
    GetterType (Style::*m_getter)() const;
};
template<typename T, typename GetterType = T, typename SetterType = T> class DiscreteWrapper : public WrapperWithGetter<T, GetterType> {
public:
    DiscreteWrapper(Prop property, GetterType (Style::*getter)() const, void (Style::*setter)(SetterType))
        : WrapperWithGetter<T, GetterType>(property, getter), m_setter(setter) {}
    int kind() const final { return 1; }
    void (Style::*m_setter)(SetterType);
};
template<typename GetterType, typename SetterType> DiscreteWrapper(Prop, GetterType (Style::*)() const, void (Style::*)(SetterType)) -> DiscreteWrapper<std::remove_cvref_t<GetterType>, GetterType, SetterType>;
template<typename T> class StyleTypeWrapper : public WrapperWithGetter<T> {
public:
    StyleTypeWrapper(Prop property, T (Style::*getter)() const, void (Style::*setter)(T)) : WrapperWithGetter<T>(property, getter), m_setter(setter) {}
    int kind() const final { return 2; }
    void (Style::*m_setter)(T);
};
template<typename T> StyleTypeWrapper(Prop, T (Style::*)() const, void (Style::*)(T)) -> StyleTypeWrapper<T>;
class WrapperMap {
public:
    WrapperMap();
    WrapperBase* wrapper(Prop id) { return m_wrappers[(unsigned short)id]; }
private:
    std::array<WrapperBase*, propCount> m_wrappers;
};
WrapperMap::WrapperMap()
    : m_wrappers {
        nullptr, // Invalid
        nullptr, // Custom
        new DiscreteWrapper(Prop::Appearance, &Style::appearance, &Style::setAppearance), // Appearance
        new StyleTypeWrapper(Prop::MathDepth, &Style::name, &Style::setName), // MathDepth
        new DiscreteWrapper(Prop::Ruby, &Style::name, &Style::setName), // Ruby
    }
{
}
int main()
{
    WrapperMap map;
    printf("%d %d %d %d\n", map.wrapper(Prop::Invalid) == nullptr, map.wrapper(Prop::Appearance)->kind(), map.wrapper(Prop::MathDepth)->kind(), (int)map.wrapper(Prop::Ruby)->m_property);
    WrapperBase* one = new StyleTypeWrapper(Prop::Custom, &Style::appearance, &Style::setAppearance);
    auto* two = new DiscreteWrapper { Prop::Custom, &Style::name, &Style::setName };
    printf("%d %d %d\n", one->kind(), two->kind(), (int)two->m_property);
    return 2;
}
