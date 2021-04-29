
#pragma once

#include <lib/mathlib.h>
#include <vk/render.h>

struct Triangle {
    Vec3 v0, v1, v2;
    BBox bbox() const;
};

template<int W>
class WBVH {
public:
    static constexpr int N = 1 << W;
    alignas(16) struct Node {
        BBox lb[N];
        BBox rb[N];
        int l[N];
        int r[N];
    };
    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
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
        wbvh.triangles = triangles;

        wbvh.nodes.push_back({});
        build_wide(wbvh, 0, 0);
    }

    template<int W>
    void build_wide(WBVH<W>& wbvh, int wn, int n) {

        WBVH<W>::template Node& wnode = wbvh.nodes[wn];
        gather(wnode, nodes[n], 0, 0);

        for(int i = 0; i < WBVH<W>::N; i++) {
            if(wnode.r[i] >= 0) {
                
                size_t wl_c = wbvh.nodes.size();
                wbvh.nodes.push_back({});
                build_wide(wbvh, wl_c, wnode.l[i]);
                wnode.l[i] = wl_c;

                size_t wr_c = wbvh.nodes.size();
                wbvh.nodes.push_back({});
                build_wide(wbvh, wl_c, wnode.r[i]);
                wnode.r[i] = wr_c;
            }
        }
    }

    template<int W>
    int gather(WBVH<W>::Node& wn, Node& n, int s, int d) {
        if(d == W || n.l == n.r) {
            wn.lb = nodes[n.l].bbox;
            wn.rb = nodes[n.r].bbox;
            wn.l[s] = n.l;
            wn.r[s] = n.r;
            if(wn.l[s] == wn.r[s]) {
                wn.l[s] = -n.start;
                wn.r[s] = -n.size;
            }
            s++;
        } else {
            s = gather(n, nodes[n.l], s, d + 1);
            s = gather(n, nodes[n.r], s, d + 1);
        }
        return s;
    }

private:
    size_t new_node(BBox box = {}, size_t start = 0, size_t size = 0, size_t l = 0, size_t r = 0);
    void build_links(Node& node, int next_right);
    void build_parents(int idx);
    void build_rec(size_t n, size_t max_leaf_size);

    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
};
