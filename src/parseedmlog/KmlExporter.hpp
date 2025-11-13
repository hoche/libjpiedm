/*
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */

#pragma once

#include <ctime>
#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <vector>

#include <memory>

namespace jpi_edm {
class FlightHeader;
class FlightMetricsRecord;
} // namespace jpi_edm

namespace parseedmlog::kml {

struct FlightTrackPoint {
    std::time_t timestamp{};
    double latitude{};
    double longitude{};
    std::optional<double> altitudeFeet{};
    std::optional<double> speed{};
};

struct FlightTrackData {
    std::shared_ptr<jpi_edm::FlightHeader> header;
    std::vector<FlightTrackPoint> samples;
};

std::optional<FlightTrackData> collectFlightTrackData(std::istream &stream, int flightId);

void writeKmlOrKmz(const std::filesystem::path &outputPath, const FlightTrackData &trackData,
                   const std::string &sourceName);

} // namespace parseedmlog::kml

