/** * Copyright @ 2024 Michel Hoche-Mong * SPDX-License-Identifier: CC-BY-4.0
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

Flight::Flight(const std::shared_ptr<Metadata> &metadata) : m_metadata(metadata)
{
    m_bit2MetricMap = Metrics::getBitToMetricMap(m_metadata->ProtoVersion());

    for (const auto &[bitidx, metric] : m_bit2MetricMap) {
        m_metricValues[metric.getMetricId()] = metric.getInitialValue();
    }
}

// The input to this is just the raw data from this time in the file.
// Here, we update the m_metricValues with that by adding it to the
// previous and scaling it.
// We also calculate any derived values.
void Flight::updateMetrics(const std::map<int, int> &valuesMap)
{
#ifdef DEBUG_FLIGHT_RECORD
    for (const auto &pair : valuesMap) {
        printf("[%d] %8x\t(%d)\t(%u)\n", pair.first, pair.second, (int)(pair.second), (unsigned)(pair.second));
    };
#endif

    for (const auto &[bitIdx, bitValue] : valuesMap) {
        auto it = m_bit2MetricMap.find(bitIdx);
        if (it == m_bit2MetricMap.end()) {
            // if not found, the bitIdx is probably pointing at a high byte and
            // we'll handle that when we do the low byte;
            continue;
        }

        int value = bitValue;
        auto highByteBitIdx = it->second.getHighByteBitIdx();
        if (highByteBitIdx.has_value()) {
            if (valuesMap.find(highByteBitIdx.value()) != valuesMap.end()) {
                // yup, there's a high byte
                int highByte = valuesMap.at(highByteBitIdx.value());
                bool isNegative = (value < 0);
                if (isNegative) {
                    value = 0 - value; // make it positive to make the bitshift easier
                }
                value = ((0xFF & highByte) << 8) + (0xFF & value); // shift the new high byte on
                if (isNegative) {
                    value = 0 - value; // make it negative again if necessary
                }
            }
        }

        float scaledValue = value;
        switch (it->second.getScaleFactor()) {
        case (Metric::ScaleFactor::TEN):
            scaledValue /= 10;
        case (Metric::ScaleFactor::TEN_IF_GPH):
            if (m_metadata->IsGPH()) {
                value /= 10;
            }
            // no default case
        }

        m_metricValues[it->second.getMetricId()] += value;
    }

    // Now do derived values

    // calculate DIF
    // TODO: this should see how many cylinders we have. 4? 6? 9?
    //       check the FileHeader.ConfigInfo featureFlags
    // TODO: we also need to handle DIF2 for multi
    // TODO: Convert to F if featureFlags says to
    // TODO:
    if (m_metadata->NumCylinders() == 4) {
    }
    /*
    auto bounds = std::minmax(
        {m_dataMap[EGT1], m_dataMap[EGT2], m_dataMap[EGT3], m_dataMap[EGT4], m_dataMap[EGT5], m_dataMap[EGT6]});
    m_dataMap[DIF] = bounds.second - bounds.first;
    */
}

std::shared_ptr<FlightMetricsRecord> Flight::getFlightMetricsRecord()
{
    // This is just a copy of the m_metricValues map, with some additional info like fastFlag and seqno

    // placeholder for now
    auto fmr = std::make_shared<FlightMetricsRecord>(m_fastFlag, m_recordSeq);
    fmr->m_metrics[EGT11] = 0;
    return fmr;
}

} // namespace jpi_edm
