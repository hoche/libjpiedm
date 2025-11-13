/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * Unit tests for Flight classes
 */

#include <gtest/gtest.h>
#include <Flight.hpp>
#include <Metadata.hpp>
#include <sstream>
#include <memory>

using namespace jpi_edm;

// Test fixture with helper methods
class FlightTest : public ::testing::Test {
protected:
    std::shared_ptr<Metadata> metadata;
    std::shared_ptr<Flight> flight;

    void SetUp() override {
        // Create metadata with default values
        metadata = std::make_shared<Metadata>();
        metadata->m_configInfo.edm_model = 930;
        metadata->m_configInfo.firmware_version = 200;
        metadata->m_configInfo.numCylinders = 6;
        metadata->m_configInfo.isTwin = false;
        metadata->m_fuelLimits.units = 0; // GPH
        metadata->m_protoHeader.value = 2;
    }

    void createFlight() {
        flight = std::make_shared<Flight>(metadata);
    }
};

// FlightHeader Tests
class FlightHeaderTest : public ::testing::Test {
protected:
    FlightHeader header;
};

TEST_F(FlightHeaderTest, DefaultConstruction) {
    EXPECT_EQ(0, header.startDate.tm_year);
    EXPECT_EQ(0, header.startDate.tm_mon);
    EXPECT_EQ(0, header.startDate.tm_mday);
}

TEST_F(FlightHeaderTest, DumpProducesOutput) {
    header.flight_num = 42;
    header.flags = 0x12345678;
    header.interval = 6;

    std::ostringstream output;
    header.dump(output);

    std::string result = output.str();
    EXPECT_TRUE(result.find("Flight Header:") != std::string::npos);
    EXPECT_TRUE(result.find("flight_num: 42") != std::string::npos);
    EXPECT_TRUE(result.find("interval: 6") != std::string::npos);
}

TEST_F(FlightHeaderTest, SetAndReadValues) {
    header.flight_num = 123;
    header.flags = 0xABCDEF01;
    header.interval = 1;
    header.startLat = 37500000;
    header.startLng = -122400000;

    EXPECT_EQ(123, header.flight_num);
    EXPECT_EQ(0xABCDEF01, header.flags);
    EXPECT_EQ(1, header.interval);
    EXPECT_EQ(37500000, header.startLat);
    EXPECT_EQ(-122400000, header.startLng);
}

// FlightMetricsRecord Tests
TEST(FlightMetricsRecordTest, ConstructionWithData) {
    std::map<MetricId, float> metrics;
    metrics[EGT11] = 1450.0f;
    metrics[CHT11] = 380.0f;
    metrics[RPM1] = 2500.0f;

    FlightMetricsRecord record(false, 42, metrics);

    EXPECT_FALSE(record.m_isFast);
    EXPECT_EQ(42, record.m_recordSeq);
    EXPECT_EQ(3, record.m_metrics.size());
    EXPECT_FLOAT_EQ(1450.0f, record.m_metrics[EGT11]);
    EXPECT_FLOAT_EQ(380.0f, record.m_metrics[CHT11]);
    EXPECT_FLOAT_EQ(2500.0f, record.m_metrics[RPM1]);
}

TEST(FlightMetricsRecordTest, FastFlagIsRespected) {
    std::map<MetricId, float> metrics;

    FlightMetricsRecord slowRecord(false, 1, metrics);
    FlightMetricsRecord fastRecord(true, 2, metrics);

    EXPECT_FALSE(slowRecord.m_isFast);
    EXPECT_TRUE(fastRecord.m_isFast);
}

// Flight Class Tests
TEST_F(FlightTest, CannotDefaultConstruct) {
    // Flight() = delete, so this shouldn't compile
    // Just verify that explicit construction works
    EXPECT_NO_THROW(createFlight());
}

TEST_F(FlightTest, ConstructionInitializesMetrics) {
    createFlight();

    EXPECT_NE(nullptr, flight);
    EXPECT_EQ(metadata, flight->m_metadata);
    EXPECT_GT(flight->m_metricValues.size(), 0);
}

TEST_F(FlightTest, InitialSequenceIsZero) {
    createFlight();

    EXPECT_EQ(0, flight->m_recordSeq);
    EXPECT_FALSE(flight->m_fastFlag);
    EXPECT_EQ(0, flight->m_stdRecCount);
    EXPECT_EQ(0, flight->m_fastRecCount);
}

TEST_F(FlightTest, IncrementSequence) {
    createFlight();

    flight->incrementSequence();
    EXPECT_EQ(1, flight->m_recordSeq);

    flight->incrementSequence();
    EXPECT_EQ(2, flight->m_recordSeq);
}

TEST_F(FlightTest, SetFastFlag) {
    createFlight();

    EXPECT_FALSE(flight->m_fastFlag);

    flight->setFastFlag(true);
    EXPECT_TRUE(flight->m_fastFlag);

    flight->setFastFlag(false);
    EXPECT_FALSE(flight->m_fastFlag);
}

TEST_F(FlightTest, MetricMapIsPopulated) {
    createFlight();

    // Should have metrics based on EDM version
    EXPECT_GT(flight->m_bit2MetricMap.size(), 0);
    EXPECT_GT(flight->m_metricValues.size(), 0);
}

TEST_F(FlightTest, InitialValuesAreSetCorrectly) {
    createFlight();

    // Check that DIF1 and DIF2 are initialized to 0
    EXPECT_FLOAT_EQ(0.0f, flight->m_metricValues[DIF1]);
    EXPECT_FLOAT_EQ(0.0f, flight->m_metricValues[DIF2]);
}

TEST_F(FlightTest, UpdateMetricsWithSimpleValue) {
    createFlight();

    // Get initial value for a metric
    auto initialIt = flight->m_metricValues.find(RPM1);
    if (initialIt != flight->m_metricValues.end()) {
        float initialValue = initialIt->second;

        // Find the bit index for RPM1
        int rpm1BitIdx = -1;
        for (const auto& [bitIdx, metric] : flight->m_bit2MetricMap) {
            if (metric.getMetricId() == RPM1) {
                rpm1BitIdx = bitIdx;
                break;
            }
        }

        if (rpm1BitIdx >= 0) {
            // Update with a delta
            std::map<int, int> values;
            values[rpm1BitIdx] = 100;

            flight->updateMetrics(values);

            // Value should have increased
            EXPECT_FLOAT_EQ(initialValue + 100.0f, flight->m_metricValues[RPM1]);
        }
    }
}

TEST_F(FlightTest, UpdateMetricsWithScaling) {
    // Test with GPH (units = 0) for fuel flow scaling
    metadata->m_fuelLimits.units = 0; // GPH
    createFlight();

    // Find a metric that uses TEN_IF_GPH scaling
    for (const auto& [bitIdx, metric] : flight->m_bit2MetricMap) {
        if (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH) {
            MetricId metricId = metric.getMetricId();
            float initialValue = flight->m_metricValues[metricId];

            std::map<int, int> values;
            values[bitIdx] = 50; // Raw value

            flight->updateMetrics(values);

            // Should be scaled by 10
            EXPECT_FLOAT_EQ(initialValue + 5.0f, flight->m_metricValues[metricId]);
            return; // Test passed
        }
    }
}

TEST_F(FlightTest, UpdateMetricsIgnoresInvalidBits) {
    createFlight();

    std::map<int, int> values;
    values[999] = 100; // Invalid bit index

    // Should not throw or crash
    EXPECT_NO_THROW(flight->updateMetrics(values));
}

TEST_F(FlightTest, UpdateMetricsHandlesMultiByteValues) {
    createFlight();

    // Find a metric with high byte
    for (const auto& [bitIdx, metric] : flight->m_bit2MetricMap) {
        if (metric.getHighByteBitIdx().has_value()) {
            int lowBitIdx = bitIdx;
            int highBitIdx = metric.getHighByteBitIdx().value();
            MetricId metricId = metric.getMetricId();

            float initialValue = flight->m_metricValues[metricId];

            std::map<int, int> values;
            values[lowBitIdx] = 0x34;  // Low byte
            values[highBitIdx] = 0x12; // High byte

            flight->updateMetrics(values);

            // Should combine to 0x1234 = 4660
            float scaleFactor = 1.0f;
            if (metric.getScaleFactor() == Metric::ScaleFactor::TEN ||
                (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH && metadata->IsGPH())) {
                scaleFactor = 10.0f;
            }

            EXPECT_FLOAT_EQ(initialValue + (4660.0f / scaleFactor), flight->m_metricValues[metricId]);
            return; // Test passed
        }
    }
}

TEST_F(FlightTest, UpdateMetricsHandlesNegativeValues) {
    createFlight();

    // Find any metric
    if (!flight->m_bit2MetricMap.empty()) {
        auto& [bitIdx, metric] = *flight->m_bit2MetricMap.begin();
        MetricId metricId = metric.getMetricId();
        float initialValue = flight->m_metricValues[metricId];

        std::map<int, int> values;
        values[bitIdx] = -50;

        flight->updateMetrics(values);

        // Check for scaling
        float scaleFactor = 1.0f;
        if (metric.getScaleFactor() == Metric::ScaleFactor::TEN ||
            (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH && metadata->IsGPH())) {
            scaleFactor = 10.0f;
        }

        EXPECT_FLOAT_EQ(initialValue + (-50.0f / scaleFactor), flight->m_metricValues[metricId]);
    }
}

TEST_F(FlightTest, GetFlightMetricsRecordReturnsValidPointer) {
    createFlight();

    auto record = flight->getFlightMetricsRecord();

    EXPECT_NE(nullptr, record);
}

TEST_F(FlightTest, GetFlightMetricsRecordContainsData) {
    createFlight();

    flight->m_recordSeq = 42;
    flight->m_fastFlag = true;

    auto record = flight->getFlightMetricsRecord();

    EXPECT_EQ(42, record->m_recordSeq);
    EXPECT_TRUE(record->m_isFast);
    EXPECT_GT(record->m_metrics.size(), 0);
}

TEST_F(FlightTest, GetFlightMetricsRecordCopiesCurrentValues) {
    createFlight();

    // Modify a metric value
    flight->m_metricValues[OAT] = 75.5f;

    auto record = flight->getFlightMetricsRecord();

    EXPECT_FLOAT_EQ(75.5f, record->m_metrics[OAT]);
}

TEST_F(FlightTest, DifferentEDMVersionsLoadDifferentMetrics) {
    // Test V1 (old EDM < 900)
    metadata->m_configInfo.edm_model = 800;
    metadata->m_protoHeader.value = 1;
    createFlight();
    size_t v1MapSize = flight->m_bit2MetricMap.size();

    // Test V4 (EDM 900/930)
    metadata = std::make_shared<Metadata>();
    metadata->m_configInfo.edm_model = 930;
    metadata->m_configInfo.firmware_version = 200;
    metadata->m_configInfo.numCylinders = 6;
    metadata->m_fuelLimits.units = 0;
    metadata->m_protoHeader.value = 2;
    flight = std::make_shared<Flight>(metadata);
    size_t v4MapSize = flight->m_bit2MetricMap.size();

    // V4 should have more metrics than V1
    EXPECT_GT(v4MapSize, v1MapSize);
}

TEST_F(FlightTest, GPHvsLPHScaling) {
    // Test with GPH (units = 0)
    metadata->m_fuelLimits.units = 0;
    createFlight();

    // Find a TEN_IF_GPH metric and get its initial value
    for (const auto& [bitIdx, metric] : flight->m_bit2MetricMap) {
        if (metric.getScaleFactor() == Metric::ScaleFactor::TEN_IF_GPH) {
            MetricId metricId = metric.getMetricId();

            // Initial value should be scaled
            float gphInitial = flight->m_metricValues[metricId];

            // Now test with LPH (units = 1)
            metadata->m_fuelLimits.units = 1;
            flight = std::make_shared<Flight>(metadata);
            float lphInitial = flight->m_metricValues[metricId];

            // LPH should be 10x GPH for initial values
            EXPECT_FLOAT_EQ(gphInitial * 10.0f, lphInitial);
            return; // Test passed
        }
    }
}

TEST_F(FlightTest, FlightHeaderCanBeSet) {
    createFlight();

    auto header = std::make_shared<FlightHeader>();
    header->flight_num = 99;
    header->interval = 6;

    flight->m_flightHeader = header;

    EXPECT_EQ(99, flight->m_flightHeader->flight_num);
    EXPECT_EQ(6, flight->m_flightHeader->interval);
}

TEST_F(FlightTest, RecordCountsCanBeTracked) {
    createFlight();

    flight->m_stdRecCount = 100;
    flight->m_fastRecCount = 50;

    EXPECT_EQ(100, flight->m_stdRecCount);
    EXPECT_EQ(50, flight->m_fastRecCount);
}
