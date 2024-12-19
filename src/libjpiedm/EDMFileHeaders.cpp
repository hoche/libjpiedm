/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Various objects created from the text headers of an EDM flight file.
 * These have no data about a specific flight, just the generic data that was
 * created when the file was created, i.e. the date, whether temps are in C or
 * F, maximum values recorded, etc.
 *
 * Included are:
 *
 * EDMEDMConfigLimits
 *      Maximum values recorded.
 *
 * EDMEDMConfigInfo
 *      EDM Model, firmware version, etc. Also contains a flags field which was
 * used to indicate which kinds of measurement were captured, but may be mostly
 * obsolete with the new format (it only is 32-bits and the new format supports
 * up to 128 different measurements). It is still useful for telling whether
 * temperatures are in C or F.
 *
 * EDMEDMFuelLimits
 *      Fueltank sizes and fuel flow scaling rates (K-factors)
 *
 * EDMProtoHeader
 *      Protocol version
 *
 * EDMEDMTimeStamp
 *      The date and time the file was created for downloading from the EDM.
 */

#include <bitset>
#include <iostream>
#include <string>
#include <vector>
#include <iostream>

#include "EDMFileHeaders.hpp"

namespace jpi_edm {

#define DEBUG_HEADERS 1

const int maxheaderlen = 128;

/**
 * EDMConfigLimits
 */
void EDMConfigLimits::apply(std::vector<unsigned long> values)
{
    if (values.size() < 8) {
        throw std::invalid_argument{"Incorrect number of arguments in $A line"};
    }
    volts_hi = values[0];
    volts_lo = values[1];
    egt_diff = values[2];
    cht_temp_hi = values[3];
    shock_cooling_cld = values[4];
    turbo_inlet_temp_hi = values[5];
    oil_temp_hi = values[6];
    oil_temp_lo = values[7];
}

void EDMConfigLimits::dump(std::ostream& outStream)
{
    outStream << "EDMConfigLimits:" << "\n    volts_hi: " << volts_hi
              << "\n    volts_lo: " << volts_lo << "\n    egt_diff: " << egt_diff
              << "\n    cht_temp_hi: " << cht_temp_hi
              << "\n    shock_cooling_cld: " << shock_cooling_cld
              << "\n    turbo_inlet_temp_hi: " << turbo_inlet_temp_hi
              << "\n    oil_temp_hi: " << oil_temp_hi << "\n    oil_temp_lo: " << oil_temp_lo
              << "\n";
}

void EDMConfigInfo::apply(std::vector<unsigned long> values)
{
    bool short_header{false};

    if (values.size() < 5) {
        throw std::invalid_argument{"Incorrect number of arguments in $C line"};
    }

    edm_model = values[0];
    flags = (values[2] << 16) | (values[1] & 0x0000FFFF);
    unk1 = values[3];

    // iterate backwards from end
    auto rit = values.rbegin();
    if (values.size() > 8) {
        build_min = *rit++;
        build_maj = *rit++;
    }
    firmware_version = *rit++;
}

const uint32_t F_BAT = 0x00000001;
const uint32_t F_C1 = 0x00000004;
const uint32_t F_C2 = 0x00000008;
const uint32_t F_C3 = 0x00000010;
const uint32_t F_C4 = 0x00000020;
const uint32_t F_C5 = 0x00000040;
const uint32_t F_C6 = 0x00000080;
const uint32_t F_C7 = 0x00000100;
const uint32_t F_C8 = 0x00000200;
const uint32_t F_C9 = 0x00000400;
const uint32_t F_E1 = 0x00000800;
const uint32_t F_E2 = 0x00001000;
const uint32_t F_E3 = 0x00002000;
const uint32_t F_E4 = 0x00004000;
const uint32_t F_E5 = 0x00008000;
const uint32_t F_E6 = 0x00010000;
const uint32_t F_E7 = 0x00020000;
const uint32_t F_E8 = 0x00040000;
const uint32_t F_E9 = 0x00080000;
const uint32_t F_OIL = 0x00100000;
const uint32_t F_T1 = 0x00200000;
const uint32_t F_T2 = 0x00400000;
const uint32_t F_CDT = 0x00800000; // also CRB
const uint32_t F_IAT = 0x01000000;
const uint32_t F_OAT = 0x02000000;
const uint32_t F_RPM = 0x04000000;
const uint32_t F_FF = 0x08000000;
const uint32_t F_USD = F_FF; // duplicate
const uint32_t F_TEMP_IN_F = 0x10000000;
const uint32_t F_MAP = 0x40000000;
const uint32_t F_DIF = F_E1 | F_E2; // DIF exists if there's more than one EGT
const uint32_t F_HP = F_RPM | F_MAP | F_FF;
const uint32_t F_MARK = 0x00000001; // 1 bit always seems to exist

void EDMConfigInfo::dump(std::ostream& outStream)
{
    outStream << "EDMConfigInfo:" 
              << "\n    edm_model: " << edm_model
              << "\n    flags: " << flags << " 0x" << std::hex << flags << std::dec << " b" << std::bitset<32>(flags)
              << "\n    firmware_version: " << firmware_version
              << "\n    build: " << build_maj << "." << build_min
              << "\n";
    outStream << "Temperatures for CHT, EGT, and TIT are in " << (flags & F_TEMP_IN_F ? "F" : "C")
              << "\n";
}

/**
 * EDMFuelLimits
 */
void EDMFuelLimits::apply(std::vector<unsigned long> values)
{
    if (values.size() < 5) {
        throw std::invalid_argument{"Incorrect number of arguments in $F line"};
    }

    empty = values[0];
    main_tank_size = values[1];
    aux_tank_size = values[2];
    k_factor_1 = values[3];
    k_factor_2 = values[4];
}

void EDMFuelLimits::dump(std::ostream& outStream)
{
    outStream << "EDMFuelLimits:" << "\n    empty: " << empty
              << "\n    main_tank_size: " << main_tank_size
              << "\n    aux_tank_size: " << aux_tank_size << "\n    k_factor_2: " << k_factor_1
              << "\n    k_factor_1: " << k_factor_2 << "\n";
}

/**
 * EDMProtoHeader
 */
void EDMProtoHeader::apply(std::vector<unsigned long> values)
{
    if (values.size() < 1) {
        throw std::invalid_argument{"Incorrect number of arguments in $P line"};
    }
    value = values[0];
}

void EDMProtoHeader::dump(std::ostream& outStream) {
    outStream << "EDM ProtoHeader:" << "\n    value: " << value << "\n";
}

/**
 * Timestamp
 */
void EDMTimeStamp::apply(std::vector<unsigned long> values)
{
    if (values.size() < 6) {
        throw std::invalid_argument{"Incorrect number of arguments in $T line"};
    }

    mon = values[0];
    day = values[1];
    yr = values[2];
    hh = values[3];
    mm = values[4];
    flight_num = values[5];
}

void EDMTimeStamp::dump(std::ostream& outStream)
{
    outStream << "EDMTimeStamp:" << "\n    mon: " << mon << "\n    day: " << day
              << "\n    yr: " << yr << "\n    hh: " << hh << "\n    mm: " << mm
              << "\n    flight_num: " << flight_num << "\n";
}

bool EDMMetaData::isTwin() {
    return (m_configInfo.edm_model==760 || m_configInfo.edm_model==960);
}

int EDMMetaData::protoVersion()
{
    // peel out twins first
    if (m_configInfo.edm_model == 760) {
        return PROTO_V2;
    }
    if (m_configInfo.edm_model == 960) {
        return PROTO_V5;
    }

    if (m_configInfo.edm_model < 900) {
        if (m_protoHeader.value < 2) {
            return PROTO_V1; // old 700 or 800
        }
        return PROTO_V4; // updated 700 or 800, has proto header
    }

    // 900+. check firmware version
    if (m_configInfo.firmware_version <= 108) {
        return PROTO_V1;
    }

    return PROTO_V4;
}

bool EDMMetaData::isOldRecFormat() {
    return (protoVersion() == PROTO_V1 || protoVersion() == PROTO_V2);
}

int EDMMetaData::guessFlightHeaderVersion() {
    if (m_protoHeader.value > 1 || m_configInfo.edm_model >= 900) {
        if (m_configInfo.build_maj > 2010) {
            return HEADER_V4;
        }
        if (m_configInfo.build_maj > 880) {
            return HEADER_V3;
        }
        return HEADER_V2;
    }
    return HEADER_V1;
}

bool EDMMetaData::tempInC() {
    return ((m_configInfo.flags & F_TEMP_IN_F) == 0);
}

void EDMMetaData::dump(std::ostream& outStream)
{
    outStream << "Tailnumber: " << m_tailNum << "\n";
    outStream << "Old Rec Format: " << (isOldRecFormat() ? "yes" : "no") << "\n";
    m_configLimits.dump(outStream);
    m_configInfo.dump(outStream);
    m_fuelLimits.dump(outStream);
    m_protoHeader.dump(outStream);
    m_timeStamp.dump(outStream);
}

} // namespace jpi_edm
