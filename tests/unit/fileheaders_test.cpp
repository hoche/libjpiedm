/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * Unit tests for FileHeaders classes
 */

#include <gtest/gtest.h>
#include <FileHeaders.hpp>
#include <sstream>
#include <vector>

using namespace jpi_edm;

// ConfigLimits Tests
class ConfigLimitsTest : public ::testing::Test {
protected:
    ConfigLimits configLimits;
};

TEST_F(ConfigLimitsTest, ApplyWithValidValues) {
    std::vector<unsigned long> values = {305, 230, 500, 415, 60, 1650, 230, 90};

    EXPECT_NO_THROW(configLimits.apply(values));

    EXPECT_EQ(305, configLimits.volts_hi);
    EXPECT_EQ(230, configLimits.volts_lo);
    EXPECT_EQ(500, configLimits.egt_diff);
    EXPECT_EQ(415, configLimits.cht_temp_hi);
    EXPECT_EQ(60, configLimits.shock_cooling_cld);
    EXPECT_EQ(1650, configLimits.turbo_inlet_temp_hi);
    EXPECT_EQ(230, configLimits.oil_temp_hi);
    EXPECT_EQ(90, configLimits.oil_temp_lo);
}

TEST_F(ConfigLimitsTest, ApplyThrowsWithInsufficientValues) {
    std::vector<unsigned long> values = {305, 230, 500}; // Only 3 values

    EXPECT_THROW(configLimits.apply(values), std::invalid_argument);
}

TEST_F(ConfigLimitsTest, DumpOutputsExpectedFormat) {
    std::vector<unsigned long> values = {305, 230, 500, 415, 60, 1650, 230, 90};
    configLimits.apply(values);

    std::ostringstream output;
    configLimits.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("ConfigLimits:") != std::string::npos);
    EXPECT_TRUE(result.find("volts_hi: 305") != std::string::npos);
    EXPECT_TRUE(result.find("volts_lo: 230") != std::string::npos);
    EXPECT_TRUE(result.find("egt_diff: 500") != std::string::npos);
}

// ConfigInfo Tests
class ConfigInfoTest : public ::testing::Test {
protected:
    ConfigInfo configInfo;
};

TEST_F(ConfigInfoTest, ApplyWithValidValues) {
    // Example: $C,700,63741,6193,1552,292
    std::vector<unsigned long> values = {700, 63741, 6193, 1552, 292};

    EXPECT_NO_THROW(configInfo.apply(values));

    EXPECT_EQ(700, configInfo.edm_model);
    EXPECT_EQ(292, configInfo.firmware_version);
}

TEST_F(ConfigInfoTest, ApplyThrowsWithInsufficientValues) {
    std::vector<unsigned long> values = {700, 63741}; // Only 2 values

    EXPECT_THROW(configInfo.apply(values), std::invalid_argument);
}

TEST_F(ConfigInfoTest, DefaultValuesAreCorrect) {
    EXPECT_EQ(0, configInfo.edm_model);
    EXPECT_EQ(0, configInfo.flags);
    EXPECT_FALSE(configInfo.isTwin);
    EXPECT_EQ(4, configInfo.numCylinders); // Default is 4 cylinders
}

TEST_F(ConfigInfoTest, DumpOutputsExpectedFormat) {
    std::vector<unsigned long> values = {930, 63741, 6193, 1552, 200};
    configInfo.apply(values);

    std::ostringstream output;
    configInfo.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("ConfigInfo:") != std::string::npos);
    EXPECT_TRUE(result.find("EDM Model: 930") != std::string::npos);
}

// FuelLimits Tests
class FuelLimitsTest : public ::testing::Test {
protected:
    FuelLimits fuelLimits;
};

TEST_F(FuelLimitsTest, ApplyWithValidValues) {
    // Example: $F,0,999,0,2950,2950
    std::vector<unsigned long> values = {0, 999, 0, 2950, 2950};

    EXPECT_NO_THROW(fuelLimits.apply(values));

    EXPECT_EQ(0, fuelLimits.units); // 0 = GPH
    EXPECT_EQ(999, fuelLimits.main_tank_size);
    EXPECT_EQ(0, fuelLimits.aux_tank_size);
    EXPECT_EQ(2950, fuelLimits.k_factor_1);
    EXPECT_EQ(2950, fuelLimits.k_factor_2);
}

TEST_F(FuelLimitsTest, ApplyThrowsWithInsufficientValues) {
    std::vector<unsigned long> values = {0, 999}; // Only 2 values

    EXPECT_THROW(fuelLimits.apply(values), std::invalid_argument);
}

TEST_F(FuelLimitsTest, DefaultValuesAreZero) {
    EXPECT_EQ(0, fuelLimits.units);
    EXPECT_EQ(0, fuelLimits.main_tank_size);
    EXPECT_EQ(0, fuelLimits.aux_tank_size);
    EXPECT_EQ(0, fuelLimits.k_factor_1);
    EXPECT_EQ(0, fuelLimits.k_factor_2);
}

TEST_F(FuelLimitsTest, DumpOutputsExpectedFormat) {
    std::vector<unsigned long> values = {0, 999, 0, 2950, 2950};
    fuelLimits.apply(values);

    std::ostringstream output;
    fuelLimits.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("FuelLimits:") != std::string::npos);
    EXPECT_TRUE(result.find("units: 0") != std::string::npos);
}

// ProtoHeader Tests
class ProtoHeaderTest : public ::testing::Test {
protected:
    ProtoHeader protoHeader;
};

TEST_F(ProtoHeaderTest, ApplyWithValidValue) {
    std::vector<unsigned long> values = {2};

    EXPECT_NO_THROW(protoHeader.apply(values));
    EXPECT_EQ(2, protoHeader.value);
}

TEST_F(ProtoHeaderTest, ApplyThrowsWithNoValues) {
    std::vector<unsigned long> values = {};

    EXPECT_THROW(protoHeader.apply(values), std::invalid_argument);
}

TEST_F(ProtoHeaderTest, DefaultValueIsZero) {
    EXPECT_EQ(0, protoHeader.value);
}

TEST_F(ProtoHeaderTest, DumpOutputsExpectedFormat) {
    std::vector<unsigned long> values = {2};
    protoHeader.apply(values);

    std::ostringstream output;
    protoHeader.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("ProtoHeader:") != std::string::npos);
    EXPECT_TRUE(result.find("2") != std::string::npos);
}

// TimeStamp Tests
class TimeStampTest : public ::testing::Test {
protected:
    TimeStamp timeStamp;
};

TEST_F(TimeStampTest, ApplyWithValidValues) {
    // Example: $T,5,13,5,23,2,2222
    // MM, DD, YY, hh, mm, seq
    std::vector<unsigned long> values = {5, 13, 5, 23, 2, 2222};

    EXPECT_NO_THROW(timeStamp.apply(values));

    EXPECT_EQ(5, timeStamp.mon);
    EXPECT_EQ(13, timeStamp.day);
    EXPECT_EQ(5, timeStamp.yr);
    EXPECT_EQ(23, timeStamp.hh);
    EXPECT_EQ(2, timeStamp.mm);
}

TEST_F(TimeStampTest, ApplyThrowsWithInsufficientValues) {
    std::vector<unsigned long> values = {5, 13, 5}; // Only 3 values

    EXPECT_THROW(timeStamp.apply(values), std::invalid_argument);
}

TEST_F(TimeStampTest, DumpOutputsExpectedFormat) {
    std::vector<unsigned long> values = {5, 13, 5, 23, 2, 2222};
    timeStamp.apply(values);

    std::ostringstream output;
    timeStamp.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("TimeStamp:") != std::string::npos);
}
