/**
 * This file the header file for a C++ version of Keith Wannamaker's Java code
 * for parsing JPI EDM data. It has been modified by Michel Hoche-Mong to handle
 * more data elements from the EDM files.
 *
 * Original copyright below. Modifications copyright Michel Hoche-Mong, under
 * the same license as the original (Apache License, Version 2.0).
 */

/**
 *    Copyright 2015 Keith Wannamaker
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <optional>
#include <vector>
#include <memory>

#include "Metadata.hpp"

namespace jpi_edm {

enum MeasurementId {
    EGT00,
    EGT01,
    EGT02,
    EGT03,
    EGT04,
    EGT05,
    EGT06,
    EGT07,
    EGT08,

    EGT10,
    EGT11,
    EGT12,
    EGT13,
    EGT14,
    EGT15,
    EGT16,
    EGT17,
    EGT18,

    CHT00,
    CHT01,
    CHT02,
    CHT03,
    CHT04,
    CHT05,
    CHT06,
    CHT07,
    CHT08,

    CHT10,
    CHT11,
    CHT12,
    CHT13,
    CHT14,
    CHT15,
    CHT16,
    CHT17,
    CHT18,

    CLD0,
    CLD1,

    TIT00,
    TIT01,
    TIT10,
    TIT11,

    OILT0,
    OILT1,

    OILP0,
    OILP1,

    CRB0,
    CRB1,

    IAT0,
    IAT1,

    MP0,
    MP1,

    VOLT0,
    VOLT1,

    AMP0,
    AMP1,

    FF00,
    FF01,
    FF10,
    FF11,

    FLVL00,
    FLVL01,
    FLVL02,
    FLVL10,
    FLVL11,
    FLVL12,

    FUSD00,
    FUSD01,
    FUSD10,
    FUSD11, // Where is this?

    FP0,
    FP1,

    HP0,
    HP1,

    RPM0,
    RPM1,

    HRS0,
    HRS1,

    TORQ0,
    TORQ1,

    LMAIN,
    RMAIN,

    LAUX,
    RAUX,

    HYDP00,
    HYDP01,
    HYDP10,
    HYDP11,

    MARK,
    OAT,
    SPD,
    ALT,
    LAT,
    LNG,

    DIF0, // temp diff between hottest and coldest EGT, engine 0
    DIF1, // temp diff between hottest and coldest EGT, engine 1
};

class Metric {
public:
    enum ScaleFactor {
        NONE,
        TEN,
        TEN_IF_GPH
    };

    enum InitValue {
        ZERO = 0x00,
        DEFAULT = 0xF0,
    };

    Metric(int versionMask, int lowByteBit, std::optional<int> highByteBit, MeasurementId mid,
           const std::string& name, ScaleFactor scale = ScaleFactor::NONE, InitValue initValue = InitValue::DEFAULT)
        : m_versionMask(versionMask)
        , m_lowByteBit(lowByteBit)
        , m_highByteBit(highByteBit)
        , m_measurementId(mid)
        , m_name(name)
        , m_scaleFactor(scale)
        , m_initValue(static_cast<int>(initValue)) {}

    Metric(int versionMask, int lowByteBit, MeasurementId mid, const std::string& name, 
           ScaleFactor scale = ScaleFactor::NONE)
        : Metric(versionMask, lowByteBit, std::nullopt, mid, name, scale) {}

    int getVersionMask() const { return m_versionMask; }
    int getLowByteBit() const { return m_lowByteBit; }
    const std::optional<int>& getHighByteBit() const { return m_highByteBit; }
    const std::string& getName() const { return m_name; }
    ScaleFactor getScaleFactor() const { return m_scaleFactor; }

private:
    int m_versionMask;
    int m_lowByteBit;
    std::optional<int> m_highByteBit;
    int m_measurementId;
    std::string m_name;
    ScaleFactor m_scaleFactor;
    int m_initValue;
};

class Metrics {
public:
    static std::map<int, Metric> getBitToMetricMap(const Metadata& metadata);

private:
    static const std::string UNSUPPORTED_METRIC;
    static const std::vector<Metric> m_metrics;
};

}
