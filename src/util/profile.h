
#pragma once

#include <lib/lib.h>

// NOTE(max): if PROFILE is defined to 0, enter, exit, and alloc will no-op,
// but begin_frame and end_frame will still keep track of frame timing, and
// data structures will be allocated.
#define PROFILE 1

#define Prof_Func Profiler::Scope __prof_scope(Here)
#define Prof_Scope(name) Profiler::Scope __prof_scope(name)

struct Profiler {

    static void start_thread();
    static void end_thread();

    static void begin_frame();
    static void end_frame();

    using thread_id = std::thread::id;
    using Time_Point = u64;

    static void enter(Location l);
    static void enter(literal l);
    static void exit();

    struct Alloc {
        literal name;
        void* addr = null;
        usize size = 0; // 0 implies free
    };
    static void alloc(Alloc a);

    static Time_Point timestamp();
    static f32 ms(Time_Point start);

    struct Scope {
        Scope(Location loc) {
            Profiler::enter(loc);
        }
        Scope(literal name) {
            Profiler::enter(name);
        }
        ~Scope() {
            Profiler::exit();
        }
    };

    struct Timing_Node {
        Location loc;

        Time_Point begin, end;
        Time_Point self_time, heir_time;
        u64 calls = 0;

        Slice<Timing_Node> children;
        Timing_Node* parent = null;

        void compute_times();

        template<typename T> void visit(T&& f) {
            f(*this);
            for(auto& n : children) {
                n.visit(f);
            }
        }
    };

    template<typename T> static void iterate_timings(T f) {
        std::lock_guard lock(threads_lock);

        for(auto& entry : threads) {

            thread_id id = entry.key;
            Thread_Profile* tp = entry.value;

            Frame_Profile* fp = null;
            if(tp->during_frame && tp->frames.size() > 1u) {
                fp = &tp->frames.penultimate();
            } else if(!tp->during_frame && !tp->frames.empty()) {
                fp = &tp->frames.back();
            } else
                continue;

            fp->root->visit([&f, id](Timing_Node n) { return f(id, n); });
        }
    }

private:
    struct Alloc_Profile {
        i64 allocates = 0, frees = 0;
        i64 allocate_size = 0, free_size = 0;
        i64 current_set_size = 0;
        Map<void*, usize, Mhidden> current_set;
    };

    struct Frame_Profile {

        static constexpr usize mem_per_frame = KB(256);
        static constexpr usize max_allocs = 2048;
        static constexpr usize max_children = 8;

        void begin();
        void end();
        void enter(Location l);
        void exit();

        Marena<mem_per_frame> memory;
        Timing_Node* root = null;
        Timing_Node* current = null;
        Slice<Alloc> allocations;
    };

    struct Thread_Profile {
        bool during_frame;
        Queue<Frame_Profile, Mhidden> frames;
    };

    static inline std::mutex threads_lock;
    static inline std::mutex allocs_lock;
    static inline thread_local Thread_Profile this_thread;

    static inline Map<thread_id, Thread_Profile*, Mhidden> threads;
    static inline Map<literal, Alloc_Profile, Mhidden> allocs;
};

template<> struct Type_Info<Location> {
    static constexpr char name[] = "Location";
    static constexpr usize size = sizeof(Location);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _func[] = "func";
    static constexpr char _file[] = "file";
    static constexpr char _line[] = "line";
    using members = Type_List<Record_Field<literal, offsetof(Location, func), _func>,
                              Record_Field<literal, offsetof(Location, file), _file>,
                              Record_Field<usize, offsetof(Location, line), _line>>;
};

template<> struct Type_Info<Profiler::Alloc> {
    static constexpr char name[] = "Alloc";
    static constexpr usize size = sizeof(Profiler::Alloc);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _name[] = "name";
    static constexpr char _addr[] = "addr";
    static constexpr char _size[] = "size";
    using members = Type_List<Record_Field<literal, offsetof(Profiler::Alloc, name), _name>,
                              Record_Field<void*, offsetof(Profiler::Alloc, addr), _addr>,
                              Record_Field<usize, offsetof(Profiler::Alloc, size), _size>>;
};
