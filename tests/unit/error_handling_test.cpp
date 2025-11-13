/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * @brief Error handling and edge case tests for libjpiedm
 */

#include <gtest/gtest.h>

#include <climits>
#include <sstream>
#include <stdexcept>

#include "FileHeaders.hpp"
#include "FlightFile.hpp"
#include "Metadata.hpp"

using namespace jpi_edm;

// ============================================================================
// Helper Functions
// ============================================================================

// Calculate XOR checksum for EDM protocol lines
std::string calculateChecksum(const std::string& data) {
    // Data should be everything between $ and *
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

// ============================================================================
// FileHeader Error Path Tests
// ============================================================================

class FileHeaderErrorTest : public ::testing::Test {};

TEST_F(FileHeaderErrorTest, ConfigLimitsThrowsOnInsufficientFields) {
    ConfigLimits limits;
    std::vector<unsigned long> tooFew = {1, 2, 3};  // Need 8

    EXPECT_THROW(limits.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderErrorTest, ConfigInfoThrowsOnInsufficientFields) {
    ConfigInfo info;
    std::vector<unsigned long> tooFew = {1, 2};  // Need at least 5

    EXPECT_THROW(info.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderErrorTest, FuelLimitsThrowsOnInsufficientFields) {
    FuelLimits limits;
    std::vector<unsigned long> tooFew = {1, 2};  // Need 5

    EXPECT_THROW(limits.apply(tooFew), std::invalid_argument);
}

TEST_F(FileHeaderErrorTest, ProtoHeaderThrowsOnInsufficientFields) {
    ProtoHeader header;
    std::vector<unsigned long> empty = {};  // Need 1

    EXPECT_THROW(header.apply(empty), std::invalid_argument);
}

TEST_F(FileHeaderErrorTest, TimeStampThrowsOnInsufficientFields) {
    TimeStamp ts;
    std::vector<unsigned long> tooFew = {1, 2, 3};  // Need 6

    EXPECT_THROW(ts.apply(tooFew), std::invalid_argument);
}

// ============================================================================
// FlightFile Error Path Tests
// ============================================================================

class FlightFileErrorTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(FlightFileErrorTest, ThrowsOnEmptyStream) {
    std::istringstream emptyStream("");

    EXPECT_THROW(flightFile.processFile(emptyStream), std::runtime_error);
}

TEST_F(FlightFileErrorTest, ThrowsOnStreamWithoutDollarSign) {
    std::istringstream invalidStream("No dollar sign at start\n");

    EXPECT_THROW(flightFile.processFile(invalidStream), std::invalid_argument);
}

TEST_F(FlightFileErrorTest, ThrowsOnInvalidChecksum) {
    // Valid format but wrong checksum (using 00 instead of correct checksum)
    std::istringstream badChecksum("$A,305,230,500,415,60,1650,230,90*00\r\n");

    EXPECT_THROW(flightFile.processFile(badChecksum), std::invalid_argument);
}

TEST_F(FlightFileErrorTest, ThrowsOnMissingRequiredHeader) {
    // Missing the $L header that indicates end of headers
    std::string data = buildEDMLine("U,N12345");
    std::istringstream incompleteHeaders(data);

    EXPECT_THROW(flightFile.processFile(incompleteHeaders), std::runtime_error);
}

TEST_F(FlightFileErrorTest, ThrowsOnMalformedHeaderLine) {
    // Missing the asterisk separator for checksum
    std::istringstream malformed("$U,N1234500\r\n");

    EXPECT_THROW(flightFile.processFile(malformed), std::invalid_argument);
}

TEST_F(FlightFileErrorTest, ThrowsOnNonNumericFieldValue) {
    // Non-numeric value in $A record
    std::istringstream badValue("$A,305,ABC,500,415,60,1650,230,90*7F\r\n");

    EXPECT_THROW(flightFile.processFile(badValue), std::invalid_argument);
}

TEST_F(FlightFileErrorTest, HandlesUnknownHeaderGracefully) {
    // Unknown header type should log warning but not throw
    std::string data =
        buildEDMLine("Z,unknown,header,type") +
        buildEDMLine("U,N12345") +
        buildEDMLine("A,305,230,500,415,60,1650,230,90") +
        buildEDMLine("C,930,63741,6193,1552,200") +
        buildEDMLine("F,0,999,0,2950,2950") +
        buildEDMLine("P,2") +
        buildEDMLine("T,6,1,25,18,36,1") +
        buildEDMLine("L,0");

    std::istringstream unknownHeader(data);

    // Should not throw - unknown headers are warnings only
    EXPECT_NO_THROW(flightFile.processFile(unknownHeader));
}

TEST_F(FlightFileErrorTest, HandlesSentinelValueStorage) {
    // Test that 999999999 sentinel value is stored (note: conversion to USHRT_MAX not implemented)
    ConfigLimits limits;
    std::vector<unsigned long> values = {305, 230, 500, 415, 999999999, 1650, 230, 90};

    ASSERT_NO_THROW(limits.apply(values));
    // Currently the sentinel value is stored as-is (conversion not implemented)
    EXPECT_EQ(limits.shock_cooling_cld, 999999999UL);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

class EdgeCaseTest : public ::testing::Test {};

TEST_F(EdgeCaseTest, ConfigInfoHandlesMaxCylinders) {
    ConfigInfo info;
    // Set flags to indicate 9 cylinders (maximum)
    std::vector<unsigned long> values = {930, 0x01FC, 63741, 6193, 108};  // Flags with 9 cylinder bits set

    ASSERT_NO_THROW(info.apply(values));
    EXPECT_LE(info.numCylinders, ConfigInfo::MAX_CYLS);
}

TEST_F(EdgeCaseTest, ConfigInfoHandlesZeroCylinders) {
    ConfigInfo info;
    // Flags with no cylinder bits set
    std::vector<unsigned long> values = {930, 0, 63741, 6193, 108};

    ASSERT_NO_THROW(info.apply(values));
    EXPECT_EQ(info.numCylinders, 0);
}

TEST_F(EdgeCaseTest, MetadataHandlesAllModelNumbers) {
    Metadata md760, md960, md700, md900, md930;

    // Test EDM 760 (twin) - need to call apply() to set isTwin flag
    std::vector<unsigned long> values760 = {760, 0, 0, 0, 0};
    md760.m_configInfo.apply(values760);
    EXPECT_EQ(md760.ProtoVersion(), EDMVersion::V2);
    EXPECT_TRUE(md760.IsTwin());

    // Test EDM 960 (twin) - need to call apply() to set isTwin flag
    std::vector<unsigned long> values960 = {960, 0, 0, 0, 0};
    md960.m_configInfo.apply(values960);
    EXPECT_EQ(md960.ProtoVersion(), EDMVersion::V5);
    EXPECT_TRUE(md960.IsTwin());

    // Test EDM 700 (single, old)
    std::vector<unsigned long> values700 = {700, 0, 0, 0, 0};
    md700.m_configInfo.apply(values700);
    md700.m_protoHeader.value = 1;
    EXPECT_EQ(md700.ProtoVersion(), EDMVersion::V1);
    EXPECT_FALSE(md700.IsTwin());

    // Test EDM 900 with old firmware
    std::vector<unsigned long> values900 = {900, 0, 0, 0, 100};
    md900.m_configInfo.apply(values900);
    EXPECT_EQ(md900.ProtoVersion(), EDMVersion::V1);

    // Test EDM 930 with new firmware
    std::vector<unsigned long> values930 = {930, 0, 0, 0, 200};
    md930.m_configInfo.apply(values930);
    EXPECT_EQ(md930.ProtoVersion(), EDMVersion::V4);
}

TEST_F(EdgeCaseTest, TimeStampHandlesEdgeDates) {
    TimeStamp ts;

    // Test minimum values
    std::vector<unsigned long> minValues = {1, 1, 0, 0, 0, 0};
    ASSERT_NO_THROW(ts.apply(minValues));
    EXPECT_EQ(ts.mon, 1);
    EXPECT_EQ(ts.day, 1);
    EXPECT_EQ(ts.yr, 0);

    // Test maximum reasonable values
    std::vector<unsigned long> maxValues = {12, 31, 9999, 23, 59, 999};
    ASSERT_NO_THROW(ts.apply(maxValues));
    EXPECT_EQ(ts.mon, 12);
    EXPECT_EQ(ts.day, 31);
    EXPECT_EQ(ts.yr, 9999);
}

TEST_F(EdgeCaseTest, FuelLimitsHandlesBothUnits) {
    FuelLimits gph, lph;

    // Test GPH (units = 0)
    std::vector<unsigned long> gphValues = {0, 100, 50, 2950, 2950};
    gph.apply(gphValues);
    EXPECT_EQ(gph.units, 0);

    // Test LPH (units = 1)
    std::vector<unsigned long> lphValues = {1, 380, 190, 11000, 11000};
    lph.apply(lphValues);
    EXPECT_EQ(lph.units, 1);
}

// ============================================================================
// Boundary Value Tests
// ============================================================================

class BoundaryValueTest : public ::testing::Test {};

TEST_F(BoundaryValueTest, ConfigLimitsAcceptsExactFieldCount) {
    ConfigLimits limits;
    std::vector<unsigned long> exact = {305, 230, 500, 415, 60, 1650, 230, 90};

    ASSERT_NO_THROW(limits.apply(exact));
}

TEST_F(BoundaryValueTest, ConfigLimitsAcceptsExtraFields) {
    ConfigLimits limits;
    std::vector<unsigned long> extra = {305, 230, 500, 415, 60, 1650, 230, 90, 999};

    // Should not throw - extra fields are ignored
    EXPECT_NO_THROW(limits.apply(extra));
}

TEST_F(BoundaryValueTest, ConfigInfoAcceptsMinimumFields) {
    ConfigInfo info;
    std::vector<unsigned long> minimum = {930, 63741, 6193, 1552, 200};

    ASSERT_NO_THROW(info.apply(minimum));
}

TEST_F(BoundaryValueTest, ProtoHeaderAcceptsExactFieldCount) {
    ProtoHeader header;
    std::vector<unsigned long> exact = {2};

    ASSERT_NO_THROW(header.apply(exact));
}

// ============================================================================
// Callback Error Handling Tests
// ============================================================================

class CallbackErrorTest : public ::testing::Test {
protected:
    FlightFile flightFile;
};

TEST_F(CallbackErrorTest, ContinuesIfCallbackNotSet) {
    // Don't set any callbacks - should still parse without error
    std::string data =
        buildEDMLine("U,N12345") +
        buildEDMLine("A,305,230,500,415,60,1650,230,90") +
        buildEDMLine("C,930,63741,6193,1552,200") +
        buildEDMLine("F,0,999,0,2950,2950") +
        buildEDMLine("P,2") +
        buildEDMLine("T,6,1,25,18,36,1") +
        buildEDMLine("L,0");

    std::istringstream validStream(data);

    EXPECT_NO_THROW(flightFile.processFile(validStream));
}

TEST_F(CallbackErrorTest, CallbackCanAccessMetadata) {
    bool callbackCalled = false;
    std::shared_ptr<Metadata> capturedMetadata;

    flightFile.setMetadataCompletionCb([&](std::shared_ptr<Metadata> md) {
        callbackCalled = true;
        capturedMetadata = md;
        EXPECT_NE(nullptr, md);
        EXPECT_NE("", md->m_tailNum);
    });

    std::string data =
        buildEDMLine("U,N12345") +
        buildEDMLine("A,305,230,500,415,60,1650,230,90") +
        buildEDMLine("C,930,63741,6193,1552,200") +
        buildEDMLine("F,0,999,0,2950,2950") +
        buildEDMLine("P,2") +
        buildEDMLine("T,6,1,25,18,36,1") +
        buildEDMLine("L,0");

    std::istringstream validStream(data);

    flightFile.processFile(validStream);

    EXPECT_TRUE(callbackCalled);
    ASSERT_NE(nullptr, capturedMetadata);
    EXPECT_EQ("N12345", capturedMetadata->m_tailNum);
}
