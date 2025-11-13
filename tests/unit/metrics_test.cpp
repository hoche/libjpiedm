/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * Unit tests for Metrics classes
 */

#include <gtest/gtest.h>
#include <Metrics.hpp>
#include <MetricId.hpp>

using namespace jpi_edm;

// Test Metric class basic functionality
TEST(MetricTest, ConstructorSetsPropertiesCorrectly) {
    Metric metric(0x01, 5, 10, MetricId::EGT11, "EGT11", "Exhaust Gas Temperature 1-1",
                  Metric::ScaleFactor::TEN, Metric::InitialValue::ZERO);

    EXPECT_EQ(0x01, metric.getVersionMask());
    EXPECT_EQ(5, metric.getLowByteBitIdx());
    EXPECT_TRUE(metric.getHighByteBitIdx().has_value());
    EXPECT_EQ(10, metric.getHighByteBitIdx().value());
    EXPECT_EQ(MetricId::EGT11, metric.getMetricId());
    EXPECT_EQ("EGT11", metric.getShortName());
    EXPECT_EQ("Exhaust Gas Temperature 1-1", metric.getName());
    EXPECT_EQ(Metric::ScaleFactor::TEN, metric.getScaleFactor());
    EXPECT_FLOAT_EQ(0.0f, metric.getInitialValue());
}

TEST(MetricTest, ConstructorWithoutHighByteBit) {
    Metric metric(0x02, 3, MetricId::CHT11, "CHT11", "Cylinder Head Temperature 1-1");

    EXPECT_EQ(0x02, metric.getVersionMask());
    EXPECT_EQ(3, metric.getLowByteBitIdx());
    EXPECT_FALSE(metric.getHighByteBitIdx().has_value());
    EXPECT_EQ(MetricId::CHT11, metric.getMetricId());
    EXPECT_EQ("CHT11", metric.getShortName());
    EXPECT_EQ("Cylinder Head Temperature 1-1", metric.getName());
}

TEST(MetricTest, DefaultInitialValueIsF0) {
    Metric metric(0x01, 0, MetricId::RPM1, "RPM1", "RPM1");
    EXPECT_FLOAT_EQ(0xF0, metric.getInitialValue());
}

TEST(MetricTest, ZeroInitialValueIsZero) {
    Metric metric(0x01, 0, MetricId::RPM1, "RPM1", "RPM1",
                  Metric::ScaleFactor::NONE, Metric::InitialValue::ZERO);
    EXPECT_FLOAT_EQ(0.0f, metric.getInitialValue());
}

// Test ScaleFactor enum
TEST(MetricTest, ScaleFactorValues) {
    EXPECT_EQ(Metric::ScaleFactor::NONE, Metric::ScaleFactor::NONE);
    EXPECT_EQ(Metric::ScaleFactor::TEN, Metric::ScaleFactor::TEN);
    EXPECT_EQ(Metric::ScaleFactor::TEN_IF_GPH, Metric::ScaleFactor::TEN_IF_GPH);
}

// Test getters return correct values
TEST(MetricTest, GettersReturnCorrectValues) {
    Metric metric(0x04, 7, 15, MetricId::TIT11, "TIT11", "Turbine Inlet Temp 1-1",
                  Metric::ScaleFactor::TEN_IF_GPH);

    EXPECT_EQ(0x04, metric.getVersionMask());
    EXPECT_EQ(7, metric.getLowByteBitIdx());
    EXPECT_EQ(15, metric.getHighByteBitIdx().value());
    EXPECT_EQ(MetricId::TIT11, metric.getMetricId());
    EXPECT_EQ("TIT11", metric.getShortName());
    EXPECT_EQ("Turbine Inlet Temp 1-1", metric.getName());
    EXPECT_EQ(Metric::ScaleFactor::TEN_IF_GPH, metric.getScaleFactor());
}

// Test various version masks
TEST(MetricTest, VersionMaskCanBeVariousValues) {
    Metric v1(0x01, 0, MetricId::EGT11, "EGT11", "EGT11");
    Metric v2(0x02, 0, MetricId::EGT11, "EGT11", "EGT11");
    Metric v3(0x04, 0, MetricId::EGT11, "EGT11", "EGT11");
    Metric v4(0x08, 0, MetricId::EGT11, "EGT11", "EGT11");
    Metric v5(0x10, 0, MetricId::EGT11, "EGT11", "EGT11");

    EXPECT_EQ(0x01, v1.getVersionMask());
    EXPECT_EQ(0x02, v2.getVersionMask());
    EXPECT_EQ(0x04, v3.getVersionMask());
    EXPECT_EQ(0x08, v4.getVersionMask());
    EXPECT_EQ(0x10, v5.getVersionMask());
}

// Test bit indices can be in valid ranges
TEST(MetricTest, BitIndicesWithinValidRange) {
    // Old format uses 0-47 bits
    Metric old_format(0x01, 47, MetricId::MARK, "MARK", "Mark");
    EXPECT_EQ(47, old_format.getLowByteBitIdx());

    // New format uses 0-127 bits
    Metric new_format(0x08, 127, MetricId::DIF1, "DIF1", "DIF1");
    EXPECT_EQ(127, new_format.getLowByteBitIdx());
}

// Test short names and long names
TEST(MetricTest, NamesCanContainSpaces) {
    Metric metric(0x01, 0, MetricId::OAT, "OAT", "Outside Air Temperature");
    EXPECT_EQ("OAT", metric.getShortName());
    EXPECT_EQ("Outside Air Temperature", metric.getName());
}

TEST(MetricTest, NamesCanBeIdentical) {
    Metric metric(0x01, 0, MetricId::RPM1, "RPM1", "RPM1");
    EXPECT_EQ("RPM1", metric.getShortName());
    EXPECT_EQ("RPM1", metric.getName());
}

// Test MetricId enum values (spot check a few)
TEST(MetricIdTest, MetricIdEnumValues) {
    EXPECT_NE(MetricId::EGT11, MetricId::EGT12);
    EXPECT_NE(MetricId::CHT11, MetricId::CHT12);
    EXPECT_NE(MetricId::TIT11, MetricId::TIT12);
    EXPECT_NE(MetricId::RPM1, MetricId::MAP1);
}
