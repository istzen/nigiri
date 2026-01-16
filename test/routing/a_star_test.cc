#include <chrono>
#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/routing/a_star/a_star.h"
#include "nigiri/routing/search.h"
#include "nigiri/routing/tb/preprocess.h"
#include "nigiri/routing/tb/tb_data.h"

using namespace date;
using namespace nigiri;
using namespace nigiri::routing;
using namespace nigiri::routing::tb;
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

a_star a_star_algo(timetable const& tt,
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
S3,S3,,,,,,

# calendar.txt
service_id,monday,tuesday,wednesday,thursday,friday,saturday,sunday,start_date,end_date
MON,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S0 -> S2",2
R3,DTA,R3,R3,"S3 -> S0 -> S2 -> S1",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1
R1,MON,R1_MON,R1_MON,2
R2,MON,R2_MON,R2_MON,3
R3,MON,R3_MON,R3_MON,4
R0,MON,R0B_MON,R0B_MON,5

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,05:00:00,05:00:00,S0,0,0,0
R0_MON,06:00:00,06:00:00,S1,1,0,0
R1_MON,12:00:00,12:00:00,S1,0,0,0
R1_MON,13:00:00,13:00:00,S2,1,0,0
R2_MON,01:00:00,01:00:00,S0,0,0,0
R2_MON,11:00:00,11:00:00,S2,1,0,0
R3_MON,04:30:00,04:30:00,S3,0,0,0
R3_MON,05:31:00,05:31:00,S0,1,0,0
R3_MON,07:00:00,07:00:00,S2,2,0,0
R3_MON,13:00:00,13:00:00,S1,3,0,0
R0B_MON,06:30:00,06:30:00,S0,0,0,0
R0B_MON,06:00:00,06:00:00,S1,1,0,0
)");
}

mem_dir add_start_test_files() {
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
MON,0,1,0,0,0,0,0,20210301,20210307
TUE,0,0,1,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S0 -> S2",2
R3,DTA,R3,R3,"S3 -> S0 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1
R0,TUE,R0_TUE,R0_TUE,2
R1,MON,R1_MON,R1_MON,3
R2,MON,R2_MON,R2_MON,4
R2,MON,R2B_MON,R2B_MON,5
R3,MON,R3_MON,R3_MON,6

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,01:00:00,01:00:00,S0,0,0,0
R0_MON,02:00:00,02:00:00,S1,1,0,0
R0_TUE,02:00:00,02:00:00,S0,0,0,0
R0_TUE,03:00:00,03:00:00,S1,1,0,0
R1_MON,04:00:00,04:00:00,S1,0,0,0
R1_MON,04:30:00,04:30:00,S2,1,0,0
R2_MON,11:00:00,11:00:00,S0,0,0,0
R2_MON,12:00:00,12:00:00,S2,1,0,0
R2B_MON,11:30:00,11:30:00,S0,0,0,0
R2B_MON,12:30:00,12:30:00,S2,1,0,0
R3_MON,06:30:00,06:30:00,S3,0,0,0
R3_MON,07:00:00,07:00:00,S0,1,0,0
R3_MON,08:00:00,08:00:00,S2,2,0,0
)");
}

mem_dir multiple_segment_run_files() {
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
MON,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S0 -> S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,04:30:00,04:30:00,S0,0,0,0
R0_MON,05:30:00,05:30:00,S3,1,0,0
R0_MON,13:00:00,13:00:00,S1,2,0,0
R0_MON,14:00:00,14:00:00,S2,3,0,0
)");
}

mem_dir edge_case_two_dest_segments_reached() {
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
MON,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S0 -> S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,04:30:00,04:30:00,S0,0,0,0
R0_MON,04:35:00,04:35:00,S3,1,0,0
R0_MON,04:40:00,04:40:00,S1,2,0,0
R0_MON,04:45:00,04:45:00,S2,3,0,0

# transfers.txt
from_stop_id,to_stop_id,transfer_type,min_transfer_time
S3,S2,2,900
)");
}

TEST(a_star, add_start) {
  auto const tt = load_gtfs(add_start_test_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time = unixtime_t{sys_days{March / 02 / 2021}};
  auto algo_state = a_star_state{tbd};
  fmt::println("Arrival_time size: {}", algo_state.arrival_time_.size());
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  // auto const get_transport_info = [&](segment_idx_t const s,
  //                                     event_type const ev_type)
  //     -> std::tuple<transport, stop_idx_t, location_idx_t, unixtime_t> {
  //   auto const d = algo_state.arrival_day_[s];
  //   auto const t = algo_state.tbd_.segment_transports_[s];
  //   auto const i = static_cast<stop_idx_t>(
  //       to_idx(s - algo_state.tbd_.transport_first_segment_[t] +
  //              (ev_type == event_type::kArr ? 1 : 0)));
  //   auto const loc_seq = tt.route_location_seq_[tt.transport_route_[t]];
  //   return {{t, d},
  //           i,
  //           stop{loc_seq[i]}.location_idx(),
  //           tt.event_time({t, d}, i, ev_type)};
  // };
  // fmt::println("Segment info:");
  // for (auto s = segment_idx_t{0};
  //      s < segment_idx_t{tbd.segment_transports_.size()};
  //      s = segment_idx_t{s + 1}) {
  //   auto [transport_dep, stop_idx_dep, loc_idx_dep, time_dep] =
  //       get_transport_info(s, event_type::kDep);
  //   fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}",
  //   s,
  //                tt.transport_name(transport_dep.t_idx_), stop_idx_dep,
  //                location{tt, loc_idx_dep}, time_dep);
  //   auto [transport, stop_idx, loc_idx, time] =
  //       get_transport_info(s, event_type::kArr);
  //   fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}",
  //   s,
  //                tt.transport_name(transport.t_idx_), stop_idx,
  //                location{tt, loc_idx}, time);
  // }
  auto const location_idx =
      tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}});
  algo.add_start(location_idx, start_time + 3_hours);  // 03:00
  EXPECT_EQ(&algo_state, &algo.get_state());
  // It adds the correct start segments
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{1}));  // starts
  // one day later
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{3}));  // earliest
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{6}));
  // It does not add other segments
  EXPECT_FALSE(
      algo_state.start_segments_.test(segment_idx_t{0}));  // starts before
  EXPECT_FALSE(
      algo_state.start_segments_.test(segment_idx_t{2}));  // wrong location
  EXPECT_FALSE(
      algo_state.start_segments_.test(segment_idx_t{4}));  // not earliest
  EXPECT_FALSE(
      algo_state.start_segments_.test(segment_idx_t{5}));  // wrong location

  // It adds the correct arrival times for the start segments
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{3}), day_idx_t{0});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{3}),
            minutes_after_midnight_t{720});  // 12:00
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{6}), day_idx_t{0});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{6}),
            minutes_after_midnight_t{480});  // 08:00
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{1}), day_idx_t{1});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{1}),
            minutes_after_midnight_t{180});  // 03:00
}

TEST(a_star, reconstruct_only_one_segment_runs) {
  auto const tt = load_gtfs(same_day_transfer_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time =
      unixtime_t{sys_days{February / 28 / 2021} + std::chrono::hours{23}};
  auto algo_state = a_star_state{tbd};
  // setup state
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  algo_state.update_segment(segment_idx_t{0}, delta{0, 0},
                            a_star_state::startSegmentPredecessor, 0U);
  algo_state.update_segment(segment_idx_t{2}, delta{0, 20}, segment_idx_t{0},
                            2U);
  algo_state.settled_segments_.set(segment_idx_t{0}, true);
  algo_state.settled_segments_.set(segment_idx_t{2}, true);
  journey j;
  j.start_time_ = start_time;
  j.dest_time_ = start_time + duration_t{840};  // 14 hours later
  query q;
  q.start_time_ = start_time;

  auto const get_transport_info = [&](segment_idx_t const s,
                                      event_type const ev_type)
      -> std::tuple<transport, stop_idx_t, location_idx_t, unixtime_t> {
    auto const d = algo_state.arrival_day_[s];
    auto const t = algo_state.tbd_.segment_transports_[s];
    auto const i = static_cast<stop_idx_t>(
        to_idx(s - algo_state.tbd_.transport_first_segment_[t] +
               (ev_type == event_type::kArr ? 1 : 0)));
    auto const loc_seq = tt.route_location_seq_[tt.transport_route_[t]];
    return {{t, d},
            i,
            stop{loc_seq[i]}.location_idx(),
            tt.event_time({t, d}, i, ev_type)};
  };
  fmt::println("Segment info:");
  for (auto s = segment_idx_t{0}; s < segment_idx_t{6};
       s = segment_idx_t{s + 1}) {
    auto [transport_dep, stop_idx_dep, loc_idx_dep, time_dep] =
        get_transport_info(s, event_type::kDep);
    fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}", s,
                 transport_dep.t_idx_.v_, stop_idx_dep, loc_idx_dep, time_dep);
    auto [transport, stop_idx, loc_idx, time] =
        get_transport_info(s, event_type::kArr);
    fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}", s,
                 transport.t_idx_.v_, stop_idx, loc_idx, time);
  }
  algo.reconstruct(q, j);
  j.print(std::cout, tt);
  EXPECT_EQ(j.legs_.size(), 4U);
}

TEST(a_star, reconstruct_multiple_segment_runs) {
  auto const tt = load_gtfs(multiple_segment_run_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time =
      unixtime_t{sys_days{February / 28 / 2021} + std::chrono::hours{23}};
  auto algo_state = a_star_state{tbd};
  // setup state
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  algo_state.update_segment(segment_idx_t{0}, delta{0, 0},
                            a_star_state::startSegmentPredecessor, 0U);
  algo_state.update_segment(segment_idx_t{1}, delta{0, 390}, segment_idx_t{0},
                            0U);
  algo_state.update_segment(segment_idx_t{2}, delta{0, 840}, segment_idx_t{1},
                            0U);
  algo_state.settled_segments_.set(segment_idx_t{0}, true);
  algo_state.settled_segments_.set(segment_idx_t{1}, true);
  algo_state.settled_segments_.set(segment_idx_t{2}, true);
  journey j;
  j.start_time_ = start_time;
  j.dest_time_ = start_time + duration_t{900};  // 15 hours later
  query q;
  q.start_time_ = start_time;
  algo.reconstruct(q, j);
  j.print(std::cout, tt);
  EXPECT_EQ(j.legs_.size(), 2U);
}

void run_execute_test(auto const& files) {
  auto const tt = load_gtfs(files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  tbd.print(std::cout, tt);
  auto start_time = unixtime_t{sys_days{March / 02 / 2021}};
  auto algo_state = a_star_state{tbd};
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  auto size = tt.locations_.location_id_to_idx_.size();
  tt.locations_.location_id_to_idx_.find(location_id{"S0", source_idx_t{size}});
  auto const location_idx =
      tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}});
  algo.add_start(location_idx, start_time);
  pareto_set<journey> results;
  algo.execute(start_time + 2_hours, 2U, unixtime_t::max(), profile_idx_t{0},
               results);
  auto const get_transport_info = [&](segment_idx_t const s,
                                      event_type const ev_type)
      -> std::tuple<transport, stop_idx_t, location, unixtime_t> {
    auto const d = algo_state.arrival_day_[s];
    auto const t = algo_state.tbd_.segment_transports_[s];
    auto const i = static_cast<stop_idx_t>(
        to_idx(s - algo_state.tbd_.transport_first_segment_[t] +
               (ev_type == event_type::kArr ? 1 : 0)));
    auto const loc_seq = tt.route_location_seq_[tt.transport_route_[t]];
    return {{t, d},
            i,
            location{tt, stop{loc_seq[i]}.location_idx()},
            tt.event_time({t, d}, i, ev_type)};
  };
  fmt::println("Segment info:");
  for (auto s = segment_idx_t{0};
       s < segment_idx_t{tbd.segment_transports_.size()};
       s = segment_idx_t{s + 1}) {
    auto [transport_dep, stop_idx_dep, loc_idx_dep, time_dep] =
        get_transport_info(s, event_type::kDep);
    fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}", s,
                 transport_dep.t_idx_.v_, stop_idx_dep, loc_idx_dep, time_dep);
    auto [transport, stop_idx, loc_idx, time] =
        get_transport_info(s, event_type::kArr);
    fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}", s,
                 transport.t_idx_.v_, stop_idx, loc_idx, time);
  }
  for (auto j : results) {
    algo.reconstruct(routing::query{}, j);
    j.print(std::cout, tt);
    EXPECT_EQ(j.arrival_time(), j.legs_.back().arr_time_);
  }
  EXPECT_EQ(results.size(), 1U);
}

TEST(a_star, execute_1) { run_execute_test(same_day_transfer_files_as); }
TEST(a_star, execute_2) { run_execute_test(multiple_segment_run_files); }
TEST(a_star, execute_3) {
  run_execute_test(edge_case_two_dest_segments_reached);
}