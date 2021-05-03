

#pragma once 

#include <lib/mathlib.h>

struct OBB {

	OBB() : ext(-1.0f) {}

	BBox box() const {
		BBox ret;
        for(auto pt : points()) {
            ret.enclose(pt);
        }
		return ret;
	}

	BBox local_box() const {
		BBox ret;
		ret.min = -ext;
		ret.max = ext;
		return ret;
	}

	std::vector<Vec3> points() const {
		
		Vec3 c = -T.cols[3].xyz();
		Vec3 min = c - ext;
		Vec3 max = c + ext;

		std::vector<Vec3> v;
		Mat4 u = T;
        u.cols[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

		v.push_back(u * Vec3(min.x, min.y, min.z));
		v.push_back(u * Vec3(max.x, min.y, min.z));
		v.push_back(u * Vec3(min.x, max.y, min.z));
		v.push_back(u * Vec3(min.x, min.y, max.z));
		v.push_back(u * Vec3(max.x, max.y, min.z));
		v.push_back(u * Vec3(min.x, max.y, max.z));
		v.push_back(u * Vec3(max.x, min.y, max.z));
		v.push_back(u * Vec3(max.x, max.y, max.z));

		return v;
	}

	float surface_area() const {
		Vec3 e = hmax(2.0f * ext, Vec3(1e-5f));
        Vec3 d = Vec3(e.x*e.y*e.z) / e;
		return 2.0f * (d.x + d.y + d.z);
	}

	float volume() const {
		return 8.0f * ext.x * ext.y * ext.z;
	}

	bool valid() {
		return ext.x >= 0.0f && ext.y > 0.0f && ext.z >= 0.0f;
	}

    static OBB fitPCA(const std::vector<Vec3>& points);

	Vec3 ext;
	Mat4 T;
};