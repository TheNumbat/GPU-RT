
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Constants
{
	float exposure;
    float gamma;
} consts;

layout(location = 0) in vec2 fragTexcoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

void main() {
    vec4 t = texture(image, fragTexcoord);
    vec3 c = vec3(1) - exp(-t.xyz * consts.exposure);
    vec3 g = vec3(1.0 / consts.gamma);
    outColor = vec4(pow(c, g), t.w);
}
