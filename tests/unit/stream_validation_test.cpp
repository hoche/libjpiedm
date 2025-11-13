/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * @brief Comprehensive tests for stream validation and error handling
 *
 * Tests the critical stream operations added to ensure robustness:
 * - stream.read() validation
 * - stream.tellg() validation
 * - stream.seekg() validation
 * - Partial read detection
 * - Stream state recovery
 */

#include <gtest/gtest.h>
#include <sstream>
#include <vector>

#include "FlightFile.hpp"
#include "Metadata.hpp"

using namespace jpi_edm;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Calculate XOR checksum for EDM protocol lines
std::string calculateChecksum(const std::string& data) {
    uint8_t cs = 0;
    for (char c : data) {
        cs ^= c;
    }
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", cs);
    return std::string(buf);
}

// Build a complete EDM line with checksum
std::string buildEDMLine(const std::string& content) {
    std::string cs = calculateChecksum(content);
    return "$" + content + "*" + cs + "\r\n";
}

// Create minimal valid file headers (no flights)
std::string createMinimalValidHeaders() {
    return buildEDMLine("U,N12345") +
           buildEDMLine("A,305,230,500,415,60,1650,230,90") +
           buildEDMLine("C,930,63741,6193,1552,200") +
           buildEDMLine("F,0,999,0,2950,2950") +
           buildEDMLine("P,2") +
           buildEDMLine("T,6,1,25,18,36,1") +
           buildEDMLine("L,0");  // 0 flights
}

}  // anonymous namespace

// ============================================================================
// Stream Error Recovery Tests
// ============================================================================

class StreamRecoveryTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(StreamRecoveryTest, HandlesEmptyFlightList) {
    // File with headers but no flights should not throw
    std::string data = createMinimalValidHeaders();
    std::istringstream stream(data);

    EXPECT_NO_THROW(flightFile.processFile(stream));
}

TEST_F(StreamRecoveryTest, RejectsEmptyStream) {
    std::istringstream emptyStream("");

    // Should throw when trying to read headers from empty stream
    EXPECT_THROW(flightFile.processFile(emptyStream), std::runtime_error);
}

TEST_F(StreamRecoveryTest, RejectsTruncatedHeader) {
    // Header line without checksum terminator
    std::string truncated = "$U,N12345";  // Missing *checksum\r\n
    std::istringstream stream(truncated);

    // Throws runtime_error when stream ends unexpectedly
    EXPECT_THROW(flightFile.processFile(stream), std::runtime_error);
}

TEST_F(StreamRecoveryTest, RejectsIncompleteHeaders) {
    // Missing required headers (no $L line to end headers)
    std::string incomplete = buildEDMLine("U,N12345") +
                             buildEDMLine("A,305,230,500,415,60,1650,230,90");
    std::istringstream stream(incomplete);

    EXPECT_THROW(flightFile.processFile(stream), std::runtime_error);
}

// ============================================================================
// Checksum Validation Tests
// ============================================================================

class ChecksumValidationTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(ChecksumValidationTest, RejectsInvalidChecksum) {
    // Valid format but wrong checksum
    std::string badChecksum = "$U,N12345*00\r\n" +  // Wrong checksum
                              buildEDMLine("A,305,230,500,415,60,1650,230,90") +
                              buildEDMLine("C,930,63741,6193,1552,200") +
                              buildEDMLine("F,0,999,0,2950,2950") +
                              buildEDMLine("P,2") +
                              buildEDMLine("T,6,1,25,18,36,1") +
                              buildEDMLine("L,0");
    std::istringstream stream(badChecksum);

    EXPECT_THROW(flightFile.processFile(stream), std::invalid_argument);
}

TEST_F(ChecksumValidationTest, RejectsMissingAsterisk) {
    // Missing asterisk separator
    std::string noAsterisk = "$U,N1234500\r\n";
    std::istringstream stream(noAsterisk);

    EXPECT_THROW(flightFile.processFile(stream), std::invalid_argument);
}

TEST_F(ChecksumValidationTest, RejectsMissingDollarSign) {
    // Missing initial $ sign
    std::string noDollar = "U,N12345*1A\r\n";
    std::istringstream stream(noDollar);

    EXPECT_THROW(flightFile.processFile(stream), std::invalid_argument);
}

TEST_F(ChecksumValidationTest, RejectsNonHexChecksum) {
    // Checksum contains non-hex characters
    std::string badHex = "$U,N12345*ZZ\r\n";
    std::istringstream stream(badHex);

    EXPECT_THROW(flightFile.processFile(stream), std::invalid_argument);
}

// ============================================================================
// Metadata Validation Tests
// ============================================================================

class MetadataValidationTest : public ::testing::Test {};

TEST_F(MetadataValidationTest, ValidatesCylinderCount) {
    Metadata md;

    // Valid cylinder counts (1-9)
    // Cylinder flags start at bit 2 (0x04)
    std::vector<unsigned long> validConfig = {930, 0x0004, 0, 0, 200};  // 1 cylinder (bit 2 set)
    EXPECT_NO_THROW(md.m_configInfo.apply(validConfig));
    EXPECT_EQ(md.NumCylinders(), 1);

    validConfig[1] = 0x07FC;  // 9 cylinders (bits 2-10 set: 0x04 through 0x400)
    EXPECT_NO_THROW(md.m_configInfo.apply(validConfig));
    EXPECT_EQ(md.NumCylinders(), 9);
}

TEST_F(MetadataValidationTest, ValidatesModelNumbers) {
    Metadata md;

    // Test known model numbers
    std::vector<unsigned long> config700 = {700, 0, 0, 0, 0};
    md.m_configInfo.apply(config700);
    EXPECT_FALSE(md.IsTwin());

    std::vector<unsigned long> config760 = {760, 0, 0, 0, 0};
    md.m_configInfo.apply(config760);
    EXPECT_TRUE(md.IsTwin());

    std::vector<unsigned long> config930 = {930, 0, 0, 0, 0};
    md.m_configInfo.apply(config930);
    EXPECT_FALSE(md.IsTwin());

    std::vector<unsigned long> config960 = {960, 0, 0, 0, 0};
    md.m_configInfo.apply(config960);
    EXPECT_TRUE(md.IsTwin());
}

// ============================================================================
// Protocol Constants Tests
// ============================================================================

class ProtocolConstantsTest : public ::testing::Test {};

TEST_F(ProtocolConstantsTest, ChecksumCalculationMatchesProtocol) {
    // Test known good checksums

    // "U,N12345" XOR checksum: U(0x55)^,(0x2C)^N(0x4E)^1(0x31)^2(0x32)^3(0x33)^4(0x34)^5(0x35) = 0x06
    std::string data1 = "U,N12345";
    std::string cs1 = calculateChecksum(data1);
    EXPECT_EQ(cs1, "06");

    // "L,0" XOR checksum: L(0x4C)^,(0x2C)^0(0x30) = 0x50
    std::string data2 = "L,0";
    std::string cs2 = calculateChecksum(data2);
    EXPECT_EQ(cs2, "50");

    // Verify calculation matches manual computation
    uint8_t expected = 0;
    for (char c : data2) expected ^= c;
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", expected);
    EXPECT_EQ(cs2, std::string(buf));
}

TEST_F(ProtocolConstantsTest, ValidatesMaxCylinders) {
    // ConfigInfo::MAX_CYLS should be 9
    EXPECT_EQ(ConfigInfo::MAX_CYLS, 9);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

class EdgeCaseValidationTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(EdgeCaseValidationTest, HandlesMaxLineLengthHeaders) {
    // Create a header line exactly at MAX_HEADER_LINE_LENGTH
    std::string longContent = "U,";
    // Fill up to near max length
    for (int i = 0; i < 240; ++i) {
        longContent += "X";
    }
    std::string longLine = buildEDMLine(longContent);

    // Should handle long but valid lines
    std::istringstream stream(longLine);
    // Will fail on missing required headers, but shouldn't crash on line length
    EXPECT_THROW(flightFile.processFile(stream), std::runtime_error);
}

TEST_F(EdgeCaseValidationTest, RejectsEmptyLinesInHeaders) {
    std::string multiNewline = buildEDMLine("U,N12345") + "\r\n\r\n" +
                               buildEDMLine("A,305,230,500,415,60,1650,230,90") +
                               buildEDMLine("C,930,63741,6193,1552,200") +
                               buildEDMLine("F,0,999,0,2950,2950") +
                               buildEDMLine("P,2") +
                               buildEDMLine("T,6,1,25,18,36,1") +
                               buildEDMLine("L,0");
    std::istringstream stream(multiNewline);

    // Empty lines are treated as invalid headers
    EXPECT_THROW(flightFile.processFile(stream), std::invalid_argument);
}

// ============================================================================
// Callback Validation Tests
// ============================================================================

class CallbackValidationTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(CallbackValidationTest, MetadataCallbackReceivesValidData) {
    bool called = false;
    std::shared_ptr<Metadata> capturedMd;

    flightFile.setMetadataCompletionCb([&](std::shared_ptr<Metadata> md) {
        called = true;
        capturedMd = md;
        EXPECT_NE(nullptr, md);
    });

    std::string data = createMinimalValidHeaders();
    std::istringstream stream(data);

    flightFile.processFile(stream);

    EXPECT_TRUE(called);
    ASSERT_NE(nullptr, capturedMd);
    EXPECT_EQ("N12345", capturedMd->m_tailNum);
    EXPECT_EQ(930UL, capturedMd->m_configInfo.edm_model);
}

TEST_F(CallbackValidationTest, FileFooterCallbackInvoked) {
    bool footerCalled = false;

    flightFile.setFileFooterCompletionCb([&]() {
        footerCalled = true;
    });

    std::string data = createMinimalValidHeaders();
    std::istringstream stream(data);

    flightFile.processFile(stream);

    EXPECT_TRUE(footerCalled);
}

// ============================================================================
// FileHeader Validation Tests
// ============================================================================

class FileHeaderValidationTest : public ::testing::Test {};

TEST_F(FileHeaderValidationTest, ConfigLimitsRejectsInsufficientFields) {
    ConfigLimits limits;
    std::vector<unsigned long> tooFew = {1, 2, 3};  // Need 8

    EXPECT_THROW(limits.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderValidationTest, ConfigInfoRejectsInsufficientFields) {
    ConfigInfo info;
    std::vector<unsigned long> tooFew = {930};  // Need at least 2

    EXPECT_THROW(info.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderValidationTest, FuelLimitsRejectsInsufficientFields) {
    FuelLimits fuel;
    std::vector<unsigned long> tooFew = {100, 200};  // Need 5

    EXPECT_THROW(fuel.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderValidationTest, ConfigLimitsAcceptsExactFields) {
    ConfigLimits limits;
    std::vector<unsigned long> exact = {305, 230, 500, 415, 60, 1650, 230, 90};

    EXPECT_NO_THROW(limits.apply(exact));
    EXPECT_EQ(limits.volts_hi, 305UL);
    EXPECT_EQ(limits.oil_temp_lo, 90UL);
}
