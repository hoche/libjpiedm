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
    FlightHeader(){};
    virtual ~FlightHeader(){};

    virtual void dump(std::ostream &outStream)
    {
        std::tm local;

#ifdef _WIN32
        time_t recordTime = _mkgmtime(&startDate);
        gmtime_s(&local, &recordTime);
#else
        time_t recordTime = timegm(&startDate);
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
    FlightMetricsRecord(bool isFast, unsigned long recordSeq, std::map<int,int>& metrics) :
        m_isFast(isFast), m_recordSeq(recordSeq), m_metrics(metrics) {};
    virtual ~FlightMetricsRecord(){};

  public:
    bool m_isFast{false};
    unsigned long m_recordSeq{0};
    std::map<int, int> m_metrics;
};

class Flight
{
  public:
    Flight() = delete;
    explicit Flight(const std::shared_ptr<Metadata> &metadata);
    virtual ~Flight(){};

    void setFastFlag(bool flag) { m_fastFlag = flag; }
    void incrementSequence() { ++m_recordSeq; }
    void updateMetrics(const std::map<int, int> &values);

    std::shared_ptr<FlightMetricsRecord> getFlightMetricsRecord();

  public:
    unsigned long m_recordSeq{0};
    bool m_fastFlag{false};
    unsigned long m_stdRecCount{0};
    unsigned long m_fastRecCount{0};

    std::shared_ptr<FlightHeader> m_flightHeader;
    std::map<int, Metric> m_metricsMap;

    // the running total
    std::map<int, int> m_metricValues;
};

} // namespace jpi_edm
