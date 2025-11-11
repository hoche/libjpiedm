/**
 * This file is a C++ version of Keith Wannamaker's Java code for parsing
 * JPI EDM data. It has been modified by Michel Hoche-Mong to work with the
 * other components of this library and to handle a few more data elements
 * from the EDM files.
 *
 * Original copyright below. Modifications copyright Michel Hoche-Mong,
 * under the Creative Commons CC-BY-4.0 license.
 * SPDX-License-Identifier: CC-BY-4.0
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

#include <iostream>
#include <map>
#include <vector>

#include "Metadata.hpp"
#include "MetricId.hpp"
#include "Metrics.hpp"

// #define DEBUG_METRICS 1

namespace jpi_edm {

// Maps low-byte bit indexes to Metric entries.
// Only the low byte indexes are mapped.
std::map<int, Metric> Metrics::getBitToMetricMap(const EDMVersion &edmversion)
{
    std::map<int, Metric> result;

    for (const auto &metric : m_metrics) {
        if ((metric.getVersionMask() & edmversion) > 0) {
#if DEBUG_METRICS
            if (result.count(metric.getLowByteBitIdx()) > 0) {
                std::cout << "Duplicate metric entry for " << metric.getLowByteBitIdx() << std::endl;
            }
#endif
            result.emplace(metric.getLowByteBitIdx(), metric);
        }
    }

    return result;
}

#define IDSTR(x) x, #x

// clang-format off
const std::vector<Metric> Metrics::m_metrics = {
    // bytes 0 and 6
    Metric(V1|V2|V3|V4|V5,   0,  48, IDSTR(EGT11), "engine[1].exhaust_gas_temperature[1]"),
    Metric(V1|V2|V3|V4|V5,   1,  49, IDSTR(EGT12), "engine[1].exhaust_gas_temperature[2]"),
    Metric(V1|V2|V3|V4|V5,   2,  50, IDSTR(EGT13), "engine[1].exhaust_gas_temperature[3]"),
    Metric(V1|V2|V3|V4|V5,   3,  51, IDSTR(EGT14), "engine[1].exhaust_gas_temperature[4]"),
    Metric(V1|V2|V3|V4|V5,   4,  52, IDSTR(EGT15), "engine[1].exhaust_gas_temperature[5]"),
    Metric(V1|V2|V3|V4|V5,   5,  53, IDSTR(EGT16), "engine[1].exhaust_gas_temperature[6]"),
    Metric(V1|V2|V3|V4|V5,   6,  54, IDSTR(TIT11), "engine[1].turbine_inlet_temperature[1]", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),
    Metric(V1|V2|V3|V4|V5,   7,  55, IDSTR(TIT12), "engine[1].turbine_inlet_temperature[2]", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),

    // byte 1
    Metric(V1|V2|V3|V4|V5,   8,      IDSTR(CHT11), "engine[1].cylinder_head_temperature[1]"),
    Metric(V1|V2|V3|V4|V5,   9,      IDSTR(CHT12), "engine[1].cylinder_head_temperature[2]"),
    Metric(V1|V2|V3|V4|V5,  10,      IDSTR(CHT13), "engine[1].cylinder_head_temperature[3]"),
    Metric(V1|V2|V3|V4|V5,  11,      IDSTR(CHT14), "engine[1].cylinder_head_temperature[4]"),
    Metric(V1|V2|V3|V4|V5,  12,      IDSTR(CHT15), "engine[1].cylinder_head_temperature[5]"),
    Metric(V1|V2|V3|V4|V5,  13,      IDSTR(CHT16), "engine[1].cylinder_head_temperature[6]"),
    Metric(V1|V2|V3|V4|V5,  14,      IDSTR(CLD1),  "engine[1].cylinder_head_temperature_cooling_rate"),
    Metric(V1|V2|V3|V4|V5,  15,      IDSTR(OILT1), "engine[1].oil_temperature"),

    // byte 2
    Metric(V1|V2|V3|V4|V5,  16,      IDSTR(MARK),  "mark"),
    Metric(V1   |V3|V4|V5,  17,      IDSTR(OILP1), "engine[1].oil_pressure"),
    Metric(V1|V2|V3|V4|V5,  18,      IDSTR(CRB1),  "engine[1].carb_temperature"),
    Metric(V1   |V3|V4|V5,  19,      IDSTR(IAT1),  "engine[1].induction_air_temperature"),
    Metric(   V2         ,  19,      IDSTR(MAP2),  "engine[2].manifold_pressure", Metric::ScaleFactor::TEN),
    Metric(V1|V2|V3|V4|V5,  20,      IDSTR(VOLT1), "voltage[1]", Metric::ScaleFactor::TEN),
    Metric(V1|V2|V3|V4|V5,  21,      IDSTR(OAT),   "outside_air_temperature"),
    Metric(V1|V2|V3|V4|V5,  22,      IDSTR(FUSD11),"engine[1].fuel_used[1]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(V1|V2|V3|V4|V5,  23,      IDSTR(FF11),  "engine[1].fuel_flow[1]", Metric::ScaleFactor::TEN_IF_GPH),

    // bytes 3 and 7
    Metric(V1   |V3|V4   ,  24,  56, IDSTR(EGT17), "engine[1].exhaust_gas_temperature[7]"),
    Metric(   V2      |V5,  24,  56, IDSTR(EGT21), "engine[2].exhaust_gas_temperature[1]"),
    Metric(V1   |V3|V4   ,  25,  57, IDSTR(EGT18), "engine[1].exhaust_gas_temperature[8]"),
    Metric(   V2      |V5,  25,  57, IDSTR(EGT22), "engine[2].exhaust_gas_temperature[2]"),
    Metric(V1   |V3|V4   ,  26,  58, IDSTR(EGT19), "engine[1].exhaust_gas_temperature[9]"),
    Metric(   V2      |V5,  26,  58, IDSTR(EGT23), "engine[2].exhaust_gas_temperature[3]"),
    Metric(V1   |V3|V4   ,  27,      IDSTR(CHT17), "engine[1].cylinder_head_temperature[7]"),
    Metric(   V2      |V5,  27,  59, IDSTR(EGT24), "engine[2].exhaust_gas_temperature[4]"),
    Metric(V1   |V3|V4   ,  28,      IDSTR(CHT18), "engine[1].cylinder_head_temperature[8]"),
    Metric(   V2      |V5,  28,  60, IDSTR(EGT25), "engine[2].exhaust_gas_temperature[5]"),
    Metric(V1   |V3|V4   ,  29,      IDSTR(CHT19), "engine[1].cylinder_head_temperature[9]"),
    Metric(   V2      |V5,  29,  61, IDSTR(EGT26), "engine[2].exhaust_gas_temperature[6]"),
    Metric(V1   |V3|V4   ,  30,      IDSTR(HP1),   "engine[1].horsepower", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),
    Metric(   V2      |V5,  30,  62, IDSTR(TIT21), "engine[2].turbine_inlet_temperature[1]", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),
    Metric(   V2      |V5,  31,  63, IDSTR(TIT22), "engine[2].turbine_inlet_temperature[2]", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),

    // byte 4
    Metric(   V2      |V5,  32,      IDSTR(CHT21), "engine[2].cylinder_head_temperature[1]"),
    Metric(   V2      |V5,  33,      IDSTR(CHT22), "engine[2].cylinder_head_temperature[2]"),
    Metric(   V2      |V5,  34,      IDSTR(CHT23), "engine[2].cylinder_head_temperature[3]"),
    Metric(   V2      |V5,  35,      IDSTR(CHT24), "engine[2].cylinder_head_temperature[4]"),
    Metric(   V2      |V5,  36,      IDSTR(CHT25), "engine[2].cylinder_head_temperature[5]"),
    Metric(   V2      |V5,  37,      IDSTR(CHT26), "engine[2].cylinder_head_temperature[6]"),
    Metric(   V2      |V5,  38,      IDSTR(CLD2),  "engine[2].cylinder_head_temperature_cooling_rate"),
    Metric(   V2      |V5,  39,      IDSTR(OILT2), "engine[2].oil_temperature"),

    // byte 5
    Metric(V1|V2|V3|V4|V5,  40,      IDSTR(MAP1),  "engine[1].manifold_pressure", Metric::ScaleFactor::TEN),
    Metric(V1|V2|V3|V4|V5,  41,  42, IDSTR(RPM1),  "engine[1].rpm"),
    Metric(   V2      |V5,  43,  44, IDSTR(RPM2),  "engine[2].rpm"),
    Metric(         V4   ,  44,      IDSTR(HYDP12),"engine[1].hydraulic_pressure[2]"),
    Metric(   V2      |V5,  45,      IDSTR(CRB2),  "engine[2].carb_temperature"),
    Metric(         V4   ,  45,      IDSTR(HYDP11),"engine[1].hydraulic_pressure[1]"),
    Metric(   V2      |V5,  46,      IDSTR(FUSD21),"engine[2].fuel_used[1]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(         V4   ,  46,      IDSTR(FF12),  "engine[1].fuel_flow[2]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(         V4   ,  47,      IDSTR(FUSD12),"engine[1].fuel_used[2]", Metric::ScaleFactor::TEN_IF_GPH, Metric::InitialValue::NEGATIVE_TEN),
    Metric(   V2      |V5,  47,      IDSTR(FF21),  "engine[2].fuel_flow[1]", Metric::ScaleFactor::TEN_IF_GPH),

    // bytes 6 & 7 are all high bytes for earlier values

    // byte 8
    Metric(      V3|V4|V5,  64,      IDSTR(AMP1),  "amperage[1]"),
    Metric(      V3|V4|V5,  65,      IDSTR(VOLT2), "voltage[2]", Metric::ScaleFactor::TEN),
    Metric(      V3|V4|V5,  66,      IDSTR(AMP2),  "amperage[2]"),
    Metric(      V3|V4   ,  67,      IDSTR(RMAIN), "right_main.fuel_level", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5,  67,      IDSTR(FLVL11),"engine[1].fuel_level[1]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(      V3|V4   ,  68,      IDSTR(LMAIN), "left_main.fuel_level", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5,  68,      IDSTR(FLVL12),"engine[1].fuel_level[2]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(      V3|V4|V5,  69,      IDSTR(FP1),   "engine[1].fuel_pressure", Metric::ScaleFactor::TEN),
    Metric(            V5,  70,      IDSTR(HP1),   "engine[1].horsepower", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),
    Metric(         V4   ,  71,      IDSTR(LAUX),  "left_aux.fuel_level", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5,  71,      IDSTR(FLVL13),"engine[1].fuel_level[3]", Metric::ScaleFactor::TEN_IF_GPH),

    // byte 9
    //Metric(         V4|V5,  72,  76, IDSTR(UNSUPPORTED_METRIC), Metric::ScaleFactor::TEN),  // left ng ?
    //Metric(         V4|V5,  73,  77, IDSTR(UNSUPPORTED_METRIC),  // left np ?
    Metric(         V4|V5,  74,      IDSTR(TORQ1), "engine[1].torque"),
    //Metric(         V4|V5,  75,      IDSTR(UNSUPPORTED_METRIC),  // left itt, but no high byte ?
    Metric(         V4|V5,  78,  79, IDSTR(HRS1), "engine[1].hours", Metric::ScaleFactor::TEN),

    // byte 10
    Metric(         V4|V5,  83,      IDSTR(ALT), "altitude", Metric::ScaleFactor::NONE, Metric::InitialValue::NEGATIVE_ONE), // in feet
    Metric(         V4   ,  84,      IDSTR(RAUX),"right_aux.fuel_level", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(         V4|V5,  85,      IDSTR(SPD), "airspeed", Metric::ScaleFactor::NONE, Metric::InitialValue::NEGATIVE_ONE), // in knots
    Metric(         V4|V5,  86,      IDSTR(LAT), "latitude", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),
    Metric(         V4|V5,  87,      IDSTR(LNG), "longitude", Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO),

    // byte 11
    Metric(            V5,  88,      IDSTR(MAP2),  "engine[2].manifold_pressure", Metric::ScaleFactor::TEN),
    Metric(            V5,  89,      IDSTR(HP2),   "engine[2].horsepower"),
    Metric(            V5,  90,      IDSTR(IAT2),  "engine[2].induction_air_temperature"),
    Metric(            V5,  91,      IDSTR(FLVL21),"engine[2].fuel_level[1]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5,  92,      IDSTR(FLVL22),"engine[2].fuel_level[2]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5,  93,      IDSTR(FP2),   "engine[2].fuel_pressure", Metric::ScaleFactor::TEN),
    Metric(            V5,  94,      IDSTR(OILP2), "engine[2].oil_pressure", Metric::ScaleFactor::TEN),
    Metric(            V5,  95,      IDSTR(FLVL23),"engine[2].fuel_level[3]", Metric::ScaleFactor::TEN_IF_GPH),

    // byte 12
    //Metric(            V5,  96, 100, IDSTR(UNSUPPORTED_METRIC), Metric::ScaleFactor::TEN),  // right ng ?
    //Metric(            V5,  97, 101, IDSTR(UNSUPPORTED_METRIC),  // right np ?
    Metric(            V5,  98,      IDSTR(TORQ2), "engine[2].torque"),
    //Metric(            V5,  99,      IDSTR(UNSUPPORTED_METRIC),  // right itt, but no high byte ?
    Metric(            V5, 102, 103, IDSTR(HRS2), "engine[2].hours", Metric::ScaleFactor::TEN),

    // byte 13
    Metric(            V5, 104, 108, IDSTR(EGT17), "engine[1].exhaust_gas_temperature[7]"),
    Metric(            V5, 105, 109, IDSTR(EGT18), "engine[1].exhaust_gas_temperature[8]"),
    Metric(            V5, 106, 110, IDSTR(EGT19), "engine[1].exhaust_gas_temperature[9]"),
    Metric(            V5, 107,      IDSTR(FF12),  "engine[1].fuel_flow[2]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5, 111,      IDSTR(HYDP11),"engine[1].hydraulic_pressure[1]"),

    // byte 14
    Metric(            V5, 112, 116, IDSTR(EGT27), "engine[2].exhaust_gas_temperature[7]"),
    Metric(            V5, 113, 117, IDSTR(EGT28), "engine[2].exhaust_gas_temperature[8]"),
    Metric(            V5, 114, 118, IDSTR(EGT29), "engine[2].exhaust_gas_temperature[9]"),
    Metric(            V5, 115,      IDSTR(FF22),  "engine[2].fuel_flow[2]", Metric::ScaleFactor::TEN_IF_GPH),
    Metric(            V5, 119,      IDSTR(HYDP21),"engine[2].hydraulic_pressure[1]"),

    // byte 15
    Metric(            V5, 120,      IDSTR(CHT17), "engine[1].cylinder_head_temperature[7]"),
    Metric(            V5, 121,      IDSTR(CHT18), "engine[1].cylinder_head_temperature[8]"),
    Metric(            V5, 122,      IDSTR(CHT19), "engine[1].cylinder_head_temperature[9]"),
    Metric(            V5, 123,      IDSTR(HYDP12),"engine[1].hydraulic_pressure[2]"),
    Metric(            V5, 124,      IDSTR(CHT27), "engine[2].cylinder_head_temperature[7]"),
    Metric(            V5, 125,      IDSTR(CHT28), "engine[2].cylinder_head_temperature[8]"),
    Metric(            V5, 126,      IDSTR(CHT29), "engine[2].cylinder_head_temperature[9]"),
    Metric(            V5, 127,      IDSTR(HYDP22), "engine[2].hydraulic_pressure[2]")
};
// clang-format on

} // namespace jpi_edm
