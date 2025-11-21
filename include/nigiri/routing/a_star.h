#pragma once

#include "nigiri/routing/journey.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/types.h"

namespace nigiri {
namespace routing {


// * The next three lines are the data structures needed as per info paper
  // TODO: this should be a datetime already so the type needs to change or the second line is not needed
using arrival_time_map = hash_map<tb::segment_idx_t, minutes_after_midnight_t>; // segment -> arrival_time mapping
using day_index_map = hash_map<tb::segment_idx_t, day_idx_t>; // segment -> arrival_day
using pred_table = hash_map<tb::segment_idx_t, tb::segment_idx_t>; // pred_table initialized with nullopt for everything

struct queue_entry {
  queue_entry(tb::segment_idx_t const s, std::uint8_t t) : segment_{s}, transfers_{t} {}

  tb::segment_idx_t const segment_;
  std::uint8_t transfers_;
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

};

template <bool UseLowerBounds>
struct a_star {
  using algo_state_t = a_star_state;
  using algo_stats_t = a_star_stats;

  static constexpr bool kUseLowerBounds = UseLowerBounds;
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

  void reset_arrivals();

  void next_start_time();

  void add_start(location_idx_t, unixtime_t);

  void execute(unixtime_t const start_time,
               std::uint8_t const max_transfers,
               unixtime_t const worst_time_at_dest,
               profile_idx_t const,
               journey result);

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

// ! Not sure if this is needed

// TODO: remove once everyrthing is moved into the struct above
pareto_set<journey> prelime_algo(timetable const& tt,
               tb::tb_data const& tbd,
               unixtime_t const start_time,
               location_idx_t const source,
               location_idx_t const dest);
struct at_map {
  at_map(tb::segment_idx_t const l, timezone_idx_t const at) : l_{l}, at_{at} {}
  friend bool operator>(at_map a,at_map b) { return a.at_ > b.at_; }
  tb::segment_idx_t l_;
  timezone_idx_t at_;
};

struct di_map {
  di_map(tb::segment_idx_t const l, day_idx_t const di) : l_{l}, di_{di} {}
  friend bool operator>(di_map a,di_map b) { return a.di_ > b.di_; }
  tb::segment_idx_t l_;
  day_idx_t di_;
};

} // namespace routing
} // namespace nigiri