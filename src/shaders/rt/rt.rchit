
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload payload;

hitAttributeEXT vec3 attribs;

void main() {
	payload.hit = true;
	payload.barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);;
	payload.obj_id = gl_InstanceCustomIndexEXT;
	payload.prim_id = gl_PrimitiveID;
}
