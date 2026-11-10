#include <stdio.h>
namespace hides { unsigned run(); }
int main() { printf("%u\n", (unsigned)hides::run()); return 0; }
