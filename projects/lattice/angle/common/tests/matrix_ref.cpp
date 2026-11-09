// The C++ reference of the matrix_utils differential test: the same
// matrices and vectors as matrix_test.mlx, each result's floats as bits.
#include "common/matrix_utils.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
using namespace angle;
static void bits(float f) { uint32_t u; memcpy(&u, &f, 4); printf("%u ", u); }
static void matrix(const Mat4 &a) { const float *p = reinterpret_cast<const float *>(&a); for (int k = 0; k < 16; k++) bits(p[k]); printf("\n"); }
static void vector4(const Vector4 &v) { for (int k = 0; k < 4; k++) bits(v.data()[k]); printf("\n"); }
int main() {
    Mat4 identity;
    matrix(identity);
    Mat4 a(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);
    matrix(a);
    const float elements[16] = {0.5f, -1.0f, 2.0f, 0.25f, 1.5f, 3.0f, -2.5f, 0.0f, 4.0f, 0.125f, -8.0f, 1.0f, 0.75f, 2.25f, 6.0f, -0.5f};
    Mat4 b(elements);
    matrix(b);
    const Vector3 axes[4] = {Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 2.0f, 3.0f)};
    const float angles[4] = {90.0f, 45.0f, 30.0f, 123.0f};
    for (int k = 0; k < 4; k++) {
        matrix(Mat4::Rotate(angles[k], axes[k]));
        matrix(Mat4::Translate(axes[k]));
        matrix(Mat4::Scale(axes[k]));
    }
    Mat4 frustum = Mat4::Frustum(-1.0f, 1.0f, -0.75f, 0.75f, 0.1f, 100.0f);
    matrix(frustum);
    Mat4 perspective = Mat4::Perspective(60.0f, 1.5f, 0.5f, 50.0f);
    matrix(perspective);
    Mat4 ortho = Mat4::Ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.0f, 10.0f);
    matrix(ortho);
    matrix(a.product(b));
    matrix(frustum.product(perspective));
    Vector4 v(1.0f, 2.0f, 3.0f, 1.0f);
    vector4(a.product(v));
    vector4(ortho.product(v));
    return 0;
}
