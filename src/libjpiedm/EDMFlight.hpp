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
#include <iomanip>
#include <iostream>
#include <map>
#include <ctime>

namespace jpi_edm {

class EDMFlightHeader
{
  public:
    EDMFlightHeader(){};
    virtual ~EDMFlightHeader(){};

    virtual void dump()
    {
        time_t recordTime = mktime(&startDate);
        std::tm local;
#ifdef _WIN32
	localtime_s(&local, &recordTime);
#else
	local = *std::localtime(&recordTime);
#endif
        std::cout << "Flight Header:"
                  << "\n    flight_num: " << flight_num
                  << "\n    flags: " << flags << " 0x" << std::hex << flags << std::dec << " b" << std::bitset<32>(flags)
                  << "\n    interval: " << interval
                  << "\n    date: " << std::put_time(&local, "%m/%d/%Y")
                  << "\n    time: " << std::put_time(&local,   "%T")
                  << "\n";

    }

  public:
    unsigned int flight_num; // matches what's in the $D record
    uint32_t flags;      // matches the flags in the $C record
    uint16_t unknown[8]; // unknown[0] exists even in the old variant. The other
                         // 7 elements are new.
    unsigned int interval;   // Record interval in seconds. Default is 6. Savvy runs
                         // should be 1.
    struct tm startDate;
};

/**
 * @brief A representation of the data found in a flight measurement record
 *
 * Multi-element measurements are combined into one final measurement, but
 * otherwise minimal data transformation is done. For instance, if the EDM is
 * configured to report in F, the temperatures will be in F. This gives the
 * application * more control over how to display.
 *
 */
class EDMFlightRecord
{
  public:
    EDMFlightRecord() = delete;
    EDMFlightRecord(int recordSeq, bool isFast) : m_recordSeq(recordSeq), m_isFast(isFast) {};
    virtual ~EDMFlightRecord(){};

    enum Measurement {
        EGT1,
        EGT2,
        EGT3,
        EGT4,
        EGT5,
        EGT6,
        CHT1,
        CHT2,
        CHT3,
        CHT4,
        CHT5,
        CHT6,
        CLD,
        OILT,
        MARK,
        OILP,
        CARB,
        VOLTS,
        AMPS,
        OAT,
        USD,
        USD2,
        FF,
        FF2,
        FP,
        HP,
        MAP,
        RPM,
        HRS,
        DIF,
        RFL,
        LFL,
        RAUX,
        LAUX,
        SPD,
        ALT,
        LAT,
        LNG
    };

    static const int MARK_IDX = 16;

    // ordered map
    // TODO: add more of these as they're figured out
    const std::map<Measurement, std::vector<int>> offsets = {
        {EGT1, {0, 48}}, {EGT2, {1, 49}}, {EGT3, {2, 50}}, {EGT4, {3, 51}}, {EGT5, {4, 52}},
        {EGT6, {5, 53}}, {CHT1, {8}},     {CHT2, {9}},     {CHT3, {10}},    {CHT4, {11}},
        {CHT5, {12}},    {CHT6, {13}},    {CLD, {14}},     {OILT, {15}},    {MARK, {MARK_IDX}},
        {OILP, {17}},    {CARB, {18}},    {VOLTS, {20}},   {OAT, {21}},     {USD, {22}},
        {FF, {23}},      {HP, {30}},      {MAP, {40}},     {RPM, {41, 42}}, {AMPS, {64}},
        {RFL, {67}},     {LFL, {68}},     {FP, {69}},      {RAUX, {71}},    {LAUX, {84}},
        {HRS, {78, 79}}};

    virtual void apply(std::vector<int> &values);

  public:
    int m_recordSeq;
    bool m_isFast;
    std::map<int, int> m_dataMap; // ordered map
};

} // namespace jpi_edm
