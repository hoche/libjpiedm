/*
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 */

#pragma once

#include <map>

#include "libjpiedm/MetricId.hpp"

namespace parseedmlog {

inline float getMetric(const std::map<jpi_edm::MetricId, float> &metrics, jpi_edm::MetricId id,
                       float defaultValue = 0.0f)
{
    auto it = metrics.find(id);
    return (it != metrics.end()) ? it->second : defaultValue;
}

} // namespace parseedmlog

