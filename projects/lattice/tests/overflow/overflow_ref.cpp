#include <stdio.h>
namespace overflow { int run(); }
int main() { printf("%u\n", (unsigned)overflow::run()); return 0; }
