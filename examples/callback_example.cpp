/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Example demonstrating the callback-based API for parsing JPI EDM files.
 *
 * This example shows how to use the original callback-based interface to parse
 * EDM flight data files. The callback API provides event-driven processing and
 * does NOT load the entire file into memory.
 *
 * Compilation:
 *   g++ -std=c++17 -I../src/libjpiedm callback_example.cpp -L../build -ljpiedm -o callback_example
 *
 * Usage:
 *   ./callback_example <path_to_edm_file>
 */

#include "Flight.hpp"
#include "FlightFile.hpp"
#include "Metadata.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace jpi_edm;

// Global state for tracking across callbacks
struct FlightStats {
    int flightCount = 0;
    int currentFlightNumber = 0;
    int recordsInCurrentFlight = 0;
    int totalRecords = 0;
    unsigned long stdRecords = 0;
    unsigned long fastRecords = 0;
};

FlightStats stats;

// Metadata callback - called once when file headers are parsed
void onMetadataComplete(std::shared_ptr<Metadata> metadata)
{
    std::cout << "==========================================\n";
    std::cout << "File Metadata\n";
    std::cout << "==========================================\n";
    std::cout << "Protocol Version: " << metadata->ProtoVersion() << "\n";
    std::cout << "Number of Cylinders: " << metadata->NumCylinders() << "\n";
    std::cout << "Is Twin Engine: " << (metadata->IsTwin() ? "Yes" : "No") << "\n";
    std::cout << "Uses GPH: " << (metadata->IsGPH() ? "Yes" : "No") << "\n";

    if (!metadata->m_tailNum.empty()) {
        std::cout << "Tail Number: " << metadata->m_tailNum << "\n";
    }

    std::cout << "\n";
}

// Flight header callback - called when a flight header is parsed
void onFlightHeaderComplete(std::shared_ptr<FlightHeader> header)
{
    stats.flightCount++;
    stats.currentFlightNumber = header->flight_num;
    stats.recordsInCurrentFlight = 0;

    std::cout << "==========================================\n";
    std::cout << "Flight #" << header->flight_num << "\n";
    std::cout << "==========================================\n";
    std::cout << "Interval: " << header->interval << " seconds\n";
    std::cout << "Flags: 0x" << std::hex << header->flags << std::dec << "\n";

    if (header->startLat != 0 || header->startLng != 0) {
        std::cout << "Starting Position: " << std::fixed << std::setprecision(6) << (header->startLat / 10000000.0)
                  << ", " << (header->startLng / 10000000.0) << "\n";
    }

    // Display start date/time
    std::tm local = header->startDate;
    std::cout << "Start Date: " << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "\n";
}

// Record callback - called for each metric record
void onFlightRecordComplete(std::shared_ptr<FlightMetricsRecord> record)
{
    stats.recordsInCurrentFlight++;
    stats.totalRecords++;

    // Only show first 5 records of each flight to avoid overwhelming output
    const int MAX_RECORDS_TO_SHOW = 5;

    if (stats.recordsInCurrentFlight <= MAX_RECORDS_TO_SHOW) {
        std::cout << "  Record " << record->m_recordSeq << " (" << (record->m_isFast ? "fast" : "standard") << ")"
                  << " - " << record->m_metrics.size() << " metrics";

        // Show a sample metric if available
        if (!record->m_metrics.empty()) {
            auto it = record->m_metrics.begin();
            std::cout << " [first: " << static_cast<int>(it->first) << " = " << it->second << "]";
        }
        std::cout << "\n";
    } else if (stats.recordsInCurrentFlight == MAX_RECORDS_TO_SHOW + 1) {
        std::cout << "  ... (remaining records not displayed)\n";
    }
}

// Flight completion callback - called when a flight is fully parsed
void onFlightComplete(unsigned long stdRecCount, unsigned long fastRecCount)
{
    stats.stdRecords = stdRecCount;
    stats.fastRecords = fastRecCount;

    std::cout << "\nFlight #" << stats.currentFlightNumber << " Summary:\n";
    std::cout << "  Total records: " << stats.recordsInCurrentFlight << "\n";
    std::cout << "  Standard records: " << stdRecCount << "\n";
    std::cout << "  Fast records: " << fastRecCount << "\n";
    std::cout << "\n";
}

// File footer callback - called when entire file is parsed
void onFileFooterComplete()
{
    std::cout << "==========================================\n";
    std::cout << "File Parsing Complete\n";
    std::cout << "==========================================\n";
}

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

        // Create parser
        FlightFile parser;

        // Register callbacks
        parser.setMetadataCompletionCb(onMetadataComplete);
        parser.setFlightHeaderCompletionCb(onFlightHeaderComplete);
        parser.setFlightRecordCompletionCb(onFlightRecordComplete);
        parser.setFlightCompletionCb(onFlightComplete);
        parser.setFileFooterCompletionCb(onFileFooterComplete);

        // Process file - callbacks will be invoked as data is parsed
        parser.processFile(stream);

        // Display final statistics
        std::cout << "\n";
        std::cout << "==========================================\n";
        std::cout << "Summary\n";
        std::cout << "==========================================\n";
        std::cout << "Total flights: " << stats.flightCount << "\n";
        std::cout << "Total records: " << stats.totalRecords << "\n";
        std::cout << "\n";

        if (stats.flightCount == 0) {
            std::cout << "Note: File contains no flight data (headers only)\n";
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
