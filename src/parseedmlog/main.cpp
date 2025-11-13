/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Sample app for using libjpiedm.
 *
 * Shows how to install callbacks and use the library to parse EDM files.
 * This is just an example.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include "KmlExporter.hpp"
#include "MetricUtils.hpp"
#include "libjpiedm/FlightFile.hpp"
#include "libjpiedm/MetricId.hpp"
#include "libjpiedm/ProtocolConstants.hpp"

using namespace jpi_edm;
using parseedmlog::getMetric;

constexpr float kGpsOffset = 241.0f;
static bool g_verbose = false;

void printFlightInfo(std::shared_ptr<jpi_edm::FlightHeader> &hdr, unsigned long stdReqs, unsigned long fastReqs,
                     std::ostream &outStream)
{
    std::tm local;
#ifdef _WIN32
    time_t flightStartTime = _mkgmtime(&hdr->startDate);
    gmtime_s(&local, &flightStartTime);
#else
    time_t flightStartTime = timegm(&hdr->startDate);
    gmtime_r(&flightStartTime, &local);
#endif

    auto min = (fastReqs / MINUTES_PER_HOUR) + (stdReqs * hdr->interval) / MINUTES_PER_HOUR;
    auto hrs = HOURS_ROUNDING_OFFSET + static_cast<float>(min) / MINUTES_PER_HOUR;
    // min = min % 60;

    outStream << "Flt #" << hdr->flight_num << " - ";
    // outStream << hrs << ":" << std::setfill('0') << std::setw(2) << min << " Hours";
    outStream << std::fixed << std::setprecision(2) << hrs << " Hours";
    outStream << " @ " << hdr->interval << " sec";
    outStream << " " << std::put_time(&local, "%m/%d/%Y") << " " << std::put_time(&local, "%T");
    outStream << std::endl;
}

void printLatLng(float measurement, bool isLatitude, std::ostream &outStream)
{
    if (std::fabs(measurement) < 0.5f) {
        outStream << "NA,";
        return;
    }

    int scaledMeasurement = static_cast<int>(std::lround(measurement));
    char hemisphere = isLatitude ? (scaledMeasurement >= 0 ? 'N' : 'S') : (scaledMeasurement >= 0 ? 'E' : 'W');

    int absCoordinate = std::abs(scaledMeasurement);
    int degrees = absCoordinate / GPS_COORD_SCALE_DENOMINATOR;
    int remainder = absCoordinate % GPS_COORD_SCALE_DENOMINATOR;
    int minutes = remainder / GPS_MINUTES_DECIMAL_DIVISOR;
    int hundredths = remainder % GPS_MINUTES_DECIMAL_DIVISOR;

    outStream << hemisphere << degrees << "." << std::setfill('0') << std::setw(2) << minutes << "." << std::setw(2)
              << hundredths << ",";

    outStream << std::setfill(' ');
}

struct FlightRenderRecord {
    std::shared_ptr<jpi_edm::FlightMetricsRecord> record;
    std::tm timestamp{};
};

bool isMetricSupported(const std::shared_ptr<jpi_edm::FlightMetricsRecord> &rec, jpi_edm::MetricId id)
{
    return rec && rec->m_supportedMetrics.count(id) > 0;
}

void writeSeparatedInt(std::ostream &outStream, float value, bool includeSpace = true)
{
    outStream << (includeSpace ? ", " : ",") << static_cast<int>(std::lround(value));
}

void writeSeparatedFloat(std::ostream &outStream, float value, int precision = 1, bool includeLeadingSpace = false)
{
    auto previousPrecision = outStream.precision();
    outStream << (includeLeadingSpace ? ", " : ",");
    outStream << std::fixed << std::setprecision(precision) << value;
    outStream << std::setprecision(previousPrecision);
}

void writeNAField(std::ostream &outStream) { outStream << ",NA"; }

void writeSeparatedFuelUsed(std::ostream &outStream, float value)
{
    if (value < 0.0f) {
        writeNAField(outStream);
        return;
    }
    writeSeparatedFloat(outStream, value, 1);
}

float normalizeHorsepower(float rawValue)
{
    if (rawValue < 0.0f) {
        return rawValue + 240.0f;
    }
    return rawValue;
}

void printSingleEngineFlightRecord(const FlightRenderRecord &entry, bool includeTit1, bool includeTit2,
                                   std::ostream &outStream)
{
    static constexpr jpi_edm::MetricId kEgtIds[] = {jpi_edm::EGT11, jpi_edm::EGT12, jpi_edm::EGT13,
                                                    jpi_edm::EGT14, jpi_edm::EGT15, jpi_edm::EGT16};
    static constexpr jpi_edm::MetricId kChtIds[] = {jpi_edm::CHT11, jpi_edm::CHT12, jpi_edm::CHT13,
                                                    jpi_edm::CHT14, jpi_edm::CHT15, jpi_edm::CHT16};

    const auto &rec = entry.record;
    const auto &timeinfo = entry.timestamp;

    auto previousPrecision = outStream.precision();
    auto previousFlags = outStream.flags();

    outStream.setf(std::ios::fixed, std::ios::floatfield);
    outStream << std::setprecision(0);

    outStream << rec->m_recordSeq - 1 << "," << (timeinfo.tm_mon + 1) << '/' << timeinfo.tm_mday << '/'
              << (timeinfo.tm_year + TM_YEAR_BASE) << "," << std::put_time(&timeinfo, "%T");

    for (auto id : kEgtIds) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, id), false);
    }

    for (auto id : kChtIds) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, id), false);
    }

    if (includeTit1) {
        if (isMetricSupported(rec, jpi_edm::TIT11)) {
            writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::TIT11), false);
        } else {
            writeNAField(outStream);
        }
    }

    if (includeTit2) {
        if (isMetricSupported(rec, jpi_edm::TIT12)) {
            writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::TIT12), false);
        } else {
            writeNAField(outStream);
        }
    }

    if (isMetricSupported(rec, jpi_edm::OAT)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OAT), false);
    } else {
        writeNAField(outStream);
    }

    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::DIF1), false);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::CLD1), false);

    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::MAP1), 1);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::RPM1), false);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::HP1), false);

    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF11), 1);
    if (isMetricSupported(rec, jpi_edm::FF12)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF12), 1);
    } else {
        writeNAField(outStream);
    }
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FP1), 1);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILP1), false);
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::VOLT1), 1);

    if (isMetricSupported(rec, jpi_edm::AMP1)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::AMP1), false);
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::OILT1)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILT1), false);
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::FUSD11)) {
        writeSeparatedFuelUsed(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FUSD11, -1.0f));
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::FUSD12)) {
        writeSeparatedFuelUsed(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FUSD12, -1.0f));
    } else {
        writeNAField(outStream);
    }

    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::RMAIN), 1);
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::LMAIN), 1);
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::LAUX), 1);
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::RAUX), 1);
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::HRS1), 1);

    auto spd = parseedmlog::getMetric(rec->m_metrics, jpi_edm::SPD, -1.0f);
    if (spd == -1.0f) {
        outStream << ",NA";
    } else {
        outStream << "," << (spd + kGpsOffset);
    }

    auto alt = parseedmlog::getMetric(rec->m_metrics, jpi_edm::ALT, -1.0f);
    if (alt == -1.0f) {
        outStream << ",NA,";
    } else {
        outStream << "," << (alt + kGpsOffset) << ",";
    }

    printLatLng(getMetric(rec->m_metrics, LAT), true, outStream);
    printLatLng(getMetric(rec->m_metrics, LNG), false, outStream);

    int markVal = static_cast<int>(getMetric(rec->m_metrics, MARK));
    switch (markVal) {
    case MARK_START:
        outStream << "[";
        break;
    case MARK_END:
        outStream << "]";
        break;
    case MARK_UNKNOWN:
        outStream << "<";
        break;
    }

    outStream << "\n";

    outStream.precision(previousPrecision);
    outStream.flags(previousFlags);
}

void printSingleEngineFlight(const std::vector<FlightRenderRecord> &records,
                             const std::shared_ptr<jpi_edm::Metadata> &metadata, std::ostream &outStream,
                             bool &headerPrinted)
{
    if (records.empty()) {
        return;
    }

    bool includeTit1 = metadata && metadata->m_configInfo.hasTurbo1;
    bool includeTit2 = metadata && metadata->m_configInfo.hasTurbo2;

    if (!headerPrinted) {
        outStream << "INDEX,DATE,TIME,E1,E2,E3,E4,E5,E6,C1,C2,C3,C4,C5,C6";
        if (includeTit1) {
            outStream << ",TIT1";
        }
        if (includeTit2) {
            outStream << ",TIT2";
        }
        outStream << ",OAT,DIF,CLD,MAP,RPM,HP,FF,FF2,FP,OILP,BAT,AMP,OILT"
                  << ",USD,USD2,RFL,LFL,LAUX,RAUX,HRS,SPD,ALT,LAT,LNG,MARK" << "\n";
        headerPrinted = true;
    }

    for (const auto &entry : records) {
        const auto &rec = entry.record;
        const auto &timeinfo = entry.timestamp;

        printSingleEngineFlightRecord(entry, includeTit1, includeTit2, outStream);
    }
}

void printTwinFlightRecord(const FlightRenderRecord &entry, int cylinderCount, std::ostream &outStream)
{
    static constexpr jpi_edm::MetricId kLeftEgtIds[] = {jpi_edm::EGT11, jpi_edm::EGT12, jpi_edm::EGT13,
                                                        jpi_edm::EGT14, jpi_edm::EGT15, jpi_edm::EGT16,
                                                        jpi_edm::EGT17, jpi_edm::EGT18, jpi_edm::EGT19};
    static constexpr jpi_edm::MetricId kLeftChtIds[] = {jpi_edm::CHT11, jpi_edm::CHT12, jpi_edm::CHT13,
                                                        jpi_edm::CHT14, jpi_edm::CHT15, jpi_edm::CHT16,
                                                        jpi_edm::CHT17, jpi_edm::CHT18, jpi_edm::CHT19};
    static constexpr jpi_edm::MetricId kRightEgtIds[] = {jpi_edm::EGT21, jpi_edm::EGT22, jpi_edm::EGT23,
                                                         jpi_edm::EGT24, jpi_edm::EGT25, jpi_edm::EGT26,
                                                         jpi_edm::EGT27, jpi_edm::EGT28, jpi_edm::EGT29};
    static constexpr jpi_edm::MetricId kRightChtIds[] = {jpi_edm::CHT21, jpi_edm::CHT22, jpi_edm::CHT23,
                                                         jpi_edm::CHT24, jpi_edm::CHT25, jpi_edm::CHT26,
                                                         jpi_edm::CHT27, jpi_edm::CHT28, jpi_edm::CHT29};

    const auto &rec = entry.record;
    const auto &timeinfo = entry.timestamp;

    auto previousPrecision = outStream.precision();
    auto previousFlags = outStream.flags();

    outStream.setf(std::ios::fixed, std::ios::floatfield);
    outStream << std::setprecision(0);

    outStream << rec->m_recordSeq - 1 << "," << (timeinfo.tm_mon + 1) << '/' << timeinfo.tm_mday << '/'
              << (timeinfo.tm_year + TM_YEAR_BASE) << "," << std::put_time(&timeinfo, "%T");

    int leftEgtCount = std::min(cylinderCount, static_cast<int>(sizeof(kLeftEgtIds) / sizeof(kLeftEgtIds[0])));
    for (int i = 0; i < leftEgtCount; ++i) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, kLeftEgtIds[i]));
    }

    int leftChtCount = std::min(cylinderCount, static_cast<int>(sizeof(kLeftChtIds) / sizeof(kLeftChtIds[0])));
    for (int i = 0; i < leftChtCount; ++i) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, kLeftChtIds[i]));
    }

    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OAT));
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::DIF1));
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::CLD1));
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::MAP1), 1);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::RPM1));
    writeSeparatedInt(outStream, normalizeHorsepower(parseedmlog::getMetric(rec->m_metrics, jpi_edm::HP1)));
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF11), 1);

    if (isMetricSupported(rec, jpi_edm::FF12)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF12), 1);
    } else {
        writeNAField(outStream);
    }

    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FP1), 1);
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILP1));
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::VOLT1), 1);

    writeNAField(outStream);

    if (isMetricSupported(rec, jpi_edm::AMP1)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::AMP1));
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::AMP2)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::AMP2));
    } else {
        writeNAField(outStream);
    }

    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILT1));
    writeSeparatedFuelUsed(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FUSD11, -1.0f));
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::HRS1), 1);

    int rightEgtCount = std::min(cylinderCount, static_cast<int>(sizeof(kRightEgtIds) / sizeof(kRightEgtIds[0])));
    for (int i = 0; i < rightEgtCount; ++i) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, kRightEgtIds[i]));
    }

    int rightChtCount = std::min(cylinderCount, static_cast<int>(sizeof(kRightChtIds) / sizeof(kRightChtIds[0])));
    for (int i = 0; i < rightChtCount; ++i) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, kRightChtIds[i]));
    }

    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::DIF2));
    writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::CLD2));
    writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::MAP2), 1);

    if (isMetricSupported(rec, jpi_edm::RPM2)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::RPM2));
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::HP2)) {
        writeSeparatedInt(outStream, normalizeHorsepower(parseedmlog::getMetric(rec->m_metrics, jpi_edm::HP2)));
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::FF21)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF21), 1);
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::FF22)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FF22), 1);
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::FP2)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FP2), 1);
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::OILP2)) {
        writeSeparatedInt(outStream, 10.0f * parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILP2));
    } else {
        writeNAField(outStream);
    }

    if (isMetricSupported(rec, jpi_edm::OILT2)) {
        writeSeparatedInt(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::OILT2));
    } else {
        writeNAField(outStream);
    }

    writeSeparatedFuelUsed(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::FUSD21, -1.0f));

    if (isMetricSupported(rec, jpi_edm::HRS2)) {
        writeSeparatedFloat(outStream, parseedmlog::getMetric(rec->m_metrics, jpi_edm::HRS2), 1);
    } else {
        writeNAField(outStream);
    }

    auto spd = parseedmlog::getMetric(rec->m_metrics, jpi_edm::SPD, -1.0f);
    if (spd == -1.0f) {
        writeNAField(outStream);
    } else {
        outStream << "," << (spd + kGpsOffset);
    }

    auto alt = parseedmlog::getMetric(rec->m_metrics, jpi_edm::ALT, -1.0f);
    if (alt == -1.0f) {
        outStream << ",NA";
    } else {
        outStream << "," << (alt + kGpsOffset);
    }
    outStream << ",NA,NA,";

    int markVal = static_cast<int>(parseedmlog::getMetric(rec->m_metrics, jpi_edm::MARK));
    switch (markVal) {
    case MARK_START:
        outStream << "[";
        break;
    case MARK_END:
        outStream << "]";
        break;
    case MARK_UNKNOWN:
        outStream << "<";
        break;
    default:
        outStream << "";
        break;
    }

    outStream << "\r\n";

    outStream.precision(previousPrecision);
    outStream.flags(previousFlags);
}

void printTwinFlight(const std::vector<FlightRenderRecord> &records, const std::shared_ptr<jpi_edm::Metadata> &metadata,
                     float leftTachStart, float leftTachEnd, float rightTachStart, float rightTachEnd,
                     std::ostream &outStream, bool &headerPrinted)
{
    if (records.empty()) {
        return;
    }

    auto previousPrecision = outStream.precision();
    auto previousFlags = outStream.flags();

    outStream.setf(std::ios::fixed, std::ios::floatfield);

    int cylinderCount = metadata ? metadata->NumCylinders() : jpi_edm::SINGLE_ENGINE_CYLINDER_COUNT;
    if (cylinderCount <= 0) {
        cylinderCount = jpi_edm::SINGLE_ENGINE_CYLINDER_COUNT;
    }
    cylinderCount = std::min(cylinderCount, 9);

    if (!headerPrinted) {
        outStream << "INDEX,DATE,TIME";
        for (int i = 0; i < cylinderCount; ++i) {
            outStream << ",LE" << (i + 1);
        }
        for (int i = 0; i < cylinderCount; ++i) {
            outStream << ",LC" << (i + 1);
        }
        outStream << ",OAT,LDIF,LCLD,LMAP,LRPM,LHP,LFF,LFF2,LFP,LOILP,BAT,BAT2,AMP,AMP2,LOILT,LUSD,LHRS";
        for (int i = 0; i < cylinderCount; ++i) {
            outStream << ",RE" << (i + 1);
        }
        for (int i = 0; i < cylinderCount; ++i) {
            outStream << ",RC" << (i + 1);
        }
        outStream << ",RDIF,RCLD,RMAP,RRPM,RHP,RFF,RFF2,RFP,ROILP,ROILT,RUSD,RHRS,SPD,ALT,LAT,LNG,MARK" << "\r\n";
        headerPrinted = true;
    }

    outStream << std::setprecision(1);
    if (!std::isnan(leftTachStart) && !std::isnan(leftTachEnd)) {
        outStream << "Left Engine - Tach Start = " << leftTachStart << ",Tach End = " << leftTachEnd
                  << ",Tach Duration = " << (leftTachEnd - leftTachStart) << "\r\n";
    }
    if (!std::isnan(rightTachStart) && !std::isnan(rightTachEnd)) {
        outStream << "Right Engine - Tach Start = " << rightTachStart << " ,Tach End = " << rightTachEnd
                  << ",Tach Duration = " << (rightTachEnd - rightTachStart) << "\r\n";
    }
    outStream << std::setprecision(0);

    for (const auto &entry : records) {
        printTwinFlightRecord(entry, cylinderCount, outStream);
    }

    outStream.precision(previousPrecision);
    outStream.flags(previousFlags);
}

void printFlightData(std::istream &stream, std::optional<int> flightId, std::ostream &outStream)
{
    jpi_edm::FlightFile ff;
    std::shared_ptr<jpi_edm::FlightHeader> hdr;
    std::shared_ptr<jpi_edm::Metadata> metadata;
    time_t recordTime{};
    std::vector<FlightRenderRecord> currentFlightRecords;
    float leftTachStart = std::numeric_limits<float>::quiet_NaN();
    float leftTachEnd = std::numeric_limits<float>::quiet_NaN();
    float rightTachStart = std::numeric_limits<float>::quiet_NaN();
    float rightTachEnd = std::numeric_limits<float>::quiet_NaN();
    bool headerPrinted = false;

    if (flightId.has_value()) {
        try {
            jpi_edm::FlightFile flightDetector;
            auto flights = flightDetector.detectFlights(stream);
            bool found = std::any_of(flights.begin(), flights.end(),
                                     [&](const auto &info) { return info.flightNumber == flightId.value(); });
            stream.clear();
            stream.seekg(0);
            if (!found) {
                outStream << "Flight #" << flightId.value() << " not found in file" << std::endl;
                return;
            }
        } catch (const std::exception &ex) {
            stream.clear();
            stream.seekg(0);
            std::cerr << "Error detecting flights: " << ex.what() << std::endl;
            return;
        }
    } else {
        stream.clear();
        stream.seekg(0);
    }

    ff.setMetadataCompletionCb([&](std::shared_ptr<jpi_edm::Metadata> md) {
        metadata = md;
        if (g_verbose) {
            md->dump(outStream);
        }
    });

    ff.setFlightHeaderCompletionCb([&hdr, &recordTime, &outStream, &currentFlightRecords, &leftTachStart, &leftTachEnd,
                                    &rightTachStart, &rightTachEnd](std::shared_ptr<jpi_edm::FlightHeader> fh) {
        hdr = fh;

        std::tm local;
#ifdef _WIN32
        recordTime = _mkgmtime(&hdr->startDate);
        gmtime_s(&local, &recordTime);
#else
        recordTime = timegm(&hdr->startDate);
        gmtime_r(&recordTime, &local);
#endif

        currentFlightRecords.clear();
        leftTachStart = leftTachEnd = std::numeric_limits<float>::quiet_NaN();
        rightTachStart = rightTachEnd = std::numeric_limits<float>::quiet_NaN();

        if (g_verbose) {
            outStream << "Flt #" << hdr->flight_num << "\n";
            outStream << "Interval: " << hdr->interval << " sec\n";
            outStream << "Flight Start Time: " << std::put_time(&local, "%m/%d/%Y") << " "
                      << std::put_time(&local, "%T") << "\n";
        }
    });

    ff.setFlightRecordCompletionCb([&](std::shared_ptr<jpi_edm::FlightMetricsRecord> rec) {
        if (!hdr) {
            std::cerr << "Warning: Flight record callback invoked without flight header" << std::endl;
            return;
        }

        std::tm timeinfo;
#ifdef _WIN32
        gmtime_s(&timeinfo, &recordTime);
#else
        gmtime_r(&recordTime, &timeinfo);
#endif

        currentFlightRecords.push_back(FlightRenderRecord{rec, timeinfo});

        if (isMetricSupported(rec, jpi_edm::HRS1)) {
            float leftHrs = parseedmlog::getMetric(rec->m_metrics, jpi_edm::HRS1);
            if (std::isnan(leftTachStart)) {
                leftTachStart = leftHrs;
            }
            leftTachEnd = leftHrs;
        }

        if (isMetricSupported(rec, jpi_edm::HRS2)) {
            float rightHrs = parseedmlog::getMetric(rec->m_metrics, jpi_edm::HRS2);
            if (std::isnan(rightTachStart)) {
                rightTachStart = rightHrs;
            }
            rightTachEnd = rightHrs;
        }

        rec->m_isFast ? ++recordTime : recordTime += hdr->interval;
    });

    ff.setFlightCompletionCb([&](unsigned long /*stdReqs*/, unsigned long /*fastReqs*/) {
        if (currentFlightRecords.empty()) {
            return;
        }

        if (metadata && metadata->IsTwin()) {
            printTwinFlight(currentFlightRecords, metadata, leftTachStart, leftTachEnd, rightTachStart, rightTachEnd,
                            outStream, headerPrinted);
        } else {
            printSingleEngineFlight(currentFlightRecords, metadata, outStream, headerPrinted);
        }

        currentFlightRecords.clear();
    });

    // Now do the work - use the new single-flight API if a specific flight is requested
    if (flightId.has_value()) {
        ff.processFile(stream, flightId.value());
    } else {
        ff.processFile(stream);
    }
}

void printFlightList(std::istream &stream, std::ostream &outStream)
{
    jpi_edm::FlightFile ff;

    try {
        // Use the efficient detectFlights() to get flight information without parsing flight data
        std::shared_ptr<jpi_edm::Metadata> metadata;
        auto flights = ff.detectFlights(stream, metadata);

        if (flights.empty()) {
            outStream << "No flights found in file\n";
            return;
        }

        // For each detected flight, we need to parse it to get full details for printFlightInfo
        // Reset the stream to process flights one by one
        stream.clear();
        stream.seekg(0);

        std::shared_ptr<jpi_edm::FlightHeader> hdr;
        ff.setFlightHeaderCompletionCb([&hdr](std::shared_ptr<jpi_edm::FlightHeader> fh) { hdr = fh; });
        ff.setFlightCompletionCb([&hdr, &outStream](unsigned long stdReqs, unsigned long fastReqs) {
            printFlightInfo(hdr, stdReqs, fastReqs, outStream);
        });

        // Parse all flights
        try {
            ff.processFile(stream);
        } catch (const std::exception &ex) {
            std::cerr << "Warning: Failed to parse flights with full detail (" << ex.what()
                      << "). Falling back to header-only listing.\n";
            for (const auto &flight : flights) {
                outStream << "Flt #" << flight.flightNumber << " - approx " << flight.recordCount
                          << " records (details unavailable)\n";
            }
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error listing flights: " << ex.what() << "\n";
    }
}

void processFiles(std::vector<std::string> &filelist, std::optional<int> flightId, bool onlyListFlights,
                  const std::string &outputFile, const std::string &kmlOutput)
{
    bool exportKml = !kmlOutput.empty();

    for (auto &&filename : filelist) {
        if (filelist.size() > 1) {
            std::cout << filename << std::endl;
        }

        std::filesystem::path inputFilePath{filename};
        std::error_code ec;

        auto length = std::filesystem::file_size(inputFilePath, ec);
        if (ec.value() != 0) {
            std::cerr << "No such file\n";
            return;
        }
        if (length == 0) {
            std::cerr << "Empty file\n";
            return;
        }

        std::ifstream inStream(filename, std::ios_base::binary);
        if (!inStream.is_open()) {
            std::cerr << "Couldn't open file\n";
            return;
        }

        if (exportKml) {
            if (!flightId.has_value()) {
                std::cerr << "KML/KMZ export requires selecting a specific flight with -f\n";
                return;
            }

            auto trackData = parseedmlog::kml::collectFlightTrackData(inStream, flightId.value());
            if (!trackData.has_value()) {
                return;
            }

            try {
                parseedmlog::kml::writeKmlOrKmz(kmlOutput, trackData.value(), inputFilePath.filename().string());
                if (g_verbose) {
                    std::cout << "Wrote " << kmlOutput << " for flight #" << flightId.value() << "\n";
                }
            } catch (const std::exception &ex) {
                std::cerr << ex.what() << "\n";
                return;
            }

            inStream.clear();
            inStream.seekg(0);
        }

        std::ofstream outFileStream;
        if (!outputFile.empty()) {
            outFileStream.open(outputFile, std::ios::out | std::ios::trunc);
            if (!outFileStream.is_open()) {
                std::cerr << "Couldn't open output file\n";
                return;
            }
        }
        std::ostream &outStream = (outputFile.empty() ? std::cout : outFileStream);

        if (onlyListFlights) {
            printFlightList(inStream, outStream);
        } else {
            printFlightData(inStream, flightId, outStream);
        }
    }
}

void showHelp(char *progName)
{
    std::cout << "Usage: " << progName << "[options] jpifile..." << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "    -h              print this help" << std::endl;
    std::cout << "    -f <flightno>   only output a specific flight number" << std::endl;
    std::cout << "    -l              list flights" << std::endl;
    std::cout << "    -o <filename>   output to a file" << std::endl;
    std::cout << "    -k <filename>   export flight path to KML or KMZ (requires -f)" << std::endl;
    std::cout << "    -v              verbose output of the flight header" << std::endl;
}

int main(int argc, char *argv[])
{
    bool onlyListFlights{false};
    std::vector<std::string> filelist{};
    std::string outputFile{};
    std::string kmlOutput{};
    std::optional<int> flightId; // std::nullopt means all flights

    int c;
    while ((c = getopt(argc, argv, "hf:lo:vk:")) != -1) {
        switch (c) {
        case 'h':
            showHelp(argv[0]);
            return 0;
        case 'f':
            if (!optarg) {
                showHelp(argv[0]);
                return 0;
            }
            try {
                size_t idx = 0;
                int flightNum = std::stoi(optarg, &idx);
                if (idx != strlen(optarg)) {
                    std::cerr << "Error: Invalid flight number format (contains non-numeric characters): " << optarg
                              << std::endl;
                    return 1;
                }
                if (flightNum < 0) {
                    std::cerr << "Error: Flight number must be non-negative" << std::endl;
                    return 1;
                }
                flightId = flightNum;
            } catch (const std::invalid_argument &) {
                std::cerr << "Error: Flight number must be a valid integer: " << optarg << std::endl;
                return 1;
            } catch (const std::out_of_range &) {
                std::cerr << "Error: Flight number out of range: " << optarg << std::endl;
                return 1;
            }
            break;
        case 'l':
            onlyListFlights = true;
            break;
        case 'o':
            outputFile = optarg;
            break;
        case 'k':
            kmlOutput = optarg ? optarg : "";
            break;
        case 'v':
            g_verbose = true;
            break;
        }
    }

    if (optind == argc) {
        showHelp(argv[0]);
        return 0;
    }

    if (!kmlOutput.empty() && onlyListFlights) {
        std::cerr << "Error: KML/KMZ export (-k) cannot be combined with -l (list flights)\n";
        return 1;
    }

    if (!kmlOutput.empty() && !flightId.has_value()) {
        std::cerr << "Error: KML/KMZ export (-k) requires specifying a flight with -f\n";
        return 1;
    }

    for (int i = optind; i < argc; ++i) {
        filelist.push_back(argv[i]);
    }

    if (!kmlOutput.empty() && filelist.size() != 1) {
        std::cerr << "Error: KML/KMZ export supports exactly one input file\n";
        return 1;
    }

    processFiles(filelist, flightId, onlyListFlights, outputFile, kmlOutput);
    return 0;
}
