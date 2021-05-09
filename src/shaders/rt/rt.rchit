
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;

hitAttributeEXT vec3 attribs;

layout(binding = 1, std430) readonly buffer SceneDescs {
	Scene_Obj objects[];
};

layout(binding = 2, scalar) readonly buffer Vertices { 
	Vertex v[]; 
} vertices[];

layout(binding = 3) readonly buffer Indices {
	uint i[];
} indices[];

layout(binding = 4) uniform sampler2D Textures[];

layout(binding = 5) uniform accelerationStructureEXT TLAS;

void main() {

	uint objId = objects[gl_InstanceCustomIndexEXT].index;

	vec3 hitPos, hitNormal, hitTangent;
	vec2 hitTexCoord;
	{
		ivec3 ind = ivec3(indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 0],
						  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1],
						  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);

		Vertex v0 = vertices[nonuniformEXT(objId)].v[ind.x];
		Vertex v1 = vertices[nonuniformEXT(objId)].v[ind.y];
		Vertex v2 = vertices[nonuniformEXT(objId)].v[ind.z];

		const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

		hitNormal = v0.norm_ty.xyz * barycentrics.x + v1.norm_ty.xyz * barycentrics.y + v2.norm_ty.xyz * barycentrics.z;
		hitNormal = normalize(vec3(objects[gl_InstanceCustomIndexEXT].modelIT * vec4(hitNormal, 0.0)));

		vec3 t0 = v0.tangent.xyz * v0.tangent.w;
		vec3 t1 = v1.tangent.xyz * v1.tangent.w;
		vec3 t2 = v2.tangent.xyz * v2.tangent.w;
		hitTangent = t0 * barycentrics.x + t1 * barycentrics.y + t2 * barycentrics.z;
		hitTangent = normalize(vec3(objects[gl_InstanceCustomIndexEXT].modelIT * vec4(hitTangent, 0.0)));

		hitPos = v0.pos_tx.xyz * barycentrics.x + v1.pos_tx.xyz * barycentrics.y + v2.pos_tx.xyz * barycentrics.z;
		hitPos = vec3(objects[gl_InstanceCustomIndexEXT].model * vec4(hitPos, 1.0));

		vec2 tc0 = vec2(v0.pos_tx.w, v0.norm_ty.w);
		vec2 tc1 = vec2(v1.pos_tx.w, v1.norm_ty.w);
		vec2 tc2 = vec2(v2.pos_tx.w, v2.norm_ty.w);
		hitTexCoord = tc0 * barycentrics.x + tc1 * barycentrics.y + tc2 * barycentrics.z;
	}

	vec3 albedo, emissive, tanspaceNormal;
	bool use_tanspace;
	vec2 metal_rough;
	{
		int albedoIdx = objects[gl_InstanceCustomIndexEXT].albedo_tex;
		albedo = objects[gl_InstanceCustomIndexEXT].albedo.xyz;
		if(albedoIdx >= 0) {
			albedo = texture(Textures[albedoIdx], hitTexCoord).xyz;
		}

		int emissiveIdx = objects[gl_InstanceCustomIndexEXT].emissive_tex;
		emissive = objects[gl_InstanceCustomIndexEXT].emissive.xyz;
		if(emissiveIdx >= 0) {
			emissive = texture(Textures[emissiveIdx], hitTexCoord).xyz;
		}

		int mrIdx = objects[gl_InstanceCustomIndexEXT].metal_rough_tex;
		metal_rough = objects[gl_InstanceCustomIndexEXT].metal_rough.xy;
		if(mrIdx >= 0) {
			metal_rough = texture(Textures[mrIdx], hitTexCoord).xy;
		}

		int nIdx = objects[gl_InstanceCustomIndexEXT].normal_tex;
		use_tanspace = nIdx >= 0;
		if(use_tanspace) {
			tanspaceNormal = texture(Textures[nIdx], hitTexCoord).xyz * 2.0 - 1.0;
		}
	}

	{
		vec3 d = gl_WorldRayDirectionEXT;
		if(dot(d, hitNormal) > 0) hitNormal = -hitNormal;

		vec3 T = hitTangent, N = hitNormal, B;
		vec3 mapN = N;
		if(use_tanspace && consts.use_normal_map == 1) {
			B = cross(N, T);
			N = normalize(mat3(T, B, N) * tanspaceNormal);
		}
		normalCoords(N, T, B);

		vec3 scatter;

		if(metal_rough.y == 0) {
			
			scatter = reflect(d, N);
			payload.nextWeight = albedo;

		} else {

			float exponent = 1 / metal_rough.y;
			bp_sample(exponent, d, payload.seed, T, B, N, scatter);
			
			float pdf = bp_pdf(exponent, d, scatter, N);
			vec3 atten = bp_eval(albedo, pdf) * abs(dot(scatter, mapN));
			if(pdf > 0) {
				payload.nextWeight = atten / pdf;
			} else {
				payload.depth = consts.max_depth;
			}
		}
		
		payload.emissive = emissive;
		payload.nextO = hitPos;
		payload.nextD = scatter;
	}
}
