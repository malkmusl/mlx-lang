#include "shared.h"
void bump();
int main() {
    bump();
    single().n++; plain()++; tsingle<Counter>().n += 10; sharedCount += 4; twice()++;
    printf("%d %d %d %d %d\n", single().n, plain(), tsingle<Counter>().n, sharedCount, twice());
    return 0;
}
