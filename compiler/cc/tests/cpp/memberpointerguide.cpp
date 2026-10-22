// C++17's implicit deduction guides of constructors taking pointers to
// member functions: a named one, `T (C::*getter)() const`, and one by a
// member alias, `ListGetter getter` with `using ListGetter = const List &
// (Base::*)() const`, whose spelled-out type takes no name after it. In
// a new-expression and in a temporary that is an argument (WebCore's
// generated StyleInterpolationWrapperMap: CoordinatedValueListPropertyWrapper
// of a CoordinatedValueListPropertyStyleTypeWrapper).
#include <cstdio>
#include <vector>
enum class Prop { Invalid, Image, Count };
template<Prop P> struct PropertyNameConstant { static constexpr Prop value = P; };
struct Layer {
    const int& image() const { return m_image; }
    void setImage(int&& v) { m_image = v; }
    int m_image { 4 };
};
using Layers = std::vector<Layer>;
struct StyleBase {
    const Layers& layers() const { return m_layers; }
    Layers& ensureLayers() { return m_layers; }
    void setLayers(Layers&& l) { m_layers = l; }
    Layers m_layers { Layer { } };
};
struct Style : StyleBase { };
struct WrapperBase { WrapperBase(Prop p) : m_property(p) {} virtual ~WrapperBase() = default; virtual int kind() const = 0; Prop m_property; };
template<typename StyleType, typename ValueType, typename GetterType = const StyleType&, typename SetterType = StyleType&&>
class RepeatedStyleTypeWrapper final {
public:
    RepeatedStyleTypeWrapper(Prop property, const StyleType& (ValueType::*getter)() const, void (ValueType::*setter)(StyleType&&)) : m_property(property), m_getter(getter), m_setter(setter) {}
    int value(const ValueType& v) const { return (v.*m_getter)(); }
    Prop m_property;
    GetterType (ValueType::*m_getter)() const;
    void (ValueType::*m_setter)(SetterType);
};
template<Prop ID, typename T, typename RepeatedValueWrapper>
class CoordinatedWrapper final : public WrapperBase {
public:
    static constexpr Prop propertyID = ID;
    using List = T;
    using ValueType = typename List::value_type;
    using ListGetter = const List& (StyleBase::*)() const;
    using ListAccessor = List& (StyleBase::*)();
    using ListSetter = void (StyleBase::*)(List&&);
    CoordinatedWrapper(PropertyNameConstant<ID> property, ListGetter getter, ListAccessor accessor, ListSetter setter, RepeatedValueWrapper repeated)
        : WrapperBase(property.value), m_getter(getter), m_accessor(accessor), m_setter(setter), m_repeated(repeated) {}
    int kind() const final { return 7; }
    int first(const Style& style) const { return m_repeated.value((style.*m_getter)()[0]); }
    ListGetter m_getter;
    ListAccessor m_accessor;
    ListSetter m_setter;
    RepeatedValueWrapper m_repeated;
};
int main()
{
    Style style;
    auto* w = new CoordinatedWrapper(PropertyNameConstant<Prop::Image> { }, &Style::layers, &Style::ensureLayers, &Style::setLayers, RepeatedStyleTypeWrapper(Prop::Image, &Layers::value_type::image, &Layers::value_type::setImage));
    printf("%d %d %d\n", w->kind(), (int)w->m_property, w->first(style));
    RepeatedStyleTypeWrapper repeated(Prop::Image, &Layers::value_type::image, &Layers::value_type::setImage);
    printf("%d %d\n", (int)repeated.m_property, repeated.value(style.m_layers[0]));
    return 4;
}
