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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
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
inline float getMetric(const std::map<MetricId, float> &metrics, MetricId id, float defaultValue = 0.0f)
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

    constexpr float kGpsOffset = 241.0f;
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
                  std::string &outputFile)
{
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
    std::cout << "    -v              verbose output of the flight header" << std::endl;
}

int main(int argc, char *argv[])
{
    bool onlyListFlights{false};
    std::vector<std::string> filelist{};
    std::string outputFile{};
    std::optional<int> flightId; // std::nullopt means all flights

    int c;
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
        case 'v':
            g_verbose = true;
            break;
        }
    }

    if (optind == argc) {
        showHelp(argv[0]);
        return 0;
    }

    for (int i = optind; i < argc; ++i) {
        filelist.push_back(argv[i]);
    }

    processFiles(filelist, flightId, onlyListFlights, outputFile);
    return 0;
}
