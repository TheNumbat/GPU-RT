
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT vec3 payload;

void main() {
    payload = clearColor.xyz;
}
