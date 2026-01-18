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

std::string result_str(auto const& result, timetable const& tt) {
  std::stringstream ss;
  ss << "\n";
  result.print(ss, tt);
  ss << "\n";
  return ss.str();
}

std::string results_str(auto const& results, timetable const& tt) {
  std::stringstream ss;
  for (auto const& r : results) {
    ss << "\n";
    r.print(ss, tt);
    ss << "\n";
  }
  return ss.str();
}

pareto_set<routing::journey> a_star_search(timetable const& tt,
                                           tb_data const& tbd,
                                           routing::query q) {
  static auto search_state = routing::search_state{};
  auto algo_state = a_star_state{tbd};

  return *(routing::search<direction::kForward, a_star>{
      tt, nullptr, search_state, algo_state, std::move(q)}
               .execute()
               .journeys_);
}

pareto_set<routing::journey> a_star_search(timetable const& tt,
                                           tb_data const& tbd,
                                           std::string_view from,
                                           std::string_view to,
                                           routing::start_time_t const time) {
  auto const src = source_idx_t{0};
  auto q = routing::query{
      .start_time_ = time,
      .start_ = {{tt.locations_.location_id_to_idx_.at({from, src}), 0_minutes,
                  0U}},
      .destination_ = {{tt.locations_.location_id_to_idx_.at({to, src}),
                        0_minutes, 0U}},
      .use_start_footpaths_ = true};
  return a_star_search(tt, tbd, std::move(q));
}

// void print_segment_info(timetable const& tt, a_star_state const& algo_state)
// {

//   auto const get_transport_info = [&](segment_idx_t const s,
//                                       event_type const ev_type)
//       -> std::tuple<transport, stop_idx_t, location_idx_t, unixtime_t> {
//     auto const d = algo_state.arrival_day_[s];
//     auto const t = algo_state.tbd_.segment_transports_[s];
//     auto const i = static_cast<stop_idx_t>(
//         to_idx(s - algo_state.tbd_.transport_first_segment_[t] +
//                (ev_type == event_type::kArr ? 1 : 0)));
//     auto const loc_seq = tt.route_location_seq_[tt.transport_route_[t]];
//     return {{t, d},
//             i,
//             stop{loc_seq[i]}.location_idx(),
//             tt.event_time({t, d}, i, ev_type)};
//   };
//   fmt::println("Segment info:");
//   for (auto s = segment_idx_t{0}; s < segment_idx_t{6};
//        s = segment_idx_t{s + 1}) {
//     auto [transport_dep, stop_idx_dep, loc_idx_dep, time_dep] =
//         get_transport_info(s, event_type::kDep);
//     fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}",
//     s,
//                  transport_dep.t_idx_.v_, stop_idx_dep, loc_idx_dep,
//                  time_dep);
//     auto [transport, stop_idx, loc_idx, time] =
//         get_transport_info(s, event_type::kArr);
//     fmt::println(" Segment {}: transport {}, stop {}, location {}, time {}",
//     s,
//                  transport.t_idx_.v_, stop_idx, loc_idx, time);
//   }
// }

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

// ==================
// ADD START TESTS
// ------------------

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
TUE,0,1,0,0,0,0,0,20210301,20210307
WED,0,0,1,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S0 -> S2",2
R3,DTA,R3,R3,"S3 -> S0 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R0,WED,R0_WED,R0_WED,2
R1,TUE,R1_TUE,R1_TUE,3
R2,TUE,R2_TUE,R2_TUE,4
R2,TUE,R2B_TUE,R2B_TUE,5
R3,TUE,R3_TUE,R3_TUE,6

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,01:00:00,01:00:00,S0,0,0,0
R0_TUE,02:00:00,02:00:00,S1,1,0,0
R0_WED,02:00:00,02:00:00,S0,0,0,0
R0_WED,03:00:00,03:00:00,S1,1,0,0
R1_TUE,04:00:00,04:00:00,S1,0,0,0
R1_TUE,04:30:00,04:30:00,S2,1,0,0
R2_TUE,11:00:00,11:00:00,S0,0,0,0
R2_TUE,12:00:00,12:00:00,S2,1,0,0
R2B_TUE,11:30:00,11:30:00,S0,0,0,0
R2B_TUE,12:30:00,12:30:00,S2,1,0,0
R3_TUE,06:30:00,06:30:00,S3,0,0,0
R3_TUE,07:00:00,07:00:00,S0,1,0,0
R3_TUE,08:00:00,08:00:00,S2,2,0,0
)");
}

TEST(a_star, add_start) {
  auto const tt = load_gtfs(add_start_test_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time = unixtime_t{sys_days{March / 02 / 2021}};
  auto algo_state = a_star_state{tbd};
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  auto const location_idx =
      tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}});
  algo.add_start(location_idx, start_time + 3_hours);  // 03:00

  // It adds the correct start segments with the correct times
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{1}));
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{1}), day_idx_t{1});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{1}),
            minutes_after_midnight_t{180});  // 03:00 one day later
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{3}));
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{3}), day_idx_t{0});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{3}),
            minutes_after_midnight_t{720});  // 12:00 same day
  EXPECT_TRUE(algo_state.start_segments_.test(segment_idx_t{6}));
  EXPECT_EQ(algo_state.arrival_day_.at(segment_idx_t{6}), day_idx_t{0});
  EXPECT_EQ(algo_state.arrival_time_.at(segment_idx_t{6}),
            minutes_after_midnight_t{480});  // 08:00 same day

  // It does not add other segments or times
  EXPECT_EQ(algo_state.start_segments_.count(), 3);
  EXPECT_EQ(algo_state.arrival_day_.size(), 3);
  EXPECT_EQ(algo_state.arrival_time_.size(), 3);
}

// ==================
// RECONSTRUCT TESTS
// ------------------

mem_dir resconstruct_only_one_segment_runs() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S2 -> S3",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,TUE,R1_TUE,R1_TUE,2
R2,TUE,R2_TUE,R2_TUE,3

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,05:00:00,05:00:00,S0,0,0,0
R0_TUE,06:00:00,06:00:00,S1,1,0,0
R1_TUE,12:00:00,12:00:00,S1,0,0,0
R1_TUE,13:00:00,13:00:00,S2,1,0,0
R2_TUE,14:00:00,14:00:00,S2,0,0,0
R2_TUE,15:00:00,15:00:00,S3,1,0,0
)");
}

constexpr auto const only_one_segment_runs_journey = R"(
[2021-03-02 00:00, 2021-03-02 15:00]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 05:00]
       TO: (S3, S3) [2021-03-02 15:00]
leg 0: (S0, S0) [2021-03-02 05:00] -> (S1, S1) [2021-03-02 06:00]
   0: S0      S0..............................................                               d: 02.03 05:00 [02.03 05:00]  [{name=R0, day=2021-03-02, id=R0_TUE, src=0}]
   1: S1      S1.............................................. a: 02.03 06:00 [02.03 06:00]
leg 1: (S1, S1) [2021-03-02 06:00] -> (S1, S1) [2021-03-02 06:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-02 12:00] -> (S2, S2) [2021-03-02 13:00]
   0: S1      S1..............................................                               d: 02.03 12:00 [02.03 12:00]  [{name=R1, day=2021-03-02, id=R1_TUE, src=0}]
   1: S2      S2.............................................. a: 02.03 13:00 [02.03 13:00]
leg 3: (S2, S2) [2021-03-02 13:00] -> (S2, S2) [2021-03-02 13:02]
  FOOTPATH (duration=2)
leg 4: (S2, S2) [2021-03-02 14:00] -> (S3, S3) [2021-03-02 15:00]
   0: S2      S2..............................................                               d: 02.03 14:00 [02.03 14:00]  [{name=R2, day=2021-03-02, id=R2_TUE, src=0}]
   1: S3      S3.............................................. a: 02.03 15:00 [02.03 15:00]
leg 5: (S3, S3) [2021-03-02 15:00] -> (S3, S3) [2021-03-02 15:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, reconstruct_only_one_segment_runs) {
  auto const tt = load_gtfs(resconstruct_only_one_segment_runs);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time = unixtime_t{sys_days{March / 02 / 2021}};
  auto algo_state = a_star_state{tbd};
  a_star algo = a_star_algo(tt, algo_state, "S0", "S3", start_time);
  // setup state
  algo_state.setup(delta{0, 0});
  algo_state.update_segment(segment_idx_t{0}, delta{0, 0},
                            a_star_state::startSegmentPredecessor, 0U);
  algo_state.update_segment(segment_idx_t{1}, delta{0, 20}, segment_idx_t{0},
                            2U);
  algo_state.update_segment(segment_idx_t{2}, delta{0, 0}, segment_idx_t{1},
                            0U);
  algo_state.settled_segments_.set(segment_idx_t{0}, true);
  algo_state.settled_segments_.set(segment_idx_t{1}, true);
  algo_state.settled_segments_.set(segment_idx_t{2}, true);
  journey j;
  j.start_time_ = start_time;
  j.dest_time_ = start_time + duration_t{900};  // 15 hours later
  query q = query{
      .start_time_ = start_time,
      .start_ = {{tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}}),
                  0_minutes, 0U}},
      .destination_ = {
          {tt.locations_.location_id_to_idx_.at({"S3", source_idx_t{0}}),
           0_minutes, 0U}}};

  algo.reconstruct(q, j);
  EXPECT_EQ(only_one_segment_runs_journey, result_str(j, tt));
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S0 -> S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_MON,R0_MON,1

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,04:30:00,04:30:00,S0,0,0,0
R0_MON,05:30:00,05:30:00,S3,1,0,0
R0_MON,13:00:00,13:00:00,S1,2,0,0
R0_MON,14:00:00,14:00:00,S2,3,0,0
)");
}

constexpr auto const reconstruct_multiple_segment_runs_journey = R"(
[2021-03-02 00:00, 2021-03-02 14:00]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 04:30]
       TO: (S2, S2) [2021-03-02 14:00]
leg 0: (S0, S0) [2021-03-02 04:30] -> (S2, S2) [2021-03-02 14:00]
   0: S0      S0..............................................                               d: 02.03 04:30 [02.03 04:30]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   1: S3      S3.............................................. a: 02.03 05:30 [02.03 05:30]  d: 02.03 05:30 [02.03 05:30]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   2: S1      S1.............................................. a: 02.03 13:00 [02.03 13:00]  d: 02.03 13:00 [02.03 13:00]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   3: S2      S2.............................................. a: 02.03 14:00 [02.03 14:00]
leg 1: (S2, S2) [2021-03-02 14:00] -> (S2, S2) [2021-03-02 14:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, reconstruct_multiple_segment_runs) {
  auto const tt = load_gtfs(multiple_segment_run_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto start_time = unixtime_t{sys_days{March / 02 / 2021}};
  auto algo_state = a_star_state{tbd};
  a_star algo = a_star_algo(tt, algo_state, "S0", "S2", start_time);
  // setup state
  algo_state.setup(delta{0, 0});
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
  j.dest_time_ = start_time + duration_t{840};  // 14 hours later
  query q = query{
      .start_time_ = start_time,
      .start_ = {{tt.locations_.location_id_to_idx_.at({"S0", source_idx_t{0}}),
                  0_minutes, 0U}},
      .destination_ = {
          {tt.locations_.location_id_to_idx_.at({"S2", source_idx_t{0}}),
           0_minutes, 0U}}};
  algo.reconstruct(q, j);
  EXPECT_EQ(reconstruct_multiple_segment_runs_journey, result_str(j, tt));
}

// ==================
// EXECUTE TESTS
// ------------------

mem_dir one_run_journey_files() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DATA,R2,R2,"S0 -> S2",2
R3,DTA,R3,R3,"S3 -> S0 -> S2 -> S1",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,TUE,R1_TUE,R1_TUE,2
R2,TUE,R2_TUE,R2_TUE,3
R3,TUE,R3_TUE,R3_TUE,4
R0,TUE,R0B_TUE,R0B_TUE,5

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,05:00:00,05:00:00,S0,0,0,0
R0_TUE,06:00:00,06:00:00,S1,1,0,0
R1_TUE,12:00:00,12:00:00,S1,0,0,0
R1_TUE,13:00:00,13:00:00,S2,1,0,0
R2_TUE,01:00:00,01:00:00,S0,0,0,0
R2_TUE,11:00:00,11:00:00,S2,1,0,0
R3_TUE,04:30:00,04:30:00,S3,0,0,0
R3_TUE,05:31:00,05:31:00,S0,1,0,0
R3_TUE,07:00:00,07:00:00,S2,2,0,0
R3_TUE,13:00:00,13:00:00,S1,3,0,0
R0B_TUE,06:30:00,06:30:00,S0,0,0,0
R0B_TUE,06:00:00,06:00:00,S1,1,0,0
)");
}

constexpr auto const execute_journey = R"(
[2021-03-02 00:00, 2021-03-02 07:00]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 05:31]
       TO: (S2, S2) [2021-03-02 07:00]
leg 0: (S0, S0) [2021-03-02 05:31] -> (S2, S2) [2021-03-02 07:00]
   1: S0      S0..............................................                               d: 02.03 05:31 [02.03 05:31]  [{name=R3, day=2021-03-02, id=R3_TUE, src=0}]
   2: S2      S2.............................................. a: 02.03 07:00 [02.03 07:00]
leg 1: (S2, S2) [2021-03-02 07:00] -> (S2, S2) [2021-03-02 07:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_one_run_journey) {
  auto const tt = load_gtfs(one_run_journey_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(tt, tbd, "S0", "S2",
                                     unixtime_t{sys_days{March / 02 / 2021}});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_journey, results_str(results, tt));
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,TUE,R1_TUE,R1_TUE,2

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,00:00:00,00:00:00,S0,0,0,0
R0_TUE,06:00:00,06:00:00,S1,1,0,0
R1_TUE,12:00:00,12:00:00,S1,0,0,0
R1_TUE,13:00:00,13:00:00,S2,1,0,0
)");
}

constexpr auto const execute_same_day_transfer_journey = R"(
[2021-03-02 00:00, 2021-03-02 13:00]
TRANSFERS: 1
     FROM: (S0, S0) [2021-03-02 00:00]
       TO: (S2, S2) [2021-03-02 13:00]
leg 0: (S0, S0) [2021-03-02 00:00] -> (S1, S1) [2021-03-02 06:00]
   0: S0      S0..............................................                               d: 02.03 00:00 [02.03 00:00]  [{name=R0, day=2021-03-02, id=R0_TUE, src=0}]
   1: S1      S1.............................................. a: 02.03 06:00 [02.03 06:00]
leg 1: (S1, S1) [2021-03-02 06:00] -> (S1, S1) [2021-03-02 06:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-02 12:00] -> (S2, S2) [2021-03-02 13:00]
   0: S1      S1..............................................                               d: 02.03 12:00 [02.03 12:00]  [{name=R1, day=2021-03-02, id=R1_TUE, src=0}]
   1: S2      S2.............................................. a: 02.03 13:00 [02.03 13:00]
leg 3: (S2, S2) [2021-03-02 13:00] -> (S2, S2) [2021-03-02 13:00]
  FOOTPATH (duration=0)

)";

constexpr auto const execute_multiple_segment_run_journey = R"(
[2021-03-02 00:00, 2021-03-02 14:00]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 04:30]
       TO: (S2, S2) [2021-03-02 14:00]
leg 0: (S0, S0) [2021-03-02 04:30] -> (S2, S2) [2021-03-02 14:00]
   0: S0      S0..............................................                               d: 02.03 04:30 [02.03 04:30]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   1: S3      S3.............................................. a: 02.03 05:30 [02.03 05:30]  d: 02.03 05:30 [02.03 05:30]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   2: S1      S1.............................................. a: 02.03 13:00 [02.03 13:00]  d: 02.03 13:00 [02.03 13:00]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   3: S2      S2.............................................. a: 02.03 14:00 [02.03 14:00]
leg 1: (S2, S2) [2021-03-02 14:00] -> (S2, S2) [2021-03-02 14:00]
  FOOTPATH (duration=0)

)";
TEST(a_star, execute_multiple_segment_run) {
  auto const tt = load_gtfs(multiple_segment_run_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(tt, tbd, "S0", "S2",
                                     unixtime_t{sys_days{March / 02 / 2021}});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_multiple_segment_run_journey, results_str(results, tt));
}

TEST(a_star, execute_same_day_transfer) {
  auto const tt = load_gtfs(same_day_transfer_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(tt, tbd, "S0", "S2",
                                     unixtime_t{sys_days{March / 02 / 2021}});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_same_day_transfer_journey, results_str(results, tt));
}

mem_dir footpaths_before_and_after_files() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S1 -> S3",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_MON,R0_MON,1

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,06:00:00,06:00:00,S1,0,0,0
R0_MON,07:00:00,07:00:00,S3,1,0,0

# transfers.txt
from_stop_id,to_stop_id,transfer_type,min_transfer_time
S0,S1,2,900
S3,S2,2,600
)");
}

constexpr auto const execute_footpaths_before_and_after_journey = R"(
[2021-03-02 00:00, 2021-03-02 07:10]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 05:45]
       TO: (S2, S2) [2021-03-02 07:10]
leg 0: (S0, S0) [2021-03-02 05:45] -> (S1, S1) [2021-03-02 06:00]
  FOOTPATH (duration=15)
leg 1: (S1, S1) [2021-03-02 06:00] -> (S3, S3) [2021-03-02 07:00]
   0: S1      S1..............................................                               d: 02.03 06:00 [02.03 06:00]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   1: S3      S3.............................................. a: 02.03 07:00 [02.03 07:00]
leg 2: (S3, S3) [2021-03-02 07:00] -> (S2, S2) [2021-03-02 07:10]
  FOOTPATH (duration=10)

)";

TEST(a_star, execute_footpaths_before_and_after) {
  auto const tt = load_gtfs(footpaths_before_and_after_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(tt, tbd, "S0", "S2",
                                     unixtime_t{sys_days{March / 02 / 2021}});
  EXPECT_EQ(results.size(), 1U);
  for (auto j : results) {
    j.print(std::cout, tt);
  }
  EXPECT_EQ(execute_footpaths_before_and_after_journey,
            results_str(results, tt));
}

mem_dir next_day_transfer_files_as() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307
WED,0,0,1,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,WED,R1_WED,R1_WED,2

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,12:00:00,12:00:00,S0,0,0,0
R0_TUE,23:00:00,23:00:00,S1,1,0,0
R1_WED,06:00:00,06:00:00,S1,0,0,0
R1_WED,08:00:00,08:00:00,S2,1,0,0
)");
}

constexpr auto const execute_next_day_transfer_journey = R"(
[2021-03-02 11:00, 2021-03-03 08:00]
TRANSFERS: 1
     FROM: (S0, S0) [2021-03-02 12:00]
       TO: (S2, S2) [2021-03-03 08:00]
leg 0: (S0, S0) [2021-03-02 12:00] -> (S1, S1) [2021-03-02 23:00]
   0: S0      S0..............................................                               d: 02.03 12:00 [02.03 12:00]  [{name=R0, day=2021-03-02, id=R0_TUE, src=0}]
   1: S1      S1.............................................. a: 02.03 23:00 [02.03 23:00]
leg 1: (S1, S1) [2021-03-02 23:00] -> (S1, S1) [2021-03-02 23:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-03 06:00] -> (S2, S2) [2021-03-03 08:00]
   0: S1      S1..............................................                               d: 03.03 06:00 [03.03 06:00]  [{name=R1, day=2021-03-03, id=R1_WED, src=0}]
   1: S2      S2.............................................. a: 03.03 08:00 [03.03 08:00]
leg 3: (S2, S2) [2021-03-03 08:00] -> (S2, S2) [2021-03-03 08:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_next_day_transfer) {
  auto const tt = load_gtfs(next_day_transfer_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(
      tt, tbd, "S0", "S2", unixtime_t{sys_days{March / 02 / 2021} + 11_hours});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_next_day_transfer_journey, results_str(results, tt));
}
mem_dir transfer_to_journey_from_previous_day_files() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,MON,R0_MON,R0_MON,1
R1,TUE,R1_TUE,R1_TUE,2

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_MON,22:00:00,22:00:00,S3,0,0,0
R0_MON,30:00:00,30:00:00,S1,1,0,0
R0_MON,31:00:00,31:00:00,S2,2,0,0
R1_TUE,02:00:00,02:00:00,S0,0,0,0
R1_TUE,03:00:00,03:00:00,S1,1,0,0
)");
}

constexpr auto const execute_transfer_to_journey_from_previous_day_journey = R"(
[2021-03-02 01:00, 2021-03-02 07:00]
TRANSFERS: 1
     FROM: (S0, S0) [2021-03-02 02:00]
       TO: (S2, S2) [2021-03-02 07:00]
leg 0: (S0, S0) [2021-03-02 02:00] -> (S1, S1) [2021-03-02 03:00]
   0: S0      S0..............................................                               d: 02.03 02:00 [02.03 02:00]  [{name=R1, day=2021-03-02, id=R1_TUE, src=0}]
   1: S1      S1.............................................. a: 02.03 03:00 [02.03 03:00]
leg 1: (S1, S1) [2021-03-02 03:00] -> (S1, S1) [2021-03-02 03:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-02 06:00] -> (S2, S2) [2021-03-02 07:00]
   1: S1      S1..............................................                               d: 02.03 06:00 [02.03 06:00]  [{name=R0, day=2021-03-01, id=R0_MON, src=0}]
   2: S2      S2.............................................. a: 02.03 07:00 [02.03 07:00]
leg 3: (S2, S2) [2021-03-02 07:00] -> (S2, S2) [2021-03-02 07:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_transfer_to_journey_from_previous_day) {
  auto const tt = load_gtfs(transfer_to_journey_from_previous_day_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(
      tt, tbd, "S0", "S2", unixtime_t{sys_days{March / 02 / 2021} + 1_hours});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_transfer_to_journey_from_previous_day_journey,
            results_str(results, tt));
}

mem_dir transfer_on_next_day_files_as() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307
WED,0,0,1,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,WED,R1_WED,R1_WED,2

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,12:00:00,12:00:00,S0,0,0,0
R0_TUE,25:00:00,25:00:00,S1,1,0,0
R1_WED,06:00:00,06:00:00,S1,0,0,0
R1_WED,08:00:00,08:00:00,S2,1,0,0
)");
}

constexpr auto const execute_transfer_on_next_day_journey = R"(
[2021-03-02 11:00, 2021-03-03 08:00]
TRANSFERS: 1
     FROM: (S0, S0) [2021-03-02 12:00]
       TO: (S2, S2) [2021-03-03 08:00]
leg 0: (S0, S0) [2021-03-02 12:00] -> (S1, S1) [2021-03-03 01:00]
   0: S0      S0..............................................                               d: 02.03 12:00 [02.03 12:00]  [{name=R0, day=2021-03-02, id=R0_TUE, src=0}]
   1: S1      S1.............................................. a: 03.03 01:00 [03.03 01:00]
leg 1: (S1, S1) [2021-03-03 01:00] -> (S1, S1) [2021-03-03 01:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-03 06:00] -> (S2, S2) [2021-03-03 08:00]
   0: S1      S1..............................................                               d: 03.03 06:00 [03.03 06:00]  [{name=R1, day=2021-03-03, id=R1_WED, src=0}]
   1: S2      S2.............................................. a: 03.03 08:00 [03.03 08:00]
leg 3: (S2, S2) [2021-03-03 08:00] -> (S2, S2) [2021-03-03 08:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_transfer_on_next_day) {
  auto const tt = load_gtfs(transfer_on_next_day_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(
      tt, tbd, "S0", "S2", unixtime_t{sys_days{March / 02 / 2021} + 11_hours});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_transfer_on_next_day_journey, results_str(results, tt));
}

mem_dir transfer_on_next_day_follow_up_files_as() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307
WED,0,0,1,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R0,R0,"S0 -> S1",2
R1,DTA,R1,R1,"S1 -> S2",2
R2,DTA,R2,R2,"S2 -> S3",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1
R1,WED,R1_WED,R1_WED,2
R2,WED,R2_WED,R2_WED,3

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,12:00:00,12:00:00,S0,0,0,0
R0_TUE,25:00:00,25:00:00,S1,1,0,0
R1_WED,06:00:00,06:00:00,S1,0,0,0
R1_WED,08:00:00,08:00:00,S2,1,0,0
R2_WED,08:30:00,08:30:00,S2,0,0,0
R2_WED,09:00:00,09:00:00,S3,1,0,0
)");
}

constexpr auto const execute_transfer_on_next_day_follow_up_journey = R"(
[2021-03-02 11:00, 2021-03-03 09:00]
TRANSFERS: 2
     FROM: (S0, S0) [2021-03-02 12:00]
       TO: (S3, S3) [2021-03-03 09:00]
leg 0: (S0, S0) [2021-03-02 12:00] -> (S1, S1) [2021-03-03 01:00]
   0: S0      S0..............................................                               d: 02.03 12:00 [02.03 12:00]  [{name=R0, day=2021-03-02, id=R0_TUE, src=0}]
   1: S1      S1.............................................. a: 03.03 01:00 [03.03 01:00]
leg 1: (S1, S1) [2021-03-03 01:00] -> (S1, S1) [2021-03-03 01:02]
  FOOTPATH (duration=2)
leg 2: (S1, S1) [2021-03-03 06:00] -> (S2, S2) [2021-03-03 08:00]
   0: S1      S1..............................................                               d: 03.03 06:00 [03.03 06:00]  [{name=R1, day=2021-03-03, id=R1_WED, src=0}]
   1: S2      S2.............................................. a: 03.03 08:00 [03.03 08:00]
leg 3: (S2, S2) [2021-03-03 08:00] -> (S2, S2) [2021-03-03 08:02]
  FOOTPATH (duration=2)
leg 4: (S2, S2) [2021-03-03 08:30] -> (S3, S3) [2021-03-03 09:00]
   0: S2      S2..............................................                               d: 03.03 08:30 [03.03 08:30]  [{name=R2, day=2021-03-03, id=R2_WED, src=0}]
   1: S3      S3.............................................. a: 03.03 09:00 [03.03 09:00]
leg 5: (S3, S3) [2021-03-03 09:00] -> (S3, S3) [2021-03-03 09:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_transfer_on_next_day_follow_up) {
  auto const tt = load_gtfs(transfer_on_next_day_follow_up_files_as);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(
      tt, tbd, "S0", "S3", unixtime_t{sys_days{March / 02 / 2021} + 11_hours});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_transfer_on_next_day_follow_up_journey,
            results_str(results, tt));
}

mem_dir two_dest_segments_reached_files() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S0 -> S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_MON,R0_MON,1

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

constexpr auto const execute_two_dest_segments_reached_journey = R"(
[2021-03-02 00:00, 2021-03-02 04:45]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 04:30]
       TO: (S2, S2) [2021-03-02 04:45]
leg 0: (S0, S0) [2021-03-02 04:30] -> (S2, S2) [2021-03-02 04:45]
   0: S0      S0..............................................                               d: 02.03 04:30 [02.03 04:30]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   1: S3      S3.............................................. a: 02.03 04:35 [02.03 04:35]  d: 02.03 04:35 [02.03 04:35]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   2: S1      S1.............................................. a: 02.03 04:40 [02.03 04:40]  d: 02.03 04:40 [02.03 04:40]  [{name=R3, day=2021-03-02, id=R0_MON, src=0}]
   3: S2      S2.............................................. a: 02.03 04:45 [02.03 04:45]
leg 1: (S2, S2) [2021-03-02 04:45] -> (S2, S2) [2021-03-02 04:45]
  FOOTPATH (duration=0)

)";

TEST(a_star, execute_two_dest_segments_reached) {
  auto const tt = load_gtfs(two_dest_segments_reached_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(tt, tbd, "S0", "S2",
                                     unixtime_t{sys_days{March / 02 / 2021}});
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(execute_two_dest_segments_reached_journey,
            results_str(results, tt));
}

// TODO: dest == start test

mem_dir midnight_cross_files() {
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
TUE,0,1,0,0,0,0,0,20210301,20210307

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
R0,DTA,R3,R3,"S0 -> S3 -> S1 -> S2",2

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
R0,TUE,R0_TUE,R0_TUE,1

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
R0_TUE,23:00:00,23:00:00,S0,0,0,0
R0_TUE,23:30:00,23:30:00,S3,1,0,0
R0_TUE,24:00:00,24:00:00,S1,2,0,0
R0_TUE,25:00:00,25:00:00,S2,3,0,0
)");
}

constexpr auto const midnight_cross_journey = R"(
[2021-03-02 11:00, 2021-03-03 01:00]
TRANSFERS: 0
     FROM: (S0, S0) [2021-03-02 23:00]
       TO: (S2, S2) [2021-03-03 01:00]
leg 0: (S0, S0) [2021-03-02 23:00] -> (S2, S2) [2021-03-03 01:00]
   0: S0      S0..............................................                               d: 02.03 23:00 [02.03 23:00]  [{name=R3, day=2021-03-02, id=R0_TUE, src=0}]
   1: S3      S3.............................................. a: 02.03 23:30 [02.03 23:30]  d: 02.03 23:30 [02.03 23:30]  [{name=R3, day=2021-03-02, id=R0_TUE, src=0}]
   2: S1      S1.............................................. a: 03.03 00:00 [03.03 00:00]  d: 03.03 00:00 [03.03 00:00]  [{name=R3, day=2021-03-02, id=R0_TUE, src=0}]
   3: S2      S2.............................................. a: 03.03 01:00 [03.03 01:00]
leg 1: (S2, S2) [2021-03-03 01:00] -> (S2, S2) [2021-03-03 01:00]
  FOOTPATH (duration=0)

)";

TEST(a_star, midnight_cross) {
  auto const tt = load_gtfs(midnight_cross_files);
  auto const tbd = tb::preprocess(tt, profile_idx_t{0});
  auto const results = a_star_search(
      tt, tbd, "S0", "S2", unixtime_t{sys_days{March / 02 / 2021}} + 11_hours);
  EXPECT_EQ(results.size(), 1U);
  EXPECT_EQ(midnight_cross_journey, results_str(results, tt));
}
