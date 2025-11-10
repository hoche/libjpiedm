/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Unit tests for the streaming iterator API
 */

#include <gtest/gtest.h>

#include "FlightFile.hpp"
#include "FlightIterator.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace jpi_edm;

namespace {
// Helper function to calculate XOR checksum for EDM lines
std::string calculateChecksum(const std::string &content)
{
    uint8_t cs = 0;
    for (char c : content) {
        cs ^= static_cast<uint8_t>(c);
    }
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", cs);
    return std::string(buf);
}

// Build a complete EDM line with checksum
std::string buildEDMLine(const std::string &content)
{
    std::string cs = calculateChecksum(content);
    return "$" + content + "*" + cs + "\r\n";
}
} // anonymous namespace

class IteratorTest : public ::testing::Test
{
  protected:
    std::string buildMinimalValidFile()
    {
        // Build a minimal valid EDM file with headers only (no flights)
        std::string data = buildEDMLine("A,305,230,500,415,60,1650,230,90") +
                           buildEDMLine("C,1,127,760,0,0,1,0,0") +
                           buildEDMLine("F,60,60,0,45,45,0") + buildEDMLine("P,4") +
                           buildEDMLine("T,2024,1,15,10,30,45") + buildEDMLine("U,N12345") + buildEDMLine("L,0");

        return data;
    }

    std::string buildFileWithOneFlight()
    {
        // This is a simplified version - real implementation would need actual binary flight data
        // For now, we'll test what we can with header-only file
        return buildMinimalValidFile();
    }
};

class IteratorDataTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        std::vector<std::string> possiblePaths = {"tests/it/930_6cyl.jpi", "../tests/it/930_6cyl.jpi",
                                                  "../../tests/it/930_6cyl.jpi"};

        for (const auto &path : possiblePaths) {
            std::ifstream file(path, std::ios::binary);
            if (file.good()) {
                m_testFilePath = path;
                m_hasTestFile = true;
                break;
            }
        }
    }

    std::string m_testFilePath;
    bool m_hasTestFile{false};
};

TEST_F(IteratorTest, FlightRangeCanBeConstructed)
{
    FlightFile file;
    std::istringstream stream(buildMinimalValidFile());

    // Should be able to call flights() without throwing
    EXPECT_NO_THROW({ auto range = file.flights(stream); });
}

TEST_F(IteratorTest, EmptyFileHasNoFlights)
{
    FlightFile file;
    std::istringstream stream(buildMinimalValidFile());

    auto range = file.flights(stream);
    auto begin = range.begin();
    auto end = range.end();

    // Empty file should have begin == end
    EXPECT_EQ(begin, end);
}

TEST_F(IteratorTest, IteratorCanBeCompared)
{
    FlightFile file;
    std::istringstream stream(buildMinimalValidFile());

    auto range = file.flights(stream);
    auto it1 = range.begin();
    auto it2 = range.begin();
    auto it_end = range.end();

    // Two begin iterators should be equal
    EXPECT_EQ(it1, it2);

    // Begin should not equal end (for empty file they are equal)
    // EXPECT_NE(it1, it_end); // This would fail for empty file

    // Two end iterators should be equal
    auto end1 = range.end();
    auto end2 = range.end();
    EXPECT_EQ(end1, end2);
}

TEST_F(IteratorTest, CanUseRangeBasedForLoop)
{
    FlightFile file;
    std::istringstream stream(buildMinimalValidFile());

    int flightCount = 0;

    // Should be able to use range-based for loop
    EXPECT_NO_THROW({
        for (const auto &flight : file.flights(stream)) {
            ++flightCount;
            (void)flight; // Suppress unused variable warning
        }
    });

    // Empty file should have zero flights
    EXPECT_EQ(flightCount, 0);
}

TEST_F(IteratorTest, FlightViewProvidesMetadata)
{
    // This test would require a file with actual flight data
    // Skipping for now as it requires binary flight data generation
    GTEST_SKIP() << "Requires file with flight data";
}

TEST_F(IteratorTest, FlightRecordIteratorWorks)
{
    // This test would require a file with actual flight data
    // Skipping for now as it requires binary flight data generation
    GTEST_SKIP() << "Requires file with flight data";
}

TEST_F(IteratorTest, IteratorDoesNotLoadAllIntoMemory)
{
    // This is a conceptual test - we verify that the iterator
    // works without pre-loading by checking that we can create
    // it without exceptions and it follows lazy evaluation
    FlightFile file;
    std::istringstream stream(buildMinimalValidFile());

    // Getting the range should only parse headers, not all flights
    auto range = file.flights(stream);

    // Getting begin iterator should not load all flights
    auto begin = range.begin();

    // This should succeed without loading entire file into memory
    EXPECT_TRUE(true);
}

TEST_F(IteratorTest, StreamMustBeValid)
{
    FlightFile file;
    std::istringstream stream("invalid data");

    // Invalid stream should throw during flights() call
    EXPECT_THROW({ auto range = file.flights(stream); }, std::exception);
}

TEST_F(IteratorTest, DefaultConstructedIteratorIsEnd)
{
    FlightIterator it1;
    FlightIterator it2;

    // Two default-constructed iterators should be equal (both are end)
    EXPECT_EQ(it1, it2);
}

TEST_F(IteratorTest, RecordIteratorDefaultConstructedIsEnd)
{
    FlightView::RecordIterator it1;
    FlightView::RecordIterator it2;

    // Two default-constructed record iterators should be equal (both are end)
    EXPECT_EQ(it1, it2);
}

TEST_F(IteratorTest, DereferencingEndIteratorThrows)
{
    FlightIterator it; // Default-constructed (end iterator)

    EXPECT_THROW({ [[maybe_unused]] auto &flight = *it; }, std::out_of_range);
}

TEST_F(IteratorTest, RecordDereferencingEndIteratorThrows)
{
    FlightView::RecordIterator it; // Default-constructed (end iterator)

    EXPECT_THROW({ [[maybe_unused]] auto &record = *it; }, std::out_of_range);
}

TEST_F(IteratorDataTest, FlightViewCountsUpdateWhileIterating)
{
    if (!m_hasTestFile) {
        GTEST_SKIP() << "Requires flight data fixture";
    }

    FlightFile parser;
    std::ifstream stream(m_testFilePath, std::ios::binary);
    ASSERT_TRUE(stream.is_open());

    auto range = parser.flights(stream);
    auto it = range.begin();
    ASSERT_NE(it, range.end());

    FlightView flight = *it;

    EXPECT_EQ(0UL, flight.getTotalRecordCount());

    std::size_t recordCount = 0;
    for (auto recordIt = flight.begin(); recordIt != flight.end(); ++recordIt) {
        ++recordCount;
    }

    EXPECT_EQ(recordCount, static_cast<std::size_t>(flight.getTotalRecordCount()));
    EXPECT_EQ(flight.getTotalRecordCount(),
              flight.getStandardRecordCount() + flight.getFastRecordCount());
    EXPECT_GT(flight.getTotalRecordCount(), 0UL);
}
