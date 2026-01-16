#pragma once

#include "nigiri/types.h"

namespace nigiri::routing {

static constexpr auto const maxASTravelTime = 1_days;
constexpr auto const kASMaxTravelTimeDays = 2U;
constexpr auto const kASMaxDayOffset =
    std::int8_t{kTimetableOffset / 1_days + kASMaxTravelTimeDays};
}  // namespace nigiri::routing