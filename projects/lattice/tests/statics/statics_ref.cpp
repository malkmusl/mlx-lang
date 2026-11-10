#include <stdio.h>
namespace statics { unsigned run(); }
int main() { printf("%u\n", (unsigned)statics::run()); return 0; }
