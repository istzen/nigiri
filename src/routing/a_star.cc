#include "nigiri/routing/a_star.h"

#include "nigiri/routing/journey.h"
#include "nigiri/common/dial.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

namespace nigiri::routing {

static constexpr auto const maxTravelTime = 1_days;
static constexpr auto const transferCost = duration_t(5);

struct cost_function {
  cost_function(minutes_after_midnight_t const st,
                day_idx_t const sd,
                const arrival_time_map* at, 
                const day_index_map* ad) 
            : start_time_{st}, start_day_{sd}, arrival_time_{at}, arrival_day_{ad} {}

  duration_t operator()(queue_entry const& q) const { 
    // TODO: what happens if there is no arrival_time, is that possible?
    auto const time_travelled = duration_t((arrival_day_->find(q.segment_)->second.v_ - start_day_.v_)*24*60 + arrival_time_->find(q.segment_)->second.count() - start_time_.count());
    return std::chrono::duration_cast<duration_t>(time_travelled + transferCost * q.transfers_);
  }

  const arrival_time_map* const arrival_time_; // segment -> arrival_time
  const day_index_map* const arrival_day_; // segment -> arrival_day
  day_idx_t const start_day_; // day_idx_t of start_time
  minutes_after_midnight_t const start_time_; // minutes_after_midnight_t of start_time

private:
  // TODO: fix
  template <typename Map>
  uint16_t get_value(Map const& map, tb::segment_idx_t const segment) {
    auto it = map.find(segment);
    if (it == map.end()) {
        throw std::runtime_error("Key not found in map");
    }
    return it->second.v_; // assuming the value type has .v_ member
}
};

pareto_set<journey> a_star(timetable const& tt,
               tb::tb_data const& tbd,
               unixtime_t const start_time,
               location_idx_t const source,
               location_idx_t const dest) {

  arrival_time_map arrival_time;
  day_index_map arrival_day;
  pred_table pred_table;
  // Convert start_time to day_idx_t and minutes_after_midnight_t
  auto const [sd_idx, st_idx] = tt.day_idx_mam(start_time);

  auto pq = dial<queue_entry, cost_function>{maxTravelTime.count(), cost_function(st_idx, sd_idx, &arrival_time, &arrival_day)}; // priority_queue
  bitvec_map<tb::segment_idx_t> end_reachable{tbd.segment_transfers_.size()}; // segments leading to dest

  // Get segments leading to dest from location_idx_t l
  // * Used from query_engine
  auto const mark_dest_segments = [&](location_idx_t const l,
                                      duration_t const d) {
    for (auto const r : tt.location_routes_[l]) {
      auto const stop_seq = tt.route_location_seq_[r];
      for (auto i = stop_idx_t{1U}; i != stop_seq.size(); ++i) {
        auto const stp = stop{stop_seq[i]};
        if (stp.location_idx() != l || !stp.out_allowed()) {
          continue;
        }

        for (auto const t : tt.route_transport_ranges_[r]) {
          auto const segment = tbd.transport_first_segment_[t] + i - 1;
          end_reachable.set(segment, true);
        }
      }
    }
  };
  // Mark given dest
  mark_dest_segments(dest, duration_t{0U});
  // Mark all locations with footpath to dest
  for (auto const fp :
           tt.locations_.footpaths_in_[tbd.prf_idx_][dest]) {
        mark_dest_segments(fp.target(), fp.duration());
      }

  // Get segments starting from source

  // TODO: how to handle beginning segments because they should have no pred but are still valid
  // TODO: include day_idx_t
  // This updates the arr_time of a segment based on the pred or start_time if none is given
  auto const update_segment_arr_time = [&](tb::segment_idx_t const s,
                                        duration_t const d) {
    if(auto const it_pt = pred_table.find(s); it_pt != end(pred_table)) {
      // Update arr_time and arr_day based on the time of pred if improved
      auto const pred = it_pt->second;
      // TODO: can we assume that this is present?
      auto const it_at_p = arrival_time.find(pred);
      auto const new_arr_time = minutes_after_midnight_t (it_at_p->second + d);
      auto [it_at_s, inserted] = arrival_time.try_emplace(s, new_arr_time);
      if(!inserted) {
        it_at_s->second = std::min(it_at_s->second, new_arr_time);
      }
    } else {
      // s is a starting segment and thus has no pred
      arrival_time.emplace(s, start_time + d);
    }
  };

  // Perform algorithm
  while(!pq.empty()) {
    auto const qe = pq.top();
    // TODO: mark s as reached

    // stop if the segment is in the destination segments
    if(end_reachable[qe.segment_]){
      break;
    }

    // Update transfers of s
    for(auto const t : tbd.segment_transfers_[qe.segment_]){
      
    }
  }

  // Translate output back to location_idx_t

  // Create and return journey
}
} // namespace nigiri::routing