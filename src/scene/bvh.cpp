
#include "bvh.h"
#include <stack>

struct BVHBuildData {
    BVHBuildData(BBox bb, size_t start, size_t range, size_t dst)
        : bb(bb), start(start), range(range), node(dst) {
    }
    BBox bb;
    size_t start;
    size_t range;
    size_t node;
};

struct SAHBucketData {
    BBox bb;
    size_t num_prims;
};

BBox Triangle::bbox() const {
    BBox bb;
    bb.enclose(v0);
    bb.enclose(v1);
    bb.enclose(v2);
    return bb;
}

void BVH::build(const VK::Mesh& mesh, size_t max_leaf_size) {

    nodes.clear();
    triangles.clear();

    const auto& verts = mesh.verts();
    const auto& inds = mesh.inds();

    for(size_t i = 0; i < inds.size(); i += 3) {
        triangles.push_back({verts[inds[i]].pos, verts[inds[i + 1]].pos, verts[inds[i + 2]].pos});
    }

    std::stack<BVHBuildData> bstack;

    if(triangles.empty()) {
        return;
    }

    BBox bb;
    for(size_t i = 0; i < triangles.size(); ++i) {
        bb.enclose(triangles[i].bbox());
    }

    new_node(bb, 0, triangles.size(), 0, 0);
    build_rec(0, max_leaf_size);
    build_links(nodes[0], -1);
    build_parents(0);
}

void BVH::build_rec(size_t n, size_t max_leaf_size) {

    Node& node = nodes[n];
    if(node.size <= max_leaf_size) {
        return;
    }

    static const int kMaxNumBuckets = 24;
    static const float EPS = 0.00001f;

    size_t num_buckets = std::min(kMaxNumBuckets, node.size);
    std::vector<SAHBucketData> buckets(num_buckets);

    // with a default being the cost of doing no split.
    BBox split_Ba;
    BBox split_Bb;
    int split_dim = -1;
    float split_val = 0.0f;
    float split_cost = node.bbox.surface_area() * node.size;

    // try all three dimensions and find best split
    for(int dim = 0; dim < 3; dim++) {
        Vec3 extent = node.bbox.max - node.bbox.min;
        if(extent[dim] < EPS) {
            continue; // ignore flat dimension
        }

        // initialize SAH evaluation buckets
        float bucket_width = extent[dim] / num_buckets;
        for(size_t i = 0; i < num_buckets; ++i) {
            buckets[i].bb = BBox();
            buckets[i].num_prims = 0;
        }

        // compute bucket bboxes
        for(size_t i = node.start; i < node.start + node.size; ++i) {
            Triangle& p = triangles[i];
            BBox pbb = p.bbox();
            float d = (pbb.center()[dim] - node.bbox.min[dim]) / bucket_width;
            size_t b = clamp((int)d, 0, ((int)num_buckets) - 1);
            buckets[b].bb.enclose(pbb);
            buckets[b].num_prims++;
        }

        // evaluation split costs
        for(size_t idx = 1; idx < num_buckets; ++idx) {

            // Sa * Na
            size_t Na = 0;
            BBox Ba;
            for(size_t i = 0; i < idx; ++i) {
                Ba.enclose(buckets[i].bb);
                Na += buckets[i].num_prims;
            }

            // Sb * Nb
            size_t Nb = 0;
            BBox Bb;
            for(size_t i = idx; i < num_buckets; ++i) {
                Bb.enclose(buckets[i].bb);
                Nb += buckets[i].num_prims;
            }

            // sah cost & actual split value
            float cost = Na * Ba.surface_area() + Nb * Bb.surface_area();
            float val = node.bbox.min[dim] + idx * bucket_width;
            if(cost < split_cost) {
                split_Ba = Ba;
                split_Bb = Bb;
                split_dim = dim;
                split_val = val;
                split_cost = cost;
            }
        }
    }

    // edge case - if all dimensions are flat (all centroids on a single spot)
    // split equally into two build nodes with the same bbox
    if(split_dim == -1) {
        size_t startl = node.start;
        size_t rangel = node.size / 2;
        size_t startr = startl + rangel;
        size_t ranger = node.size - rangel;

        size_t l = new_node(nodes[n].bbox, startl, rangel);
        build_rec(l, max_leaf_size);

        size_t r = new_node(nodes[n].bbox, startr, ranger);
        build_rec(r, max_leaf_size);

        nodes[n].l = l;
        nodes[n].r = r;
        return;
    }

    auto it =
        std::partition(triangles.begin() + node.start, triangles.begin() + node.start + node.size,
                       [split_dim, split_val](const Triangle& p) {
                           return p.bbox().center()[split_dim] < split_val;
                       });

    size_t startl = node.start;
    size_t rangel = std::distance(triangles.begin(), it) - node.start;
    size_t startr = startl + rangel;
    size_t ranger = node.size - rangel;

    if(rangel == 0 || ranger == 0) {
        rangel = node.size / 2;
        startr = startl + rangel;
        ranger = node.size - rangel;
    }

    size_t l = new_node(split_Ba, startl, rangel);
    build_rec(l, max_leaf_size);

    size_t r = new_node(split_Bb, startr, ranger);
    build_rec(r, max_leaf_size);

    nodes[n].l = l;
    nodes[n].r = r;
}

void BVH::build_parents(int idx) {
    Node& node = nodes[idx];
    if(!node.is_leaf()) {
        nodes[node.l].parent = idx;
        nodes[node.r].parent = idx;
        build_parents(node.l);
        build_parents(node.r);
    }
}

void BVH::build_links(Node& node, int next_right) {

    if(node.is_leaf()) {
        node.hit = next_right;
        node.miss = next_right;
    } else {
        node.hit = node.l;
        node.miss = next_right;
        build_links(nodes[node.l], node.r);
        build_links(nodes[node.r], next_right);
    }
}

BVH::BVH(const VK::Mesh& mesh, size_t max_leaf_size) {
    build(mesh, max_leaf_size);
}

bool BVH::Node::is_leaf() const {
    return l == r;
}

size_t BVH::new_node(BBox box, size_t start, size_t size, size_t l, size_t r) {
    Node n;
    n.bbox = box;
    n.start = start;
    n.size = size;
    n.l = l;
    n.r = r;
    nodes.push_back(n);
    return nodes.size() - 1;
}

struct SAHBucketDataOBB {
    OBB bb;
    std::vector<Vec3> points;
    size_t num_prims;
};

void OBBBVH::build(const VK::Mesh& mesh, size_t max_leaf_size) {

    nodes.clear();
    triangles.clear();

    const auto& verts = mesh.verts();
    const auto& inds = mesh.inds();

    for(size_t i = 0; i < inds.size(); i += 3) {
        triangles.push_back({verts[inds[i]].pos, verts[inds[i + 1]].pos, verts[inds[i + 2]].pos});
    }

    std::stack<BVHBuildData> bstack;

    if(triangles.empty()) {
        return;
    }

    std::vector<Vec3> all_points;
    for(size_t i = 0; i < triangles.size(); ++i) {
        all_points.push_back(triangles[i].v0);
        all_points.push_back(triangles[i].v1);
        all_points.push_back(triangles[i].v2);
    }
    OBB root_bb = OBB::fitPCA(all_points);

    new_node(root_bb, 0, triangles.size(), 0, 0);
    build_rec(0, max_leaf_size);
    build_parents(0);
}

void OBBBVH::build_rec(size_t n, size_t max_leaf_size) {

    Node& node = nodes[n];
    if(node.size <= max_leaf_size) {
        return;
    }

    static const int kMaxNumBuckets = 24;
    static const float EPS = 0.00001f;

    size_t num_buckets = std::min(kMaxNumBuckets, node.size);
    std::vector<SAHBucketDataOBB> buckets(num_buckets);

    // with a default being the cost of doing no split.
    OBB split_Ba, split_Bb;
    int split_dim = -1;
    float split_val = 0.0f;
    float split_cost = node.bbox.surface_area() * node.size;

    // try all three dimensions and find best split
    for(int dim = 0; dim < 3; dim++) {
        Vec3 extent = node.bbox.box().max - node.bbox.box().min;
        if(extent[dim] < EPS) {
            continue; // ignore flat dimension
        }

        // initialize SAH evaluation buckets
        float bucket_width = extent[dim] / num_buckets;
        for(size_t i = 0; i < num_buckets; ++i) {
            buckets[i].bb = OBB();
            buckets[i].num_prims = 0;
        }

        // compute bucket bboxes
        for(size_t i = node.start; i < node.start + node.size; ++i) {
            Triangle& p = triangles[i];
            Vec3 center = (1.0f / 3.0f) * (p.v0 + p.v1 + p.v2);
            float d = (center[dim] - node.bbox.box().min[dim]) / bucket_width;
            size_t b = clamp((int)d, 0, ((int)num_buckets) - 1);
            buckets[b].points.push_back(p.v0);
            buckets[b].points.push_back(p.v1);
            buckets[b].points.push_back(p.v2);
            buckets[b].num_prims++;
        }

        // evaluation split costs
        for(size_t idx = 1; idx < num_buckets; ++idx) {

            // Sa * Na
            size_t Na = 0;
            std::vector<Vec3> Apoints;
            for(size_t i = 0; i < idx; ++i) {
                Apoints.insert(Apoints.end(), buckets[i].points.begin(), buckets[i].points.end());
                Na += buckets[i].num_prims;
            }

            // Sb * Nb
            size_t Nb = 0;
            std::vector<Vec3> Bpoints;
            for(size_t i = idx; i < num_buckets; ++i) {
                Bpoints.insert(Bpoints.end(), buckets[i].points.begin(), buckets[i].points.end());
                Nb += buckets[i].num_prims;
            }

            OBB Ba = OBB::fitPCA(Apoints);
            OBB Bb = OBB::fitPCA(Bpoints);

            if(!Ba.valid() || !Ba.valid()) continue;

            // sah cost & actual split value
            float cost = Na * Ba.surface_area() + Nb * Bb.surface_area();
            float val = node.bbox.box().min[dim] + idx * bucket_width;
            if(cost < split_cost) {
                split_Ba = Ba;
                split_Bb = Bb;
                split_dim = dim;
                split_val = val;
                split_cost = cost;
            }
        }
    }

    // edge case - if all dimensions are flat (all centroids on a single spot)
    // split equally into two build nodes with the same bbox
    if(split_dim == -1) {
        size_t startl = node.start;
        size_t rangel = node.size / 2;
        size_t startr = startl + rangel;
        size_t ranger = node.size - rangel;

        size_t l = new_node(nodes[n].bbox, startl, rangel);
        build_rec(l, max_leaf_size);

        size_t r = new_node(nodes[n].bbox, startr, ranger);
        build_rec(r, max_leaf_size);

        nodes[n].l = l;
        nodes[n].r = r;
        return;
    }

    auto it =
        std::partition(triangles.begin() + node.start, triangles.begin() + node.start + node.size,
                       [split_dim, split_val](const Triangle& p) {
                           return p.bbox().center()[split_dim] < split_val;
                       });

    size_t startl = node.start;
    size_t rangel = std::distance(triangles.begin(), it) - node.start;
    size_t startr = startl + rangel;
    size_t ranger = node.size - rangel;

    if(rangel == 0 || ranger == 0) {
        rangel = node.size / 2;
        startr = startl + rangel;
        ranger = node.size - rangel;
    }

    size_t l = new_node(split_Ba, startl, rangel);
    build_rec(l, max_leaf_size);

    size_t r = new_node(split_Bb, startr, ranger);
    build_rec(r, max_leaf_size);

    nodes[n].l = l;
    nodes[n].r = r;
}

void OBBBVH::build_parents(int idx) {
    Node& node = nodes[idx];
    if(!node.is_leaf()) {
        nodes[node.l].parent = idx;
        nodes[node.r].parent = idx;
        build_parents(node.l);
        build_parents(node.r);
    }
}

OBBBVH::OBBBVH(const VK::Mesh& mesh, size_t max_leaf_size) {
    build(mesh, max_leaf_size);
}

bool OBBBVH::Node::is_leaf() const {
    return l == r;
}

size_t OBBBVH::new_node(OBB box, size_t start, size_t size, size_t l, size_t r) {
    Node n;
    n.bbox = box;
    n.start = start;
    n.size = size;
    n.l = l;
    n.r = r;
    nodes.push_back(n);
    return nodes.size() - 1;
}

