/*
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 */

#include "KmlExporter.hpp"

#include "MetricUtils.hpp"
#include "libjpiedm/FlightFile.hpp"
#include "libjpiedm/MetricId.hpp"
#include "libjpiedm/ProtocolConstants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace parseedmlog::kml {

namespace {

constexpr float kGpsOffset = 241.0f;
constexpr double kFeetToMeters = 0.3048;
constexpr const char *kKmzDefaultEntryName = "doc.kml";

std::optional<double> decodeGpsCoordinate(float measurement)
{
    if (std::fabs(measurement) < 0.5f) {
        return std::nullopt;
    }

    int scaledMeasurement = static_cast<int>(std::lround(measurement));
    bool isNegative = scaledMeasurement < 0;
    int absCoordinate = std::abs(scaledMeasurement);
    int degrees = absCoordinate / jpi_edm::GPS_COORD_SCALE_DENOMINATOR;
    int remainder = absCoordinate % jpi_edm::GPS_COORD_SCALE_DENOMINATOR;
    int minutes = remainder / jpi_edm::GPS_MINUTES_DECIMAL_DIVISOR;
    int hundredths = remainder % jpi_edm::GPS_MINUTES_DECIMAL_DIVISOR;

    double decimalMinutes = static_cast<double>(minutes) +
                            static_cast<double>(hundredths) / static_cast<double>(jpi_edm::GPS_MINUTES_DECIMAL_DIVISOR);
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

std::string formatHeaderStartTime(const std::shared_ptr<jpi_edm::FlightHeader> &header)
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

} // namespace

std::optional<FlightTrackData> collectFlightTrackData(std::istream &stream, int flightId)
{
    stream.clear();
    stream.seekg(0);

    jpi_edm::FlightFile ff;
    FlightTrackData trackData;
    std::time_t recordTime = 0;

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

        auto lat = parseedmlog::getMetric(record->m_metrics, jpi_edm::LAT);
        auto lng = parseedmlog::getMetric(record->m_metrics, jpi_edm::LNG);
        auto decodedLat = decodeGpsCoordinate(lat);
        auto decodedLng = decodeGpsCoordinate(lng);
        if (decodedLat.has_value() && decodedLng.has_value()) {
            FlightTrackPoint point;
            point.timestamp = recordTime;
            point.latitude = decodedLat.value();
            point.longitude = decodedLng.value();
            point.altitudeFeet = decodeAltitude(parseedmlog::getMetric(record->m_metrics, jpi_edm::ALT, -1.0f));
            point.speed = decodeSpeed(parseedmlog::getMetric(record->m_metrics, jpi_edm::SPD, -1.0f));
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

void writeKmlOrKmz(const std::filesystem::path &outputPath, const FlightTrackData &trackData,
                   const std::string &sourceName)
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

    const auto kmlContent = buildKmlDocument(trackData, sourceName);

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

} // namespace parseedmlog::kml
