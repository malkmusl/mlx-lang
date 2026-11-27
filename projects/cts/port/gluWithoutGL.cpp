// What the suite's Vulkan package takes from its OpenGL utilities (glu)
// that glu defines in files built on the OpenGL wrapper and its render
// contexts (gluShaderProgram.cpp, gluRenderContext.cpp), which this port
// does not build: it runs no OpenGL. Here, without OpenGL:
//
//   - the log of a program's shaders (vktTestPackage.cpp's `log <<
//     buildInfo`) and the log's name of a shader type (vkShaderProgram.cpp);
//   - whether a context type takes an API's version, for gluShaderUtil.cpp's
//     glslVersionIsSupported (no context of this port has OpenGL).
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluRenderContext.hpp"
#include "tcuTestLog.hpp"

namespace glu
{

qpShaderType getLogShaderType(ShaderType shaderType)
{
    switch (shaderType)
    {
    case SHADERTYPE_VERTEX:
        return QP_SHADER_TYPE_VERTEX;
    case SHADERTYPE_FRAGMENT:
        return QP_SHADER_TYPE_FRAGMENT;
    case SHADERTYPE_GEOMETRY:
        return QP_SHADER_TYPE_GEOMETRY;
    case SHADERTYPE_TESSELLATION_CONTROL:
        return QP_SHADER_TYPE_TESS_CONTROL;
    case SHADERTYPE_TESSELLATION_EVALUATION:
        return QP_SHADER_TYPE_TESS_EVALUATION;
    case SHADERTYPE_COMPUTE:
        return QP_SHADER_TYPE_COMPUTE;
    case SHADERTYPE_RAYGEN:
        return QP_SHADER_TYPE_RAYGEN;
    case SHADERTYPE_ANY_HIT:
        return QP_SHADER_TYPE_ANY_HIT;
    case SHADERTYPE_CLOSEST_HIT:
        return QP_SHADER_TYPE_CLOSEST_HIT;
    case SHADERTYPE_MISS:
        return QP_SHADER_TYPE_MISS;
    case SHADERTYPE_INTERSECTION:
        return QP_SHADER_TYPE_INTERSECTION;
    case SHADERTYPE_CALLABLE:
        return QP_SHADER_TYPE_CALLABLE;
    case SHADERTYPE_TASK:
        return QP_SHADER_TYPE_TASK;
    case SHADERTYPE_MESH:
        return QP_SHADER_TYPE_MESH;
    default:
        DE_ASSERT(false);
        return QP_SHADER_TYPE_LAST;
    }
}

tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderProgramInfo &shaderProgramInfo)
{
    const ProgramInfo &program = shaderProgramInfo.program;
    log << tcu::TestLog::ShaderProgram(program.linkOk, program.infoLog);
    bool allCompiled = true;
    for (const ShaderInfo &shader : shaderProgramInfo.shaders)
    {
        log << tcu::TestLog::Shader(getLogShaderType(shader.type), shader.source, shader.compileOk, shader.infoLog);
        allCompiled = allCompiled && shader.compileOk;
    }
    log << tcu::TestLog::EndShaderProgram;
    for (const ShaderInfo &shader : shaderProgramInfo.shaders)
    {
        const std::string name = std::string(getShaderTypeName(shader.type)) + "CompileTime";
        log << tcu::TestLog::Float(name, name, "ms", QP_KEY_TAG_TIME, (float)shader.compileTimeUs / 1000.0f);
    }
    if (allCompiled)
        log << tcu::TestLog::Float("LinkTime", "Link time", "ms", QP_KEY_TAG_TIME, (float)program.linkTimeUs / 1000.0f);
    return log;
}

// The context's profile is the one asked (a compatibility context takes
// the core profile's versions too) and its version is that one or later.
bool contextSupports(ContextType ctxType, ApiType requiredApiType)
{
    const ApiType api = ctxType.getAPI();
    const bool profile = api.getProfile() == requiredApiType.getProfile() ||
                         (api.getProfile() == PROFILE_COMPATIBILITY && requiredApiType.getProfile() == PROFILE_CORE);
    if (!profile)
        return false;
    if (api.getMajorVersion() != requiredApiType.getMajorVersion())
        return api.getMajorVersion() > requiredApiType.getMajorVersion();
    return api.getMinorVersion() >= requiredApiType.getMinorVersion();
}

} // namespace glu
