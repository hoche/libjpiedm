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

// Figure out which version of the metrics to use (V1, V2, etc),
// and set the initial values.
Flight::Flight(const std::shared_ptr<Metadata> &metadata) : m_metadata(metadata)
{
    m_bit2MetricMap = Metrics::getBitToMetricMap(m_metadata->ProtoVersion());

#ifdef DEBUG_FLIGHT_RECORD
    std::cout << "Using map for proto " << m_metadata->ProtoVersion() << "\n";
#endif
    bool isGPH = m_metadata->IsGPH();
    for (const auto &[bitidx, metric] : m_bit2MetricMap) {
#ifdef DEBUG_FLIGHT_RECORD
        std::cout << "[" << bitidx << "] ==> " << metric.getShortName() << "\n";
#endif
        m_metricValues[metric.getMetricId()] = metric.getInitialValue();
        if ((metric.getScaleFactor() == Metric::ScaleFactor::TEN) ||
            (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH && isGPH)) {
            m_metricValues[metric.getMetricId()] /= 10;
        }
    }

    // derived data that's not in the bit map
    m_metricValues[DIF1] = 0;
    m_metricValues[DIF2] = 0;

#ifdef DEBUG_FLIGHT_RECORD
    std::cout << "Initial metric values:\n";
    for (const auto &[metricId, initialValue] : m_metricValues) {
        std::cout << "[" << metricId << "] == " << initialValue << "\n";
    }
#endif
}

// The input to this is just the raw data from this time in the file.
// Here, we update the m_metricValues with that by adding it to the
// previous and scaling it.
// We also calculate any derived values.
void Flight::updateMetrics(const std::map<int, int> &valuesMap)
{
    bool isGPH = m_metadata->IsGPH();
    for (const auto &[bitIdx, bitValue] : valuesMap) {
        auto it = m_bit2MetricMap.find(bitIdx);
#ifdef DEBUG_FLIGHT_RECORD
        std::cout << "[" << std::setw(3) << std::right << std::setfill('0') << bitIdx << "] ";
#endif
        if (it == m_bit2MetricMap.end()) {
#ifdef DEBUG_FLIGHT_RECORD
            std::cout << "highval, skipped\n";
#endif
            // if not found, the bitIdx is probably pointing at a high byte and
            // we'll handle that when we do the low byte;
            continue;
        }

#ifdef DEBUG_FLIGHT_RECORD
        std::cout << "lowval, " << std::left << std::setw(10) << std::setfill(' ') << bitValue;
#endif
        int value = bitValue;
        auto highByteBitIdx = it->second.getHighByteBitIdx();
        if (highByteBitIdx.has_value()) {
#ifdef DEBUG_FLIGHT_RECORD
            std::cout << "ORing with high at [" << highByteBitIdx.value() << "]";
#endif
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
#ifdef DEBUG_FLIGHT_RECORD
        std::cout << "newval: " << value;
#endif

        float scaledValue = value;
        auto metric = it->second;
        if ((metric.getScaleFactor() == Metric::ScaleFactor::TEN) ||
            (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH && isGPH)) {
            scaledValue /= 10;
        }

#ifdef DEBUG_FLIGHT_RECORD
        std::cout << ", [" << it->second.getShortName() << "] -> " << m_metricValues[it->second.getMetricId()] << " + "
                  << scaledValue << " == ";
#endif
        m_metricValues[it->second.getMetricId()] += scaledValue;
#ifdef DEBUG_FLIGHT_RECORD
        std::cout << m_metricValues[it->second.getMetricId()] << "\n";
#endif
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
    return std::make_shared<FlightMetricsRecord>(m_fastFlag, m_recordSeq, m_metricValues);
}

} // namespace jpi_edm
