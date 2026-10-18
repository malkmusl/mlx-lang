// Argument-dependent lookup through a template's arguments: a function
// of the namespace of a class template's argument found for an argument
// of the template's type (JavaScriptCore's GLib API calls
// sanitizeStackForVM(vm) on a WTF::Ref<JSC::VM> outside namespace JSC),
// by the conversion the template has, by reference and by pointer.
#include <stdio.h>

namespace Engine {

struct Machine {
    int cycles = 3;
};

void sanitize(Machine& machine) { machine.cycles = 0; }
int describe(const Machine* machine) { return machine->cycles * 10; }

template<typename T> struct Tag { };
int tagged(Tag<Machine>) { return 7; }

} // namespace Engine

namespace Library {

template<typename T> struct Ref {
    T* pointer;
    explicit Ref(T& object) : pointer(&object) { }
    operator T&() const { return *pointer; }
    T* operator->() const { return pointer; }
};

template<typename T> struct Box {
    T* pointer;
    operator T*() const { return pointer; }
};

} // namespace Library

int main(int argc, char** argv)
{
    Engine::Machine machine;
    Library::Ref<Engine::Machine> ref(machine);
    Library::Box<Engine::Machine> box { &machine };
    printf("describe %d\n", describe(box));
    sanitize(ref);
    printf("cycles %d describe %d\n", ref->cycles, describe(box));
    printf("tagged %d\n", tagged(Engine::Tag<Engine::Machine>()));
    return argc - 1;
}
