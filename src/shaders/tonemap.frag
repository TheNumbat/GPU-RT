
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Constants
{
	float exposure;
    float gamma;
    int type;
} consts;

layout(location = 0) in vec2 fragTexcoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

vec3 Uncharted2Tonemap(vec3 color) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec4 tonemapUT(vec4 color) {
	vec3 outcol = Uncharted2Tonemap(color.rgb * consts.exposure);
	outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	return vec4(pow(outcol, vec3(1.0f / consts.gamma)), color.a);
}

vec4 tonemapExp(vec4 color) {
    vec3 c = vec3(1) - exp(-color.xyz * consts.exposure);
    vec3 g = vec3(1.0 / consts.gamma);
    return vec4(pow(c, g), color.w);
}

void main() {
    vec4 t = texture(image, fragTexcoord);
    if(consts.type == 0) {
        outColor = tonemapUT(t);
    } else if(consts.type == 1) {
        outColor = tonemapExp(t);
    } else if(consts.type == 2) {
        outColor = t;
    }
}
