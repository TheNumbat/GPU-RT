
#version 460
#extension GL_EXT_ray_tracing : enable

layout (location = 0) rayPayloadInEXT vec3 intersection;

void main() {
    intersection = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
