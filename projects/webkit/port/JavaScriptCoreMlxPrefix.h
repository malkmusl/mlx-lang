// The Mlx port's JavaScriptCore prefix header, included after
// JavaScriptCorePrefix.h in every JavaScriptCore source (build.mlx's
// javaScriptCore): what the port's configuration needs that WebKit's
// unified bundles include only for the configurations it builds.
#pragma once

#ifdef __cplusplus

// The JIT without WebAssembly: ScriptExecutable.cpp's setupJIT compares
// with CompilationResult::CompilationSuccessful, an enumerator of the
// enum CodeBlock.h only declares; its bundle (runtime-39) includes the
// definition only through WebAssembly's headers.
#include "CompilationResult.h"

#endif // __cplusplus
