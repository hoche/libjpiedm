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
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include "EDMFileHeaders.hpp"
#include "EDMFlight.hpp"
#include "EDMFlightFile.hpp"

using jpi_edm::EDMFlightRecord;

void printFlightInfo(jpi_edm::EDMFlightHeader &hdr, unsigned long stdReqs, unsigned long fastReqs,
                     std::ostream &outStream)
{
    std::tm local;
#ifdef _WIN32
    time_t flightStartTime = _mkgmtime(&hdr.startDate);
    gmtime_s(&local, &flightStartTime);
#else
    time_t flightStartTime = timegm(&hdr.startDate);
    local = *gmtime(&flightStartTime);
#endif

    auto min = (fastReqs / 60) + (stdReqs * hdr.interval) / 60;
    auto hrs = 0.01 + static_cast<float>(min) / 60;
    // min = min % 60;

    outStream << "Flt #" << hdr.flight_num << " - ";
    // outStream << hrs << ":" << std::setfill('0') << std::setw(2) << min << " Hours";
    outStream << std::fixed << std::setprecision(2) << hrs << " Hours";
    outStream << " @ " << hdr.interval << " sec";
    outStream << " " << std::put_time(&local, "%m/%d/%Y") << " " << std::put_time(&local, "%T");
    outStream << std::endl;
}

// Ugh. these should all be a macro or an inline that range checks.
void printFlightDataRecord(const jpi_edm::EDMFlightRecord &rec, std::ostream &outStream)
{
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT1) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT2) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT3) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT4) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT5) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT6) << ",";

    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT1) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT2) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT3) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT4) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT5) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT6) << ",";

    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::OAT) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::DIF) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::CLD) << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::MAP)) / 10
              << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::RPM) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::HP) << ",";

    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::FF)) / 10 << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::FF2) << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::FP)) / 10 << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::OILP) << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::VLT)) / 10
              << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::AMPS) << ",";

    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::OILT) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::FUSD) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::FUSD2) << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::LFL)) / 10
              << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::RFL)) / 10
              << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::LAUX)) / 10
              << ",";
    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::RAUX)) / 10
              << ",";

    outStream << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::HRS)) / 10
              << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::SPD) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::ALT) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::LAT) << ",";
    outStream << rec.m_dataMap.at(EDMFlightRecord::Measurement::LNG) << ",";

    switch (rec.m_dataMap.at(EDMFlightRecord::Measurement::MARK)) {
    case 0x02:
        outStream << "[";
        break;
    case 0x03:
        outStream << "]";
        break;
    case 0x04:
        outStream << "<";
        break;
    }

    outStream << "\n";
}

void printFlightData(std::istream &stream, int flightId, std::ostream &outStream)
{
    jpi_edm::EDMFlightFile ff;
    jpi_edm::EDMFlightHeader hdr;
    time_t recordTime;

    ff.setMetaDataCompletionCb([&outStream](jpi_edm::EDMMetaData md) { md.dump(outStream); });

    ff.setFlightHeaderCompletionCb(
        [&flightId, &hdr, &recordTime, &outStream](jpi_edm::EDMFlightHeader fh) {
            hdr = fh;

            if (flightId != -1 && hdr.flight_num != flightId) {
                return;
            }

            std::tm local;
#ifdef _WIN32
            recordTime = _mkgmtime(&hdr.startDate);
            gmtime_s(&local, &recordTime);
#else
            recordTime = timegm(&hdr.startDate);
            local = *gmtime(&recordTime);
#endif

            outStream << "Flt #" << hdr.flight_num << "\n";
            outStream << "Interval: " << hdr.interval << " sec\n";
            outStream << "Flight Start Time: " << std::put_time(&local, "%m/%d/%Y") << " "
                      << std::put_time(&local, "%T") << "\n";

            outStream << "INDEX,DATE,TIME,E1,E2,E3,E4,E5,E6,C1,C2,C3,C4,C5,C6"
                      << ",OAT,DIF,CLD,MAP,RPM,HP,FF,FF2,FP,OILP,BAT,AMP,OILT"
                      << ",USD,USD2,RFL,LFL,LAUX,RAUX,HRS,SPD,ALT,LAT,LNG,MARK" << "\n";
        });

    ff.setFlightRecordCompletionCb(
        [&flightId, &hdr, &recordTime, &outStream](jpi_edm::EDMFlightRecord rec) {
            if (flightId != -1 && hdr.flight_num != flightId) {
                return;
            }

            std::tm local = *std::gmtime(&recordTime);

            outStream << rec.m_recordSeq << "," << std::put_time(&local, "%m/%d/%Y") << ","
                      << std::put_time(&local, "%T") << ",";
            printFlightDataRecord(rec, outStream);

            rec.m_isFast ? ++recordTime : recordTime += hdr.interval;
        });

    // Now do the work
    ff.processFile(stream);
}

void printFlightList(std::istream &stream, std::ostream &outStream)
{
    jpi_edm::EDMFlightFile ff;
    jpi_edm::EDMFlightHeader hdr;

    ff.setFlightHeaderCompletionCb([&hdr](jpi_edm::EDMFlightHeader fh) { hdr = fh; });
    ff.setFlightCompletionCb([&hdr, &outStream](unsigned long stdReqs, unsigned long fastReqs) {
        printFlightInfo(hdr, stdReqs, fastReqs, outStream);
    });
    ff.processFile(stream);
}

void processFiles(std::vector<std::string> &filelist, int flightId, bool onlyListFlights,
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
}

int main(int argc, char *argv[])
{
    bool onlyListFlights{false};
    std::vector<std::string> filelist{};
    std::string outputFile{};
    int flightId{-1};

    int c;
    while ((c = getopt(argc, argv, "hf:lo:")) != -1) {
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
            flightId = atoi(optarg);
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
