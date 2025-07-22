/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */
 
#pragma once

#include <string>
#include <FileHeaders.hpp>

namespace jpi_edm {

enum EDMVersion {
    V1 = 0x01, // <900
    V2 = 0x02, // 760
    V3 = 0x04, // 900/930, pre-version 108
    V4 = 0x08, // 900/930, later firmware, or has protocol header
    V5 = 0x10  // 960
};

enum HeaderVersion {
    HEADER_V1 = 0x01, // 1 word unknown[] array
    HEADER_V2 = 0x02, // 3 word array
    HEADER_V3 = 0x04, // 4 word array
    HEADER_V4 = 0x08, // 8 word array
};

class Metadata
{
  public:
    Metadata(){};
    virtual ~Metadata(){};

    virtual void dump(std::ostream &outStream);

    bool isTwin() const;
    EDMVersion protoVersion() const;
    bool isOldRecFormat() const;
    HeaderVersion guessFlightHeaderVersion() const;

  public:
    std::string m_tailNum{""};
    ConfigLimits m_configLimits;
    ConfigInfo m_configInfo;
    FuelLimits m_fuelLimits;
    ProtoHeader m_protoHeader;
    TimeStamp m_timeStamp;
};

} // namespace jpi_edm
