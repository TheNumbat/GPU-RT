
#pragma once

void* base_alloc(usize sz);
void base_free(void* mem);

void mem_validate();

template<const char* tname, bool log = true> struct Mallocator {
    static constexpr const char* name = tname;

    template<typename T> static T* make(usize n = 1) {
        return new(alloc<T>(sizeof(T) * n)) T[n];
    }

    template<typename T> static T* alloc(usize size);

    template<typename T> static void dealloc(T* mem);
};

inline char Mdefault_name[] = "Mdefault";
using Mdefault = Mallocator<Mdefault_name>;

inline char Mhidden_name[] = "Mhidden";
using Mhidden = Mallocator<Mhidden_name, false>;

template<usize N> struct Marena {
    u8 mem[N] = {};
    usize used = 0;
    usize high_water = 0;

    template<typename T> T* make(usize n = 1) {
        T* ret = alloc<T>(sizeof(T) * n, alignof(T));
        for(usize i = 0; i < n; i++) {
            new(ret + i) T;
        }
        return ret;
    }

    template<typename T> void dealloc(T* mem) {
    }

    void reset() {
        used = 0;
    }

private:
    template<typename T> T* alloc(usize size, usize align = 1);
};

template<const char* tname, usize N> struct static_Marena {

    static constexpr const char* name = tname;
    static inline Marena<N> arena;

    template<typename T> static T* make(usize n = 1) {
        return arena.template make<T>(n);
    }

    template<typename T> static T* alloc(usize size, usize align = 1) {
        return arena.alloc(size, align);
    }

    template<typename T> static void dealloc(T* mem) {
    }

    static void reset() {
        arena.reset();
    };
};

inline char Mframe_name[] = "Mframe";
using Mframe = static_Marena<Mframe_name, MB(32)>;

template<const char* tname, typename T, typename Base> struct Free_List {

    union Free_Node {
        T value;
        Free_Node* next = null;
    };

    static constexpr const char* name = tname;
    static inline Free_Node* list = null;

    static T* make() {
        return new(alloc()) T;
    }

    static T* alloc() {
        if(list) {
            Free_Node* ret = list;
            list = list->next;
            memset(ret, 0, sizeof(Free_Node));
            return (T*)ret;
        }
        return (T*)Base::template make<Free_Node>();
    }

    static void dealloc(T* mem) {
        Free_Node* node = (Free_Node*)mem;
        node->next = list;
        list = node;
    }

    static void clear() {
        while(list) {
            Free_Node* next = list->next;
            Base::dealloc(list);
            list = next;
        }
    }
};
