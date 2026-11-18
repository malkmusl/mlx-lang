// Exceptions (-fexceptions): thrown and caught by their class and its
// bases, by value and by reference, catch (...), throw; again, a handler
// left by another throw, nested tries, a throw out of called functions
// whose locals are destroyed on the way, a return and a break out of a
// try, the standard's classes, and what the handler sees of the values
// the try set.
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <vector>

struct Error {
    Error(int c) : code(c) {}
    Error(const Error &other) : code(other.code) { copies++; }
    virtual ~Error() {}
    virtual const char *name() const { return "Error"; }
    int code;
    static int copies;
};
int Error::copies = 0;

struct NotSupported : public Error {
    NotSupported(int c, const std::string &w) : Error(c), why(w) {}
    const char *name() const { return "NotSupported"; }
    std::string why;
};

struct Pair {
    long a;
    long b;
};

// A class whose second base is the one caught: its offset applied.
struct Tagged : public Pair, public std::runtime_error {
    Tagged(const char *what) : std::runtime_error(what) {
        a = 1;
        b = 2;
    }
};

static int made = 0;
static int unmade = 0;

struct Noisy {
    Noisy(const char *n) : label(n) {
        made++;
        printf("make %s\n", label);
    }
    ~Noisy() {
        unmade++;
        printf("unmake %s\n", label);
    }
    const char *label;
};

static void deepest(int how) {
    Noisy n("deepest");
    if (how == 1) throw NotSupported(7, "no such feature");
    if (how == 2) throw Error(3);
    if (how == 3) throw std::out_of_range("index 9");
    if (how == 4) throw 42;
    printf("deepest returns\n");
}

static void middle(int how) {
    Noisy n("middle");
    std::string s("a string on the way");
    deepest(how);
    printf("middle returns\n");
}

static int classify(int how) {
    try {
        middle(how);
    } catch (const NotSupported &e) {
        printf("caught %s %d %s\n", e.name(), e.code, e.why.c_str());
        return 1;
    } catch (Error &e) {
        printf("caught %s %d\n", e.name(), e.code);
        return 2;
    } catch (const std::exception &e) {
        printf("caught std::exception: %s\n", e.what());
        return 3;
    } catch (...) {
        printf("caught something else\n");
        return 4;
    }
    printf("nothing thrown\n");
    return 0;
}

static int rethrown() {
    try {
        try {
            throw NotSupported(11, "inner");
        } catch (Error &e) {
            printf("inner handler %d, throwing on\n", e.code);
            throw;
        }
    } catch (NotSupported e) {
        printf("outer handler by value %d %s\n", e.code, e.why.c_str());
        return e.code;
    }
    return 0;
}

static int replaced() {
    try {
        try {
            throw Error(1);
        } catch (Error &e) {
            printf("handling %d, throwing another\n", e.code);
            throw Error(e.code + 1);
        }
    } catch (Error &e) {
        printf("the other one %d\n", e.code);
        return e.code;
    }
    return 0;
}

static int leftBy(int how) {
    for (int i = 0; i < 5; i++) {
        try {
            if (i == 2 && how == 0) break;
            if (i == 3 && how == 1) return 100 + i;
            if (i == 4) throw Error(i);
        } catch (Error &e) {
            return e.code;
        }
    }
    return -1;
}

static int seen() {
    int counted = 0;
    std::vector<int> values;
    try {
        for (int i = 0; i < 10; i++) {
            counted++;
            values.push_back(i * i);
            if (i == 6) throw Error(i);
        }
    } catch (Error &e) {
        printf("counted %d, %d values, last %d\n", counted, (int)values.size(), values.back());
    }
    return counted;
}

int main() {
    printf("classify %d\n", classify(0));
    printf("classify %d\n", classify(1));
    printf("classify %d\n", classify(2));
    printf("classify %d\n", classify(3));
    printf("classify %d\n", classify(4));
    printf("rethrown %d\n", rethrown());
    printf("replaced %d\n", replaced());
    printf("left by break %d\n", leftBy(0));
    printf("left by return %d\n", leftBy(1));
    printf("left by throw %d\n", leftBy(2));
    printf("seen %d\n", seen());
    try {
        throw Tagged("second base");
    } catch (std::runtime_error &e) {
        printf("runtime_error: %s\n", e.what());
    }
    try {
        throw Tagged("first base");
    } catch (Pair &p) {
        printf("pair %ld %ld\n", p.a, p.b);
    }
    printf("made %d unmade %d\n", made, unmade);
    return 0;
}
