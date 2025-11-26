#include "nigiri/routing/a_star/a_star.h"

#include "fmt/ranges.h"

#include "utl/enumerate.h"

#include "nigiri/common/dial.h"
#include "nigiri/routing/get_earliest_transport.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

// #define as_debug fmt::println
#define as_debug(...)

namespace nigiri {
namespace routing {

a_star::a_star(timetable const& tt,
               rt_timetable const*,
               a_star_state& state,
               bitvec const& is_dest,
               std::array<bitvec, kMaxVias> const&,
               std::vector<std::uint16_t> const& dist_to_dest,
               hash_map<location_idx_t, std::vector<td_offset>> const&,
               std::vector<std::uint16_t> const& lb,
               std::vector<via_stop> const&,
               day_idx_t base,
               clasz_mask_t,
               bool,
               bool,
               bool,
               transfer_time_settings)
    : tt_{tt},
      state_{state},
      is_dest_{is_dest},
      dist_to_dest_{dist_to_dest},
      lb_{lb},
      base_{base} {  // TODO: in query engine this is QUERY_DAY_SHIFT check why
                     // needed and what is does
  // TODO: initialize other stuff
  // Get segments leading to dest from location_idx_t l
  // * Used from query_engine
  auto const mark_dest_segments = [&](location_idx_t const l,
                                      duration_t const d) {
    for (auto const r : tt_.location_routes_[l]) {
      auto const stop_seq = tt_.route_location_seq_[r];
      for (auto i = stop_idx_t{1U}; i != stop_seq.size(); ++i) {
        auto const stp = stop{stop_seq[i]};
        if (stp.location_idx() != l || !stp.out_allowed()) {
          continue;
        }

        for (auto const t : tt_.route_transport_ranges_[r]) {
          auto const segment = state_.tbd_.transport_first_segment_[t] + i - 1;
          state_.end_reachable_.set(segment, true);

          auto const it = state_.dist_to_dest_.find(segment);
          if (it == end(state_.dist_to_dest_)) {
            state_.dist_to_dest_.emplace_hint(it, segment, d);
          } else {
            it->second = std::min(it->second, d);
          }
        }
      }
    }
  };

  if (dist_to_dest.empty()) /* Destination is stop. */ {
    is_dest_.for_each_set_bit([&](std::size_t const i) {
      auto const l = location_idx_t{i};
      as_debug("{} is dest!", location{tt_, l});
      mark_dest_segments(l, duration_t{0U});
      for (auto const fp :
           tt_.locations_.footpaths_in_[state_.tbd_.prf_idx_][l]) {
        mark_dest_segments(fp.target(), fp.duration());
      }
    });
  } else /* Destination is coordinate. */ {
    for (auto const [l_idx, dist] : utl::enumerate(dist_to_dest_)) {
      if (dist != kUnreachable) {
        mark_dest_segments(location_idx_t{l_idx}, duration_t{dist});
      }
    }
  }
};

// void a_star::execute(unixtime_t const start_time,
//                                  std::uint8_t const max_transfers,
//                                  unixtime_t const worst_time_at_dest,
//                                  profile_idx_t const,
//                                  journey& result) {
//   // TODO: implement A* algorithm
//   // Set start time in cost_function
// }

void a_star::add_start(location_idx_t l, unixtime_t t) {
  // Used from query_engine
  // TODO: check if it works like this
  auto const [day, mam] = tt_.day_idx_mam(t);
  for (auto const r : tt_.location_routes_[l]) {
    // iterate stop sequence of route, skip last stop
    auto const stop_seq = tt_.route_location_seq_[r];
    for (auto i = stop_idx_t{0U}; i < stop_seq.size() - 1; ++i) {
      auto const stp = stop{stop_seq[i]};
      if (!stp.in_allowed() || stp.location_idx() != l) {
        continue;
      }

      auto const et = get_earliest_transport<direction::kForward>(
          tt_, tt_, 0U, r, i, day, mam, stp.location_idx(),
          [](day_idx_t, std::int16_t) { return false; });
      if (!et.is_valid()) {
        continue;
      }

      auto const query_day_offset = to_idx(et.day_) - to_idx(base_);
      if (query_day_offset < 0 || query_day_offset >= kASMaxDayOffset) {
        continue;
      }

      auto const transport_first_segment =
          state_.tbd_.transport_first_segment_[et.t_idx_];
      // TODO: this should be a state method props at least the insert
      // Update Arrival Time and Day
      auto const delta = tt_.event_mam(r, et.t_idx_, i, event_type::kDep);
      state_.arrival_day_.emplace(transport_first_segment,
                                  day_idx_t{delta.days()});
      state_.arrival_time_.emplace(transport_first_segment,
                                   minutes_after_midnight_t{delta.mam()});
      // Update Predecessor Table
      state_.pred_table_.emplace(transport_first_segment,
                                 state_.sartSegmentPredecessor);

      // Enqueue Element
      state_.pq_.push(queue_entry{transport_first_segment, 0U});
    }
  }
}

// void a_star::reconstruct(query const& q,
//                                          journey& j) const {
//  // TODO: implement reconstruct
// }

}  // namespace routing
}  // namespace nigiri