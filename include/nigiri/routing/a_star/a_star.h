#pragma once

#include "nigiri/common/dial.h"
#include "nigiri/routing/a_star/a_star_state.h"
#include "nigiri/routing/a_star/settings.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/types.h"

namespace nigiri {
namespace routing {

using tb_data = tb::tb_data;
using segment_idx_t = tb::segment_idx_t;

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

struct a_star {
  using algo_state_t = a_star_state;
  using algo_stats_t = a_star_stats;
  static constexpr bool kUseLowerBounds = false;

  static constexpr auto const kUnreachable =
      std::numeric_limits<std::uint16_t>::max();

  a_star(
      timetable const&,  // timetable
      rt_timetable const*,  // rt timetable
      a_star_state&,  // algo state
      bitvec const& is_dest,  // bitvec of destination segments
      std::array<bitvec, kMaxVias> const&,  // via segments
      std::vector<std::uint16_t> const&
          dist_to_dest,  // dest segments -> distance to dest in minutes for
                         // dest segments
      hash_map<location_idx_t,
               std::vector<td_offset>> const&,  // location_idx_t -> td_offsets
                                                // (valid_from, duration,
                                                // transport_mode_id)
      std::vector<std::uint16_t> const&
          lb,  // travel_time_lower_bound estimate for each segment // TODO:
               // check if true
      std::vector<via_stop> const&,  // via stops
      day_idx_t base,  // base day idx, presumably of start time
      clasz_mask_t,  // allowed clasz mask // TODO: what is this and why needed?
      bool,  // require_bikes_allowed
      bool,  // require_cars_allowed
      bool,  // is wheelchair profile
      transfer_time_settings);  // contains tranfer time factor in factor_ and
                                // min_transfer_time_
                                // TODO: check if other attributes are needed

  algo_stats_t get_stats() const { return stats_; };

  algo_state_t& get_state() { return state_; };

  void reset_arrivals() { state_.reset(); };

  void next_start_time() { state_.reset(); };

  void add_start(location_idx_t, unixtime_t);

  void execute(unixtime_t const start_time,
               std::uint8_t const max_transfers,
               unixtime_t const worst_time_at_dest,
               profile_idx_t const,
               pareto_set<journey>& results);

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

}  // namespace routing
}  // namespace nigiri