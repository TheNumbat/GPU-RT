
#define MAX_DEPTH 64

struct Node {
	vec4 bmin, bmax;
	int next, leaf;
};

Node get_node(uint n, uint i) {
	Node node;
	node.bmin = nodes[n].bmin[i];
	node.bmax = nodes[n].bmax[i];
	node.next = nodes[n].next[i];
	node.leaf = nodes[n].leaf[i];
	return node;
}

void ray_primitives(vec3 o, vec3 d, int s, int e, inout vec3 first_hit, inout float best_dist) {
	for (int t = s; t < e; t++) {
		vec3 hitp;
		float hitd;
		bool h = hit_triangle(o, d, hitp, hitd, triangles[t]);
		if(h && hitd < best_dist) {
			best_dist = hitd;
			first_hit = hitp;
		}
	}
}

void cpq_primitives(vec3 q, int s, int e, inout vec3 best_point, inout float best_dist) {
	for (int t = s; t < e; t++) {
		vec3 p = cp_triangle(q, triangles[t]);
		float dt = length(p-q);
		if(dt < best_dist) {
			best_dist = dt;
			best_point = p;
		}
	}
}

void traverse_ray_sort(vec3 o, vec3 d, inout vec3 first_hit, inout float best_dist) {

	int children[WIDTH];
    float children_d[WIDTH];

	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
		uint n = stack[--stack_n];

        for(uint i = 0; i < WIDTH; i++) {
			
			Node node = get_node(n, i);
            children[i] = -1;
            children_d[i] = INF;

            if(node.leaf < 0) continue;
            if(node.next < 0) {
                ray_primitives(o, d, node.leaf, node.leaf - node.next, first_hit, best_dist);
                continue;
            }

            float boxd;
            if(hit_bbox(o, d, node.bmin.xyz, node.bmax.xyz, best_dist, boxd)) {
				children[i] = node.next;
                children_d[i] = boxd;
            }
        }

        bool swapped = true;
        int j = 0;
        float tmp;
        for (int c = 0; c < WIDTH; c--)
        {
            if (!swapped)
                break;
            swapped = false;
            j++;
            for (int i = 0; i < WIDTH; i++)
            {
                if (i >= WIDTH - j)
                    break;
                if (children_d[i] > children_d[i + 1])
                {
                    tmp = children_d[i];
                    children_d[i] = children_d[i + 1];
                    children_d[i + 1] = tmp;
                    int tmp2 = children[i];
                    children[i] = children[i+1];
                    children[i+1] = tmp2;
                    swapped = true;
                }
            }
        }

        for(uint i = 0; i < WIDTH; i++) {
            if(children[i] < 0) continue;
            if(children_d[i] == INF) continue;
            stack[stack_n++] = children[i];
        }
	}
}

void traverse_cpq_sort(vec3 q, inout vec3 best_point, inout float best_dist) {

    int children[WIDTH];
    float children_d[WIDTH];

	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
        uint n = stack[--stack_n];
        uint closest = WIDTH;
        float closestd = INF;

        for(uint i = 0; i < WIDTH; i++) {
			
            Node node = get_node(n, i);
            children[i] = -1;

            if(node.leaf < 0) continue;
            if(node.next < 0) {
                cpq_primitives(q, node.leaf, node.leaf - node.next, best_point, best_dist);
                continue;
            }

            float b_close, b_far;
            box_dist(q, node.bmin.xyz, node.bmax.xyz, b_close, b_far);
            if(b_close < best_dist) {
                best_dist = min(best_dist, b_far);
                children[i] = node.next;
                children_d[i] = b_close;
            }
        }


        bool swapped = true;
        int j = 0;
        float tmp;
        for (int c = 0; c < WIDTH; c--)
        {
            if (!swapped)
                break;
            swapped = false;
            j++;
            for (int i = 0; i < WIDTH; i++)
            {
                if (i >= WIDTH - j)
                    break;
                if (children_d[i] > children_d[i + 1])
                {
                    tmp = children_d[i];
                    children_d[i] = children_d[i + 1];
                    children_d[i + 1] = tmp;
                    int tmp2 = children[i];
                    children[i] = children[i+1];
                    children[i+1] = tmp2;
                    swapped = true;
                }
            }
        }

        for(uint i = 0; i < WIDTH; i++) {
            if(children[i] < 0) continue;
            if(children_d[i] == INF) continue;
            stack[stack_n++] = children[i];
        }
	}
}

void traverse_ray_max(vec3 o, vec3 d, inout vec3 first_hit, inout float best_dist) {

	int children[WIDTH];
	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
		uint n = stack[--stack_n];
		
        uint closest = WIDTH;
        float closestd = INF;

        for(uint i = 0; i < WIDTH; i++) {
			
			Node node = get_node(n, i);
            children[i] = -1;

            if(node.leaf < 0) continue;
            if(node.next < 0) {
                ray_primitives(o, d, node.leaf, node.leaf - node.next, first_hit, best_dist);
                continue;
            }

            float boxd;
            if(hit_bbox(o, d, node.bmin.xyz, node.bmax.xyz, best_dist, boxd)) {
				if(boxd < closestd) {
                    closest = i;
                    closestd = boxd;
                }
				children[i] = node.next;
            }
        }

        if(closest < WIDTH)
            stack[stack_n++] = children[closest];
        for(uint i = 0; i < WIDTH; i++)
            if(i != closest && children[i] >= 0)
                stack[stack_n++] = children[i];
	}
}

void traverse_cpq_max(vec3 q, inout vec3 best_point, inout float best_dist) {

    int children[WIDTH];
	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
        uint n = stack[--stack_n];
        uint closest = WIDTH;
        float closestd = INF;

        for(uint i = 0; i < WIDTH; i++) {
			
            Node node = get_node(n, i);
            children[i] = -1;

            if(node.leaf < 0) continue;
            if(node.next < 0) {
                cpq_primitives(q, node.leaf, node.leaf - node.next, best_point, best_dist);
                continue;
            }

            float b_close, b_far;
            box_dist(q, node.bmin.xyz, node.bmax.xyz, b_close, b_far);
            if(b_close < best_dist) {
                best_dist = min(best_dist, b_far);
                if(b_close < closestd) {
                    closest = i;
                    closestd = b_close;
                }
                children[i] = node.next;
            }
        }

        if(closest < WIDTH)
            stack[stack_n++] = children[closest];
        for(uint i = 0; i < WIDTH; i++)
            if(i != closest && children[i] >= 0)
                stack[stack_n++] = children[i];
	}
}

void traverse_ray_stack(vec3 o, vec3 d, inout vec3 first_hit, inout float best_dist) {

	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
		uint n = stack[--stack_n];

        for(uint i = 0; i < WIDTH; i++) {
			
			Node node = get_node(n, i);

            if(node.leaf < 0) break;
            if(node.next < 0) {
                ray_primitives(o, d, node.leaf, node.leaf - node.next, first_hit, best_dist);
                continue;
            }

            float boxd;
            if(hit_bbox(o, d, node.bmin.xyz, node.bmax.xyz, best_dist, boxd)) {
                stack[stack_n++] = node.next;
            }
        }
	}
}

void traverse_cpq_stack(vec3 q, inout vec3 best_point, inout float best_dist) {

	uint stack[MAX_DEPTH];
	uint stack_n = 1;
	stack[0] = 0;

	while(stack_n > 0) {
		
        uint n = stack[--stack_n];

        for(uint i = 0; i < WIDTH; i++) {
			
            Node node = get_node(n, i);

            if(node.leaf < 0) continue;
            if(node.next < 0) {
                cpq_primitives(q, node.leaf, node.leaf - node.next, best_point, best_dist);
                continue;
            }

            float b_close, b_far;
            box_dist(q, node.bmin.xyz, node.bmax.xyz, b_close, b_far);
            if(b_close < best_dist) {
                best_dist = min(best_dist, b_far);
                stack[stack_n++] = node.next;
            }
        }
	}
}

void traverse_ray(vec3 o, vec3 d, inout vec3 first_hit, inout float best_dist) {
    if(sort_children == 0) traverse_ray_stack(o, d, first_hit, best_dist);
    else if(sort_children == 1) traverse_ray_max(o, d, first_hit, best_dist);
    else traverse_ray_sort(o, d, first_hit, best_dist);
}

void traverse_cpq(vec3 q, inout vec3 best_point, inout float best_dist) {
    if(sort_children == 0) traverse_cpq_stack(q, best_point, best_dist);
    else if(sort_children == 1) traverse_cpq_max(q, best_point, best_dist);
    else traverse_cpq_sort(q, best_point, best_dist);
} 
