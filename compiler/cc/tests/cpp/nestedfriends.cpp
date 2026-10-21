// A class's member operator== declared after a nested class's friend
// operator==: the member's overloads are the class's own, not the
// namespace's free functions the friend joins.
#include <cstdio>

namespace app {

struct Name {
    const char* text;
    Name(const char* t) : text(t) { }
};
bool operator==(const Name& a, const Name& b) { return a.text[0] == b.text[0]; }

struct Element {
    struct File {
        int start { 0 };
        friend bool operator==(const File&, const File&) = default;
    };
    struct Blob {
        int id { 0 };
        friend bool operator==(const Blob& a, const Blob& b) { return a.id == b.id; }
    };

    bool operator==(const Element& other) const { return file == other.file && blob == other.blob; }

    File file;
    Blob blob;
};

bool operator==(const Name& a, int b) { return a.text[0] == b; }

template<typename T> bool same(const T* a, const T* b, int size)
{
    for (int i = 0; i < size; ++i) {
        if (!(a[i] == b[i]))
            return false;
    }
    return true;
}

}

int main(int argc, char** argv)
{
    app::Element a[2] { { { 1 }, { 2 } }, { { 3 }, { argc } } };
    app::Element b[2] { { { 1 }, { 2 } }, { { 3 }, { 1 } } };
    std::printf("%d %d %d\n", (int)app::same(a, a, 2), (int)app::same(a, b, 2), (int)(app::Name("x") == app::Name("x")));
    return 0;
}
