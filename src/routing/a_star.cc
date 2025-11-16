#include "nigiri/routing/journey.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

namespace nigiri::routing {

journey a_star(timetable const& tt,
               tb::tb_data const& tb_graph,
               unixtime_t const start_time,
               location_idx_t const source,
               location_idx_t const dest) {
  // Get segments starting from source
  
  // Get segments leading to dest

  // Perform algorithm

  // Translate output back to location_idx_t

  // Create and return journey
}
} // namespace nigiri::routing