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
 * EDMConfigLimits
 *      Maximum values recorded.
 *
 * EDMConfigInfo
 *      EDM Model, firmware version, etc. Also contains a flags field which was
 * used to indicate which kinds of measurement were captured, but may be
 * obsolete with the new format (it only is 32-bits and the new format supports
 * up to 128 different measurements).
 *
 * EDMFuelLimits
 *      Fueltank sizes and fuel flow scaling rates (K-factors)
 *
 * EDMFuelRate
 *      Maximum rate of fuel flow? Not sure.
 *
 * EDMTimeStamp
 *      The date and time the file was created for downloading from the EDM.
 */

#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>
#include <iostream>

namespace jpi_edm {

/**
 * Base class for all the EDMFileHeader classes
 */
class EDMFileHeader
{
  public:
    /* Throws an exception if an insufficient number of arguments
     * are in the vector.
     */
    virtual void apply(std::vector<unsigned long> values) = 0;
    virtual void dump(std::ostream& outStream) = 0;
};

/**
 * $A record, configured limits
 *
 * Format:
 * VoltsHi*10,VoltsLo*10,DIF,CHT,CLD,TIT,OilHi,OilLo
 *
 * Example:
 * $A, 305,230,500,415,60,1650,230,90*7F
 */
class EDMConfigLimits : public EDMFileHeader
{
  public:
    EDMConfigLimits(){};
    virtual ~EDMConfigLimits(){};
    virtual void apply(std::vector<unsigned long> values);
    virtual void dump(std::ostream& outStream);

  public:
    unsigned long volts_hi;
    unsigned long volts_lo;
    unsigned long egt_diff;
    unsigned long cht_temp_hi;
    unsigned long shock_cooling_cld;
    unsigned long turbo_inlet_temp_hi;
    unsigned long oil_temp_hi;
    unsigned long oil_temp_lo;
};

/**
 * $C record, config info (only partially known)
 *
 * Format:
 * model#, feature flags lo, feature flags hi, unknown flags, firmware version
 *
 * Feature flags is a 32-bit set of flags.
 * -m-d fpai r2to eeee eeee eccc cccc cc-b
 *
 * e = egt (up to 9 cyls)
 * c = cht (up to 9 cyls)
 * d = probably cld, or maybe engine temps unit (1=F)
 * b = bat
 * o = oil
 * t = tit1
 * 2 = tit2
 * a = OAT
 * f = fuel flow
 * r = CDT (also CARB - not distinguished in the CSV output)
 * i = IAT
 * m = MAP
 * p = RPM
 * *** e and c may be swapped (but always exist in tandem)
 * *** d and b may be swapped (but seem to exist in tandem)
 * *** m, p and i may be swapped among themselves, haven't seen
 *     enough independent examples to know for sure.
 *
 * Example:
 * $C,700,63741, 6193, 1552, 292*58
 */
class EDMConfigInfo : public EDMFileHeader
{
  public:
    EDMConfigInfo(){};
    virtual ~EDMConfigInfo(){};
    virtual void apply(std::vector<unsigned long> values);
    virtual void dump(std::ostream& outStream);
    virtual bool tempInC();

  public:
    unsigned long edm_model;
    uint32_t flags;
    unsigned long unk1;
    unsigned long unk2;
    unsigned long unk3;
    unsigned long firmware_version; // n.nn * 100
    unsigned long build_maj;
    unsigned long build_min;
};

/**
 * $F = Fuel flow config and limits.
 *
 * Format:
 * empty,full,warning,kfactor,kfactor
 *
 * K factor is the number of pulses expected for every one volumetric unit of
 * fluid passing through a given flow meter.
 *
 * Example:
 * $F,0,999,  0,2950,2950*53
 */
class EDMFuelLimits : public EDMFileHeader
{
  public:
    EDMFuelLimits(){};
    virtual ~EDMFuelLimits(){};
    virtual void apply(std::vector<unsigned long> values);
    virtual void dump(std::ostream& outStream);

  public:
    unsigned long empty;
    unsigned long main_tank_size;
    unsigned long aux_tank_size;
    unsigned long k_factor_1;
    unsigned long k_factor_2;
};

/**
 * $P = Unknown
 *
 * Format:
 * single value
 *
 * Example:
 * $P, 2*6E
 */
class EDMPHeader : public EDMFileHeader
{
  public:
    EDMPHeader(){};
    virtual ~EDMPHeader(){};
    virtual void apply(std::vector<unsigned long> values);
    virtual void dump(std::ostream& outStream);

  public:
    unsigned long value;
};

/**
 * $T = timestamp of download, fielded (Times are UTC)
 *
 * Format:
 * MM,DD,YY,hh,mm,?? maybe some kind of seq num but not strictly sequential?
 *
 * Example:
 *   $T, 5,13, 5,23, 2, 2222*65
 */
class EDMTimeStamp : public EDMFileHeader
{
  public:
    EDMTimeStamp(){};
    virtual ~EDMTimeStamp(){};
    virtual void apply(std::vector<unsigned long> values);
    virtual void dump(std::ostream& outStream);

  public:
    unsigned long mon;
    unsigned long day;
    unsigned long yr;
    unsigned long hh;
    unsigned long mm;
    unsigned long flight_num;
};

/**
 * @brief Collection of all the flight headers that can be in an EDM file.
 *
 */
class EDMFileHeaderSet
{
  public:
    EDMFileHeaderSet(){};
    virtual ~EDMFileHeaderSet(){};

    virtual void dump(std::ostream& outStream);

  public:
    std::string m_tailNum;
    EDMConfigLimits m_configLimits;
    EDMConfigInfo m_configInfo;
    EDMFuelLimits m_fuelLimits;
    EDMPHeader m_PHeader;
    EDMTimeStamp m_timeStamp;
};

} // namespace jpi_edm
