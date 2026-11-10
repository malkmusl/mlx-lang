#include <stdio.h>
namespace casts { unsigned run(); }
int main() { printf("%u\n", (unsigned)casts::run()); return 0; }
