#version 450
// Sample shaders compiled by glslang in tests/run_vulkan.sh and validated
// with std.spirv.module.
layout(location = 0) in vec2 uv;
layout(location = 1) flat in int which;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D textures[4];
void main() {
    vec4 value = texture(textures[which], uv, 0.5);
    if (value.a < 0.1) discard;
    color = value.a > 0.5 ? value : vec4(textureOffset(textures[0], uv, ivec2(1, 0)).rgb, 1.0);
}
