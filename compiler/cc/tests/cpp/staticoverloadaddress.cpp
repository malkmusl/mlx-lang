// The address of an overloaded name of a class's member function and
// static member function, to a plain pointer to a function: the static
// one, though the member is declared first (JSC's method tables, whose
// `&ClassName::setPrototype` is JSObject's static setPrototype beside the
// member of the same name).
#include <cstdio>

struct VM { int id; };
struct Object;
using SetFn = bool (*)(Object*, int, bool);
using GetFn = int (*)(Object*);
struct Table { SetFn set; GetFn get; const char* name; };

struct Object {
    const Table* table;
    int value;
    bool set(VM&, int value, bool flag = false);
    static bool set(Object*, int value, bool flag);
    int get() const;
    static int get(Object*);
};

bool Object::set(Object* object, int value, bool flag) {
    printf("static set %d %d\n", value, flag);
    object->value = value;
    return true;
}
bool Object::set(VM&, int value, bool flag) { return table->set(this, value, flag); }
int Object::get(Object* object) { return object->value * 10; }
int Object::get() const { return table->get(const_cast<Object*>(this)); }

struct NonFinal : Object { };

template<class Parent> struct Callback : Parent {
    static const Table s_table;
};

#define MAKE_TABLE(ClassName, name) { &ClassName::set, &ClassName::get, name }

template<> const Table Callback<NonFinal>::s_table = MAKE_TABLE(Callback, "callback");

struct Plain : Object { static const Table s_table; };
const Table Plain::s_table = MAKE_TABLE(Plain, "plain");

int main(int argc, char** argv) {
    VM vm = { 1 };
    Callback<NonFinal> c;
    c.table = &Callback<NonFinal>::s_table;
    c.set(vm, 5);
    printf("%s get %d\n", c.table->name, c.get());
    Plain p;
    p.table = &Plain::s_table;
    p.set(vm, 6, true);
    printf("%s get %d\n", p.table->name, p.get());
    SetFn direct = &Callback<NonFinal>::set;
    direct(&c, 7, false);
    GetFn getter = &Object::get;
    printf("direct get %d\n", getter(&c));
    printf("args %d\n", argc);
    return 0;
}
