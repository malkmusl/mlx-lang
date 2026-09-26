#version 450
// Sample shaders compiled by glslang in tests/run_vulkan.sh and validated
// with std.spirv.module.
layout(location = 0) in vec3 position;
layout(location = 0) out vec2 uv;
layout(set = 0, binding = 0) uniform Camera { mat4 view; mat4 projection; } camera;
void main() { uv = position.xy * 0.5 + 0.5; gl_Position = camera.projection * camera.view * vec4(position, 1.0); }
