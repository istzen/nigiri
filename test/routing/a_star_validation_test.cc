#include <chrono>
#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/hrd/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/a_star/a_star.h"
#include "nigiri/routing/search.h"
#include "nigiri/routing/tb/preprocess.h"
#include "nigiri/routing/tb/query_engine.h"
#include "nigiri/routing/tb/tb_data.h"

#include "../loader/hrd/hrd_timetable.h"
#include "../raptor_search.h"

using namespace date;
using namespace nigiri;
using namespace nigiri::routing;
using namespace nigiri::routing::tb;
using namespace nigiri::loader;
using namespace nigiri::test_data::hrd_timetable;

timetable load_gtfs(auto const& files) {
  timetable tt;
  tt.date_range_ = {date::sys_days{2021_y / March / 1},
                    date::sys_days{2021_y / March / 8}};
  register_special_stations(tt);
  gtfs::load_timetable({}, source_idx_t{0}, files(), tt);
  finalize(tt);
  return tt;
}

timetable load_hrd(auto const& files) {
  timetable tt;
  tt.date_range_ = nigiri::test_data::hrd_timetable::full_period();
  register_special_stations(tt);
  hrd::load_timetable(source_idx_t{0U}, loader::hrd::hrd_5_20_26, files(), tt);
  finalize(tt);
  return tt;
}

std::string result_str_as(auto const& result, timetable const& tt) {
  std::stringstream ss;
  ss << "\n";
  result.print(ss, tt);
  ss << "\n";
  return ss.str();
}

std::string results_str_as(auto const& results, timetable const& tt) {
  std::stringstream ss;
  for (auto const& r : results) {
    ss << "\n";
    r.print(ss, tt);
    ss << "\n";
  }
  return ss.str();
}

pareto_set<routing::journey> algo_search(timetable const& tt,
                                         tb_data const& tbd,
                                         routing::query q,
                                         bool is_a_star = true) {
  static auto search_state = routing::search_state{};

  if (is_a_star) {
    auto algo_state = a_star_state{tbd};
    return *(routing::search<direction::kForward, a_star<true>>{
        tt, nullptr, search_state, algo_state, std::move(q)}
                 .execute()
                 .journeys_);
  } else {
    auto trip_based_state = tb::query_state{tt, tbd};
    return *(routing::search<direction::kForward, tb::query_engine<true>>{
        tt, nullptr, search_state, trip_based_state, std::move(q)}
                 .execute()
                 .journeys_);
  }
}

pareto_set<routing::journey> algo_search(timetable const& tt,
                                         tb_data const& tbd,
                                         std::string_view from,
                                         std::string_view to,
                                         routing::start_time_t const time,
                                         float const transfer_factor = 1.0,
                                         bool const is_a_star = true) {
  auto const src = source_idx_t{0};
  auto q = routing::query{
      .start_time_ = time,
      .start_ = {{tt.locations_.location_id_to_idx_.at({from, src}), 0_minutes,
                  0U}},
      .destination_ = {{tt.locations_.location_id_to_idx_.at({to, src}),
                        0_minutes, 0U}},
      .use_start_footpaths_ = true,
      .max_transfers_ = 8,
      .transfer_time_settings_ = {.factor_ = transfer_factor}};
  return algo_search(tt, tbd, std::move(q), is_a_star);
}

mem_dir pareto_files() {
  return mem_dir::read(R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DTA,Demo Transit Authority,,Europe/London

# stops.txt
stop_id,stop_name,stop_desc,stop_lat,stop_lon,stop_url,location_type,parent_station
S0,S0,,,,,,
S1,S1,,,,,,
S2,S2,,,,,,
S3,S3,,,,,,

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
R0_MON,01:00:00,01:00:00,S0,0,0,0
R0_MON,01:30:00,01:30:00,S1,1,0,0
R1_MON,02:00:00,02:00:00,S1,0,0,0
R1_MON,02:30:00,02:30:00,S2,1,0,0
R2_MON,01:00:00,01:00:00,S0,0,0,0
R2_MON,03:00:00,03:00:00,S2,1,0,0
)");
}

TEST(a_star_validation, pareto_files) {
  auto const tt = load_gtfs(pareto_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto results_a_star = algo_search(tt, tbd, "S0", "S2",
                                    unixtime_t{sys_days{2021_y / March / 1}});
  EXPECT_EQ(results_a_star.size(), 1U);
  auto result_without_transfer = algo_search(
      tt, tbd, "S0", "S2", unixtime_t{sys_days{2021_y / March / 1}}, 31.0F);
  EXPECT_EQ(result_without_transfer.size(), 1U);
  result_without_transfer.add_not_optimal(results_a_star.els_.at(0));
  auto const tb_results =
      algo_search(tt, tbd, "S0", "S2", unixtime_t{sys_days{2021_y / March / 1}},
                  1.0F, false);
  for (auto i = 0U; i < result_without_transfer.size(); ++i) {
    fmt::println("A*: {}", result_without_transfer.els_.at(i).arrival_time());
    fmt::println("TB: {}", tb_results.els_.at(i).arrival_time());
    EXPECT_TRUE(result_without_transfer.els_.at(i) == tb_results.els_.at(i));
  }
}