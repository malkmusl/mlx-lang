// A data member hides a type of its name around the class when assigned
// or accessed unqualified in a member function defined out of line:
// ANGLE's VertexAttribCurrentValueData has a member `Type` beside the
// namespace's struct gl::Type, and its setIntValues says `Type = ...;`.
#include <stdio.h>

namespace gl {
struct Type { int a; };
struct Point { int x; };
enum class AttribType { Float, Int };
struct Data {
    inline void setInt(const int v[4]);
    void setFloat();
    int moved() const;
    int Values[4];
    AttribType Type;
    struct Point Point;
};
inline void Data::setInt(const int v[4]) {
    for (int i = 0; i < 4; i++) Values[i] = v[i];
    Type = gl::AttribType::Int;
}
void Data::setFloat() { Type = AttribType::Float; Point.x = 9; }
int Data::moved() const { return Point.x + 1; }
}

int main() {
    gl::Data d;
    int v[4] = {1, 2, 3, 4};
    d.setInt(v);
    printf("%d %d\n", d.Values[2], static_cast<int>(d.Type));
    d.setFloat();
    printf("%d %d\n", static_cast<int>(d.Type), d.moved());
    gl::Type t = {4};
    printf("%d\n", t.a);
    return 0;
}
