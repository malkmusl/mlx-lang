// A member of a class template's partial specialization defined after
// instances of it were made (an explicit instantiation's, a call's): the
// instances get it as those of the primary template do (ANGLE's
// SetFloatUniformMatrixGLSL<cols, 4>::Run in renderer_utils.cpp).
#include <stdio.h>

template <int cols, int rows> struct Matrix { static int run(int x); };
template <int cols> struct Matrix<cols, 4> { static int run(int x); };

template int Matrix<2, 4>::run(int);
int early(int x) { return Matrix<3, 4>::run(x) + Matrix<2, 3>::run(x); }

template <int cols> int Matrix<cols, 4>::run(int x) { return x * cols * 100; }
template <int cols, int rows> int Matrix<cols, rows>::run(int x) { return x * cols * rows; }

template <int c, int r> int call(int x) { return Matrix<c, r>::run(x); }

int main() {
    printf("%d %d %d %d\n", call<2, 4>(1), call<4, 4>(1), call<2, 3>(1), early(1));
    return 0;
}
