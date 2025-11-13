/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * Unit tests for Metadata class
 */

#include <gtest/gtest.h>
#include <Metadata.hpp>
#include <sstream>

using namespace jpi_edm;

class MetadataTest : public ::testing::Test {
protected:
    Metadata metadata;

    void SetUp() override {
        // Initialize with default values
        metadata.m_tailNum = "N12345";
    }
};

// Test IsTwin() functionality
TEST_F(MetadataTest, IsTwinReturnsFalseByDefault) {
    EXPECT_FALSE(metadata.IsTwin());
}

TEST_F(MetadataTest, IsTwinReturnsTrueWhenConfigured) {
    metadata.m_configInfo.isTwin = true;
    EXPECT_TRUE(metadata.IsTwin());
}

// Test NumCylinders() functionality
TEST_F(MetadataTest, NumCylindersReturnsConfiguredValue) {
    metadata.m_configInfo.numCylinders = 6;
    EXPECT_EQ(6, metadata.NumCylinders());

    metadata.m_configInfo.numCylinders = 4;
    EXPECT_EQ(4, metadata.NumCylinders());
}

// Test IsGPH() functionality (gallons per hour vs liters per hour)
TEST_F(MetadataTest, IsGPHReturnsTrueWhenUnitsAreZero) {
    metadata.m_fuelLimits.units = 0;
    EXPECT_TRUE(metadata.IsGPH());
}

TEST_F(MetadataTest, IsGPHReturnsFalseWhenUnitsAreNonZero) {
    metadata.m_fuelLimits.units = 1;
    EXPECT_FALSE(metadata.IsGPH());
}

// Test ProtoVersion() for EDM 760
TEST_F(MetadataTest, ProtoVersionReturnsV2ForEDM760) {
    metadata.m_configInfo.edm_model = 760;
    EXPECT_EQ(EDMVersion::V2, metadata.ProtoVersion());
}

// Test ProtoVersion() for EDM 960
TEST_F(MetadataTest, ProtoVersionReturnsV5ForEDM960) {
    metadata.m_configInfo.edm_model = 960;
    EXPECT_EQ(EDMVersion::V5, metadata.ProtoVersion());
}

// Test ProtoVersion() for old models (< 900)
TEST_F(MetadataTest, ProtoVersionReturnsV1ForOldModelWithLowProtoValue) {
    metadata.m_configInfo.edm_model = 800;
    metadata.m_protoHeader.value = 1;
    EXPECT_EQ(EDMVersion::V1, metadata.ProtoVersion());
}

TEST_F(MetadataTest, ProtoVersionReturnsV4ForOldModelWithHighProtoValue) {
    metadata.m_configInfo.edm_model = 800;
    metadata.m_protoHeader.value = 2;
    EXPECT_EQ(EDMVersion::V4, metadata.ProtoVersion());
}

// Test ProtoVersion() for 900+ models
TEST_F(MetadataTest, ProtoVersionReturnsV1For900WithOldFirmware) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_configInfo.firmware_version = 108;
    EXPECT_EQ(EDMVersion::V1, metadata.ProtoVersion());
}

TEST_F(MetadataTest, ProtoVersionReturnsV4For900WithNewFirmware) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_configInfo.firmware_version = 109;
    EXPECT_EQ(EDMVersion::V4, metadata.ProtoVersion());
}

// Test IsOldRecFormat()
TEST_F(MetadataTest, IsOldRecFormatReturnsTrueForV1) {
    metadata.m_configInfo.edm_model = 800;
    metadata.m_protoHeader.value = 1;
    EXPECT_TRUE(metadata.IsOldRecFormat());
}

TEST_F(MetadataTest, IsOldRecFormatReturnsTrueForV2) {
    metadata.m_configInfo.edm_model = 760;
    EXPECT_TRUE(metadata.IsOldRecFormat());
}

TEST_F(MetadataTest, IsOldRecFormatReturnsFalseForV4) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_configInfo.firmware_version = 200;
    EXPECT_FALSE(metadata.IsOldRecFormat());
}

// Test GuessFlightHeaderVersion()
TEST_F(MetadataTest, GuessFlightHeaderVersionReturnsV1ForOldModel) {
    metadata.m_configInfo.edm_model = 800;
    metadata.m_protoHeader.value = 0;
    EXPECT_EQ(HeaderVersion::HEADER_V1, metadata.GuessFlightHeaderVersion());
}

TEST_F(MetadataTest, GuessFlightHeaderVersionReturnsV2ForMidRangeModel) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_protoHeader.value = 2;
    metadata.m_configInfo.build_maj = 900;
    EXPECT_EQ(HeaderVersion::HEADER_V2, metadata.GuessFlightHeaderVersion());
}

TEST_F(MetadataTest, GuessFlightHeaderVersionReturnsV3ForNewerModel) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_protoHeader.value = 2;
    metadata.m_configInfo.build_maj = 1000;
    EXPECT_EQ(HeaderVersion::HEADER_V3, metadata.GuessFlightHeaderVersion());
}

TEST_F(MetadataTest, GuessFlightHeaderVersionReturnsV4ForNewestModel) {
    metadata.m_configInfo.edm_model = 930;
    metadata.m_protoHeader.value = 2;
    metadata.m_configInfo.build_maj = 2015;
    EXPECT_EQ(HeaderVersion::HEADER_V4, metadata.GuessFlightHeaderVersion());
}

// Test dump() functionality
TEST_F(MetadataTest, DumpOutputsExpectedFormat) {
    metadata.m_tailNum = "N12345";
    metadata.m_configInfo.edm_model = 930;
    metadata.m_configInfo.firmware_version = 200;

    std::ostringstream output;
    metadata.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("Tailnumber: N12345") != std::string::npos);
    EXPECT_TRUE(result.find("Old Rec Format:") != std::string::npos);
}
