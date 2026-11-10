// The idioms test: C++ habits of ANGLE's sources the translator meets
// everywhere. Pointers stepped through members (*p++, *--p), std::array
// members, static locals of one name in several functions, a class of an
// anonymous namespace beside a function of its name, references made from
// pointers, an object in bytes of storage, an atomic pointer.
#include <array>
#include <atomic>

namespace idioms {

struct Lexer {
    char *cursor;
    unsigned char block[64];
    unsigned n;
    void feed(const char *data, unsigned len) {
        while (len--) {
            block[n++] = *data++;
        }
    }
    void back(char c) { *--cursor = c; }
    char next() { return *cursor++; }
};

struct Point {
    int x;
    int y;
    Point() : x(1), y(2) {}
};

struct Shapes {
    std::array<int, 3> sizes;
    std::array<Point, 4> points;
    std::array<const char *, 6> names;
    int sum() const {
        int total = 0;
        for (unsigned i = 0; i < sizes.size(); i++) {
            total += sizes[i];
        }
        for (const Point &p : points) {
            total += p.x * 10 + p.y;
        }
        return total;
    }
};

static int first(int i) {
    static const int kTable[4] = {3, 6, 11, 16};
    return kTable[i & 3];
}

static int second(int i) {
    static const int kTable[4] = {100, 200, 300, 400};
    return kTable[i & 3];
}

namespace {
class Checker {
  public:
    explicit Checker(int limit) : mLimit(limit) {}
    bool check(int v) const { return v < mLimit; }

  private:
    int mLimit;
};
}

bool Checker(int value, int limit) {
    class Checker checker(limit);
    return checker.check(value);
}

struct Holder {
    Point *held;
    Point &get() const { return *held; }
    Point &pick(Point *other, bool which) const { return which ? *held : *other; }
};

struct Slot {
    alignas(8) char storage[16];
    Point *get() { return reinterpret_cast<Point *>(storage); }
};

struct Chain {
    std::atomic<Point *> head{nullptr};
    Point *swap(Point *next) { return head.exchange(next); }
    Point *load() const { return head.load(); }
};

int run() {
    Lexer l;
    char buffer[16];
    l.cursor = buffer + 8;
    l.n = 0;
    l.feed("hello", 5);
    l.back('a');
    l.back('b');
    int total = l.next();
    total += l.next() * 3 + l.block[4] + l.n;

    Shapes s;
    s.sizes[0] = 5;
    s.sizes[1] = 6;
    s.sizes[2] = 7;
    s.names = {};
    total += s.sum() + (s.names[3] == nullptr ? 100 : 0);

    total += first(2) + second(3) + first(1);
    total += idioms::Checker(3, 5) ? 1000 : 0;
    total += idioms::Checker(7, 5) ? 5000 : 0;

    Point a;
    Point b;
    b.x = 40;
    Holder h;
    h.held = &a;
    h.get().y = 9;
    total += h.get().x + a.y + h.pick(&b, false).x;

    Slot slot;
    slot.get()->x = 70;
    slot.get()->y = 2;
    total += slot.get()->x * slot.get()->y;

    Chain chain;
    total += chain.load() == nullptr ? 3 : 0;
    Point *before = chain.swap(&b);
    total += before == nullptr ? 4 : 0;
    total += chain.load()->x;
    return total;
}

}
