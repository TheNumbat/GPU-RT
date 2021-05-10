
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 1) rayPayloadInEXT bool isShadowed;

void main() {
    isShadowed = false;
}
