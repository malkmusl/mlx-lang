// ANGLE's SPIR-V helpers (src/common/spirv/angle_spirv_utils.cpp) without
// SPIRV-Tools, which WebKit's copy of ANGLE does not have and the port
// builds no third-party library for: Validate is only ever asked in an
// ASSERT (compiled out of the port's release build of ANGLE, NDEBUG), so it
// answers that the module is valid; Print, a debugging aid, says that no
// disassembler is here.
#include "common/spirv/spirv_types.h"

#include "common/log_utils.h"

namespace angle
{
namespace spirv
{
bool Validate(const Blob &blob)
{
    return true;
}

void Print(const Blob &blob)
{
    INFO() << "SPIR-V of " << blob.size() << " words (no disassembler: SPIRV-Tools is not built)";
}
}  // namespace spirv
}  // namespace angle
