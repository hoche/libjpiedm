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

#include "Flight.hpp"

namespace jpi_edm {

// #define DEBUG_FLIGHT_RECORD

void FlightRecord::apply(std::vector<int> &values)
{
#ifdef DEBUG_FLIGHT_RECORD
    for (int i = 0; i < values.size(); ++i) {
        printf("[%d] %8x\t(%d)\t(%u)\n", i, values[i], (int)values[i], (unsigned)values[i]);
    };
#endif

/*
    for (auto &[key, indices] : offsets) {
        if (indices.size() > 1) {
            m_dataMap[key] = values[indices[0]] + (values[indices[1]] << 8);
        } else {
            m_dataMap[key] = values[indices[0]];
        }
    }
*/

    // calculate DIF
    // TODO: this should see how many cylinders we have. 4? 6? 9?
    // TODO: we also need to handle DIF2 for multi
    /*
    auto bounds = std::minmax(
        {m_dataMap[EGT1], m_dataMap[EGT2], m_dataMap[EGT3], m_dataMap[EGT4], m_dataMap[EGT5], m_dataMap[EGT6]});
    m_dataMap[DIF] = bounds.second - bounds.first;
    */

}

} // namespace jpi_edm
