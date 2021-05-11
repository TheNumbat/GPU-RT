
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec3 attribs;

layout(binding = 1, std430) readonly buffer SceneDescs {
	Scene_Obj objects[];
};

layout(binding = 2, std430) readonly buffer SceneLights {
	Scene_Light lights[];
};

layout(binding = 3, std430) readonly buffer Vertices { 
	Vertex v[]; 
} vertices[];

layout(binding = 4, std430) readonly buffer Indices {
	uint i[];
} indices[];

layout(binding = 5) uniform sampler2D Textures[];

layout(binding = 6) uniform accelerationStructureEXT TLAS;

vec3 light_sample(vec3 p) {
	
	uint l_idx = randu(payload.seed, 0, consts.n_lights);
	uint o_idx = lights[l_idx].index;
	uint n_tris = lights[l_idx].n_triangles;
	uint t_idx = randu(payload.seed, 0, n_tris);

	ivec3 ind = ivec3(indices[o_idx].i[3 * t_idx + 0],
					  indices[o_idx].i[3 * t_idx + 1],
					  indices[o_idx].i[3 * t_idx + 2]);

	Vertex v0 = vertices[o_idx].v[ind.x];
	Vertex v1 = vertices[o_idx].v[ind.y];
	Vertex v2 = vertices[o_idx].v[ind.z];

	vec3 bary = sampleTriangle(payload.seed);
	vec3 point = v0.pos_tx.xyz * bary.x + v1.pos_tx.xyz * bary.y + v2.pos_tx.xyz * bary.z;
	point = vec3(objects[o_idx].model * vec4(point, 1.0));

	return normalize(point - p);
}

float light_pdf(vec3 p, vec3 d) {

	float oacc = 0;
	for(uint l = 0; l < consts.n_lights; l++) {
		
		float tacc = 0;
		uint o_idx = lights[l].index;
		uint n_tris = lights[l].n_triangles;

		for(uint t = 0; t < n_tris; t++) {

			ivec3 ind = ivec3(indices[o_idx].i[3 * t + 0],
							  indices[o_idx].i[3 * t + 1],
							  indices[o_idx].i[3 * t + 2]);

			vec3 v0 = vertices[o_idx].v[ind.x].pos_tx.xyz;
			vec3 v1 = vertices[o_idx].v[ind.y].pos_tx.xyz;
			vec3 v2 = vertices[o_idx].v[ind.z].pos_tx.xyz;

			v0 = vec3(objects[o_idx].model * vec4(v0, 1.0));
			v1 = vec3(objects[o_idx].model * vec4(v1, 1.0));
			v2 = vec3(objects[o_idx].model * vec4(v2, 1.0));

			tacc += trianglePDF(p, d, v0, v1, v2);
		}

		oacc += tacc / float(n_tris);
	}

	return oacc / float(consts.n_lights);
}

void main() {

	uint objId = objects[gl_InstanceCustomIndexEXT].index;

	vec3 hitPos, hitNormal, hitTangent;
	vec2 hitTexCoord;
	{
		ivec3 ind = ivec3(indices[objId].i[3 * gl_PrimitiveID + 0],
						  indices[objId].i[3 * gl_PrimitiveID + 1],
						  indices[objId].i[3 * gl_PrimitiveID + 2]);

		Vertex v0 = vertices[objId].v[ind.x];
		Vertex v1 = vertices[objId].v[ind.y];
		Vertex v2 = vertices[objId].v[ind.z];

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

	vec3 d = gl_WorldRayDirectionEXT;
	if(dot(d, hitNormal) > 0) hitNormal = -hitNormal;

	vec3 T = hitTangent, N = hitNormal, B;
	if(use_tanspace && consts.use_normal_map == 1) {
		B = cross(N, T);
		N = normalize(mat3(T, B, N) * tanspaceNormal);
	}
	normalCoords(N, T, B);

	if(consts.use_nee == 1 && consts.n_lights > 0) {

		payload.emissive = payload.misWeight * emissive;
		if(payload.is_direct) return;

		payload.nextO = hitPos;

		if(any(greaterThan(emissive, vec3(0)))) {
			payload.depth = consts.max_depth;
			return;
		}
		
		if(metal_rough.y == 0 && consts.use_d_only == 0) {
			
			payload.nextD = reflect(d, N);
			payload.nextWeight = albedo;
			payload.misWeight = 1;

		} else {

			float exponent = 1 / metal_rough.y;
				
			vec3 next_event = light_sample(hitPos);
			float light_pdf_l = light_pdf(hitPos, next_event);

			if(light_pdf_l != 0) {
				float light_pdf_m = bp_pdf(exponent, d, next_event, N);
				vec3 light_atten = bp_eval(albedo, light_pdf_m);
				float mis = consts.use_d_only == 1 ? 1 : powerHeuristic(light_pdf_l, light_pdf_m);
				payload.shadowWeight = light_atten / light_pdf_l * mis;
				payload.shadowD = next_event;
			}

			vec3 scatter;
			if(!bp_sample(exponent, d, payload.seed, T, B, N, scatter)) {
				payload.depth = consts.max_depth;
				return;
			}

			float brdf_pdf_m = bp_pdf(exponent, d, scatter, N);

			if(brdf_pdf_m != 0) {
				float brdf_pdf_l = light_pdf(hitPos, scatter);
				vec3 brdf_atten = bp_eval(albedo, brdf_pdf_m);
				payload.nextWeight = brdf_atten / brdf_pdf_m;
				payload.misWeight = consts.use_d_only == 1 ? 0 : powerHeuristic(brdf_pdf_m, brdf_pdf_l);
			} else {
				payload.depth = consts.max_depth;
				return;
			}

			payload.nextD = scatter;
		}

	} else {
		
		payload.nextO = hitPos;
		payload.emissive = emissive;

		if(any(greaterThan(emissive, vec3(0)))) {
			payload.depth = consts.max_depth;
			return;
		}
		
		if(metal_rough.y == 0) {
			
			payload.nextD = reflect(d, N);
			payload.nextWeight = albedo;

		} else {

			vec3 scatter;
			float exponent = 1 / metal_rough.y;
			if(!bp_sample(exponent, d, payload.seed, T, B, N, scatter)) {
				payload.depth = consts.max_depth;
				return;
			}
			
			float pdf = bp_pdf(exponent, d, scatter, N);
			vec3 atten = bp_eval(albedo, pdf);
			if(pdf != 0) {
				payload.nextWeight = atten / pdf;
			} else {
				payload.depth = consts.max_depth;
				return;
			}
		
			payload.nextD = scatter;
		}
	}
}
