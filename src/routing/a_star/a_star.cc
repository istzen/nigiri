#include "nigiri/routing/a_star/a_star.h"

#include "fmt/ranges.h"

#include "utl/enumerate.h"
#include "utl/raii.h"

#include "nigiri/common/dial.h"
#include "nigiri/routing/get_earliest_transport.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

#define as_debug fmt::println
// #define as_debug(...)

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
      base_{base - 5} {  // TODO: where does this come from?
  // TODO: initialize other stuff
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
          as_debug("Marking segment {} as reaching dest with dist {}", segment,
                   d.count());

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

void a_star::execute(unixtime_t const start_time,
                     std::uint8_t const max_transfers,
                     unixtime_t const worst_time_at_dest,
                     profile_idx_t const,
                     pareto_set<journey>& results) {
  // Setup state and other stuff
  auto const [day, time] = tt_.day_idx_mam(start_time);
  state_.setup(day - base_, time);
  delta const worst_delta =
      delta(to_idx(tt_.day_idx_mam(worst_time_at_dest).first) - base_.v_,
            tt_.day_idx_mam(worst_time_at_dest).second.count());
  uint16_t current_best_arrival = std::numeric_limits<uint16_t>::max();
  while (!state_.pq_.empty()) {
    auto const& current = state_.pq_.top();
    // Terminate if the lowest bucket is worse than the current best arrival
    if (state_.cost_function(current) > current_best_arrival) {
      return;
    }
    state_.pq_.pop();
    auto const& segment = current.segment_;
    // Check if segment is already settled in case of multiple entries in pq
    if (state_.settled_segments_.test(segment)) {
      continue;
    }
    state_.settled_segments_.set(segment, true);

    as_debug("Visiting segment {} with transfers {}", segment,
             current.transfers_);

    // Check if current segment reaches destination
    // TODO: cleanup horrible code
    if (state_.end_reachable_.test(segment)) [[unlikely]] {
      // check if the reached segment has a better arrival_time than the
      // currently best one
      auto bucket = state_.cost_function(current);
      auto const it = state_.dist_to_dest_.find(segment);
      if (it != end(state_.dist_to_dest_)) {
        bucket += static_cast<uint16_t>(it->second.count());
      }
      if (bucket >= current_best_arrival) {
        continue;
      }
      current_best_arrival = bucket;
      auto const d = base_ + state_.arrival_day_[segment];
      auto const t = state_.tbd_.segment_transports_[segment];
      auto const i = static_cast<stop_idx_t>(
          to_idx(segment - state_.tbd_.transport_first_segment_[t] + (1)));
      auto dest_time = tt_.event_time({t, d}, i, event_type::kArr) + 5_days;
      // Add dist_to_dest if needed
      if (it != end(state_.dist_to_dest_)) {
        dest_time += it->second;
      }
      as_debug("Reached destination via segment {} at time: {}", segment,
               dest_time);
      // remove any journey that was there before and add new one
      results.clear();
      results.add(
          {.legs_{},
           .start_time_ = start_time,
           .dest_time_ = dest_time,
           .dest_ = location_idx_t::invalid(),
           .transfers_ = static_cast<std::uint8_t>(current.transfers_)});
    }
    // Handle next segment for transport if exists
    auto const transport_current = state_.tbd_.segment_transports_[segment];
    auto const rel_segment =
        segment - state_.tbd_.transport_first_segment_[transport_current];
    auto const segment_range = state_.tbd_.get_segment_range(transport_current);
    auto const semgent_size = segment_range.size();
    // Note: -1 needed since that means it's the last segment and has no next
    if (rel_segment < semgent_size - 1) {
      auto const next_segment = segment + 1;
      auto const to = static_cast<stop_idx_t>(to_idx(rel_segment) + 2);
      auto const next_stop_arr =
          tt_.event_mam(transport_current, to, event_type::kArr);
      if (worst_delta < next_stop_arr) {
        state_.update_segment(next_segment, next_stop_arr, segment,
                              static_cast<uint8_t>(current.transfers_));
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
      auto const to = static_cast<stop_idx_t>(
          to_idx(new_segment -
                 state_.tbd_.transport_first_segment_[transport_new] + 1));
      auto const new_arrival_time =
          tt_.event_mam(transport_new, to, event_type::kArr);
      if (worst_delta < new_arrival_time) {
        state_.update_segment(new_segment, new_arrival_time, segment,
                              static_cast<uint8_t>(current.transfers_ + 1));
      }
    }
  }
  // Throw error if there was no journey found
  // TODO: check expected behavior in this case
  assert(!results.empty() && "No journey found!");
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
      // TODO: better with saving the time or just saving the segment and
      // calculating the time again?
      // Add to start_segments_ with arrival time at the segment
      // * important to use i + 1 as we want the arrival at the second stop of
      // * the segment and substract the base as start time is relative to base
      auto arr_time = event_day_idx_mam(et, static_cast<stop_idx_t>(i + 1),
                                        event_type::kArr);
      as_debug("Arrival_day size: {}, arrival time: size: {}",
               state_.arrival_day_.size(), state_.arrival_time_.size());
      auto [_, inserted] =
          state_.arrival_day_.emplace(start_segment, arr_time.days());
      // TODO: debug print
      assert(inserted &&
             "add_start: start segment already in arrival_day_ map");
      state_.arrival_time_.emplace(start_segment, arr_time.mam());
      state_.start_segments_.set(start_segment);
      as_debug("Adding start segment {} for location {}", start_segment,
               location{tt_, l});
    }
  }
}

void a_star::reconstruct(query const& q, journey& j) const {
  assert(q.dest_match_mode_ != location_match_mode::kIntermodal &&
         "Intermodal reconstruct not implemented yet");
  UTL_FINALLY([&]() { std::reverse(begin(j.legs_), end(j.legs_)); });

  auto const get_transport_info = [&](segment_idx_t const s,
                                      event_type const ev_type)
      -> std::tuple<transport, stop_idx_t, location_idx_t, unixtime_t> {
    auto const d = base_ + state_.arrival_day_[s] + 5;
    auto const t = state_.tbd_.segment_transports_[s];
    auto const i = static_cast<stop_idx_t>(
        to_idx(s - state_.tbd_.transport_first_segment_[t] +
               (ev_type == event_type::kArr ? 1 : 0)));
    auto const loc_seq = tt_.route_location_seq_[tt_.transport_route_[t]];
    return {{t, d},
            i,
            stop{loc_seq[i]}.location_idx(),
            tt_.event_time({t, d}, i, ev_type)};
  };

  auto const get_fp = [&](location_idx_t const from, location_idx_t const to) {
    if (from == to) {
      return footpath{to, tt_.locations_.transfer_time_[from]};
    }
    auto const from_fps =
        tt_.locations_.footpaths_out_[state_.tbd_.prf_idx_][from];
    auto const it = utl::find_if(
        from_fps, [&](footpath const& fp) { return fp.target() == to; });
    utl::verify(it != end(from_fps),
                "as  reconstruct: footpath from {} to {} not found",
                location{tt_, from}, location{tt_, to});
    return *it;
  };

  // ==================
  // (1) Last leg
  // ------------------
  auto dest_segment = segment_idx_t::invalid();

  // TODO: think about intermodal match mode
  for (auto arr_candidate_segment = state_.end_reachable_.next_set_bit(0);
       arr_candidate_segment != std::nullopt;
       arr_candidate_segment = state_.end_reachable_.next_set_bit(
           static_cast<uint32_t>(arr_candidate_segment.value()) + 1)) {
    as_debug("dest candidate {}", arr_candidate_segment.value());

    if (!state_.settled_segments_.test(arr_candidate_segment.value())) {
      as_debug("no dest candidate {} => has not been settled",
               arr_candidate_segment.value());
      continue;
    }

    auto const [_, dep_l, arr_l, arr_time] =
        get_transport_info(arr_candidate_segment.value(), event_type::kArr);

    auto const handle_fp = [&](footpath const& fp) {
      if (arr_time + fp.duration() != j.arrival_time()) {
        as_debug(
            "no dest candidate {} arr_l={}: arr_time={} + fp.duration={} = "
            "{} != j.arrival_time={}",
            arr_candidate_segment.value(), arr_l, arr_time, fp.duration(),
            location{tt_, arr_l}, arr_time, fp.duration(),
            arr_time + fp.duration(), j.arrival_time());
        return false;
      }
      as_debug("FOUND!");
      // add journey destination
      j.dest_ = fp.target();
      j.legs_.emplace_back(journey::leg{direction::kForward, arr_l, fp.target(),
                                        arr_time, j.arrival_time(), fp});
      return true;
    };

    if (handle_fp(footpath{arr_l, duration_t{0}})) {
      dest_segment = segment_idx_t{arr_candidate_segment.value()};
      break;
    }

    for (auto const fp :
         tt_.locations_.footpaths_out_[state_.tbd_.prf_idx_][arr_l]) {
      if (handle_fp(fp)) {
        dest_segment = segment_idx_t{arr_candidate_segment.value()};
        break;
      }
    }
    if (dest_segment != segment_idx_t::invalid()) {
      break;
    }
  }

  assert(dest_segment != segment_idx_t::invalid() &&
         "no dest segment found in reconstruct");

  // ==================
  // (2) Transport legs
  // ------------------
  auto current = dest_segment;
  auto [transport, arr_stop_idx, arr_l, arr_time] =
      get_transport_info(dest_segment, event_type::kArr);
  while (true) {
    auto const pred = state_.pred_table_.at(current);
    // check whether current is the last segment of current leg
    if (pred != state_.startSegmentPredecessor &&
        transport.t_idx_ == state_.tbd_.segment_transports_[pred]) {
      current = pred;
      continue;
    }
    // create leg for fp between transfer unless its the first leg added
    if (j.legs_.size() != 1) {
      auto const fp = get_fp(arr_l, j.legs_.back().from_);
      j.legs_.emplace_back(journey::leg{direction::kForward, arr_l,
                                        j.legs_.back().from_, arr_time,
                                        arr_time + fp.duration(), fp});
    }
    // create new leg for transport
    auto const [_, dep_stop_idx, dep_l, dep_time] =
        get_transport_info(current, event_type::kDep);
    j.legs_.emplace_back(journey::leg{
        direction::kForward, dep_l, arr_l, dep_time, arr_time,
        journey::run_enter_exit{
            rt::run{
                .t_ = transport,
                .stop_range_ = {static_cast<stop_idx_t>(0U),
                                static_cast<stop_idx_t>(
                                    tt_.route_location_seq_
                                        [tt_.transport_route_[transport.t_idx_]]
                                            .size())}},
            dep_stop_idx, arr_stop_idx}});
    if (pred == state_.startSegmentPredecessor) {
      break;
    }
    // Update arrival variables
    current = pred;
    std::tie(transport, arr_stop_idx, arr_l, arr_time) =
        get_transport_info(current, event_type::kArr);
  }

  // ==================
  // (3) First leg
  // ------------------
  assert(!j.legs_.empty());
  auto const start_time = j.start_time_;
  auto const first_dep_l = j.legs_.back().from_;
  auto const first_dep_time = j.legs_.back().dep_time_;
  // TODO: add intermodal
  for (auto const fp :
       tt_.locations_.footpaths_in_[state_.tbd_.prf_idx_][first_dep_l]) {
    // TODO: check why they used the has_offset here as well in query_engine
    if (start_time + fp.duration() <= first_dep_time) {
      j.legs_.push_back({direction::kForward, fp.target(), first_dep_l,
                         first_dep_time - fp.duration(), first_dep_time, fp});
      break;
    }
  }
}

}  // namespace routing
}  // namespace nigiri