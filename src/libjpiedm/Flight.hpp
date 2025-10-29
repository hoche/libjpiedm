/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Classes for representing individual flights from a JPI EDM flight
 * file.
 *
 */

#pragma once

#include <bitset>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>

#include "Metadata.hpp"
#include "Metrics.hpp"

namespace jpi_edm {

class FlightHeader
{
  public:
    FlightHeader() = default;
    virtual ~FlightHeader() = default;

    // Explicitly handle copy and move operations
    FlightHeader(const FlightHeader &) = default;
    FlightHeader &operator=(const FlightHeader &) = default;
    FlightHeader(FlightHeader &&) = default;
    FlightHeader &operator=(FlightHeader &&) = default;

    virtual void dump(std::ostream &outStream) const
    {
        std::tm local;
        std::tm dateCopy = startDate; // timegm may modify the struct, so make a copy

#ifdef _WIN32
        time_t recordTime = _mkgmtime(&dateCopy);
        gmtime_s(&local, &recordTime);
#else
        time_t recordTime = timegm(&dateCopy);
        local = *gmtime(&recordTime);
#endif
        outStream << "Flight Header:" << "\n    flight_num: " << flight_num << "\n    flags: " << flags << " 0x"
                  << std::hex << flags << std::dec << " b" << std::bitset<32>(flags) << "\n    interval: " << interval
                  << "\n    date: " << std::put_time(&local, "%m/%d/%Y")
                  << "\n    time: " << std::put_time(&local, "%T") << "\n";
    }

  public:
    unsigned int flight_num; // matches what's in the $D record
    uint32_t flags;          // matches the flags in the $C record
    uint16_t unknown[3];     // unknown[0] exists even in the old variant.
    int32_t startLat;        // from fields 3 & 4 in the data block
    int32_t startLng;        // from fields 5 & 6 in the data block
    uint16_t unknown7;       // field 7 in the data block
    unsigned int interval;   // Record interval in seconds. Default is 6. Savvy runs
                             // should be 1.
    struct tm startDate {
        0
    };
};

/* only used for reporting the metrics */
class FlightMetricsRecord
{
  public:
    FlightMetricsRecord(bool isFast, unsigned long recordSeq, const std::map<MetricId, float> &m_metricMap)
        : m_isFast(isFast), m_recordSeq(recordSeq), m_metrics(m_metricMap)
    {
    }
    virtual ~FlightMetricsRecord() = default;

    // Explicitly handle copy and move operations
    FlightMetricsRecord(const FlightMetricsRecord &) = default;
    FlightMetricsRecord &operator=(const FlightMetricsRecord &) = default;
    FlightMetricsRecord(FlightMetricsRecord &&) = default;
    FlightMetricsRecord &operator=(FlightMetricsRecord &&) = default;

  public:
    bool m_isFast{false};
    unsigned long m_recordSeq{0};
    std::map<MetricId, float> m_metrics;
};

class Flight
{
  public:
    Flight() = delete;
    explicit Flight(const std::shared_ptr<Metadata> &metadata);
    virtual ~Flight() = default;

    // Explicitly handle copy and move operations (non-copyable and non-move-assignable due to const member)
    Flight(const Flight &) = delete;
    Flight &operator=(const Flight &) = delete;
    Flight(Flight &&) = default;
    Flight &operator=(Flight &&) = delete; // Can't move-assign due to const m_metadata

    void setFastFlag(bool flag) { m_fastFlag = flag; }
    void incrementSequence() { ++m_recordSeq; }
    void updateMetrics(const std::map<int, int> &values);

    [[nodiscard]] std::shared_ptr<FlightMetricsRecord> getFlightMetricsRecord();

  public:
    unsigned long m_recordSeq{0};
    bool m_fastFlag{false};
    unsigned long m_stdRecCount{0};
    unsigned long m_fastRecCount{0};

    const std::shared_ptr<Metadata> m_metadata;
    std::shared_ptr<FlightHeader> m_flightHeader;

    // This is a fairly static object that is created when the file
    // is first opened. It is a map of bit offsets to Metric objects.
    // Note that only the low-byte offset of multiple-byte items will
    // have an entry here.
    std::map<int, Metric> m_bit2MetricMap;

    // This is the running total, updated each time a data row is read
    // out of the file. It is keyed on MetricId. Items are:
    // - Initialized according to Metric.InitValue,
    // - Scaled according to Metric.ScaleFactor,
    // - and derived data is calculated (Min/Max elements, for example)
    std::map<MetricId, float> m_metricValues;
};

} // namespace jpi_edm
