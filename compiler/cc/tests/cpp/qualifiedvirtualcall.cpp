// A member function named with its class (`area.ScrollableArea::
// createScrollbarsController()`, `p->Base::f()`, `Base::f()`) is called
// as that class's, not through the vtable: WebCore's ChromeClient calls
// ScrollableArea's own of a LocalFrameView, whose override calls the
// ChromeClient again (an endless recursion through the vtable).
#include <cstdio>

struct Area {
    virtual ~Area() { }
    virtual const char* create() { return "Area"; }
    virtual int depth() const { return 0; }
};
struct View : Area {
    const char* create() override { return "View"; }
    int depth() const override { return 1 + Area::depth(); }
    const char* base() { return this->Area::create(); }
};

static const char* ensure(Area& area) { return area.Area::create(); }
static const char* ensurePointer(Area* area) { return area->Area::create(); }

int main()
{
    View view;
    Area& area = view;
    std::printf("%s %s %s %s %d\n", area.create(), ensure(area), ensurePointer(&view), view.base(), area.depth());
    return 0;
}
