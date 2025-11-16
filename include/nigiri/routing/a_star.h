#pragma once

#include "nigiri/routing/journey.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/types.h"

namespace nigiri {
namespace routing {


using arrivial_time_map = vector_map<tb::segment_idx_t, timezone_idx_t>;
using day_index_map = vector_map<tb::segment_idx_t, day_idx_t>;

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

struct pred_table {

};

struct label {
  label(tb::segment_idx_t const s, std::uint8_t t) : segment_{s}, transfers_{t} {}

  tb::segment_idx_t segment_;
  std::uint8_t transfers_;
};


journey a_star(tb::tb_data const&,
               unixtime_t const start_time,
               location_idx_t const source,
               location_idx_t const dest);
} // namespace routing
} // namespace nigiri