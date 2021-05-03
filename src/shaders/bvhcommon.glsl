
#define INF (1.0 / 0.0)
#define EPS 0.000001
#define LARGE_DIST 100000000
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define DBL_MAX 1.7976931348623158e+308
#define DBL_MIN 2.2250738585072014e-308

#pragma optionNV (unroll all)

layout (local_size_x = 32) in;

struct Triangle {
    vec4 v0, v1, v2;
};

layout (binding = 0, std140) readonly buffer Queries
{
	vec4 queries[];
};

layout (binding = 1, std140) writeonly buffer Output
{
	vec4 results[];
};

layout (binding = 2, std140) readonly buffer Primitives
{
	Triangle triangles[];
};

layout (push_constant) uniform Constants
{
	int n_nodes;
	int n_tris;
	int start;
	int trace_rays;
	int stackless;
	int sort_children;
};

void traverse_cpq(vec3 q, inout vec3 closest, inout float best_dist);
void traverse_ray(vec3 o, vec3 d, inout vec3 hit, inout float best_dist);

void main() {
    if(trace_rays == 1) {
        
        uint q_idx = gl_GlobalInvocationID.x;
        vec3 o = queries[2 * (start + q_idx)].xyz;
        vec3 d = queries[2 * (start + q_idx) + 1].xyz;

        float hit_dist = LARGE_DIST;
        vec3 hit_point = vec3(INF);
        traverse_ray(o, d, hit_point, hit_dist);
        results[start + q_idx] = vec4(hit_point, 0.0f);

    } else {

        uint q_idx = gl_GlobalInvocationID.x;
        vec3 query = queries[start + q_idx].xyz;

        float best_dist = LARGE_DIST;
        vec3 best_point = vec3(INF);
        traverse_cpq(query, best_point, best_dist);
        results[start + q_idx] = vec4(best_point, 0.0f);
    }
}

float max4(vec3 v, float f) {
	return max(max(v.x, v.y),max(v.z, f));
}

float min4(vec3 v, float f) {
	return min(min(v.x, v.y),min(v.z, f));
}

bool hit_bbox(vec3 o, vec3 d, vec3 pMin, vec3 pMax, float maxd, out float mind) {
	vec3 invD = 1.0f / d;
	vec3 t0 = (pMin - o) * invD;
	vec3 t1 = (pMax - o) * invD;
	vec3 tNear = min(t0,t1);
	vec3 tFar = max(t0,t1);
	float tNearMax = max4(tNear, 0.0f);
	float tFarMin = min4(tFar, maxd);
	mind = tNearMax;
	return tNearMax <= tFarMin;
}

bool hit_obb(vec3 o, vec3 d, mat4 T, vec3 ext, float maxd, out float mind) {
	vec3 oT = (T * vec4(o, 1)).xyz;
	vec3 dT = (T * vec4(d, 0)).xyz;
	return hit_bbox(oT, dT, -ext, ext, maxd, mind);
}

void box_dist(vec3 q, vec3 bmin, vec3 bmax, out float close, out float far) {
	vec3 u = bmin - q;
	vec3 v = q - bmax;
	vec3 cl = max(max(u,v),0);
	vec3 fr = min(u,v);
	close = length(cl);
	far = length(fr);
}

void obb_dist(vec3 q, mat4 T, vec3 ext, out float close, out float far) {
	vec3 qT = (T * vec4(q, 1)).xyz;
	box_dist(qT, -ext, ext, close, far);
}

bool hit_triangle(vec3 o, vec3 d, out vec3 hitp, out float hitd, Triangle tri) {
	
	vec3 pa = tri.v0.xyz;
	vec3 pb = tri.v1.xyz;
	vec3 pc = tri.v2.xyz;

	vec3 v1 = pb - pa;
	vec3 v2 = pc - pa;
	vec3 p = cross(d,v2);
	float det = dot(v1,p);

	if(abs(det) < EPS) return false;
	float invDet = 1.0 / det;

	vec3 s = o - pa;
	float u = dot(s,p)*invDet;
	if (u < 0 || u > 1) return false;

	vec3 q = cross(s,v1);
	float v = dot(d,q)*invDet;
	if (v < 0 || u + v > 1) return false;

	float t = dot(v2,q)*invDet;
	if (t >= 0.0f) {
		hitd = t;
		hitp = o + t * d;
		return true;
	}
	return false;
}

vec3 cp_triangle(vec3 q, Triangle t) {

	vec3 v0 = t.v0.xyz;
	vec3 v1 = t.v1.xyz;
	vec3 v2 = t.v2.xyz;

	vec3 ab = v1 - v0;
	vec3 ac = v2 - v0;
	vec3 ax = q - v0;

	float d1 = dot(ab, ax);
	float d2 = dot(ac, ax);
	if (d1 <= 0.0f && d2 <= 0.0f) {
        return v0;
	}

	vec3 bx = q - v1;
	float d3 = dot(ab, bx);
	float d4 = dot(ac, bx);
	if (d3 >= 0.0f && d4 <= d3) {
        return v1;
	}

	vec3 cx = q - v2;
	float d5 = dot(ab, cx);
	float d6 = dot(ac, cx);
	if (d6 >= 0.0f && d5 <= d6) {
        return v2;
	}

	float vc = d1*d4 - d3*d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		float v = d1/(d1 - d3);
		return v0 + ab*v;
	}

	float vb = d5*d2 - d1*d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		float w = d2/(d2 - d6);
		return v0 + ac*w;
	}

	float va = d3*d6 - d5*d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		float w = (d4 - d3)/((d4 - d3) + (d5 - d6));
		return v1 + (v2 - v1)*w;
	}

	float denom = 1.0f/(va + vb + vc);
	float v = vb*denom;
	float w = vc*denom;
	return v0 + ab*v + ac*w;
}

