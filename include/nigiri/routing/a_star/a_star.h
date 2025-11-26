#pragma once

#include "nigiri/common/dial.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/routing/a_star/settings.h"
#include "nigiri/types.h"

namespace nigiri {
namespace routing {

using tb_data = tb::tb_data;
using segment_idx_t = tb::segment_idx_t;


// * The next three lines are the data structures needed as per info paper
  // TODO: this should be a datetime already so the type needs to change or the second line is not needed
using arrival_time_map = hash_map<segment_idx_t, minutes_after_midnight_t>; // segment -> arrival_time mapping
using day_index_map = hash_map<segment_idx_t, day_idx_t>; // segment -> arrival_day
using pred_table = hash_map<segment_idx_t, segment_idx_t>; // pred_table initialized with nullopt for everything

struct queue_entry {
  queue_entry(segment_idx_t const s, std::uint8_t t) : segment_{s}, transfers_{t} {}

  segment_idx_t const segment_;
  std::uint8_t transfers_;
};

struct cost_function {
  using dist_t = std::uint16_t;

  cost_function(minutes_after_midnight_t const st,
                day_idx_t const sd,
                float const tf = 1.0F,
                const arrival_time_map* at = nullptr, 
                const day_index_map* ad = nullptr) 
            : arrival_time_{at}, arrival_day_{ad}, transfer_factor_{tf}, start_day_{sd}, start_time_{st} {}

  dist_t operator()(queue_entry const& q) const { 
    // TODO: what happens if there is no arrival_time, is that possible?
    auto const time_travelled = static_cast<dist_t>((arrival_day_->find(q.segment_)->second.v_ - start_day_.v_)*24*60 + arrival_time_->find(q.segment_)->second.count() - start_time_.count());
    return (time_travelled + transfer_factor_ * q.transfers_);
  }

  const arrival_time_map* const arrival_time_; // segment -> arrival_time
  const day_index_map* const arrival_day_; // segment -> arrival_day
  float const transfer_factor_; // default value
  day_idx_t const start_day_; // day_idx_t of start_time
  minutes_after_midnight_t const start_time_; // minutes_after_midnight_t of start_time

  // TODO: fix
  template <typename Map>
  uint16_t get_value(Map const& map, segment_idx_t const segment) {
    auto it = map.find(segment);
    if (it == map.end()) {
        throw std::runtime_error("Key not found in map");
    }
    return it->second.v_;
}
};

struct a_star_stats {
  // TODO: implement and find out what is actually needed here.
  std::map<std::string, std::uint64_t> to_map() const {
    return {
        {"n_segments_enqueued", n_segments_enqueued_},
        {"n_journeys_found", n_journeys_found_},
    };
  }

  std::uint64_t n_segments_enqueued_{0ULL};
  std::uint64_t n_journeys_found_{0ULL};
};

struct a_star_state {
  // TODO: maybe change to different value like invalid - 1
  static constexpr auto const sartSegmentPredecessor = segment_idx_t::invalid();

  a_star_state(tb_data const& tbd)
      : tbd_{tbd}, pq_{maxASTravelTime.count(), cost_function{minutes_after_midnight_t{0}, day_idx_t{0}, 1.0F, &arrival_time_, &arrival_day_}} {
    end_reachable_.resize(tbd.segment_transfers_.size());
      }
  
  // TODO: check if needed
  void update_segment_arr_time(segment_idx_t const s,
                               duration_t const d) {
    if(auto const it_pt = pred_table_.find(s); it_pt != end(pred_table_)) {
      // Update arr_time and arr_day based on the time of pred if improved
      auto const pred = it_pt->second;
      // TODO: can we assume that this is present?
      auto const it_at_p = arrival_time_.find(pred);
      auto const new_arr_time = static_cast<minutes_after_midnight_t>(it_at_p->second + d);
      auto [it_at_s, inserted] = arrival_time_.try_emplace(s, new_arr_time);
      if(!inserted) {
        it_at_s->second = std::min(it_at_s->second, new_arr_time);
      }
    } else {
      // s is a starting segment and thus has no pred
      // TODO: fix 
      arrival_time_.emplace(s, d);
    }
  };

  tb_data const& tbd_; // preprocessed tb data

  arrival_time_map arrival_time_; // segment -> arrival_time
  day_index_map arrival_day_; // segment -> arrival_day
  pred_table pred_table_; // predecessor table
  hash_map<segment_idx_t, duration_t> dist_to_dest_;; // segment -> distance to destination
  dial<queue_entry, cost_function> pq_; // priority_queue
  bitvec_map<segment_idx_t> end_reachable_; // segments from which the destination is reachable
};

struct a_star {
  using algo_state_t = a_star_state;
  using algo_stats_t = a_star_stats;
  static constexpr bool kUseLowerBounds = false;

  static constexpr auto const kUnreachable =
      std::numeric_limits<std::uint16_t>::max();
  
  a_star(timetable const&, // timetable
         rt_timetable const*, // rt timetable
         a_star_state&, // algo state // TODO: what is this used for?
         bitvec const& is_dest, // bitvec of destination segments
         std::array<bitvec, kMaxVias> const&, // via segments
         std::vector<std::uint16_t> const& dist_to_dest, // dest segments -> distance to dest in minutes // TODO: check if true
         hash_map<location_idx_t, std::vector<td_offset>> const&, // location_idx_t -> td_offsets (valid_from, duration, transport_mode_id)
         std::vector<std::uint16_t> const& lb, // travel_time_lower_bound estimate for each segment // TODO: check if true
         std::vector<via_stop> const&, // via stops
         day_idx_t base, // base day idx, presumably of start time
         clasz_mask_t, // allowed clasz mask // TODO: what is this and why needed?
         bool, // require_bikes_allowed
         bool, // require_cars_allowed
         bool, // is wheelchair profile
        transfer_time_settings); // contains tranfer time factor in factor_ and min_transfer_time_ // TODO: check if other attruibutes are needed

  algo_stats_t get_stats() const { return stats_; };

  algo_state_t& get_state() { return state_; };

  void reset_arrivals() {
    // TODO: implement reset_arrivals
  };

  void next_start_time() {
    // TODO: implement next_start_time
  };

  void add_start(location_idx_t, unixtime_t);

  void execute(unixtime_t const start_time,
               std::uint8_t const max_transfers,
               unixtime_t const worst_time_at_dest,
               profile_idx_t const,
               journey& result);

  void reconstruct(query const& q, journey& j) const;

private:
  timetable const& tt_;
  a_star_state& state_;
  bitvec const& is_dest_;
  std::vector<std::uint16_t> const& dist_to_dest_;
  std::vector<std::uint16_t> const& lb_;
  day_idx_t const base_;
  a_star_stats stats_;
};

} // namespace routing
} // namespace nigiri