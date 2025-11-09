/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Example demonstrating the iterator-based API for parsing JPI EDM files.
 *
 * This example shows how to use the modern C++ iterator interface to parse
 * EDM flight data files. The iterator API provides lazy evaluation and does
 * NOT load the entire file into memory.
 *
 * Compilation:
 *   g++ -std=c++17 -I../src/libjpiedm iterator_example.cpp -L../build -ljpiedm -o iterator_example
 *
 * Usage:
 *   ./iterator_example <path_to_edm_file>
 */

#include "FlightFile.hpp"
#include "FlightIterator.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace jpi_edm;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <edm_file>\n";
        return 1;
    }

    const char *filename = argv[1];

    try {
        // Open the EDM file
        std::ifstream stream(filename, std::ios::binary);
        if (!stream) {
            std::cerr << "Error: Could not open file '" << filename << "'\n";
            return 1;
        }

        std::cout << "Parsing EDM file: " << filename << "\n\n";

        // Create parser and get iterator range
        FlightFile parser;

        // The flights() method parses headers and returns an iterable range
        // Flights are parsed lazily as you iterate
        auto flightRange = parser.flights(stream);

        std::cout << "==========================================\n";
        std::cout << "Iterating through flights...\n";
        std::cout << "==========================================\n\n";

        int flightCount = 0;

        // Range-based for loop - each flight is parsed on-demand
        for (const auto &flight : flightRange) {
            ++flightCount;

            const auto &header = flight.getHeader();

            std::cout << "Flight #" << header.flight_num << "\n";
            std::cout << "  Interval: " << header.interval << " seconds\n";
            std::cout << "  Standard records: " << flight.getStandardRecordCount() << "\n";
            std::cout << "  Fast records: " << flight.getFastRecordCount() << "\n";
            std::cout << "  Total records: " << flight.getTotalRecordCount() << "\n";

            // Optionally iterate through records in this flight
            // Records are also parsed lazily as you iterate
            int recordCount = 0;
            const int MAX_RECORDS_TO_SHOW = 5;

            std::cout << "  First " << MAX_RECORDS_TO_SHOW << " records:\n";

            for (const auto &record : flight) {
                if (recordCount >= MAX_RECORDS_TO_SHOW) {
                    break;
                }

                std::cout << "    Record " << record->m_recordSeq << " (" << (record->m_isFast ? "fast" : "standard")
                          << ")" << " - " << record->m_metrics.size() << " metrics\n";

                ++recordCount;
            }

            std::cout << "\n";
        }

        std::cout << "==========================================\n";
        std::cout << "Summary\n";
        std::cout << "==========================================\n";
        std::cout << "Total flights processed: " << flightCount << "\n\n";

        if (flightCount == 0) {
            std::cout << "Note: File contains no flight data (headers only)\n";
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
