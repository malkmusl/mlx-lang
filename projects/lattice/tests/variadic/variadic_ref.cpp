#include <stdio.h>
namespace variadic { unsigned run(); }
int main() { printf("%u\n", (unsigned)variadic::run()); return 0; }
