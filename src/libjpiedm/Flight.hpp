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

/**
 * @brief A representation of the data found in a flight measurement record
 *
 * Multi-element measurements are combined into one final measurement, but
 * otherwise minimal data transformation is done. For instance, if the EDM is
 * configured to report in F, the temperatures will be in F. This gives the
 * application more control over how to display.
 *
 */
class FlightRecord
{
  public:
    FlightRecord() = delete;
    FlightRecord(int recordSeq, bool isFast) : m_recordSeq(recordSeq), m_isFast(isFast){};
    virtual ~FlightRecord(){};

    static const int MARK_IDX = 16;

    virtual void apply(std::vector<int> &values);

  public:
    int m_recordSeq;
    bool m_isFast;
    std::map<int, int> m_dataMap; // ordered map
};

} // namespace jpi_edm
