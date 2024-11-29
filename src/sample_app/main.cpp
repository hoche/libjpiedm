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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <time.h>

#ifdef _WIN32
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include "EDMFlightFile.hpp"
#include "EDMFileHeaders.hpp"
#include "EDMFlight.hpp"

using jpi_edm::EDMFlightRecord;

void printFlightHeader(jpi_edm::EDMFlightHeader& hdr)
{
    time_t flightStartDate = mktime(&hdr.startDate);
    std::tm local = *std::localtime(&flightStartDate);
    std::cout << "Flt #" << hdr.flight_num
              << " - " << std::put_time(&local, "%m/%d/%Y")
              << " " << std::put_time(&local,   "%T")
              << " " << hdr.interval << " secs\n";
}

// Ugh. these should all be a macro or an inline that range checks.
void printFlightDataRecord(const jpi_edm::EDMFlightRecord& rec)
{
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT1) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT2) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT3) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT4) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT5) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::EGT6) << ",";

    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT1) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT2) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT3) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT4) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT5) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CHT6) << ",";

    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::OAT) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::DIF) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::CLD) << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::MAP))/10 << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::RPM) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::HP) << ",";

    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::FF))/10 << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::FF2) << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::FP))/10 << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::OILP) << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::VOLTS))/10 << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::AMPS) << ",";

    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::OILT) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::USD) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::USD2) << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::RFL))/10 << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::LFL))/10 << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::LAUX))/10 << ",";
    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::RAUX))/10 << ",";

    std::cout << static_cast<float>(rec.m_dataMap.at(EDMFlightRecord::Measurement::HRS))/10 << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::SPD) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::ALT) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::LAT) << ",";
    std::cout << rec.m_dataMap.at(EDMFlightRecord::Measurement::LNG) << ",";

    std::cout << ","; // MARK

    std::cout << "\n";
}

void printAllFlights(std::istream& stream)
{
    jpi_edm::EDMFlightFile ff;
    int recordIdx;
    int interval;
    time_t recordTime;

    ff.setFileHeaderCompletionCb([](jpi_edm::EDMFileHeaderSet hs) {
        hs.dump();
    });

    ff.setFlightHeaderCompletionCb([&recordIdx, &interval, &recordTime](jpi_edm::EDMFlightHeader hdr) {
        printFlightHeader(hdr);

        recordIdx = 0;
        interval = hdr.interval;
        recordTime = mktime(&hdr.startDate);

        std::cout << "INDEX,DATE,TIME,E1,E2,E3,E4,E5,E6,C1,C2,C3,C4,C5,C6"
                  << ",OAT,DIF,CLD,MAP,RPM,HP,FF,FF2,FP,OILP,BAT,AMP,OILT"
                  << ",USD,USD2,RFL,LFL,LAUX,RAUX,HRS,SPD,ALT,LAT,LNG,MARK"
                  << "\n";
    });

    ff.setFlightRecordCompletionCb([&recordIdx, &interval, &recordTime](jpi_edm::EDMFlightRecord rec) {
        std::tm local = *std::localtime(&recordTime);

        std::cout << recordIdx << "," << std::put_time(&local, "%m/%d/%Y") << "," << std::put_time(&local, "%T") << ",";
        printFlightDataRecord(rec);

        ++recordIdx;
        recordTime += interval;
    });

    // Now do the work
    ff.processFile(stream);
}

void printAvailableFlights(std::istream& stream)
{
    jpi_edm::EDMFlightFile ff;
    unsigned int interval;
    ff.setFlightHeaderCompletionCb([&interval](jpi_edm::EDMFlightHeader hdr) {
        printFlightHeader(hdr);
        interval = hdr.interval;
    });
    ff.setFlightCompletionCb([&interval](unsigned long numReqs, unsigned long nbytes) {
        std::cout << "Number of records: " << numReqs << std::endl;

        auto min = (numReqs * interval) / 60;
        std::cout << "The completely incorrect total flight time is " << min << " minutes";

        auto hrs = min / 60;
        min = min % 60;
        
        std::cout << "  (" << hrs << ":" << std::setfill('0') << std::setw(2) << min << ")" << std::endl;
    });
    ff.processFile(stream);
}

void processFiles(std::vector<std::string>& filelist, bool onlyListFlights)
{
    for (auto&& filename : filelist) {
        std::cout << filename << std::endl;

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

        std::ifstream inputFile(filename, std::ios_base::binary);
        if (!inputFile.is_open()) {
            std::cerr << "Couldn't open file\n";
            return;
        }

        if (onlyListFlights) {
            printAvailableFlights(inputFile);
        } else {
            printAllFlights(inputFile);
        }

        inputFile.close();
    }
}

void showHelp(char *progName)
{
    std::cout << "Usage: " << progName << "[options] file..." << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "    -h    print this help" << std::endl;
    std::cout << "    -l    list flights" << std::endl;
}

int main(int argc, char *argv[])
{
    bool onlyListFlights{false};
    std::vector<std::string> filelist{};

    int c;
    while ((c = getopt(argc, argv, "hl")) != -1) {
        switch (c) {
        case 'h':
            if (optarg)
                showHelp(argv[0]);
            break;
        case 'l':
            if (optarg)
                showHelp(argv[0]);
            onlyListFlights = true;
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

    processFiles(filelist, onlyListFlights);
    return 0;
}
