// The library ANGLE takes, 3: shared_ptr over a control block (make_shared,
// conversions to a base, the pointer casts, from unique_ptr, owner_less),
// weak_ptr, enable_shared_from_this; tuples ordered (std::tie in
// operator<); ostream_iterator; numeric_limits' exponents.
#include <stdio.h>
#include <memory>
#include <map>
#include <tuple>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <limits>

static int alive = 0;
struct Base {
    int id;
    Base(int i) : id(i) { alive++; }
    virtual ~Base() { alive--; }
    virtual int kind() const { return 1; }
};
struct Derived : Base {
    Derived(int i) : Base(i) {}
    int kind() const override { return 2; }
};
struct Node : std::enable_shared_from_this<Node> {
    int v;
    Node(int x) : v(x) {}
    std::shared_ptr<Node> self() { return shared_from_this(); }
};

struct Version { int major, minor, patch; };
static bool operator<(const Version &x, const Version &y) { return std::tie(x.major, x.minor, x.patch) < std::tie(y.major, y.minor, y.patch); }
static bool operator==(const Version &x, const Version &y) { return std::tie(x.major, x.minor, x.patch) == std::tie(y.major, y.minor, y.patch); }

int main() {
    std::weak_ptr<Base> watcher;
    {
        std::shared_ptr<Derived> d = std::make_shared<Derived>(7);
        std::shared_ptr<Base> b = d;
        watcher = b;
        std::shared_ptr<Base> locked = watcher.lock();
        printf("%ld %d %d %d\n", d.use_count(), b->kind(), (int)watcher.expired(), locked->id);
        std::shared_ptr<Derived> back = std::static_pointer_cast<Derived>(b);
        printf("%d %d\n", (int)(back == d), (int)(back != nullptr));
    }
    printf("%d %d %d\n", (int)watcher.expired(), (int)(watcher.lock() == nullptr), alive);
    std::shared_ptr<Base> owned(new Derived(3));
    std::shared_ptr<Base> other(std::move(owned));
    printf("%d %d %ld\n", (int)(owned == nullptr), other->id, other.use_count());
    other.reset();
    printf("%d\n", alive);
    std::shared_ptr<Node> n = std::make_shared<Node>(5);
    std::shared_ptr<Node> again = n->self();
    printf("%ld %d\n", n.use_count(), again->v);
    std::unique_ptr<Base> u(new Base(9));
    std::shared_ptr<Base> fromUnique = std::move(u);
    std::map<std::shared_ptr<Base>, int, std::owner_less<>> m;
    m[fromUnique] = 4;
    printf("%d %d %d\n", fromUnique->id, (int)(u == nullptr), m[fromUnique]);

    Version v1{1, 2, 3}, v2{1, 3, 0};
    std::vector<std::string> words = {"a", "b", "c"};
    std::ostringstream out;
    std::copy(words.begin(), words.end(), std::ostream_iterator<std::string>(out, " "));
    printf("%d %d %d [%s]\n", (int)(v1 == v1), (int)(v1 < v2), (int)(v2 < v1), out.str().c_str());
    printf("%d %d %d %d\n", std::numeric_limits<int>::max_exponent, std::numeric_limits<double>::max_exponent, std::numeric_limits<float>::max_digits10, (int)std::numeric_limits<unsigned>::is_iec559);
    return 0;
}
