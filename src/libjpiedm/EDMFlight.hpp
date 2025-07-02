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

class EDMFlightHeader
{
  public:
    EDMFlightHeader(){};
    virtual ~EDMFlightHeader(){};

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
    uint16_t unknown[8];     // unknown[0] exists even in the old variant. The other
                             // 7 elements are new.
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
 * application * more control over how to display.
 *
 */
class EDMFlightRecord
{
  public:
    EDMFlightRecord() = delete;
    EDMFlightRecord(int recordSeq, bool isFast) : m_recordSeq(recordSeq), m_isFast(isFast){};
    virtual ~EDMFlightRecord(){};

    enum Measurement {
        EGT1,
        EGT2,
        EGT3,
        EGT4,
        EGT5,
        EGT6,
        EGT7,
        EGT8,
        EGT9,
        TIT1,
        TIT2,
        CHT1,
        CHT2,
        CHT3,
        CHT4,
        CHT5,
        CHT6,
        CHT7,
        CHT8,
        CHT9,
        CLD,
        OILT,
        MARK,
        OILP,
        CRB,
        IAT,
        VLT,
        AMPS,
        OAT,
        FUSD,
        FUSD2,
        FF,
        FF2,
        FF3,
        FF4,
        EGTR1,
        EGTR2,
        EGTR3,
        EGTR4,
        EGTR5,
        EGTR6,
        EGTR7,
        EGTR8,
        EGTR9,
        TITR1,
        TITR2,
        CHTR1,
        CHTR2,
        CHTR3,
        CHTR4,
        CHTR5,
        CHTR6,
        CHTR7,
        CHTR8,
        CHTR9,
        CLDR,
        OILTR,
        FP,
        HP,
        HPR,
        MAP,
        RPM,
        RPMR,
        HRS,
        HRSR,
        TORQ,
        TORQR,
        DIF,
        LFL,  // left main tank fuel level
        RFL,  // right main tank fuel level
        LAUX, // left aux tank fuel level
        RAUX, // right aux tank fuel level
        HYD1,
        HYD2,
        HYDR1,
        HYDR2,
        SPD,
        ALT,
        LAT,
        LNG
    };

    static const int MARK_IDX = 16;

    // clang-format off
    const std::map<Measurement, std::vector<int>> offsets = {
	    // ----- byte 0
	    {EGT1, {0, 48}},
	    {EGT2, {1, 49}},
	    {EGT3, {2, 50}},
	    {EGT4, {3, 51}},
	    {EGT5, {4, 52}},
	    {EGT6, {5, 53}},
	    {TIT1, {6, 54}},
	    {TIT2, {7, 55}},

	    // ----- byte 1
	    {CHT1, {8}},
	    {CHT2, {9}},
	    {CHT3, {10}},
	    {CHT4, {11}},
	    {CHT5, {12}},
	    {CHT6, {13}},
	    {CLD,  {14}},
	    {OILT, {15}},
	    {MARK, {MARK_IDX}},

	    // ----- byte 2
	    {OILP, {17}},
	    {CRB,  {18}},
	    {IAT,  {19}},
	    {VLT,  {20}},
	    {OAT,  {21}},
	    {FUSD, {22}},
	    {FF,   {23}},

	    // ----- byte 3
	    {EGTR1, {24, 56}},
	    {EGTR2, {25, 57}},
	    {EGTR3, {26, 58}},
	    {EGTR4, {27, 59}},
	    {EGTR5, {28, 60}},
	    {EGTR6, {29, 61}},
	    {HP,    {30}},
	    {TITR1,  {30, 62}},
	    {TITR2,  {31, 63}},

	    // ----- byte 4
	    {CHTR1, {32}},
	    {CHTR2, {33}},
	    {CHTR3, {34}},
	    {CHTR4, {35}},
	    {CHTR5, {36}},
	    {CHTR6, {37}},
	    {CLDR,  {38}},

	    // ----- byte 5
	    {OILTR, {39}},
	    {MAP,   {40}},
	    {RPM,   {41, 42}},
	    {RPMR,  {43, 44}},
	    // HYDP1  45
	    // FUSED2 46
	    // FF2    47

	    // ----- byte 6
	    // EGT1 48
	    // EGT2 49
	    // EGT3 50
	    // EGT4 51
	    // EGT5 52
	    // EGT6 53
	    // TIT1 54
	    // TIT2 55

	    // ----- byte 7
	    // EGTR1 56
	    // EGTR2 57
	    // EGTR3 58
	    // EGTR4 59
	    // EGTR5 60
	    // EGTR6 61 
	    // TITR1 62
	    // TITR2 63

	    // ----- byte 8
	    {AMPS,  {64}},
	    // VLT2 65
	    // AMP2 66
	    {RFL,  {67}},
	    {LFL,  {68}},
	    {FP,    {69}},
	    {HP,    {70}},
	    {LAUX,  {71}},     // left aux tank
	    
	    // ----- byte 9
	    // unknown 72
	    // unknown 73
	    {TORQ,  {74}},
	    // unknown 75
	    // unknown 76
	    // unknown 77
	    {HRS,   {78, 79}},

	    // ----- byte 10
	    // unknown 80
	    // unknown 81
	    // unknown 82
	    {ALT,   {83}},     // altitude, in feet
	    {RAUX,  {84}},     // right aux tank
	    {SPD,   {85}},     // airspeed, in knots
	    // longitude? 86
	    // latitude?  87

	    // ----- byte 11
	    {HPR,   {89}},

	    // ----- byte 12
	    {TORQR, {98}},
	    {HRSR,  {102, 103}},

	    // ----- byte 13
	    {EGT7,  {104, 108}},
	    {EGT8,  {105, 109}},
	    {EGT9,  {106, 110}},
	    {FF3,   {107}},
	    {HYD1,  {111}},

	    // ----- byte 14
	    {EGTR7, {112, 116}},
	    {EGTR8, {113, 117}},
	    {EGTR9, {114, 118}},
	    {FF4,   {115}},
	    {HYDR1, {119}},

	    // ----- byte 15
	    {CHT7,  {120}},
	    {CHT8,  {121}},
	    {CHT9,  {122}},
	    {HYD2,  {123}},
	    {CHTR7, {124}},
	    {CHTR8, {125}},
	    {CHTR9, {126}},
	    {HYDR2, {127}},
    };
    // clang-format on

    virtual void apply(std::vector<int> &values);

  public:
    int m_recordSeq;
    bool m_isFast;
    std::map<int, int> m_dataMap; // ordered map
};

} // namespace jpi_edm
