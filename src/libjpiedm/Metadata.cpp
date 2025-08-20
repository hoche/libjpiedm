/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */

#include <FileHeaders.hpp>
#include <Metadata.hpp>

namespace jpi_edm {

bool Metadata::IsTwin() const { return m_configInfo.isTwin; }
int Metadata::NumCylinders() const { return m_configInfo.numCylinders; }
bool Metadata::IsGPH() const { return (m_fuelLimits.units == 0); }

EDMVersion Metadata::ProtoVersion() const
{
    // peel out twins first
    if (m_configInfo.edm_model == 760) {
        return EDMVersion::V2;
    }
    if (m_configInfo.edm_model == 960) {
        return EDMVersion::V5;
    }

    if (m_configInfo.edm_model < 900) {
        if (m_protoHeader.value < 2) {
            return EDMVersion::V1; // old 700 or 800
        }
        return EDMVersion::V4; // updated 700 or 800, has proto header
    }

    // 900+. check firmware version
    if (m_configInfo.firmware_version <= 108) {
        return EDMVersion::V1;
    }

    return EDMVersion::V4;
}

bool Metadata::IsOldRecFormat() const { return (ProtoVersion() == EDMVersion::V1 || ProtoVersion() == EDMVersion::V2); }

HeaderVersion Metadata::GuessFlightHeaderVersion() const
{
    if (m_protoHeader.value > 1 || m_configInfo.edm_model >= 900) {
        if (m_configInfo.build_maj > 2010) {
            return HeaderVersion::HEADER_V4;
        }
        if (m_configInfo.build_maj > 880) {
            return HeaderVersion::HEADER_V3;
        }
        return HeaderVersion::HEADER_V2;
    }
    return HeaderVersion::HEADER_V1;
}

void Metadata::dump(std::ostream &outStream)
{
    outStream << "Tailnumber: " << m_tailNum << "\n";
    outStream << "Old Rec Format: " << (IsOldRecFormat() ? "yes" : "no") << "\n";
    m_configLimits.dump(outStream);
    m_configInfo.dump(outStream);
    m_fuelLimits.dump(outStream);
    m_protoHeader.dump(outStream);
    m_timeStamp.dump(outStream);
}

} // namespace jpi_edm
