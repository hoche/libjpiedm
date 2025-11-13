/** * Copyright @ 2024 Michel Hoche-Mong * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Classes for representing individual flights from a JPI EDM flight
 * file.
 *
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Flight.hpp"
#include "ProtocolConstants.hpp"

namespace {

bool isSecondEngineMetric(jpi_edm::MetricId metricId)
{
    using namespace jpi_edm;
    switch (metricId) {
    case EGT21:
    case EGT22:
    case EGT23:
    case EGT24:
    case EGT25:
    case EGT26:
    case EGT27:
    case EGT28:
    case EGT29:
    case CHT21:
    case CHT22:
    case CHT23:
    case CHT24:
    case CHT25:
    case CHT26:
    case CHT27:
    case CHT28:
    case CHT29:
    case CLD2:
    case TIT21:
    case TIT22:
    case OILT2:
    case OILP2:
    case CRB2:
    case IAT2:
    case MAP2:
    case FF21:
    case FF22:
    case FUSD21:
    case FUSD22:
    case FP2:
    case HP2:
    case RPM2:
    case HRS2:
    case TORQ2:
        return true;
    default:
        return false;
    }
}

} // namespace

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

    for (const auto &[bitIdx, metric] : m_bit2MetricMap) {
        m_supportedMetrics.insert(metric.getMetricId());
    }

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
            m_metricValues[metric.getMetricId()] /= METRIC_SCALE_DIVISOR;
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
    m_lastUpdatedMetrics.clear();
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
            // Use C++17 if-with-initializer to avoid double lookup
            if (auto it = valuesMap.find(highByteBitIdx.value()); it != valuesMap.end()) {
                // yup, there's a high byte
                int highByte = it->second;
                bool isNegative = (value < 0);
                if (isNegative) {
                    value = 0 - value; // make it positive to make the bitshift easier
                }
                value = ((BYTE_MASK & highByte) << 8) + (BYTE_MASK & value); // shift the new high byte on
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
            scaledValue /= METRIC_SCALE_DIVISOR;
        }

#ifdef DEBUG_FLIGHT_RECORD
        std::cout << ", [" << it->second.getShortName() << "] -> " << m_metricValues[it->second.getMetricId()] << " + "
                  << scaledValue << " == ";
#endif
        auto metricId = it->second.getMetricId();
        if (metricId == LAT || metricId == LNG) {
            float rawAccum = m_rawGpsValues[metricId];
            int delta = static_cast<int>(std::lround(scaledValue));
            int baselineOffset = m_gpsBaselineOffsets[metricId];
            bool baselineSet = baselineOffset != 0;
            bool atBaseline = std::fabs(rawAccum) < 0.5f;
            bool isHandshake = !baselineSet && atBaseline && (delta == 100 || delta == -100);

            if (isHandshake) {
                baselineOffset += delta;
                m_gpsBaselineOffsets[metricId] = baselineOffset;
            } else {
                rawAccum += scaledValue;
                m_rawGpsValues[metricId] = rawAccum;
            }

            float combined = rawAccum;
            if (m_flightHeader) {
                int32_t startCoordinate = (metricId == LAT) ? m_flightHeader->startLat : m_flightHeader->startLng;
                if (startCoordinate != 0) {
                    combined = static_cast<float>(startCoordinate + m_gpsBaselineOffsets[metricId]) + rawAccum;
                }
            }
            m_metricValues[metricId] = combined;
        } else {
            m_metricValues[metricId] += scaledValue;
        }
        m_lastUpdatedMetrics.insert(metricId);
#ifdef DEBUG_FLIGHT_RECORD
        std::cout << m_metricValues[it->second.getMetricId()] << "\n";
#endif

        if (m_metadata && !m_metadata->m_configInfo.isTwin && isSecondEngineMetric(metricId)) {
            m_metadata->m_configInfo.isTwin = true;
        }
    }

    // Now do derived values

    // Calculate DIF1 (differential between hottest and coolest EGT for engine 1)
    // DIF represents the spread in exhaust gas temperatures across cylinders
    int numCylinders = m_metadata->NumCylinders();
    if (numCylinders > 0) {
        std::vector<float> egtValues;
        egtValues.reserve(numCylinders);

        // Collect all EGT values for active cylinders on engine 1
        const MetricId egtMetrics[] = {MetricId::EGT11, MetricId::EGT12, MetricId::EGT13,
                                       MetricId::EGT14, MetricId::EGT15, MetricId::EGT16,
                                       MetricId::EGT17, MetricId::EGT18, MetricId::EGT19};

        for (int i = 0; i < numCylinders && i < 9; ++i) {
            auto it = m_metricValues.find(egtMetrics[i]);
            if (it != m_metricValues.end() && it->second > 0) {
                egtValues.push_back(it->second);
            }
        }

        // Calculate DIF1 only if we have at least 2 EGT readings
        if (egtValues.size() >= 2) {
            auto bounds = std::minmax_element(egtValues.begin(), egtValues.end());
            m_metricValues[MetricId::DIF1] = *bounds.second - *bounds.first;
        }
    }

    // Calculate DIF2 for twin-engine aircraft (engine 2)
    if (m_metadata->IsTwin() && numCylinders > 0) {
        std::vector<float> egtValues;
        egtValues.reserve(numCylinders);

        // Collect all EGT values for active cylinders on engine 2
        const MetricId egtMetrics[] = {MetricId::EGT21, MetricId::EGT22, MetricId::EGT23,
                                       MetricId::EGT24, MetricId::EGT25, MetricId::EGT26,
                                       MetricId::EGT27, MetricId::EGT28, MetricId::EGT29};

        for (int i = 0; i < numCylinders && i < 9; ++i) {
            auto it = m_metricValues.find(egtMetrics[i]);
            if (it != m_metricValues.end() && it->second > 0) {
                egtValues.push_back(it->second);
            }
        }

        // Calculate DIF2 only if we have at least 2 EGT readings
        if (egtValues.size() >= 2) {
            auto bounds = std::minmax_element(egtValues.begin(), egtValues.end());
            m_metricValues[MetricId::DIF2] = *bounds.second - *bounds.first;
        }
    }
}

std::shared_ptr<FlightMetricsRecord> Flight::getFlightMetricsRecord()
{
    // This is just a copy of the m_metricValues map, with some additional info like fastFlag and seqno
    return std::make_shared<FlightMetricsRecord>(m_fastFlag, m_recordSeq, m_metricValues, m_lastUpdatedMetrics,
                                                 m_supportedMetrics);
}

} // namespace jpi_edm
