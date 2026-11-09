// The C++ reference of the utilities differential test: the same values
// and functions as utilities_test.mlx, one line per value.
#include "common/utilities.h"
#include <cstdio>
#include <string>
namespace gl { namespace priv { std::ostream *gSwallowStream = nullptr; } }
// string_utils.cpp (BeginsWith, for IsBuiltInName) reads the environment through
// system_utils.cpp; this test asks nothing of the environment.
namespace angle { std::string GetEnvironmentVarOrAndroidProperty(const char *, const char *) { return std::string(); } }
static void number(unsigned long long v) { printf("%llu ", v); }
static void sgn(int v) { number(static_cast<unsigned>(v)); }
static void flag(bool v) { number(v ? 1 : 0); }
int main() {
    for (unsigned v = 0x1400; v < 0x9400; v++) {
        number(v);
        sgn(gl::VariableComponentCount(v));
        number(gl::VariableComponentType(v));
        number(gl::VariableComponentSize(v));
        number(gl::VariableInternalSize(v));
        number(gl::VariableExternalSize(v));
        sgn(gl::VariableRowCount(v));
        sgn(gl::VariableColumnCount(v));
        flag(gl::IsSamplerType(v));
        flag(gl::IsSamplerCubeType(v));
        flag(gl::IsSamplerYUVType(v));
        flag(gl::IsImageType(v));
        flag(gl::IsImage2DType(v));
        flag(gl::IsAtomicCounterType(v));
        flag(gl::IsOpaqueType(v));
        flag(gl::IsMatrixType(v));
        flag(gl::IsFloatScalarAndVectorType(v));
        flag(gl::IsFloatVectorType(v));
        number(gl::TransposeMatrixType(v));
        sgn(gl::VariableRegisterCount(v));
        sgn(gl::VariableSortOrder(v));
        number(gl::VariableBoolVectorType(v));
        flag(gl::IsIntegerFormat(v));
        number(gl::ElementTypeSize(v));
        flag(gl::IsMipmapFiltered(v));
        flag(egl::IsCubeMapTextureTarget(v));
        flag(egl::IsTextureTarget(v));
        flag(egl::IsRenderbufferTarget(v));
        flag(egl::IsExternalImageTarget(v));
        if (gl::IsMatrixType(v)) {
            sgn(gl::MatrixRegisterCount(v, true));
            sgn(gl::MatrixComponentCount(v, false));
        }
        printf("\n");
    }
    for (unsigned mode = 0; mode < 16; mode++) {
        flag(gl::IsTriangleMode(static_cast<gl::PrimitiveMode>(mode)));
        flag(gl::IsPolygonMode(static_cast<gl::PrimitiveMode>(mode)));
    }
    printf("\n");
    for (unsigned index = 0; index < 3; index++) number(gl::GetPrimitiveRestartIndex(static_cast<gl::DrawElementsType>(index)));
    printf("\n");
    return 0;
}
