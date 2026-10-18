// What a library in the wild asks of the compiler (tinyxml2 did): const
// in overloads, member templates (declared in the class, defined after
// it, a value parameter deduced through a class template), nested and
// local classes, const objects choosing const member functions, a
// null pointer constant for a pointer parameter, enumerators of their
// enum, a template's default argument, functional casts in parentheses.
#include <cstdio>
#include <cstddef>
#include <new>

namespace lib {

enum Error { OK = 0, NOT_FOUND, PARSING };

static const char *describe(const char *text) { return text ? "text" : "none"; }
static const char *describe(char *) { return "writable"; }

template <size_t ITEM_SIZE> class Pool {
public:
    Pool() : count(0) {}
    void *take() { count++; return buffer + (count - 1) * ITEM_SIZE; }
    size_t itemSize() const { return ITEM_SIZE; }
    int count;
    char buffer[ITEM_SIZE * 16];
};

struct Node { int value; Node *next; Node() : value(0), next(0) {} };
struct Leaf { double weight; Leaf() : weight(0.5) {} };

class Document;
class Element {
public:
    explicit Element(Document *owner) : _owner(owner) {}
    Document *owner() const { return _owner; }
private:
    Document *_owner;
};

class Document : public Element {
public:
    enum Mode { PRESERVE, COLLAPSE };
    Document(bool entities = true, Mode mode = PRESERVE);
    Mode mode() const { return _mode; }
    template <class T, size_t SIZE> T *make(Pool<SIZE> &pool);
    template <class T> T *makeNode() { return make<T>(_nodes); }
    Leaf *makeLeaf() { return make<Leaf>(_leaves); }
    const Node *first() const { printf("first() const\n"); return _first; }
    Node *first() { return const_cast<Node *>(const_cast<const Document *>(this)->first()); }
    class Tracker {
    public:
        explicit Tracker(Document *document) : _document(document) { _document->_depth++; }
        ~Tracker() { _document->_depth--; }
    private:
        Document *_document;
    };
    int depth() const { return _depth; }
    void track() { Tracker tracker(this); printf("depth inside %d\n", _depth); }
    static const char *name(Error error) { return _names[error]; }
private:
    bool _entities;
    Mode _mode;
    Node *_first;
    int _depth;
    Pool<sizeof(Node)> _nodes;
    Pool<sizeof(Leaf)> _leaves;
    static const char *_names[3];
};

const char *Document::_names[3] = { "OK", "NOT_FOUND", "PARSING" };

Document::Document(bool entities, Mode mode) : Element(0), _entities(entities), _mode(mode), _first(0), _depth(0), _nodes(), _leaves() {}

template <class T, size_t SIZE> T *Document::make(Pool<SIZE> &pool) {
    T *made = new (pool.take()) T();
    printf("made %lu of %lu bytes\n", (unsigned long)sizeof(T), (unsigned long)pool.itemSize());
    return made;
}

}

using namespace lib;

bool check(const char *what, Error expected, Error found) { printf("%s: %s/%s\n", what, Document::name(expected), Document::name(found)); return expected == found; }
bool check(const char *what, bool expected, bool found) { printf("%s: %s/%s\n", what, expected ? "true" : "false", found ? "true" : "false"); return expected == found; }
template <class T> bool check(const char *what, T expected, T found, bool echo = true) {
    if (echo) printf("%s: %g/%g\n", what, (double)expected, (double)found);
    return expected == found;
}

int report(int code) { return code; }

int main() {
    char buffer[4] = "abc";
    const char *text = buffer;
    printf("%s %s %s\n", describe(text), describe(buffer), describe((const char *)0));

    Document doc;
    Node *n = doc.makeNode<Node>();
    n->value = 7;
    Node *m = doc.makeNode<Node>();
    Leaf *leaf = doc.makeLeaf();
    printf("%d %d %g %d\n", n->value, m->value, leaf->weight, doc.mode() == Document::PRESERVE);
    doc.track();
    printf("depth after %d\n", doc.depth());
    const Document &view = doc;
    printf("%d %d\n", view.first() == 0, doc.first() == 0);
    printf("%d\n", doc.owner() == 0);

    check("exit code", 0, report(0));
    check("error", OK, (Error)0);
    check("flag", false, doc.depth() > 0);
    check("ratio", 2.0, leaf->weight * 4);
    check("count", 2, doc.depth() + 2, false);

    int x = 2;
    switch (x) {
    case 2:
        x += 10;
        (void(0));
    case 3:
        x += 1;
        break;
    }
    printf("%d\n", x);

    struct Local {
        int sum;
        Local() : sum(0) {}
        void add(int v) { sum += twice(v); }
        int twice(int v) const { return v * 2; }
    };
    Local local;
    for (int i = 0; i < 4; i++) local.add(i);
    printf("%d\n", local.sum);
    return 0;
}
