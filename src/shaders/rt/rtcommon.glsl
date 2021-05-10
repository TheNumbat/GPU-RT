
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

struct hitPayload {
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

layout(push_constant) uniform Constants 
{
	vec4 clearColor;
	vec4 envlight;
	int frame;
	int samples;
	int max_depth;
	int use_normal_map;
	int use_nee;
	int n_lights;
	int n_objs;
} consts;

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
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

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev) {
	uint LCG_A = 1664525u;
	uint LCG_C = 1013904223u;
	prev = (LCG_A * prev + LCG_C);
	return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float randf(inout uint prev) {
	return (float(lcg(prev)) / float(0x01000000));
}

uint randu(inout uint prev, uint a, uint b) {
	return lcg(prev) % (b - a) + a;
}

vec3 cosineHemisphere(inout uint seed, vec3 x, vec3 y, vec3 z) {
	float phi = randf(seed);
	float cosT2 = randf(seed);
	float sinT = sqrt(1.0 - cosT2);
	vec3 direction = vec3(cos(2 * M_PI * phi) * sinT, sin(2 * M_PI * phi) * sinT, sqrt(cosT2));
	direction = direction.x * x + direction.y * y + direction.z * z;
	return direction;
}

void normalCoords(vec3 N, out vec3 Nt, out vec3 Nb) {
	if(abs(N.x) > abs(N.y))
		Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
	else
		Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
	Nb = cross(N, Nt);
}

vec3 cosinePowerHemisphere(float exponent, inout uint seed, vec3 x, vec3 y, vec3 z) {
	float phi = randf(seed);
	float cosT = pow(randf(seed), 1.0 / (exponent + 1.0));
	float sinT = sqrt(1.0 - cosT*cosT);
	vec3 direction = vec3(cos(2 * M_PI * phi) * sinT, sin(2 * M_PI * phi) * sinT, cosT);
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

bool bp_sample(float exp, vec3 d, inout uint seed, vec3 T, vec3 BT, vec3 N, out vec3 scatter) {
	vec3 samp_N = cosinePowerHemisphere(exp, seed, T, BT, N);
	scatter = reflect(d, samp_N);
	return dot(N, scatter) > 0;
}

float bp_pdf(float exp, vec3 d, vec3 scatter, vec3 N) {
	float iDn = dot(-d, N);
	if(iDn <= 0 || dot(scatter, N) <= 0) return 0;
	vec3 samp_N = normalize(scatter - d);
	float cosine = max(dot(samp_N, N), 0);
	float N_pdf = (exp + 1) / (2 * M_PI) * pow(cosine, exp);
	return N_pdf / (4 * dot(-d, samp_N));
}

vec3 bp_eval(vec3 albedo, float pdf) {
	return albedo * pdf;
}

float powerHeuristic(float a, float b) {
	return a*a / (a*a + b*b);
}
