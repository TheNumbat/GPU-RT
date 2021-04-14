
#pragma once

inline std::atomic<i64> allocs;

template<const char* tname, bool log>
template<typename T>
T* Mallocator<tname, log>::alloc(usize size) {
    if(!size) return null;
    T* ret = (T*)base_alloc(size);
    if constexpr(log) { /* TODO PROFILER */
    }
    return ret;
}

template<const char* tname, bool log>
template<typename T>
void Mallocator<tname, log>::dealloc(T* mem) {
    if(!mem) return;
    if constexpr(log) { /* TODO PROFILER */
    }
    base_free(mem);
}

template<usize N> template<typename T> T* Marena<N>::alloc(usize size, usize align) {
    if(!size) return null;
    uptr here = (uptr)mem + used;
    uptr offset = here % align;
    uptr next = here + (offset ? align - offset : 0);
    assert(next + size - (uptr)mem < N);
    T* ret = (T*)next;
    used += offset + size;
    high_water = _MAX(high_water, used);
    return ret;
}

inline void* base_alloc(usize sz) {
    void* ret = calloc(sz, 1);
    assert(ret);
    allocs++;
    return ret;
}

inline void base_free(void* mem) {
    if(!mem) return;
    allocs--;
    free(mem);
}

inline void mem_validate() {
    if(allocs != 0) {
        warn("Unbalanced allocations: %", allocs.load());
    } else {
        info("No memory leaked.");
    }
}

inline u32 __validator = atexit(mem_validate);
