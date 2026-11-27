// mlx-cc's exceptions at run time (include/c++/__mlxcc_exception says
// how the compiler writes try, catch and throw over these). Every
// thread has its chain of try frames, its list of the objects with
// destructors that an exception passing them destroys, and its stack of
// exceptions being handled. An exception is its object after a header
// of ours, in storage of its own.
//
// A type is known by what __mlxcc_exception_type writes of it: its
// spelling, then the Itanium ABI's code of the type and of every public
// base class, each with the base's offset in the object
// ("tcu::NotSupportedError|N3tcu17NotSupportedErrorE@0 ...@0"). A
// handler of the type T takes an exception whose codes name T's code.
#include <__mlxcc_exception>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exception>

namespace {

// An exception's header, before its object: its type, its destructor,
// whether a handler threw it on, and the exception handled before it
// while it is handled.
struct Header {
    const char *type;
    void (*destroy)(void *);
    long rethrown;
    Header *outerHandled;
};
// (A multiple of 16: the object after it is aligned as malloc's are.)
const unsigned long HEADER_SIZE = (sizeof(Header) + 15) & ~15UL;

struct Entry {
    void *object;
    void (*destroy)(void *);
};

struct State {
    __mlxcc_eh::try_frame *frames;
    Entry *objects;
    unsigned long objectCount;
    unsigned long objectCapacity;
    Header *handled;
    unsigned long handledCount;
    Header *propagating;
    int uncaught;
};

__thread State state;

Header *headerOf(void *object) { return (Header *)((char *)object - HEADER_SIZE); }
void *objectOf(Header *h) { return (char *)h + HEADER_SIZE; }

void destroyException(Header *h) {
    if (h->destroy) h->destroy(objectOf(h));
    free(h);
}

// The codes of a type's text (after its spelling).
const char *codesOf(const char *type) {
    const char *bar = strchr(type, '|');
    return bar ? bar + 1 : type;
}

// Where the type WANTED is in an object of the type THROWN: its offset
// there, -1 when THROWN is not one.
long offsetOf(const char *thrown, const char *wanted) {
    const char *w = codesOf(wanted);
    unsigned long wantedLength = strcspn(w, "@ ");
    const char *t = codesOf(thrown);
    while (*t) {
        unsigned long length = strcspn(t, "@ ");
        if (length == wantedLength && memcmp(t, w, length) == 0) return t[length] == '@' ? strtol(t + length + 1, 0, 10) : 0;
        t += strcspn(t, " ");
        while (*t == ' ') t++;
    }
    return -1;
}

// No handler for H: what g++'s programs say, and the end.
[[noreturn]] void terminateWith(Header *h) {
    const char *bar = strchr(h->type, '|');
    int spelled = bar ? (int)(bar - h->type) : (int)strlen(h->type);
    fprintf(stderr, "terminate called after throwing an instance of '%.*s'\n", spelled, h->type);
    long at = offsetOf(h->type, "std::exception|St9exception");
    if (at >= 0) {
        std::exception *e = (std::exception *)((char *)objectOf(h) + at);
        fprintf(stderr, "  what():  %s\n", e->what());
    }
    fflush(stderr);
    abort();
}

// H on its way to the innermost try: the objects made since it was
// entered destroyed (the last made first), the exceptions of handlers
// left on the way given up, and to its _setjmp.
[[noreturn]] void propagate(Header *h) {
    State &s = state;
    __mlxcc_eh::try_frame *f = s.frames;
    if (!f) terminateWith(h);
    while (s.objectCount > f->objects) {
        Entry e = s.objects[--s.objectCount];
        e.destroy(e.object);
    }
    while (s.handledCount > f->handled && s.handled) {
        Header *left = s.handled;
        s.handled = left->outerHandled;
        s.handledCount--;
        if (left != h && !left->rethrown) destroyException(left);
    }
    f->linked = 0;
    s.frames = f->outer;
    s.propagating = h;
    longjmp(f->jump, 1);
}

}

extern "C" void *__mlxcc_eh_allocate(unsigned long size) {
    char *p = (char *)calloc(1, HEADER_SIZE + size);
    if (!p) {
        fputs("terminate: no memory for an exception\n", stderr);
        abort();
    }
    return p + HEADER_SIZE;
}

extern "C" void __mlxcc_eh_throw(void *object, const char *type, void (*destroy)(void *)) {
    Header *h = headerOf(object);
    h->type = type;
    h->destroy = destroy;
    h->rethrown = 0;
    h->outerHandled = 0;
    state.uncaught++;
    propagate(h);
}

extern "C" void __mlxcc_eh_rethrow(void) {
    State &s = state;
    Header *h = s.handled;
    if (!h) {
        fputs("terminate called without an active exception\n", stderr);
        abort();
    }
    s.handled = h->outerHandled;
    s.handledCount--;
    h->rethrown = 1;
    s.uncaught++;
    propagate(h);
}

extern "C" void __mlxcc_eh_unmatched(void) { __mlxcc_eh_rethrow(); }

extern "C" void __mlxcc_eh_try_enter(void *frame) {
    State &s = state;
    __mlxcc_eh::try_frame *f = (__mlxcc_eh::try_frame *)frame;
    f->outer = s.frames;
    f->objects = s.objectCount;
    f->handled = s.handledCount;
    f->linked = 1;
    s.frames = f;
}

extern "C" void __mlxcc_eh_try_leave(void *frame) {
    __mlxcc_eh::try_frame *f = (__mlxcc_eh::try_frame *)frame;
    if (!f->linked) return;
    f->linked = 0;
    if (state.frames == f) state.frames = f->outer;
}

extern "C" void *__mlxcc_eh_begin_catch(void) {
    State &s = state;
    Header *h = s.propagating;
    s.propagating = 0;
    if (!h) return 0;
    s.uncaught--;
    h->rethrown = 0;
    h->outerHandled = s.handled;
    s.handled = h;
    s.handledCount++;
    return objectOf(h);
}

extern "C" void __mlxcc_eh_end_catch(void *exception) {
    if (!exception) return;
    State &s = state;
    Header *h = headerOf(exception);
    if (s.handled != h) return;
    s.handled = h->outerHandled;
    s.handledCount--;
    if (!h->rethrown) destroyException(h);
}

extern "C" void *__mlxcc_eh_adjusted(const char *type) {
    Header *h = state.handled;
    if (!h) return 0;
    long at = offsetOf(h->type, type);
    return at < 0 ? 0 : (char *)objectOf(h) + at;
}

extern "C" void __mlxcc_eh_push(void *object, void (*destroy)(void *)) {
    State &s = state;
    if (s.objectCount == s.objectCapacity) {
        unsigned long capacity = s.objectCapacity ? s.objectCapacity * 2 : 256;
        Entry *grown = (Entry *)realloc(s.objects, capacity * sizeof(Entry));
        if (!grown) {
            fputs("terminate: no memory for the objects an exception destroys\n", stderr);
            abort();
        }
        s.objects = grown;
        s.objectCapacity = capacity;
    }
    s.objects[s.objectCount].object = object;
    s.objects[s.objectCount].destroy = destroy;
    s.objectCount++;
}

extern "C" void __mlxcc_eh_pop(void *object) {
    State &s = state;
    if (s.objectCount > 0 && s.objects[s.objectCount - 1].object == object) s.objectCount--;
}

extern "C" int __mlxcc_eh_uncaught(void) { return state.uncaught; }
