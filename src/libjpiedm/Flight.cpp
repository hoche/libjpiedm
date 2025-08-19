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

Flight::Flight(const std::shared_ptr<Metadata> &metadata)
{
    m_bit2MetricMap = Metrics::getBitToMetricMap(metadata->protoVersion());

    for (const auto &pair : m_bit2MetricMap) {
        m_metricValues[pair.first] = pair.second.getInitialValue();
    }
}

// The input to this is just the raw data from this time in the file.
// We need to update the metrics with it by adding it to the previous
// value.
void Flight::updateMetrics(const std::map<int, int> &valuesMap)
{
#ifdef DEBUG_FLIGHT_RECORD
    for (const auto& pair : valuesMap) {
        printf("[%d] %8x\t(%d)\t(%u)\n", pair.first,
            pair.second, (int)(pair.second), (unsigned)(pair.second));
    };
#endif

    for (const auto& pair : valuesMap) {
        int bitIdx = pair.first;
        auto it = m_bit2MetricMap.find(bitIdx);

        // if not found, the bitIdx is probably pointing at a high byte and we'll handle that
        // when we do the low byte;
        int value = pair.second;
        if (it != m_bit2MetricMap.end()) {
            auto highByteBitIdx = it->second.getHighByteBitIdx();
            if (highByteBitIdx.has_value()) {
                if (valuesMap.find(highByteBitIdx.value()) != valuesMap.end()) {
                    // yup, there's a high byte
                    int highByte = valuesMap.at(highByteBitIdx.value());
                    bool isNegative = (value < 0);
                    if (isNegative) {
                        value = 0 - value;
                    }
                    value = ((0xFF & highByte) << 8) + (0xFF & value);
                    if (isNegative) {
                        value = 0 - value;
                    }
                }
            }
            m_metricValues[bitIdx] += value;
        }
    }

    // calculate DIF
    // TODO: this should see how many cylinders we have. 4? 6? 9?
    // TODO: we also need to handle DIF2 for multi
    /*
    auto bounds = std::minmax(
        {m_dataMap[EGT1], m_dataMap[EGT2], m_dataMap[EGT3], m_dataMap[EGT4], m_dataMap[EGT5], m_dataMap[EGT6]});
    m_dataMap[DIF] = bounds.second - bounds.first;
    */
}

std::shared_ptr<FlightMetricsRecord> Flight::getFlightMetricsRecord()
{
    auto fmr = std::make_shared<FlightMetricsRecord>(m_fastFlag, m_recordSeq);
    // convert the m_metricValues map (keyed by bitidx - low byte) to a map keyed by MetricId
    // so:
    //   - for each key in the m_metricsValue map
    //      - look up the Metric object in the m_bit2MetricMap
    //      - pull out the MetricId and scale
    //      - add an entry to a new map  such that map[MetricId] = m_metricsValue[key] * scale
    for (const auto &pair : m_metricValues) {
        Metric metric = m_bit2MetricsMap[pair.first];
        fmr->m_metric[metric] = pair.second.getInitialValue();
    }
    fmr->m_metrics[EGT11] = 0;
    return fmr;
}

} // namespace jpi_edm
