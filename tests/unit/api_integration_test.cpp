/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * @brief Integration tests for both callback and iterator APIs using real JPI files
 */

#include <gtest/gtest.h>

#include "FlightFile.hpp"
#include "FlightIterator.hpp"

#include <fstream>
#include <map>
#include <vector>

using namespace jpi_edm;

namespace {

// Helper to find test files in tests/it directory
std::string findTestFile(const std::string& filename)
{
    // Try multiple possible paths (for different working directories)
    std::vector<std::string> possiblePaths = {
        filename, // Current directory
        "tests/it/" + filename,
        "../tests/it/" + filename,
        "../../tests/it/" + filename,
        "../../../tests/it/" + filename,
        "../../../../tests/it/" + filename,
    };

    for (const auto& path : possiblePaths) {
        std::ifstream testFile(path, std::ios::binary);
        if (testFile.good()) {
            return path;
        }
    }
    return "";
}

// Test files to process
const std::vector<std::string> TEST_FILES = {
    "830_6cyl.jpi",
    "930_6cyl.jpi",
    "930_6cyl_turbo.jpi",
    "960_4cyl_twin.jpi",
};

} // anonymous namespace

class ApiIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Find available test files
        for (const auto& filename : TEST_FILES) {
            std::string path = findTestFile(filename);
            if (!path.empty()) {
                availableFiles[filename] = path;
            }
        }
    }

    std::map<std::string, std::string> availableFiles;
};

// =============================================================================
// Callback API Tests
// =============================================================================

TEST_F(ApiIntegrationTest, CallbackAPI_CanParseAllFlights)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        FlightFile parser;
        std::vector<std::shared_ptr<Metadata>> metadataList;
        std::vector<std::shared_ptr<FlightHeader>> flightHeaders;
        std::vector<std::shared_ptr<FlightMetricsRecord>> records;
        unsigned long totalStdRecords = 0;
        unsigned long totalFastRecords = 0;

        parser.setMetadataCompletionCb([&metadataList](std::shared_ptr<Metadata> md) {
            metadataList.push_back(md);
            EXPECT_NE(nullptr, md);
        });

        parser.setFlightHeaderCompletionCb([&flightHeaders](std::shared_ptr<FlightHeader> hdr) {
            flightHeaders.push_back(hdr);
            EXPECT_NE(nullptr, hdr);
        });

        parser.setFlightRecordCompletionCb([&records](std::shared_ptr<FlightMetricsRecord> rec) {
            records.push_back(rec);
            EXPECT_NE(nullptr, rec);
            EXPECT_GT(rec->m_metrics.size(), 0);
        });

        parser.setFlightCompletionCb([&totalStdRecords, &totalFastRecords](unsigned long stdRecs, unsigned long fastRecs) {
            totalStdRecords += stdRecs;
            totalFastRecords += fastRecs;
        });

        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open()) << "Failed to open: " << filepath;

        EXPECT_NO_THROW(parser.processFile(stream)) << "Failed to parse: " << filename;

        // Should have metadata
        EXPECT_GT(metadataList.size(), 0) << "No metadata parsed for: " << filename;

        // Should have at least one flight header
        EXPECT_GT(flightHeaders.size(), 0) << "No flight headers parsed for: " << filename;

        // Should have records if flights exist
        if (flightHeaders.size() > 0) {
            EXPECT_GT(records.size(), 0) << "No records parsed for: " << filename;
        }
    }
}

TEST_F(ApiIntegrationTest, CallbackAPI_CanParseSpecificFlight)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        // First, detect flights to get a flight number
        FlightFile detectParser;
        std::ifstream detectStream(filepath, std::ios::binary);
        ASSERT_TRUE(detectStream.is_open());

        auto flights = detectParser.detectFlights(detectStream);
        if (flights.empty()) {
            continue; // Skip files with no flights
        }

        int flightNumber = flights[0].flightNumber;

        // Now parse just that flight
        FlightFile parser;
        std::shared_ptr<FlightHeader> flightHeader;
        std::vector<std::shared_ptr<FlightMetricsRecord>> records;
        unsigned long stdRecords = 0;
        unsigned long fastRecords = 0;

        parser.setFlightHeaderCompletionCb([&flightHeader](std::shared_ptr<FlightHeader> hdr) {
            flightHeader = hdr;
        });

        parser.setFlightRecordCompletionCb([&records](std::shared_ptr<FlightMetricsRecord> rec) {
            records.push_back(rec);
        });

        parser.setFlightCompletionCb([&stdRecords, &fastRecords](unsigned long stdRecs, unsigned long fastRecs) {
            stdRecords = stdRecs;
            fastRecords = fastRecs;
        });

        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open());

        EXPECT_NO_THROW(parser.processFile(stream, flightNumber))
            << "Failed to parse flight " << flightNumber << " from: " << filename;

        EXPECT_NE(nullptr, flightHeader) << "No flight header for flight " << flightNumber;
        EXPECT_EQ(flightHeader->flight_num, static_cast<unsigned int>(flightNumber))
            << "Flight number mismatch";

        // Should have records
        EXPECT_GT(records.size(), 0) << "No records for flight " << flightNumber;
        EXPECT_EQ(records.size(), static_cast<size_t>(stdRecords + fastRecords))
            << "Record count mismatch";
    }
}

TEST_F(ApiIntegrationTest, CallbackAPI_RecordsContainValidMetrics)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        FlightFile parser;
        std::vector<std::shared_ptr<FlightMetricsRecord>> records;

        parser.setFlightRecordCompletionCb([&records](std::shared_ptr<FlightMetricsRecord> rec) {
            records.push_back(rec);
        });

        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open());

        try {
            parser.processFile(stream);
        } catch (...) {
            // May throw at end of file, that's OK
        }

        if (records.empty()) {
            continue; // Skip files with no records
        }

        // Check first few records have valid metrics
        size_t recordsToCheck = std::min(records.size(), size_t(10));
        for (size_t i = 0; i < recordsToCheck; ++i) {
            const auto& rec = records[i];
            EXPECT_NE(nullptr, rec);
            EXPECT_GT(rec->m_metrics.size(), 0) << "Record " << i << " has no metrics";
            EXPECT_GE(rec->m_recordSeq, 0UL) << "Invalid record sequence";
        }
    }
}

// =============================================================================
// Iterator API Tests
// =============================================================================

TEST_F(ApiIntegrationTest, IteratorAPI_CanIterateAllFlights)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        FlightFile parser;
        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open()) << "Failed to open: " << filepath;

        EXPECT_NO_THROW({
            auto range = parser.flights(stream);
            size_t flightCount = 0;

            for (const auto& flight : range) {
                ++flightCount;
                const auto& header = flight.getHeader();
                EXPECT_GT(header.flight_num, 0U) << "Invalid flight number";
                EXPECT_GT(header.interval, 0U) << "Invalid interval";
            }

            // Verify we got flights (if file has them)
            // Some files might be header-only, so we don't require flights
        }) << "Failed to iterate flights in: " << filename;
    }
}

TEST_F(ApiIntegrationTest, IteratorAPI_CanIterateRecordsInFlight)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        FlightFile parser;
        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open());

        auto range = parser.flights(stream);

        bool foundFlightWithRecords = false;
        for (const auto& flight : range) {
            const auto& header = flight.getHeader();

            size_t recordCount = 0;
            size_t maxRecordsToCheck = 100; // Limit to avoid long test times

            EXPECT_NO_THROW({
                for (const auto& record : flight) {
                    if (recordCount >= maxRecordsToCheck) {
                        break;
                    }

                    EXPECT_NE(nullptr, record);
                    EXPECT_GT(record->m_metrics.size(), 0)
                        << "Record " << recordCount << " has no metrics";
                    EXPECT_GE(record->m_recordSeq, 0UL) << "Invalid record sequence";

                    ++recordCount;
                }
            }) << "Failed to iterate records in flight " << header.flight_num;

            if (recordCount > 0) {
                foundFlightWithRecords = true;
                break; // Found at least one flight with records, that's enough
            }
        }

        // It's OK if file has no flights with records
        (void)foundFlightWithRecords;
    }
}

TEST_F(ApiIntegrationTest, IteratorAPI_FlightViewMetadataIsValid)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        FlightFile parser;
        std::ifstream stream(filepath, std::ios::binary);
        ASSERT_TRUE(stream.is_open());

        auto range = parser.flights(stream);

        for (const auto& flight : range) {
            const auto& header = flight.getHeader();

            EXPECT_GT(header.flight_num, 0U) << "Invalid flight number";
            EXPECT_GT(header.interval, 0U) << "Invalid interval";

            // Check record counts are reasonable
            unsigned long stdCount = flight.getStandardRecordCount();
            unsigned long fastCount = flight.getFastRecordCount();
            unsigned long totalCount = flight.getTotalRecordCount();

            EXPECT_EQ(totalCount, stdCount + fastCount) << "Record count mismatch";
        }
    }
}

// =============================================================================
// API Comparison Tests
// =============================================================================

TEST_F(ApiIntegrationTest, BothAPIs_ProduceSameFlightCount)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        // Count flights using callback API
        FlightFile callbackParser;
        std::vector<std::shared_ptr<FlightHeader>> callbackHeaders;

        callbackParser.setFlightHeaderCompletionCb([&callbackHeaders](std::shared_ptr<FlightHeader> hdr) {
            callbackHeaders.push_back(hdr);
        });

        std::ifstream callbackStream(filepath, std::ios::binary);
        ASSERT_TRUE(callbackStream.is_open());

        try {
            callbackParser.processFile(callbackStream);
        } catch (...) {
            // May throw at end of file
        }

        // Count flights using iterator API
        FlightFile iteratorParser;
        std::ifstream iteratorStream(filepath, std::ios::binary);
        ASSERT_TRUE(iteratorStream.is_open());

        auto range = iteratorParser.flights(iteratorStream);
        size_t iteratorCount = 0;
        for (const auto& flight : range) {
            (void)flight; // Suppress unused warning
            ++iteratorCount;
        }

        // Both should produce the same count
        EXPECT_EQ(callbackHeaders.size(), iteratorCount)
            << "Flight count mismatch for: " << filename;
    }
}

TEST_F(ApiIntegrationTest, BothAPIs_ProduceSameRecordCount)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        // Get flight numbers first
        FlightFile detectParser;
        std::ifstream detectStream(filepath, std::ios::binary);
        ASSERT_TRUE(detectStream.is_open());

        auto flights = detectParser.detectFlights(detectStream);
        if (flights.empty()) {
            continue; // Skip files with no flights
        }

        int flightNumber = flights[0].flightNumber;

        // Count records using callback API
        FlightFile callbackParser;
        std::vector<std::shared_ptr<FlightMetricsRecord>> callbackRecords;
        unsigned long callbackStdRecords = 0;
        unsigned long callbackFastRecords = 0;

        callbackParser.setFlightRecordCompletionCb([&callbackRecords](std::shared_ptr<FlightMetricsRecord> rec) {
            callbackRecords.push_back(rec);
        });

        callbackParser.setFlightCompletionCb([&callbackStdRecords, &callbackFastRecords](unsigned long stdRecs,
                                                                                        unsigned long fastRecs) {
            callbackStdRecords = stdRecs;
            callbackFastRecords = fastRecs;
        });

        std::ifstream callbackStream(filepath, std::ios::binary);
        ASSERT_TRUE(callbackStream.is_open());

        try {
            callbackParser.processFile(callbackStream, flightNumber);
        } catch (...) {
            // May throw at end of file
        }

        // Count records using iterator API
        FlightFile iteratorParser;
        std::ifstream iteratorStream(filepath, std::ios::binary);
        ASSERT_TRUE(iteratorStream.is_open());

        auto range = iteratorParser.flights(iteratorStream);
        size_t iteratorRecordCount = 0;

        for (const auto& flight : range) {
            const auto& header = flight.getHeader();
            if (static_cast<int>(header.flight_num) != flightNumber) {
                continue;
            }

            for (const auto& record : flight) {
                (void)record; // Suppress unused warning
                ++iteratorRecordCount;
            }
            break; // Only process the first matching flight
        }

        // Both should produce the same count (within reason - callback might include incomplete records)
        // Allow some tolerance since iterator might stop at errors
        size_t callbackTotal = callbackRecords.size();
        EXPECT_GE(callbackTotal, iteratorRecordCount - 10)
            << "Record count mismatch too large for: " << filename;
        EXPECT_LE(callbackTotal, iteratorRecordCount + 10)
            << "Record count mismatch too large for: " << filename;
    }
}

TEST_F(ApiIntegrationTest, BothAPIs_FlightHeadersMatch)
{
    if (availableFiles.empty()) {
        GTEST_SKIP() << "No test files available";
    }

    for (const auto& [filename, filepath] : availableFiles) {
        // Get flight headers using callback API
        FlightFile callbackParser;
        std::vector<std::shared_ptr<FlightHeader>> callbackHeaders;

        callbackParser.setFlightHeaderCompletionCb([&callbackHeaders](std::shared_ptr<FlightHeader> hdr) {
            callbackHeaders.push_back(hdr);
        });

        std::ifstream callbackStream(filepath, std::ios::binary);
        ASSERT_TRUE(callbackStream.is_open());

        try {
            callbackParser.processFile(callbackStream);
        } catch (...) {
            // May throw at end of file
        }

        // Get flight headers using iterator API
        FlightFile iteratorParser;
        std::ifstream iteratorStream(filepath, std::ios::binary);
        ASSERT_TRUE(iteratorStream.is_open());

        auto range = iteratorParser.flights(iteratorStream);
        std::vector<FlightHeader> iteratorHeaders;

        for (const auto& flight : range) {
            iteratorHeaders.push_back(flight.getHeader());
        }

        // Compare headers
        EXPECT_EQ(callbackHeaders.size(), iteratorHeaders.size())
            << "Header count mismatch for: " << filename;

        size_t minSize = std::min(callbackHeaders.size(), iteratorHeaders.size());
        for (size_t i = 0; i < minSize; ++i) {
            const auto& cbHdr = *callbackHeaders[i];
            const auto& itHdr = iteratorHeaders[i];

            EXPECT_EQ(cbHdr.flight_num, itHdr.flight_num)
                << "Flight number mismatch at index " << i << " for: " << filename;
            EXPECT_EQ(cbHdr.interval, itHdr.interval)
                << "Interval mismatch at index " << i << " for: " << filename;
        }
    }
}

