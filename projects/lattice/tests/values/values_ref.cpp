#include <stdio.h>
namespace values { int run(); }
int main() { printf("%u\n", (unsigned)values::run()); return 0; }
