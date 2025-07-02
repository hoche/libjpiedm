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

// #define DEBUG_FLIGHT_RECORD

void EDMFlightRecord::apply(std::vector<int> &values)
{
#ifdef DEBUG_FLIGHT_RECORD
    for (int i = 0; i < values.size(); ++i) {
        if (i == 83)
            continue; // already known (ALT)
        printf("[%d] %8x\t(%d)\t(%u)\n", i, values[i], (int)values[i], (unsigned)values[i]);
        // std::cout << "[" << i << "] " << std::hex << values[i] << std::dec << "\n";
    };
#endif

    for (auto &[key, indices] : offsets) {
        if (indices.size() > 1) {
            m_dataMap[key] = values[indices[0]] + (values[indices[1]] << 8);
        } else {
            m_dataMap[key] = values[indices[0]];
        }
    }

    // calculate DIF
    auto bounds = std::minmax(
        {m_dataMap[EGT1], m_dataMap[EGT2], m_dataMap[EGT3], m_dataMap[EGT4], m_dataMap[EGT5], m_dataMap[EGT6]});
    m_dataMap[DIF] = bounds.second - bounds.first;

    if (m_dataMap.find(FF2) == m_dataMap.end()) {
        m_dataMap[FF2] = 0;
    }
    if (m_dataMap.find(FUSD2) == m_dataMap.end()) {
        m_dataMap[FUSD2] = -1;
    }
    if (auto it = m_dataMap.find(SPD); it != m_dataMap.end()) {
        if (it->second == 0xF0)
            m_dataMap[SPD] = -1;
    } else {
        m_dataMap[SPD] = -1;
    }
    if (auto it = m_dataMap.find(ALT); it != m_dataMap.end()) {
        if (it->second == 0xF0)
            m_dataMap[ALT] = -1;
    } else {
        m_dataMap[ALT] = -1;
    }
    if (m_dataMap.find(LAT) == m_dataMap.end()) {
        m_dataMap[LAT] = -1;
    }
    if (m_dataMap.find(LNG) == m_dataMap.end()) {
        m_dataMap[LNG] = -1;
    }
}

} // namespace jpi_edm
