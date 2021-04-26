
#pragma once

#include <lib/mathlib.h>
#include <vk/render.h>

class BVH {
public:
    struct Node {
        BBox bbox; 
        int start, size;
        int l, r;
        int hit, miss;
        bool is_leaf() const;
    };

    struct Triangle {
        Vec3 v0, v1, v2;
        BBox bbox() const;
    };

    BVH() = default;
    BVH(const VK::Mesh& mesh, size_t max_leaf_size = 1);
    void build(const VK::Mesh& mesh, size_t max_leaf_size = 1);

    BVH(BVH&& src) = default;
    BVH& operator=(BVH&& src) = default;

    BVH(const BVH& src) = delete;
    BVH& operator=(const BVH& src) = delete;

    const std::vector<Node>& get_nodes() const { return nodes; };
    const std::vector<Triangle>& get_triangles() const { return triangles; };
    BBox box() const { return nodes[0].bbox; }

private:
    
    size_t new_node(BBox box = {}, size_t start = 0, size_t size = 0, size_t l = 0, size_t r = 0);
    void build_links(Node& node, int next_right);
    void build_rec(size_t n, size_t max_leaf_size);

    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
};
