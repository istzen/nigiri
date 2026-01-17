#pragma once

#include "nigiri/common/dial.h"
#include "nigiri/routing/a_star/settings.h"
#include "nigiri/routing/tb/tb_data.h"
#include "nigiri/types.h"

#define as_debug fmt::println
// #define as_debug(...)

namespace nigiri::routing {

using tb_data = tb::tb_data;
using segment_idx_t = tb::segment_idx_t;

// * The next three lines are the data structures needed as per info paper
// TODO: this should be a datetime already so the type needs to change or the
// second line is not needed
using arrival_time_map =
    hash_map<segment_idx_t,
             minutes_after_midnight_t>;  // segment -> arrival_time mapping
using day_index_map =
    hash_map<segment_idx_t, day_idx_t>;  // segment -> arrival_day
using pred_table = hash_map<segment_idx_t, segment_idx_t>;  // pred_table

struct queue_entry {
  queue_entry(segment_idx_t const s, std::uint8_t t)
      : segment_{s}, transfers_{t} {}

  segment_idx_t const segment_;
  std::uint8_t transfers_;
};

struct a_star_state {
  // TODO: maybe change to different value like invalid - 1
  static constexpr auto const startSegmentPredecessor =
      segment_idx_t::invalid();

  a_star_state(tb_data const& tbd)
      // TODO: thing about bucket size, since maxASTravelTime might be too small
      // as there could be a jounrey longer than that due to transfers
      : tbd_{tbd}, pq_{maxASTravelTime.count(), get_bucket_a_star(*this)} {
    end_reachable_.resize(tbd.segment_transfers_.size());
    settled_segments_.resize(tbd.segment_transfers_.size());
    start_segments_.resize(tbd.segment_transfers_.size());
  }

  bool better_arrival(queue_entry qe, delta const new_arr) {
    return !arrival_day_.contains(qe.segment_) ||
           cost_function(qe, new_arr) > cost_function(qe);
  }

  // Standard cost function used in pq
  uint16_t cost_function(queue_entry const& qe) const {
    // * Debug asserts
    assert(arrival_day_.contains(qe.segment_));
    assert(arrival_time_.contains(qe.segment_));
    auto const val = cost_function(
        to_idx(arrival_day_.find(qe.segment_)->second),
        arrival_time_.find(qe.segment_)->second.count(), qe.transfers_);
    as_debug("Cost function for segment {}: {}", qe.segment_, val);
    return val;
  }

  // Cost function used when a new arrival time is computed
  uint16_t cost_function(queue_entry const& qe, delta const arr) const {
    return cost_function(arr.days(), arr.mam(), qe.transfers_);
  }

  void update_segment(segment_idx_t const s,
                      delta const new_arr,
                      segment_idx_t pred,
                      uint8_t transfers) {
    // check if the arrival time is better
    if (better_arrival(queue_entry{s, transfers}, new_arr)) {
      // Update arrival time
      arrival_day_.insert_or_assign(s, day_idx_t{new_arr.days()});
      arrival_time_.insert_or_assign(s,
                                     minutes_after_midnight_t{new_arr.mam()});
      // Update Predecessor Table
      pred_table_.insert_or_assign(s, pred);
      pq_.push(queue_entry{s, transfers});
    }
  };

  void setup(delta const start_delta) {
    assert(start_time_ == std::numeric_limits<uint16_t>::max() &&
           start_day_ == std::numeric_limits<uint16_t>::max() &&
           "state has not been proberly reset before setup");
    start_time_ = start_delta.mam();
    start_day_ = start_delta.days();
    start_segments_.for_each_set_bit([&](segment_idx_t const s) {
      pq_.push(queue_entry{s, 0});
      pred_table_.emplace(s, startSegmentPredecessor);
    });
  }

  void reset() {
    pred_table_.clear();
    pq_.clear();
    settled_segments_.zero_out();
    // TODO: think about whether these two clears are necessary
    // arrival_time_.clear();
    // arrival_day_.clear();
    start_time_ = std::numeric_limits<uint16_t>::max();
    start_day_ = std::numeric_limits<uint16_t>::max();
  }

  struct get_bucket_a_star {
    using dist_t = std::uint16_t;

    get_bucket_a_star(a_star_state const& state) : state_{state} {}

    // TODO: value could be saved for performance
    dist_t operator()(queue_entry const& q) const {
      return state_.cost_function(q);
    }

  private:
    a_star_state const& state_;
  };

  tb_data const& tbd_;  // preprocessed tb data

  arrival_time_map arrival_time_;  // segment -> arrival_time
  day_index_map arrival_day_;  // segment -> arrival_day
  pred_table pred_table_;  // predecessor table
  hash_map<segment_idx_t, duration_t>
      dist_to_dest_;  // segment -> distance to destination
  dial<queue_entry, get_bucket_a_star> pq_;  // priority_queue
  bitvec_map<segment_idx_t>
      end_reachable_;  // segments from which the destination is reachable
  bitvec_map<segment_idx_t>
      settled_segments_;  // segments whose shortest path is finalized
  bitvec_map<segment_idx_t>
      start_segments_;  // segments that are start segments
  float transfer_factor_;  // default value
  uint16_t start_day_ =
      std::numeric_limits<uint16_t>::max();  // day_idx_t of start_time
  uint16_t start_time_ =
      std::numeric_limits<uint16_t>::max();  // minutes_after_midnight_t
                                             // of start_time

private:
  // Refactored part of cost function
  uint16_t cost_function(uint16_t const days,
                         uint16_t const mam,
                         uint8_t const transfers) const {
    auto const val = (days - start_day_) * 24 * 60 + mam - start_time_ +
                     transfer_factor_ * transfers;
    assert(val >= 0 && "Cost function should always be positive");
    return val;
  }
};
}  // namespace nigiri::routing