#include <stdio.h>
namespace system { unsigned run(); }
int main() { printf("%u\n", (unsigned)system::run()); return 0; }
