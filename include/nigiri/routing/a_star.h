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

pareto_set<journey> a_star(timetable const& tt,
                           tb::tb_data const& tbd,
                           unixtime_t const start_time,
                           location_idx_t const source,
                           location_idx_t const dest);

// ! Not sure if this is needed
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