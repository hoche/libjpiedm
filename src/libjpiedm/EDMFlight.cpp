/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Classes for representing individual flights from a JPI EDM flight
 * file.
 *
 */

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EDMFlight.hpp"

namespace jpi_edm {

void EDMFlightRecord::apply(std::vector<int> &values)
{
    /*
    for (int i = 0; i < values.size(); ++i) {
        std::cout << "[" << i << "] " << std::hex << values[i] << std::dec << "\n";
    };
    */

    for (auto &[key, indices] : offsets) {
        if (indices.size() > 1) {
            m_dataMap[key] = values[indices[0]] + (values[indices[1]] << 8);
        } else {
            m_dataMap[key] = values[indices[0]];
        }
    }

    // calculate DIF
    auto bounds = std::minmax({m_dataMap[EGT1], m_dataMap[EGT2], m_dataMap[EGT3], m_dataMap[EGT4],
                               m_dataMap[EGT5], m_dataMap[EGT6]});
    m_dataMap[DIF] = bounds.second - bounds.first;

    // TODO where are these
    m_dataMap[FF2] = 0;
    m_dataMap[FUSD2] = -1;
    m_dataMap[SPD] = -1;
    m_dataMap[ALT] = -1;
    m_dataMap[LAT] = -1;
    m_dataMap[LNG] = -1;
}

} // namespace jpi_edm
