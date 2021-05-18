
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define M_PI 3.141592
#define LARGE_DIST 10000000.0
#define EPS 0.00001

struct Scene_Obj {
	mat4 model;
	mat4 modelIT;
	vec4 albedo;
	vec4 emissive;
	vec4 metal_rough;
	int albedo_tex;
	int emissive_tex;
	int metal_rough_tex;
	int normal_tex;
	uint index;
};

struct Scene_Light {
	uint index;
	uint n_triangles;
};

struct Vertex {
	vec4 pos_tx;
	vec4 norm_ty;
	vec4 tangent;
};

struct CH_Payload {
	vec3 emissive;
	vec3 nextO;
	vec3 nextD;
	vec3 nextWeight;
	vec3 shadowWeight;
	vec3 shadowD;
	uint seed;
	int depth;
	float misWeight;
	bool is_direct;
};

struct HitInfo {
	vec3 pos, normal, tangent;
	vec2 texcoord;
};

struct MatInfo {
	vec3 albedo, emissive, tanspaceNormal;
	float roughness;
	bool use_tanspace;
};

struct ShadeInfo {
	vec3 wo, T, B, N;
};

layout(push_constant) uniform Constants 
{
	vec4 clearColor;
	vec4 envlight;
	int frame;
	int samples;
	int max_frame;
	int qmc;
	int max_depth;
	int use_normal_map;
	int use_nee;
	int use_rr;
	int use_d_only;
	int n_lights;
	int n_objs;
} consts;

// RNG //////////////////////////////////////////

uint tea(uint val0, uint val1) {
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;
	for(uint n = 0; n < 16; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

uint lcg(inout uint prev) {
	uint LCG_A = 1664525u;
	uint LCG_C = 1013904223u;
	prev = (LCG_A * prev + LCG_C);
	return prev & 0x00FFFFFF;
}

float randf(inout uint prev) {
	return (float(lcg(prev)) / float(0x01000000));
}

uint randu(inout uint prev, uint a, uint b) {
	return lcg(prev) % (b - a) + a;
}

// Sampling //////////////////////////////////////////

// Hammersley sequence code from https://learnopengl.com/PBR/IBL/Specular-IBL 
float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i)/float(N), radical_inverse(i));
}

vec3 uniformHemisphere(inout uint seed, vec3 x, vec3 y, vec3 z) {
    float Xi1 = randf(seed);
    float Xi2 = randf(seed);
    float costheta = Xi1;
	float sintheta = sqrt(1 - costheta * costheta);
    float phi = 2 * M_PI * Xi2;
    float xs = sintheta * cos(phi);
    float ys = costheta;
    float zs = sintheta * sin(phi);
    return x * xs + y * ys + z * zs;
}

vec3 cosineHemisphere(inout uint seed, vec3 x, vec3 y, vec3 z) {
	float phi = randf(seed);
	float cosT2 = randf(seed);
	float sinT = sqrt(1.0 - cosT2);
	vec3 direction = vec3(cos(2 * M_PI * phi) * sinT, sin(2 * M_PI * phi) * sinT, sqrt(cosT2));
	direction = direction.x * x + direction.y * y + direction.z * z;
	return direction;
}

vec3 cosinePowerHemisphere(float exponent, inout uint seed, vec3 x, vec3 y, vec3 z) {
	float phi = 2 * M_PI * randf(seed);
	float cosT = pow(randf(seed), 1.0 / (exponent + 1.0));
	float sinT = sqrt(1.0 - cosT*cosT);
	vec3 direction = vec3(cos(phi) * sinT, sin(phi) * sinT, cosT);
	direction = direction.x * x + direction.y * y + direction.z * z;
	return direction;
}

vec3 sampleTriangle(inout uint seed) {
    float u = sqrt(randf(seed));
    float v = randf(seed);
    float a = u * (1 - v);
    float b = u * v;
    return vec3(a, b, 1 - a - b);
}

// Triangles //////////////////////////////////////////

bool hitTriangle(vec3 o, vec3 d, vec3 pa, vec3 pb, vec3 pc, out vec3 hitp) {

	vec3 v1 = pb - pa;
	vec3 v2 = pc - pa;
	vec3 p = cross(d,v2);
	float det = dot(v1,p);

	if(abs(det) < EPS) return false;
	float invDet = 1 / det;

	vec3 s = o - pa;
	float u = dot(s,p)*invDet;
	if (u < 0 || u > 1) return false;

	vec3 q = cross(s,v1);
	float v = dot(d,q)*invDet;
	if (v < 0 || u + v > 1) return false;

	float t = dot(v2,q)*invDet;
	hitp = o + t * d;
	return t >= 0;
}

float trianglePDF(vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2) {
	vec3 hitp;
    if(hitTriangle(o, d, v0, v1, v2, hitp)) {
        float a = 2 / length(cross(v1 - v0, v2 - v0));
		vec3 dist = hitp - o;
		vec3 N = normalize(cross(v1 - v0, v2 - v0));
        float g = dot(dist, dist) / abs(dot(N, d));
        return a * g;
    }
    return 0;
}

// Misc //////////////////////////////////////////

void normalCoords(vec3 N, out vec3 Nt, out vec3 Nb) {
	if(abs(N.x) > abs(N.y))
		Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
	else
		Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
	Nb = cross(N, Nt);
}

float powerHeuristic(float a, float b) {
	return a*a / (a*a + b*b);
}

// Blinn-Phong Material //////////////////////////////////////////

bool bp_sample(float exp, vec3 d, inout uint seed, vec3 T, vec3 BT, vec3 N, out vec3 scatter) {
	vec3 H = cosinePowerHemisphere(exp, seed, T, BT, N);
	scatter = reflect(d, H);
	return dot(N, scatter) > 0;
}

float bp_pdf(float exp, vec3 d, vec3 scatter, vec3 N) {
	
	float oDn = dot(-d, N);
	float iDn = dot(scatter, N);
	if(oDn <= 0 || iDn <= 0) return 0;

	vec3 H = normalize(scatter - d);
	float cosine = max(dot(H, N), 0);
	float N_pdf = (exp + 1) / (2 * M_PI) * pow(cosine, exp);
	return N_pdf / (4 * dot(-d, H));
}

vec3 bp_eval(vec3 albedo, float pdf) {
	return albedo * pdf;
}

// GGX Material //////////////////////////////////////////

vec3 GGX_F(vec3 r0, float iDn) {
    float cos5 = pow(1 - iDn, 5);
    return r0 + (1 - r0) * cos5;
}

float GGX_G(float oDn, float iDn, float a2) {
	float sqr0 = sqrt(a2 + (1 - a2) * iDn * iDn);
	float sqr1 = sqrt(a2 + (1 - a2) * oDn * oDn);
	return 2 * oDn * iDn / (oDn * sqr0 + iDn * sqr1);
}

float GGX_D(float nDh, float a2) {
	float b = nDh * nDh * (a2 - 1) + 1;
	return a2 / (M_PI * b * b);
}

float GGX_pdf(MatInfo mat, ShadeInfo shade, vec3 wi) {

	float oDn = dot(-shade.wo, shade.N);
	float iDn = dot(wi, shade.N);
	if(oDn <= 0 || iDn <= 0) return 0;

	vec3 H = normalize(wi - shade.wo);
	
	float nDh = max(dot(H, shade.N), 0);
	float oDh = max(dot(wi, H), 0);
	float a2 = mat.roughness * mat.roughness;

    return GGX_D(nDh, a2) * nDh / (4 * oDh);
}

vec3 GGX_eval(MatInfo mat, ShadeInfo shade, vec3 wi) {

	float oDn = dot(-shade.wo, shade.N);
	float iDn = dot(wi, shade.N);
	if(oDn <= 0 || iDn <= 0) return vec3(0);

	vec3 H = normalize(wi - shade.wo);

	float nDh = max(dot(H, shade.N), 0);
	float a2 = mat.roughness * mat.roughness;

	return GGX_F(mat.albedo, iDn) * GGX_D(nDh, a2) * GGX_G(oDn, iDn, a2) / (4 * oDn);
}

bool GGX_sample(inout uint seed, MatInfo mat, ShadeInfo shade, out vec3 wi) {
	
    float a2 = mat.roughness * mat.roughness;

	vec2 Xi = vec2(randf(seed), randf(seed));
	float phi = 2.0 * M_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    vec3 dir;
    dir.x = cos(phi) * sinTheta;
    dir.y = sin(phi) * sinTheta;
    dir.z = cosTheta;

    vec3 H = shade.T * dir.x + shade.B * dir.y + shade.N * dir.z;
	wi = reflect(shade.wo, H);
    return dot(wi, shade.N) > 0;
}  
