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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include "FileHeaders.hpp"
#include "Flight.hpp"
#include "FlightFile.hpp"
#include "MetricId.hpp"
#include "ProtocolConstants.hpp"

using namespace jpi_edm;

float getMetric(const std::map<MetricId, float> &metrics, MetricId id, float defaultValue = 0.0f);

namespace {

constexpr float kGpsOffset = 241.0f;
constexpr double kFeetToMeters = 0.3048;
constexpr const char *kKmzDefaultEntryName = "doc.kml";

struct FlightTrackPoint {
    std::time_t timestamp{};
    double latitude{};
    double longitude{};
    std::optional<double> altitudeFeet{};
    std::optional<double> speed{};
};

struct FlightTrackData {
    std::shared_ptr<FlightHeader> header;
    std::vector<FlightTrackPoint> samples;
};

std::optional<double> decodeGpsCoordinate(float measurement)
{
    if (std::fabs(measurement) < 0.5f) {
        return std::nullopt;
    }

    int scaledMeasurement = static_cast<int>(std::lround(measurement));
    bool isNegative = scaledMeasurement < 0;
    int absCoordinate = std::abs(scaledMeasurement);
    int degrees = absCoordinate / GPS_COORD_SCALE_DENOMINATOR;
    int remainder = absCoordinate % GPS_COORD_SCALE_DENOMINATOR;
    int minutes = remainder / GPS_MINUTES_DECIMAL_DIVISOR;
    int hundredths = remainder % GPS_MINUTES_DECIMAL_DIVISOR;

    double decimalMinutes = static_cast<double>(minutes) +
                            static_cast<double>(hundredths) / static_cast<double>(GPS_MINUTES_DECIMAL_DIVISOR);
    double decimalDegrees = static_cast<double>(degrees) + decimalMinutes / 60.0;

    return isNegative ? -decimalDegrees : decimalDegrees;
}

std::optional<double> decodeAltitude(float measurement)
{
    if (measurement == -1.0f) {
        return std::nullopt;
    }
    return static_cast<double>(measurement + kGpsOffset);
}

std::optional<double> decodeSpeed(float measurement)
{
    if (measurement == -1.0f) {
        return std::nullopt;
    }
    return static_cast<double>(measurement + kGpsOffset);
}

std::string formatIso8601(std::time_t timestamp)
{
    std::tm timeinfo;
#ifdef _WIN32
    gmtime_s(&timeinfo, &timestamp);
#else
    gmtime_r(&timestamp, &timeinfo);
#endif
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string formatHeaderStartTime(const std::shared_ptr<FlightHeader> &header)
{
    if (!header) {
        return {};
    }
    std::tm local = header->startDate;
    std::ostringstream oss;
    oss << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool endsWithIgnoreCase(const std::string &value, const std::string &suffix)
{
    if (suffix.size() > value.size()) {
        return false;
    }
    auto valueIt = value.end() - static_cast<std::ptrdiff_t>(suffix.size());
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(valueIt[i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

uint32_t crc32(const std::string &data)
{
    uint32_t crc = 0xFFFFFFFF;
    for (unsigned char ch : data) {
        crc ^= ch;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

void toDosDateTime(std::time_t t, uint16_t &dosDate, uint16_t &dosTime)
{
    std::tm tmStruct;
#ifdef _WIN32
    gmtime_s(&tmStruct, &t);
#else
    gmtime_r(&t, &tmStruct);
#endif

    int year = tmStruct.tm_year + 1900;
    if (year < 1980) {
        year = 1980;
        tmStruct.tm_mon = 0;
        tmStruct.tm_mday = 1;
        tmStruct.tm_hour = 0;
        tmStruct.tm_min = 0;
        tmStruct.tm_sec = 0;
    }

    dosDate = static_cast<uint16_t>(((year - 1980) << 9U) | ((tmStruct.tm_mon + 1) << 5U) | tmStruct.tm_mday);
    dosTime = static_cast<uint16_t>((tmStruct.tm_hour << 11U) | (tmStruct.tm_min << 5U) | (tmStruct.tm_sec / 2));
}

void appendUint16LE(std::vector<std::uint8_t> &buffer, std::uint16_t value)
{
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendUint32LE(std::vector<std::uint8_t> &buffer, std::uint32_t value)
{
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

std::vector<std::uint8_t> buildKmzArchive(const std::string &kmlContent, const std::string &entryName)
{
    std::vector<std::uint8_t> buffer;
    buffer.reserve(kmlContent.size() + 256);

    std::uint32_t crc = crc32(kmlContent);
    std::uint32_t size = static_cast<std::uint32_t>(kmlContent.size());
    std::time_t now = std::time(nullptr);
    uint16_t dosDate = 0;
    uint16_t dosTime = 0;
    toDosDateTime(now, dosDate, dosTime);

    std::uint32_t localHeaderOffset = static_cast<std::uint32_t>(buffer.size());

    // Local file header
    appendUint32LE(buffer, 0x04034B50U);
    appendUint16LE(buffer, 20U); // version needed to extract
    appendUint16LE(buffer, 0U);  // general purpose bit flag
    appendUint16LE(buffer, 0U);  // compression method (store)
    appendUint16LE(buffer, dosTime);
    appendUint16LE(buffer, dosDate);
    appendUint32LE(buffer, crc);
    appendUint32LE(buffer, size);
    appendUint32LE(buffer, size);
    appendUint16LE(buffer, static_cast<std::uint16_t>(entryName.size()));
    appendUint16LE(buffer, 0U); // extra field length

    buffer.insert(buffer.end(), entryName.begin(), entryName.end());
    buffer.insert(buffer.end(), kmlContent.begin(), kmlContent.end());

    std::uint32_t centralDirOffset = static_cast<std::uint32_t>(buffer.size());

    // Central directory header
    appendUint32LE(buffer, 0x02014B50U);
    appendUint16LE(buffer, 20U); // version made by
    appendUint16LE(buffer, 20U); // version needed to extract
    appendUint16LE(buffer, 0U);  // general purpose bit flag
    appendUint16LE(buffer, 0U);  // compression method
    appendUint16LE(buffer, dosTime);
    appendUint16LE(buffer, dosDate);
    appendUint32LE(buffer, crc);
    appendUint32LE(buffer, size);
    appendUint32LE(buffer, size);
    appendUint16LE(buffer, static_cast<std::uint16_t>(entryName.size()));
    appendUint16LE(buffer, 0U); // extra field length
    appendUint16LE(buffer, 0U); // file comment length
    appendUint16LE(buffer, 0U); // disk number start
    appendUint16LE(buffer, 0U); // internal file attributes
    appendUint32LE(buffer, 0U); // external file attributes
    appendUint32LE(buffer, localHeaderOffset);
    buffer.insert(buffer.end(), entryName.begin(), entryName.end());

    std::uint32_t centralDirSize = static_cast<std::uint32_t>(buffer.size()) - centralDirOffset;

    // End of central directory record
    appendUint32LE(buffer, 0x06054B50U);
    appendUint16LE(buffer, 0U); // number of this disk
    appendUint16LE(buffer, 0U); // number of the disk with the start of the central directory
    appendUint16LE(buffer, 1U); // total entries on this disk
    appendUint16LE(buffer, 1U); // total entries overall
    appendUint32LE(buffer, centralDirSize);
    appendUint32LE(buffer, centralDirOffset);
    appendUint16LE(buffer, 0U); // comment length

    return buffer;
}

void writeKmlOrKmz(const std::filesystem::path &outputPath, const std::string &kmlContent)
{
    std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            std::ostringstream oss;
            oss << "Failed to create directories for " << outputPath << ": " << ec.message();
            throw std::runtime_error(oss.str());
        }
    }

    if (endsWithIgnoreCase(outputPath.string(), ".kmz")) {
        auto archive = buildKmzArchive(kmlContent, kKmzDefaultEntryName);
        std::ofstream out(outputPath, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            std::ostringstream oss;
            oss << "Couldn't open KMZ output file: " << outputPath;
            throw std::runtime_error(oss.str());
        }
        out.write(reinterpret_cast<const char *>(archive.data()), static_cast<std::streamsize>(archive.size()));
        if (!out) {
            std::ostringstream oss;
            oss << "Failed writing KMZ file: " << outputPath;
            throw std::runtime_error(oss.str());
        }
    } else {
        std::ofstream out(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            std::ostringstream oss;
            oss << "Couldn't open KML output file: " << outputPath;
            throw std::runtime_error(oss.str());
        }
        out << kmlContent;
        if (!out) {
            std::ostringstream oss;
            oss << "Failed writing KML file: " << outputPath;
            throw std::runtime_error(oss.str());
        }
    }
}

std::string buildKmlDocument(const FlightTrackData &trackData, const std::string &sourceName)
{
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n";
    oss << "  <Document>\n";

    std::string startTime = formatHeaderStartTime(trackData.header);
    oss << "    <name>Flight #" << (trackData.header ? trackData.header->flight_num : 0);
    if (!startTime.empty()) {
        oss << " - " << startTime;
    }
    oss << "</name>\n";
    oss << "    <open>1</open>\n";
    oss << "    <Snippet>Source: " << sourceName << "</Snippet>\n";

    oss << "    <Style id=\"flight-path-style\">\n";
    oss << "      <LineStyle>\n";
    oss << "        <color>ff0000ff</color>\n";
    oss << "        <width>3</width>\n";
    oss << "      </LineStyle>\n";
    oss << "      <PolyStyle>\n";
    oss << "        <color>330000ff</color>\n";
    oss << "      </PolyStyle>\n";
    oss << "    </Style>\n";

    oss << "    <Schema id=\"FlightSample\">\n";
    oss << "      <gx:SimpleArrayField name=\"speed\" type=\"float\">\n";
    oss << "        <displayName>Speed (units per JPI log)</displayName>\n";
    oss << "      </gx:SimpleArrayField>\n";
    oss << "      <gx:SimpleArrayField name=\"altitude_ft\" type=\"float\">\n";
    oss << "        <displayName>Altitude (ft)</displayName>\n";
    oss << "      </gx:SimpleArrayField>\n";
    oss << "    </Schema>\n";

    oss << "    <Placemark>\n";
    oss << "      <name>Flight Path</name>\n";
    oss << "      <styleUrl>#flight-path-style</styleUrl>\n";
    oss << "      <gx:Track>\n";
    oss << "        <altitudeMode>absolute</altitudeMode>\n";

    oss << std::setprecision(0);
    for (const auto &sample : trackData.samples) {
        oss << "        <when>" << formatIso8601(sample.timestamp) << "</when>\n";
    }

    oss << std::fixed << std::setprecision(6);
    for (const auto &sample : trackData.samples) {
        double altitudeMeters = sample.altitudeFeet.has_value() ? sample.altitudeFeet.value() * kFeetToMeters : 0.0;
        oss << "        <gx:coord>" << std::setprecision(8) << sample.longitude << " " << sample.latitude << " "
            << std::setprecision(3) << altitudeMeters << "</gx:coord>\n";
        oss << std::fixed << std::setprecision(6);
    }

    oss << "        <ExtendedData>\n";
    oss << "          <SchemaData schemaUrl=\"#FlightSample\">\n";

    oss << "            <gx:SimpleArrayData name=\"speed\">\n";
    oss << std::setprecision(1);
    for (const auto &sample : trackData.samples) {
        if (sample.speed.has_value()) {
            oss << "              <gx:value>" << sample.speed.value() << "</gx:value>\n";
        } else {
            oss << "              <gx:value>NaN</gx:value>\n";
        }
    }
    oss << "            </gx:SimpleArrayData>\n";

    oss << "            <gx:SimpleArrayData name=\"altitude_ft\">\n";
    for (const auto &sample : trackData.samples) {
        if (sample.altitudeFeet.has_value()) {
            oss << "              <gx:value>" << sample.altitudeFeet.value() << "</gx:value>\n";
        } else {
            oss << "              <gx:value>NaN</gx:value>\n";
        }
    }
    oss << "            </gx:SimpleArrayData>\n";

    oss << "          </SchemaData>\n";
    oss << "        </ExtendedData>\n";
    oss << "      </gx:Track>\n";
    oss << "    </Placemark>\n";
    oss << "  </Document>\n";
    oss << "</kml>\n";

    return oss.str();
}

std::optional<FlightTrackData> collectFlightTrackData(std::istream &stream, int flightId)
{
    stream.clear();
    stream.seekg(0);

    jpi_edm::FlightFile ff;
    FlightTrackData trackData;
    time_t recordTime = 0;

    ff.setMetadataCompletionCb([&](std::shared_ptr<jpi_edm::Metadata>) {
        // Metadata is not currently used for KML export, but hook retained for future enhancements.
    });

    ff.setFlightHeaderCompletionCb([&](std::shared_ptr<jpi_edm::FlightHeader> header) {
        trackData.header = header;
#ifdef _WIN32
        recordTime = _mkgmtime(&header->startDate);
#else
        recordTime = timegm(&header->startDate);
#endif
    });

    ff.setFlightRecordCompletionCb([&](std::shared_ptr<jpi_edm::FlightMetricsRecord> record) {
        if (!trackData.header) {
            std::cerr << "Warning: Flight record callback invoked without flight header\n";
            return;
        }

        auto lat = decodeGpsCoordinate(getMetric(record->m_metrics, LAT));
        auto lng = decodeGpsCoordinate(getMetric(record->m_metrics, LNG));
        if (lat.has_value() && lng.has_value()) {
            FlightTrackPoint point;
            point.timestamp = recordTime;
            point.latitude = lat.value();
            point.longitude = lng.value();
            point.altitudeFeet = decodeAltitude(getMetric(record->m_metrics, ALT, -1.0f));
            point.speed = decodeSpeed(getMetric(record->m_metrics, SPD, -1.0f));
            trackData.samples.emplace_back(std::move(point));
        }

        if (record->m_isFast) {
            ++recordTime;
        } else {
            recordTime += trackData.header->interval;
        }
    });

    try {
        ff.processFile(stream, flightId);
    } catch (const std::exception &ex) {
        std::cerr << "Failed to parse flight #" << flightId << " for KML export: " << ex.what() << "\n";
        return std::nullopt;
    }

    if (!trackData.header) {
        std::cerr << "Flight header unavailable for flight #" << flightId << "\n";
        return std::nullopt;
    }

    if (trackData.samples.empty()) {
        std::cerr << "Flight #" << flightId << " contains no GPS samples suitable for KML export\n";
        return std::nullopt;
    }

    return trackData;
}

} // namespace

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

// Safe metric accessor with default value
inline float getMetric(const std::map<MetricId, float> &metrics, MetricId id, float defaultValue)
{
    auto it = metrics.find(id);
    return (it != metrics.end()) ? it->second : defaultValue;
}

void printFlightMetricsRecord(const std::shared_ptr<jpi_edm::FlightMetricsRecord> &rec, bool includeTit1,
                              bool includeTit2, std::ostream &outStream)
{
    outStream << std::fixed << std::setprecision(0);

    outStream << getMetric(rec->m_metrics, EGT11) << ",";
    outStream << getMetric(rec->m_metrics, EGT12) << ",";
    outStream << getMetric(rec->m_metrics, EGT13) << ",";
    outStream << getMetric(rec->m_metrics, EGT14) << ",";
    outStream << getMetric(rec->m_metrics, EGT15) << ",";
    outStream << getMetric(rec->m_metrics, EGT16) << ",";

    outStream << getMetric(rec->m_metrics, CHT11) << ",";
    outStream << getMetric(rec->m_metrics, CHT12) << ",";
    outStream << getMetric(rec->m_metrics, CHT13) << ",";
    outStream << getMetric(rec->m_metrics, CHT14) << ",";
    outStream << getMetric(rec->m_metrics, CHT15) << ",";
    outStream << getMetric(rec->m_metrics, CHT16) << ",";

    if (includeTit1) {
        outStream << getMetric(rec->m_metrics, TIT11) << ",";
    }
    if (includeTit2) {
        outStream << getMetric(rec->m_metrics, TIT12) << ",";
    }

    outStream << getMetric(rec->m_metrics, OAT) << ",";
    outStream << getMetric(rec->m_metrics, DIF1) << ",";
    outStream << getMetric(rec->m_metrics, CLD1) << ",";
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, MAP1) << "," << std::setprecision(0);
    outStream << getMetric(rec->m_metrics, RPM1) << ",";
    outStream << getMetric(rec->m_metrics, HP1) << ",";

    outStream << std::setprecision(1) << getMetric(rec->m_metrics, FF11) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, FF12) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, FP1) << "," << std::setprecision(0);
    outStream << getMetric(rec->m_metrics, OILP1) << ",";
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, VOLT1) << "," << std::setprecision(0);
    outStream << getMetric(rec->m_metrics, AMP1) << ",";

    outStream << getMetric(rec->m_metrics, OILT1) << ",";
    {
        auto usd = getMetric(rec->m_metrics, FUSD11, -1.0f);
        if (usd < 0.0f) {
            outStream << "NA,";
        } else {
            outStream << std::setprecision(1) << usd << ",";
        }
        outStream << std::setprecision(0);
    }

    {
        auto usd = getMetric(rec->m_metrics, FUSD12, -1.0f);
        if (usd < 0.0f) {
            outStream << "NA,";
        } else {
            outStream << std::setprecision(1) << usd << ",";
        }
        outStream << std::setprecision(0);
    }

    outStream << std::setprecision(1) << getMetric(rec->m_metrics, RMAIN) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, LMAIN) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, LAUX) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, RAUX) << "," << std::setprecision(0);

    outStream << std::setprecision(1) << getMetric(rec->m_metrics, HRS1) << "," << std::setprecision(0);

    auto spd = getMetric(rec->m_metrics, SPD, -1.0f);
    if (spd == -1.0f) {
        outStream << "NA,";
    } else {
        outStream << (spd + kGpsOffset) << ",";
    }

    auto alt = getMetric(rec->m_metrics, ALT, -1.0f);
    if (alt == -1.0f) {
        outStream << "NA,";
    } else {
        outStream << (alt + kGpsOffset) << ",";
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
}

void printFlightData(std::istream &stream, std::optional<int> flightId, std::ostream &outStream)
{
    jpi_edm::FlightFile ff;
    std::shared_ptr<jpi_edm::FlightHeader> hdr;
    std::shared_ptr<jpi_edm::Metadata> metadata;
    time_t recordTime;
    bool headerPrinted = false;
    bool includeTit1 = false;
    bool includeTit2 = false;

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

    ff.setFlightHeaderCompletionCb([&hdr, &recordTime, &outStream](std::shared_ptr<jpi_edm::FlightHeader> fh) {
        hdr = fh;

        std::tm local;
#ifdef _WIN32
        recordTime = _mkgmtime(&hdr->startDate);
        gmtime_s(&local, &recordTime);
#else
        recordTime = timegm(&hdr->startDate);
        gmtime_r(&recordTime, &local);
#endif

        if (g_verbose) {
            outStream << "Flt #" << hdr->flight_num << "\n";
            outStream << "Interval: " << hdr->interval << " sec\n";
            outStream << "Flight Start Time: " << std::put_time(&local, "%m/%d/%Y") << " "
                      << std::put_time(&local, "%T") << "\n";
        }
    });

    ff.setFlightRecordCompletionCb([&](std::shared_ptr<jpi_edm::FlightMetricsRecord> rec) {
        // Check if hdr is valid before dereferencing
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

        // would be nice to use std::put_time here, but Windows doesn't support "%-m" and "%-d" (it'll compile, but
        // crash)
        if (!headerPrinted) {
            includeTit1 = (metadata && metadata->m_configInfo.hasTurbo1);
            includeTit2 = (metadata && metadata->m_configInfo.hasTurbo2);

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

        outStream << rec->m_recordSeq - 1 << "," << (timeinfo.tm_mon + 1) << '/' << timeinfo.tm_mday << '/'
                  << (timeinfo.tm_year + TM_YEAR_BASE) << "," << std::put_time(&timeinfo, "%T") << ",";
        printFlightMetricsRecord(rec, includeTit1, includeTit2, outStream);

        rec->m_isFast ? ++recordTime : recordTime += hdr->interval;
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

            auto trackData = collectFlightTrackData(inStream, flightId.value());
            if (!trackData.has_value()) {
                return;
            }

            try {
                auto kmlContent = buildKmlDocument(trackData.value(), inputFilePath.filename().string());
                writeKmlOrKmz(kmlOutput, kmlContent);
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
    // std::cout << "    -k <filename>   export flight path to KML or KMZ (requires -f)" << std::endl;
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
    // k option is not ready for prime time.
    // while ((c = getopt(argc, argv, "hf:lo:vk:")) != -1) {
    while ((c = getopt(argc, argv, "hf:lo:v")) != -1) {
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
