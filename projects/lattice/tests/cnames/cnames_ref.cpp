#include <stdio.h>
namespace cnames { unsigned run(); }
int main() { printf("%u\n", (unsigned)cnames::run()); return 0; }
