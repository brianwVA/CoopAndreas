#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t COOPANDREAS_FLOAT_STATS_COUNT = 83;
constexpr std::size_t COOPANDREAS_INT_STATS_COUNT = 224;
constexpr std::size_t COOPANDREAS_PROPERTY_COUNT = 32;
constexpr std::size_t COOPANDREAS_SCHOOL_PROGRESS_COUNT = 8;
constexpr std::size_t COOPANDREAS_SCHOOL_MEDAL_COUNT = 32;

struct PlayerProgressState
{
    int32_t money = 0;
    uint8_t wantedLevel = 0;
    float floatStats[COOPANDREAS_FLOAT_STATS_COUNT]{};
    int32_t intStats[COOPANDREAS_INT_STATS_COUNT]{};
    uint8_t ownedProperties[COOPANDREAS_PROPERTY_COUNT]{};
    uint8_t schoolProgress[COOPANDREAS_SCHOOL_PROGRESS_COUNT]{};
    uint8_t schoolMedals[COOPANDREAS_SCHOOL_MEDAL_COUNT]{};
};
