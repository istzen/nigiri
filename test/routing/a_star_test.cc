#include <chrono>
#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/a_star/a_star.h"
#include "nigiri/routing/search.h"
#include "nigiri/routing/tb/preprocess.h"

using namespace date;
using namespace nigiri;
using namespace nigiri::routing;
using namespace nigiri::loader;

timetable load_gtfs(auto const& files) {
  timetable tt;
  tt.date_range_ = {date::sys_days{2021_y / March / 1},
                    date::sys_days{2021_y / March / 8}};
  register_special_stations(tt);
  gtfs::load_timetable({}, source_idx_t{0}, files(), tt);
  finalize(tt);
  return tt;
}

a_star& a_star_algo(timetable const& tt,
                    tb::tb_data const& tbd,
                    routing::query q) {
  static auto search_state = routing::search_state{};
  auto algo_state = a_star_state{tbd};
  return routing::search<direction::kForward, a_star>{tt, nullptr, search_state,
                                                      algo_state, std::move(q)}
      .get_algo();
}

a_star a_star_algo(timetable const& tt,
                   a_star_state& state,
                   std::string_view from,
                   std::string_view to,
                   routing::start_time_t const time) {
  auto src = source_idx_t{0};
  auto q = routing::query{
      .start_time_ = time,
      .start_ = {{tt.locations_.location_id_to_idx_.at({from, src}), 0_minutes,
                  0U}},
      .destination_ = {
          {tt.locations_.location_id_to_idx_.at({to, src}), 0_minutes, 0U}}};
  src = source_idx_t{1};
  static auto search_state = routing::search_state{};
  return routing::search<direction::kForward, a_star>{tt, nullptr, search_state,
                                                      state, std::move(q)}
      .get_algo();
}

mem_dir same_day_transfer_files_as() {
  return mem_dir::read(R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DTA,Demo Transit Authority,,Europe/London

# stops.txt
stop_id,stop_name,stop_desc,stop_lat,stop_lon,stop_url,location_type,parent_station
S0,S0,,,,,,
S1,S1,,,,,,
S2,S2,,,,,,

# calendar.txt
service_id,monday,tuesday,wednesday,thursday,friday,saturday,sunday,start_date,end_date
MON,1,0,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S0 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1
R1,MON,R1_MON,R1_MON,2
R2,MON,R2_MON,R2_MON,3

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,00:00:00,00:00:00,S0,0,0,0
R0_MON,06:00:00,06:00:00,S1,1,0,0
R1_MON,12:00:00,12:00:00,S1,0,0,0
R1_MON,13:00:00,13:00:00,S2,1,0,0
R2_MON,01:00:00,01:01:00,S0,0,0,0
R2_MON,12:00:00,12:00:00,S2,1,0,0
)");
}

TEST(a_star, add_start) {
  delta d{0};
  day_index_map map;
  segment_idx_t key{0};
  day_idx_t value{d.days()};
  map.emplace(key, value);

  auto const tt = load_gtfs(same_day_transfer_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});

  auto start_time =
      unixtime_t{sys_days{February / 28 / 2021} + std::chrono::hours{23}};
  auto algo_state = a_star_state{tbd};
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  auto size = tt.locations_.location_id_to_idx_.size();
  tt.locations_.location_id_to_idx_.find(location_id{"S0", source_idx_t{size}});
  auto const location_idx =
      tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}});
  algo.add_start(location_idx, start_time);
  auto pq = algo.get_state().pq_;
  EXPECT_EQ(pq.size(), 2U);
  routing::queue_entry const& qe = pq.top();
  pq.pop();
  routing::queue_entry const& qe2 = pq.top();
  EXPECT_EQ(qe.segment_, segment_idx_t{0});
  EXPECT_EQ(qe2.segment_, segment_idx_t{2});
  EXPECT_EQ(qe.transfers_, 0U);
  EXPECT_EQ(qe.transfers_, 0U);
  EXPECT_EQ(algo.get_state().pred_table_.at(qe.segment_),
            a_star_state::sartSegmentPredecessor);
  EXPECT_EQ(algo.get_state().pred_table_.at(qe2.segment_),
            a_star_state::sartSegmentPredecessor);
}