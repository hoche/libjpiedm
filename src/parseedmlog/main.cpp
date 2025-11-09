/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Sample app for using libjpiedm.
 *
 * Shows how to install callbacks and use the library to parse EDM files.
 * This is just an example.
 */

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
    local = *gmtime(&flightStartTime);
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

void printLatLng(int measurement, std::ostream &outStream)
{
    // Comment out for now
    outStream << "00.00,";
    // int hrs = measurement / GPS_COORD_SCALE_DENOMINATOR;
    // float min = float(abs(measurement) % GPS_COORD_SCALE_DENOMINATOR) / GPS_MINUTES_DECIMAL_DIVISOR;
    // outStream << std::setfill('0') << std::setw(2) << hrs << "." << std::setw(2) << std::setprecision(2) << min <<
    // ",";
}

// Safe metric accessor with default value
inline float getMetric(const std::map<MetricId, float> &metrics, MetricId id, float defaultValue = 0.0f)
{
    auto it = metrics.find(id);
    return (it != metrics.end()) ? it->second : defaultValue;
}

void printFlightMetricsRecord(const std::shared_ptr<jpi_edm::FlightMetricsRecord> &rec, std::ostream &outStream)
{
    outStream << std::fixed << std::setprecision(0);
    outStream << rec->m_metrics.at(EGT11) << ",";
    outStream << rec->m_metrics.at(EGT12) << ",";
    outStream << rec->m_metrics.at(EGT13) << ",";
    outStream << rec->m_metrics.at(EGT14) << ",";
    outStream << rec->m_metrics.at(EGT15) << ",";
    outStream << rec->m_metrics.at(EGT16) << ",";

    outStream << rec->m_metrics.at(CHT11) << ",";
    outStream << rec->m_metrics.at(CHT12) << ",";
    outStream << rec->m_metrics.at(CHT13) << ",";
    outStream << rec->m_metrics.at(CHT14) << ",";
    outStream << rec->m_metrics.at(CHT15) << ",";
    outStream << rec->m_metrics.at(CHT16) << ",";

    outStream << getMetric(rec->m_metrics, TIT11) << ",";
    outStream << getMetric(rec->m_metrics, TIT12) << ",";

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
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, FUSD11) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, FUSD12, -1.0f) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, LMAIN) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, RMAIN) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, LAUX) << "," << std::setprecision(0);
    outStream << std::setprecision(1) << getMetric(rec->m_metrics, RAUX) << "," << std::setprecision(0);

    outStream << std::setprecision(1) << getMetric(rec->m_metrics, HRS1) << "," << std::setprecision(0);
    outStream << getMetric(rec->m_metrics, SPD, -1.0f) << ",";
    outStream << getMetric(rec->m_metrics, ALT, -1.0f) << ",";

    printLatLng(static_cast<int>(getMetric(rec->m_metrics, LAT)), outStream);
    printLatLng(static_cast<int>(getMetric(rec->m_metrics, LNG)), outStream);

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
    time_t recordTime;

    if (g_verbose) {
        ff.setMetadataCompletionCb([&outStream](std::shared_ptr<jpi_edm::Metadata> md) { md->dump(outStream); });
    }

    ff.setFlightHeaderCompletionCb(
        [&flightId, &hdr, &recordTime, &outStream](std::shared_ptr<jpi_edm::FlightHeader> fh) {
            hdr = fh;

            if (flightId.has_value() && hdr->flight_num != flightId.value()) {
                return;
            }

            std::tm local;
#ifdef _WIN32
            recordTime = _mkgmtime(&hdr->startDate);
            gmtime_s(&local, &recordTime);
#else
            recordTime = timegm(&hdr->startDate);
            local = *gmtime(&recordTime);
#endif

            if (g_verbose) {
                outStream << "Flt #" << hdr->flight_num << "\n";
                outStream << "Interval: " << hdr->interval << " sec\n";
                outStream << "Flight Start Time: " << std::put_time(&local, "%m/%d/%Y") << " "
                          << std::put_time(&local, "%T") << "\n";
            }

            outStream << "INDEX,DATE,TIME,E1,E2,E3,E4,E5,E6,C1,C2,C3,C4,C5,C6"
                      << ",TIT1,TIT2,OAT,DIF,CLD,MAP,RPM,HP,FF,FF2,FP,OILP,BAT,AMP,OILT"
                      << ",USD,USD2,RFL,LFL,LAUX,RAUX,HRS,SPD,ALT,LAT,LNG,MARK" << "\n";
        });

    ff.setFlightRecordCompletionCb(
        [&flightId, &hdr, &recordTime, &outStream](std::shared_ptr<jpi_edm::FlightMetricsRecord> rec) {
            // Check if hdr is valid before dereferencing
            if (!hdr) {
                std::cerr << "Warning: Flight record callback invoked without flight header" << std::endl;
                return;
            }

            if (flightId.has_value() && hdr->flight_num != flightId.value()) {
                return;
            }

            std::tm timeinfo = *std::gmtime(&recordTime);

            // would be nice to use std::put_time here, but Windows doesn't support "%-m" and "%-d" (it'll compile, but
            // crash)
            outStream << rec->m_recordSeq - 1 << "," << std::setfill('0') << std::setw(2) << (timeinfo.tm_mon + 1)
                      << '/' << std::setfill('0') << std::setw(2) << timeinfo.tm_mday << '/'
                      << (timeinfo.tm_year + TM_YEAR_BASE) << "," << std::put_time(&timeinfo, "%T") << ",";
            printFlightMetricsRecord(rec, outStream);

            rec->m_isFast ? ++recordTime : recordTime += hdr->interval;
        });

    // Now do the work
    ff.processFile(stream);
}

void printFlightList(std::istream &stream, std::ostream &outStream)
{
    jpi_edm::FlightFile ff;
    std::shared_ptr<jpi_edm::FlightHeader> hdr;

    ff.setFlightHeaderCompletionCb([&hdr](std::shared_ptr<jpi_edm::FlightHeader> fh) { hdr = fh; });
    ff.setFlightCompletionCb([&hdr, &outStream](unsigned long stdReqs, unsigned long fastReqs) {
        printFlightInfo(hdr, stdReqs, fastReqs, outStream);
    });
    ff.processFile(stream);
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
            if (optarg) {
                showHelp(argv[0]);
                return 0;
            }
            break;
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
            if (optarg) {
                showHelp(argv[0]);
                return 0;
            }
            onlyListFlights = true;
            break;
        case 'o':
            if (!optarg) {
                showHelp(argv[0]);
                return 0;
            }
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
