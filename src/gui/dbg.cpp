
#include "dbg.h"
#include <algorithm>
#include <util/profile.h>

void Dbg_Gui::profiler() {

    if(!prof_window.stopProfiling) {

        Map<Location, Graph_Entry, Mframe> accum;

        Profiler::iterate_timings([&accum](Profiler::thread_id id, Profiler::Timing_Node n) {
            if(id != std::this_thread::get_id()) return;
            Graph_Entry& t = accum.get_or_insert(n.loc);
            t.name = n.loc.func;
            t.endTime += Profiler::ms(n.self_time);
        });

        Vec<Graph_Entry, Mframe> data(accum.size());

        for(auto& t : accum) data.push(t.value);

        std::sort(data.begin(), data.end(), [](auto l, auto r) { return l.endTime < r.endTime; });

        for(u32 i = 1; i < data.size(); i++) {
            data[i].startTime = data[i - 1].endTime;
            data[i].endTime += data[i].startTime;
        }

        prof_window.cpuGraph.load_frame_data(data.data(), data.size());
    }

    prof_window.render();
}
