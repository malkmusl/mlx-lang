// A class converting to a pointer indexed by the built-in subscript (Skia's
// PODArray: `op.preViewMatrices[i]`).
#include <cstdio>
template <typename T> class PODArray {
public:
    PODArray() {}
    PODArray(T *ptr) : fPtr(ptr) {}
    operator T *() const { return fPtr; }
    T *operator->() const { return fPtr; }
private:
    T *fPtr;
};
struct M { int v; void bump(int *x) const { *x += v; } };
struct Op { PODArray<M> mats; int n; };
static int run(const Op &op) {
    int total = 0;
    for (int i = 0; i < op.n; i++) op.mats[i].bump(&total);
    return total + op.mats->v;
}
int main(int argc, char **argv) {
    M ms[2] = {{3}, {4}};
    Op op{PODArray<M>(ms), 2};
    printf("%d\n", run(op));
    return argc;
}
