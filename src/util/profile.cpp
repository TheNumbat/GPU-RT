
#include "profile.h"

Profiler::Time_Point Profiler::timestamp() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

f32 Profiler::ms(Profiler::Time_Point time) {
    return time / 1000.0f;
}

void Profiler::start_thread() {
    std::lock_guard lock(threads_lock);

    thread_id id = std::this_thread::get_id();
    assert(!threads.try_get(id));

    this_thread.during_frame = false;
    threads.insert(id, &this_thread);
}

void Profiler::end_thread() {
    std::lock_guard lock(threads_lock);

    thread_id id = std::this_thread::get_id();
    assert(threads.try_get(id));

    threads.erase(id);
}

void Profiler::Frame_Profile::begin() {
    allocations = Slice<Alloc>::slice(memory, max_allocs);
    root = memory.make<Timing_Node>();
    current = root;
    root->begin = timestamp();
    root->loc = {"Main Loop", "", 0};
    root->children = Slice<Timing_Node>::slice(memory, max_children);
}

void Profiler::Frame_Profile::end() {
    assert(current == root);
    root->end = timestamp();
    root->heir_time = root->end - root->begin;
    root->compute_times();
}

void Profiler::Timing_Node::compute_times() {
    u64 child_time = 0;
    for(auto& c : children) {
        c.compute_times();
        child_time = child_time + c.heir_time;
    }
    self_time = heir_time - child_time;
}

void Profiler::begin_frame() {

    Thread_Profile& prof = this_thread;

    if(!prof.frames.empty() && prof.frames.full()) {
        prof.frames.pop();
    }

    Frame_Profile& new_frame = prof.frames.push(Frame_Profile());
    new_frame.begin();

    prof.during_frame = true;
}

void Profiler::end_frame() {

    Thread_Profile& prof = this_thread;

    assert(!prof.frames.empty());

    Frame_Profile& this_frame = prof.frames.back();
    this_frame.end();

    prof.during_frame = false;
}

void Profiler::enter(literal l) {
#if PROFILE == 1
    enter({l, "", 0});
#endif
}

void Profiler::enter(Location l) {
#if PROFILE == 1
    this_thread.frames.back().enter(l);
#endif
}

void Profiler::Frame_Profile::enter(Location l) {

    bool repeat = false;
    for(auto& n : current->children) {
        if(n.loc == l) {
            current = &n;
            repeat = true;
        }
    }

    if(!repeat) {
        assert(!current->children.full());
        Timing_Node& new_child = current->children.push(Timing_Node());
        new_child.parent = current;
        new_child.loc = l;
        new_child.children = Slice<Timing_Node>::slice(memory, max_children);
        current = &new_child;
    }

    current->begin = timestamp();
    current->calls++;
}

void Profiler::exit() {
#if PROFILE == 1
    this_thread.frames.back().exit();
#endif
}

void Profiler::Frame_Profile::exit() {

    Frame_Profile& frame = this_thread.frames.back();
    Timing_Node* current_node = frame.current;

    current_node->end = timestamp();
    current_node->heir_time += current_node->end - current_node->begin;
    frame.current = current_node->parent;
}

void Profiler::alloc(Alloc a) {

#if PROFILE == 0
    return;
#endif

    if(this_thread.during_frame) this_thread.frames.back().allocations.push(a);

    std::lock_guard lock(allocs_lock);
    Alloc_Profile& prof = allocs.get_or_insert(a.name);

    if(a.size) {

        assert(!prof.current_set.try_get(a.addr));
        prof.current_set.insert(a.addr, a.size);

        prof.allocate_size += a.size;
        prof.allocates++;
        prof.current_set_size += a.size;

    } else {

        i64 size = prof.current_set.get(a.addr);
        prof.current_set.erase(a.addr);

        prof.free_size += size;
        prof.frees++;
        prof.current_set_size -= size;
    }
}
