/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Example demonstrating parsing a specific flight by ID from a JPI EDM file.
 *
 * This example shows how to use the new flight-specific parsing feature to jump
 * directly to a particular flight without parsing the entire file. This is useful
 * for large files with many flights when you only need data from one flight.
 *
 * Compilation:
 *   g++ -std=c++17 -I../src/libjpiedm single_flight_example.cpp -L../build -ljpiedm -o single_flight_example
 *
 * Usage:
 *   ./single_flight_example <path_to_edm_file> [flight_id]
 *
 *   If flight_id is not provided, the program will list available flights.
 */

#include "Flight.hpp"
#include "FlightFile.hpp"
#include "Metadata.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace jpi_edm;

// Function to list all available flights in the file
void listFlights(const char *filename)
{
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        std::cerr << "Error: Could not open file '" << filename << "'\n";
        return;
    }

    FlightFile parser;
    std::shared_ptr<Metadata> metadata;

    try {
        auto flights = parser.detectFlights(stream, metadata);

        std::cout << "==========================================\n";
        std::cout << "File: " << filename << "\n";
        std::cout << "==========================================\n";

        if (metadata) {
            std::cout << "Protocol Version: " << metadata->ProtoVersion() << "\n";
            std::cout << "EDM Model: " << metadata->m_configInfo.edm_model << "\n";
            std::cout << "Cylinders: " << metadata->NumCylinders() << "\n";
            if (!metadata->m_tailNum.empty()) {
                std::cout << "Tail Number: " << metadata->m_tailNum << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "Available Flights:\n";
        std::cout << "==========================================\n";

        if (flights.empty()) {
            std::cout << "  (No flights found in file)\n";
        } else {
            for (const auto &flight : flights) {
                std::cout << "  Flight ID " << flight.flightNumber << " - ~" << flight.recordCount << " records" << " ("
                          << flight.dataSize << " bytes)\n";
            }
        }

        std::cout << "\nTotal: " << flights.size() << " flight(s)\n";
        std::cout << "\nTo parse a specific flight, run:\n";
        std::cout << "  " << "./single_flight_example " << filename << " <flight_id>\n";

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
    }
}

// Function to parse a specific flight
void parseSpecificFlight(const char *filename, int flightId)
{
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        std::cerr << "Error: Could not open file '" << filename << "'\n";
        return;
    }

    try {
        FlightFile parser;

        // Track statistics
        int recordCount = 0;
        unsigned long stdRecords = 0;
        unsigned long fastRecords = 0;
        std::shared_ptr<Metadata> metadata;
        std::shared_ptr<FlightHeader> flightHeader;

        // Set up callbacks
        parser.setMetadataCompletionCb([&metadata](std::shared_ptr<Metadata> md) { metadata = md; });

        parser.setFlightHeaderCompletionCb([&flightHeader](std::shared_ptr<FlightHeader> hdr) { flightHeader = hdr; });

        parser.setFlightRecordCompletionCb([&recordCount](std::shared_ptr<FlightMetricsRecord> rec) {
            recordCount++;

            // Show first 10 records as examples
            if (recordCount <= 10) {
                std::cout << "  Record " << rec->m_recordSeq << " (" << (rec->m_isFast ? "fast" : "standard") << ")"
                          << " - " << rec->m_metrics.size() << " metrics\n";
            } else if (recordCount == 11) {
                std::cout << "  ... (remaining records not displayed)\n";
            }
        });

        parser.setFlightCompletionCb([&stdRecords, &fastRecords](unsigned long std, unsigned long fast) {
            stdRecords = std;
            fastRecords = fast;
        });

        std::cout << "==========================================\n";
        std::cout << "Parsing Flight #" << flightId << " from: " << filename << "\n";
        std::cout << "==========================================\n";
        std::cout << "This will jump directly to the specified flight\n";
        std::cout << "without parsing other flights in the file.\n\n";

        // Parse only the specified flight
        parser.processFile(stream, flightId);

        // Display results
        std::cout << "\n==========================================\n";
        std::cout << "Flight #" << flightId << " Summary\n";
        std::cout << "==========================================\n";

        if (metadata) {
            std::cout << "EDM Model: " << metadata->m_configInfo.edm_model << "\n";
            std::cout << "Protocol: V" << metadata->ProtoVersion() << "\n";
        }

        if (flightHeader) {
            std::cout << "Interval: " << flightHeader->interval << " seconds\n";

            // Display start date/time
            std::tm local = flightHeader->startDate;
            std::cout << "Start Date: " << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << "\n";

            if (flightHeader->startLat != 0 || flightHeader->startLng != 0) {
                std::cout << "Starting Position: " << std::fixed << std::setprecision(6)
                          << (flightHeader->startLat / 10000000.0) << ", " << (flightHeader->startLng / 10000000.0)
                          << "\n";
            }
        }

        std::cout << "\nRecords Parsed: " << recordCount << "\n";
        std::cout << "  Standard: " << stdRecords << "\n";
        std::cout << "  Fast: " << fastRecords << "\n";

        std::cout << "\n✓ Successfully parsed flight #" << flightId << "\n";

    } catch (const std::exception &ex) {
        std::cerr << "\nError: " << ex.what() << "\n";
        std::cerr << "\nNote: Flight ID " << flightId << " may not exist in this file.\n";
        std::cerr << "Run without a flight ID to see available flights:\n";
        std::cerr << "  " << "./single_flight_example " << filename << "\n";
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <edm_file> [flight_id]\n";
        std::cerr << "\n";
        std::cerr << "Examples:\n";
        std::cerr << "  List available flights:\n";
        std::cerr << "    " << argv[0] << " data.jpi\n";
        std::cerr << "\n";
        std::cerr << "  Parse a specific flight:\n";
        std::cerr << "    " << argv[0] << " data.jpi 42\n";
        return 1;
    }

    const char *filename = argv[1];

    if (argc == 2) {
        // No flight ID provided - list available flights
        listFlights(filename);
    } else {
        // Flight ID provided - parse that specific flight
        int flightId;
        std::istringstream iss(argv[2]);
        if (!(iss >> flightId)) {
            std::cerr << "Error: Invalid flight ID '" << argv[2] << "'\n";
            std::cerr << "Flight ID must be a number\n";
            return 1;
        }

        parseSpecificFlight(filename, flightId);
    }

    return 0;
}

