#include <stdio.h>
namespace library { int run(); }
int main() { printf("%d\n", library::run()); return 0; }
