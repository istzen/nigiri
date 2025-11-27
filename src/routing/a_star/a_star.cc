#include "nigiri/routing/a_star/a_star.h"

#include "fmt/ranges.h"

#include "utl/enumerate.h"

#include "nigiri/common/dial.h"
#include "nigiri/routing/get_earliest_transport.h"
#include "nigiri/routing/journey.h"
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
               transfer_time_settings tts)
    : tt_{tt},
      state_{state},
      is_dest_{is_dest},
      dist_to_dest_{dist_to_dest},
      lb_{lb},
      base_{base} {  // TODO: in query engine this is QUERY_DAY_SHIFT check why
  // TODO: initialize other stuff
  // TODO: this needs to be checked
  state_.setup({base - 5, minutes_after_midnight_t{0}});
  state_.transfer_factor_ = tts.factor_;
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
          std::cout << "Marking segment " << segment.v_
                    << " as reaching dest with dist " << d.count() << "\n";

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
      std::cout << location{tt_, l} << " is dest!\n";
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

void a_star::execute(unixtime_t const,
                     std::uint8_t const max_transfers,
                     unixtime_t const worst_time_at_dest,
                     profile_idx_t const,
                     journey&) {
  // TODO: figure out how debug print works
  delta const worst_delta =
      delta(to_idx(tt_.day_idx_mam(worst_time_at_dest).first),
            tt_.day_idx_mam(worst_time_at_dest).second.count());
  // TODO: Think about start time in cost_function for now just 0,0

  while (!state_.pq_.empty()) {
    auto const& current = state_.pq_.top();
    state_.pq_.pop();
    auto const& segment = current.segment_;
    // Check if segment is already settled in case of multiple entries in pq
    if (state_.settled_segments_.test(segment)) {
      continue;
    }
    state_.settled_segments_.set(current.segment_, true);

    as_debug("Visiting segment {} with transfers {}", segment,
             current.transfers_);

    // Check if current segment reaches destination
    if (state_.end_reachable_.test(segment)) [[unlikely]] {
      as_debug("Reached destination via segment {}", segment);
      // TODO: what do we do here
      return;
    }
    // TODO: Find next segment in transport and handle that
    // Handle next segment for transport if exists
    auto const transport_current = state_.tbd_.segment_transports_[segment];
    auto const rel_segment =
        segment - state_.tbd_.transport_first_segment_[transport_current];
    if (rel_segment < state_.tbd_.get_segment_range(transport_current).size()) {
      auto const next_segment = segment + 1;
      auto const to = static_cast<stop_idx_t>(to_idx(rel_segment) + 2);
      auto const next_stop_arr =
          tt_.event_mam(transport_current, to, event_type::kArr);
      // TODO: How to handle costs of dist_to_dest for dest segments?
      // * props gets unnecessary for second part as we have the heuristic in
      // * cost function
      if (worst_delta < next_stop_arr ||
          state_.better_arrival(current, next_stop_arr)) {
        state_.arrival_day_.insert_or_assign(next_segment,
                                             day_idx_t{next_stop_arr.days()});
        state_.arrival_time_.insert_or_assign(
            next_segment, minutes_after_midnight_t{next_stop_arr.mam()});
        // Update Predecessor Table
        state_.pred_table_.insert_or_assign(next_segment,
                                            state_.sartSegmentPredecessor);
        state_.pq_.push(queue_entry{
            next_segment, static_cast<uint8_t>(current.transfers_ + 1)});
      }
    }
    // Handle transfers

    // Check if max transfers reached
    if (current.transfers_ >= max_transfers) [[unlikely]] {
      as_debug("Max transfers reached at segment {}", segment);
      continue;
    }
    // Explore neighbors (transfers)
    for (auto const& transfer : state_.tbd_.segment_transfers_[segment]) {
      // Check if segment_to is already settled
      auto const new_segment = transfer.to_segment_;
      if (state_.settled_segments_.test(new_segment)) {
        continue;
      }
      // Check if transfer is valid on the day
      if (state_.tbd_.bitfields_[transfer.traffic_days_].test(
              to_idx(state_.arrival_day_[segment]))) {
        continue;
      }
      // Handle new_segment
      auto const transport_new = state_.tbd_.segment_transports_[new_segment];
      auto const to = static_cast<stop_idx_t>(to_idx(
          new_segment - state_.tbd_.transport_first_segment_[transport_new]));
      auto const new_arrival_time =
          tt_.event_mam(transport_new, to, event_type::kArr);
      // TODO: How to handle costs of dist_to_dest for dest segments?
      // * props gets unnecessary for second part as we have the heuristic in
      // * cost function
      if (worst_delta < new_arrival_time ||
          state_.better_arrival(current, new_arrival_time)) {
        state_.arrival_day_.insert_or_assign(
            new_segment, day_idx_t{new_arrival_time.days()});
        state_.arrival_time_.insert_or_assign(
            new_segment, minutes_after_midnight_t{new_arrival_time.mam()});
        // Update Predecessor Table
        state_.pred_table_.insert_or_assign(new_segment,
                                            state_.sartSegmentPredecessor);
        state_.pq_.push(queue_entry{
            new_segment, static_cast<uint8_t>(current.transfers_ + 1)});
      }
    }
  }
}

void a_star::add_start(location_idx_t l, unixtime_t t) {
  // * Used from query_engine
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

      auto const start_segment =
          state_.tbd_.transport_first_segment_[et.t_idx_] + i;
      // TODO: this should be a state method props at least the insert
      // Update Arrival Time and Day
      // * important to use i + 1 as we want the arrival at the second stop of
      // * the segment
      auto const delta = tt_.event_mam(r, et.t_idx_, i + 1, event_type::kArr);
      state_.arrival_day_.emplace(start_segment, day_idx_t{delta.days()});
      state_.arrival_time_.emplace(start_segment,
                                   minutes_after_midnight_t{delta.mam()});
      // Update Predecessor Table
      state_.pred_table_.emplace(start_segment, state_.sartSegmentPredecessor);

      // Enqueue Element
      std::cout << "Adding start segment " << start_segment.v_ << " to PQ\n";
      state_.pq_.push(queue_entry{start_segment, 0U});
    }
  }
}

// void a_star::reconstruct(query const& q,
//                                          journey& j) const {
//  // TODO: implement reconstruct
// }

}  // namespace routing
}  // namespace nigiri