
#pragma once

#include <lib/mathlib.h>
#include <vk/render.h>
#include <scene/obb.h>

struct Triangle {
    Vec3 v0, v1, v2;
    BBox bbox() const;
};

template<int W>
class WBVH {
public:
    static_assert(W >= 1);
    static constexpr int N = 1 << W;
    struct alignas(16) Node {
        Vec4 bmin[N];
        Vec4 bmax[N];
        int next[N];
        int leaf[N];
        Node() {
            for(int i = 0; i < N; i++) {
                next[i] = leaf[i] = -1;
            }
        }
    };
    std::vector<Node> nodes;
};

class BVH {
public:
    struct Node {
        BBox bbox;
        int start, size;
        int l, r;
        int hit, miss;
        int parent;
        bool is_leaf() const;
    };

    BVH() = default;
    BVH(const VK::Mesh& mesh, size_t max_leaf_size = 1);
    void build(const VK::Mesh& mesh, size_t max_leaf_size = 1);

    BVH(BVH&& src) = default;
    BVH& operator=(BVH&& src) = default;

    BVH(const BVH& src) = delete;
    BVH& operator=(const BVH& src) = delete;

    const std::vector<Node>& get_nodes() const {
        return nodes;
    };
    const std::vector<Triangle>& get_triangles() const {
        return triangles;
    };
    BBox box() const {
        return nodes[0].bbox;
    }

    template<int W> 
    WBVH<W> make_wide() {
        WBVH<W> wbvh;
        wbvh.nodes.push_back({});
        build_wide(wbvh, 0, 0);
        return wbvh;
    }

    template<int W>
    int gather(WBVH<W>::Node& wn, int n, int s, int d) {
        Node& node = nodes[n];
        if(d == W || node.l == node.r) {
            wn.bmin[s] = Vec4(node.bbox.min, 0.0f);
            wn.bmax[s] = Vec4(node.bbox.max, 0.0f);
            wn.next[s] = n;
            wn.leaf[s] = 0;
            if(node.l == node.r) {
                wn.bmin[s] = Vec4(BBox().min, 0.0f);
                wn.bmax[s] = Vec4(BBox().max, 0.0f);
                wn.next[s] = -node.size;
                wn.leaf[s] = node.start;
            }
            s++;
        } else {
            s = gather<W>(wn, node.l, s, d + 1);
            s = gather<W>(wn, node.r, s, d + 1);
        }
        return s;
    }

    template<int W>
    void build_wide(WBVH<W>& wbvh, int wn, int n) {

        gather<W>(wbvh.nodes[wn], n, 0, 0);

        for(int i = 0; i < WBVH<W>::N; i++) {
            if(wbvh.nodes[wn].leaf[i] < 0) break;
            if(wbvh.nodes[wn].next[i] < 0) continue;
            size_t child = wbvh.nodes.size();
            wbvh.nodes.push_back({});
            build_wide(wbvh, child, wbvh.nodes[wn].next[i]);
            wbvh.nodes[wn].next[i] = child;
        }
    }

private:
    size_t new_node(BBox box = {}, size_t start = 0, size_t size = 0, size_t l = 0, size_t r = 0);
    void build_links(Node& node, int next_right);
    void build_parents(int idx);
    void build_rec(size_t n, size_t max_leaf_size);

    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
};

class OBBBVH {
public:
    struct Node {
        OBB bbox;
        int start, size;
        int l, r;
        int parent;
        bool is_leaf() const;
    };

    OBBBVH() = default;
    OBBBVH(const VK::Mesh& mesh, size_t max_leaf_size = 1);
    void build(const VK::Mesh& mesh, size_t max_leaf_size = 1);

    OBBBVH(OBBBVH&& src) = default;
    OBBBVH& operator=(OBBBVH&& src) = default;

    OBBBVH(const OBBBVH& src) = delete;
    OBBBVH& operator=(const OBBBVH& src) = delete;

    const std::vector<Node>& get_nodes() const {
        return nodes;
    };
    const std::vector<Triangle>& get_triangles() const {
        return triangles;
    };
    BBox box() const {
        return nodes[0].bbox.box();
    }

private:
    size_t new_node(OBB box = {}, size_t start = 0, size_t size = 0, size_t l = 0, size_t r = 0);
    void build_parents(int idx);
    void build_rec(size_t n, size_t max_leaf_size);

    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
};

