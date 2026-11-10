#include <stdio.h>
namespace members { unsigned run(); }
int main() { printf("%u\n", (unsigned)members::run()); return 0; }
