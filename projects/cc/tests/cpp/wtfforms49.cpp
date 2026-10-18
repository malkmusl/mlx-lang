// offsetof of a member that hides a base's (JSCell's m_blob in an anonymous union under Structure's own m_blob)
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

namespace p_offhide {
struct Cell { uint32_t id; union { uint32_t m_blob; struct { uint8_t a, b, c, d; }; }; };
struct Blob { uint32_t word; };
struct Str : Cell { Blob m_blob; uint8_t flags; };
struct Der : Str { int extra; };
int run() {
    printf("%d %d %d %d %d\n", (int)offsetof(Cell, m_blob), (int)offsetof(Str, m_blob), (int)offsetof(Str, m_blob.word), (int)offsetof(Der, m_blob), (int)offsetof(Str, b));
    return 0;
}
}

int main() {
    p_offhide::run();
    return 0;
}
