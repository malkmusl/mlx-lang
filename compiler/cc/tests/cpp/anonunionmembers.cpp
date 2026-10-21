// A class's destructor leaves the members of its anonymous unions alone
// (variant members are the program's to destroy): JSC's
// UnlinkedFunctionExecutable holds `union { WriteBarrier<...> m_codeBlock;
// RefPtr<Decoder> m_decoder; }` and its ~UnlinkedFunctionExecutable()
// destroys m_decoder only when it is the member in use.
#include <cstdio>
#include <new>

static int destroyed = 0;

struct Counted {
    int refs = 1;
    void deref() {
        refs -= 1;
        if (!refs) destroyed += 1;
    }
};

struct Ref {
    Counted *ptr;
    ~Ref() {
        if (ptr) ptr->deref();
    }
};

struct Executable {
    explicit Executable(Counted *other) : cached(false), codeBlock(other) {}
    ~Executable() {
        if (cached) decoder.~Ref();
    }
    bool cached;
    union {
        Counted *codeBlock;
        Ref decoder;
    };
    union {
        Ref second;
        struct {
            int offset;
            int other;
        };
    };
};

// A union with a destructor of its own: only that runs.
struct Holder {
    union U {
        Ref ref;
        long bits;
        U() : bits(0) {}
        ~U() { destroyed += 100; }
    } u;
};

int main(int argc, char **argv) {
    Counted block;
    {
        Executable e(&block);
        e.offset = 4;
        e.other = 0;
        printf("%d %d\n", block.refs, e.offset);
    }
    printf("%d %d\n", block.refs, destroyed);
    Counted decoder;
    {
        Executable e(nullptr);
        e.cached = true;
        new (&e.decoder) Ref{&decoder};
        new (&e.second) Ref{&block};
    }
    printf("%d %d %d\n", decoder.refs, block.refs, destroyed);
    {
        Holder h;
        h.u.bits = 5;
    }
    printf("%d\n", destroyed);
    return argc;
}
