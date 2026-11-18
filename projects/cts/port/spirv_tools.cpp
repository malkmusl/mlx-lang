// The part of SPIRV-Tools' API the suite's Vulkan utilities call
// (vkSpirVAsm.cpp, vkPrograms.cpp), declared by SPIRV-Tools' own headers
// (include/spirv-tools at the commit external/fetch_sources.py pins), for
// a port that does not build SPIRV-Tools yet: its build generates tables
// from the SPIR-V grammars with Python scripts, for which this
// repository has no generator of its own yet (projects/cts/README.md).
//
// What this answers: assembly (spvTextToBinaryWithOptions) throws
// tcu::NotSupportedError, which the suite's program building turns into
// "loading stored binary instead" and a test result that says the
// program is not there: a test with SPIR-V assembly does not run.
// Disassembly throws tcu::NotSupportedError too: the suite asks for it
// only for the shader log, where that error becomes a note in the log
// (vktTestPackage.cpp) and the case runs. Validation answers
// SPV_UNSUPPORTED (only with --deqp-spirv-validation), the optimizer's
// Run false (only with --deqp-optimization-recipe).
#include "spirv-tools/libspirv.h"
#include "spirv-tools/optimizer.hpp"
#include "tcuDefs.hpp"

#include <stdlib.h>

struct spv_context_t
{
    spv_target_env env;
};

struct spv_validator_options_t
{
    int unused;
};

struct spv_optimizer_options_t
{
    bool runValidator;
};

spv_context spvContextCreate(spv_target_env env)
{
    spv_context context = (spv_context)calloc(1, sizeof(spv_context_t));
    if (context)
        context->env = env;
    return context;
}

void spvContextDestroy(spv_context context)
{
    free(context);
}

spv_result_t spvTextToBinaryWithOptions(const spv_const_context, const char *, const size_t, const uint32_t,
                                        spv_binary *binary, spv_diagnostic *diagnostic)
{
    if (binary)
        *binary = nullptr;
    if (diagnostic)
        *diagnostic = nullptr;
    TCU_THROW(NotSupportedError, "SPIR-V assembly: SPIRV-Tools is not built in this port yet");
}

spv_result_t spvBinaryToText(const spv_const_context, const uint32_t *, const size_t, const uint32_t, spv_text *text,
                             spv_diagnostic *diagnostic)
{
    if (text)
        *text = nullptr;
    if (diagnostic)
        *diagnostic = nullptr;
    TCU_THROW(NotSupportedError, "SPIR-V disassembly: SPIRV-Tools is not built in this port yet");
}

spv_result_t spvValidateWithOptions(const spv_const_context, const spv_const_validator_options, const spv_const_binary,
                                    spv_diagnostic *diagnostic)
{
    if (diagnostic)
        *diagnostic = nullptr;
    return SPV_UNSUPPORTED;
}

void spvBinaryDestroy(spv_binary binary)
{
    free(binary);
}

void spvTextDestroy(spv_text text)
{
    free(text);
}

void spvDiagnosticDestroy(spv_diagnostic diagnostic)
{
    free(diagnostic);
}

spv_validator_options spvValidatorOptionsCreate(void)
{
    return (spv_validator_options)calloc(1, sizeof(spv_validator_options_t));
}

void spvValidatorOptionsDestroy(spv_validator_options options)
{
    free(options);
}

void spvValidatorOptionsSetRelaxBlockLayout(spv_validator_options, bool)
{
}

void spvValidatorOptionsSetUniformBufferStandardLayout(spv_validator_options, bool)
{
}

void spvValidatorOptionsSetScalarBlockLayout(spv_validator_options, bool)
{
}

void spvValidatorOptionsSetWorkgroupScalarBlockLayout(spv_validator_options, bool)
{
}

void spvValidatorOptionsSetSkipBlockLayout(spv_validator_options, bool)
{
}

void spvValidatorOptionsSetAllowLocalSizeId(spv_validator_options, bool)
{
}

spv_optimizer_options spvOptimizerOptionsCreate(void)
{
    return (spv_optimizer_options)calloc(1, sizeof(spv_optimizer_options_t));
}

void spvOptimizerOptionsDestroy(spv_optimizer_options options)
{
    free(options);
}

void spvOptimizerOptionsSetRunValidator(spv_optimizer_options options, bool run)
{
    if (options)
        options->runValidator = run;
}

namespace spvtools
{

struct Optimizer::Impl
{
    spv_target_env env;
};

Optimizer::Optimizer(spv_target_env env) : impl_(new Impl())
{
    impl_->env = env;
}

Optimizer::~Optimizer()
{
}

Optimizer &Optimizer::RegisterPerformancePasses()
{
    return *this;
}

Optimizer &Optimizer::RegisterSizePasses()
{
    return *this;
}

bool Optimizer::Run(const uint32_t *, const size_t, std::vector<uint32_t> *, const spv_optimizer_options) const
{
    return false;
}

} // namespace spvtools
