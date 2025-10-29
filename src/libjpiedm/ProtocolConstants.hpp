/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Protocol constants for JPI EDM flight data file format
 *
 * This file contains all magic numbers and protocol-specific constants
 * extracted from the codebase for improved maintainability and documentation.
 */

#pragma once

#include <climits>
#include <cstdint>

namespace jpi_edm {

// ============================================================================
// Buffer and Size Constants
// ============================================================================

/// Maximum length for a header line in the EDM file format
constexpr int MAX_HEADER_LINE_LENGTH = 256;

/// Buffer size for header reading operations
constexpr int HEADER_BUFFER_SIZE = 1024;

/// Maximum number of metric fields supported in a data record
constexpr int MAX_METRIC_FIELDS = 128;

// ============================================================================
// EDM Model Identification
// ============================================================================

/// EDM 760 model number (twin engine)
constexpr unsigned long EDM_MODEL_760_TWIN = 760;

/// EDM 960 model number (twin engine)
constexpr unsigned long EDM_MODEL_960_TWIN = 960;

/// Model number threshold: models < 900 are single engine
constexpr unsigned long EDM_MODEL_SINGLE_THRESHOLD = 900;

/// Firmware version threshold for V1 protocol (versions <= 108 are V1)
constexpr unsigned long EDM_FIRMWARE_V1_THRESHOLD = 108;

// ============================================================================
// Header Version Detection
// ============================================================================

/// Proto header value threshold for extended features
constexpr unsigned long PROTO_HEADER_THRESHOLD = 1;

/// Build version threshold for HEADER_V4 format (build > 2010)
constexpr unsigned long BUILD_VERSION_HEADER_V4_THRESHOLD = 2010;

/// Build version threshold for HEADER_V3 format (build > 880)
constexpr unsigned long BUILD_VERSION_HEADER_V3_THRESHOLD = 880;

// ============================================================================
// File Header Record Field Counts
// ============================================================================

/// Number of fields expected in $A (ConfigLimits) record
constexpr size_t CONFIG_LIMITS_FIELD_COUNT = 8;

/// Minimum number of fields in $C (ConfigInfo) record
constexpr size_t CONFIG_INFO_MIN_FIELD_COUNT = 5;

/// Number of fields expected in $F (FuelLimits) record
constexpr size_t FUEL_LIMITS_FIELD_COUNT = 5;

/// Number of fields expected in $P (ProtoHeader) record
constexpr size_t PROTO_HEADER_FIELD_COUNT = 1;

/// Number of fields expected in $T (TimeStamp) record
constexpr size_t TIMESTAMP_FIELD_COUNT = 6;

// ============================================================================
// Configuration Flags Processing
// ============================================================================

/// Mask for extracting lower 16 bits of configuration flags
constexpr uint32_t CONFIG_FLAGS_LOWER_16_BITS_MASK = 0x0000FFFF;

/// Starting mask value for iterating cylinder feature flags
constexpr uint32_t CYLINDER_FLAG_START_MASK = 0x00000004;

// ============================================================================
// Special Sentinel Values
// ============================================================================

/// Special sentinel value in $A record indicating invalid/max value
/// When this value appears, it should be converted to USHRT_MAX
constexpr unsigned long SPECIAL_VALUE_SENTINEL_A_RECORD = 999999999;

/// Marker value indicating "all flights" (used in parseedmlog)
constexpr int ALL_FLIGHTS_MARKER = -1;

// ============================================================================
// Flight Header Detection and Parsing
// ============================================================================

/// Minimum possible flight header size in bytes
constexpr int MIN_FLIGHT_HEADER_SIZE = 14;

/// Maximum possible flight header size in bytes
constexpr int MAX_FLIGHT_HEADER_SIZE = 28;

/// Step size for header size detection algorithm
constexpr int HEADER_SIZE_STEP = 2;

/// Number of trailing bytes before interval field in header
constexpr int INTERVAL_FIELD_TRAILING_BYTES = 6;

// ============================================================================
// Flight Header Data Block Indices
// ============================================================================

/// Index for GPS latitude high word in flight header data block
constexpr int HEADER_DATA_GPS_LAT_HIGH_IDX = 3;

/// Index for GPS latitude low word in flight header data block
constexpr int HEADER_DATA_GPS_LAT_LOW_IDX = 4;

/// Index for GPS longitude high word in flight header data block
constexpr int HEADER_DATA_GPS_LNG_HIGH_IDX = 5;

/// Index for GPS longitude low word in flight header data block
constexpr int HEADER_DATA_GPS_LNG_LOW_IDX = 6;

// ============================================================================
// Date/Time Encoding Constants
// ============================================================================

/// Bit mask for extracting day of month (5 bits: 0-31)
constexpr uint16_t DATE_MDAY_MASK = 0x1f;

/// Bit mask for extracting month field (9 bits total including shift)
constexpr uint16_t DATE_MONTH_MASK = 0x01ff;

/// Bit shift for extracting month from date field
constexpr int DATE_MONTH_SHIFT = 5;

/// Bit shift for extracting year from date field
constexpr int DATE_YEAR_SHIFT = 9;

/// Offset added to year after extraction (adds 100 to account for tm_year base)
constexpr int DATE_YEAR_OFFSET = 100;

/// Bit mask for extracting seconds (5 bits: 0-31)
constexpr uint16_t TIME_SECONDS_MASK = 0x1f;

/// Scale factor for seconds (2-second resolution)
constexpr int TIME_SECONDS_SCALE = 2;

/// Bit mask for extracting minutes field (11 bits total including shift)
constexpr uint16_t TIME_MINUTES_MASK = 0x07ff;

/// Bit shift for extracting minutes from time field
constexpr int TIME_MINUTES_SHIFT = 5;

/// Bit shift for extracting hours from time field
constexpr int TIME_HOURS_SHIFT = 11;

// ============================================================================
// Data Record Processing
// ============================================================================

/// Index of first EGT high byte where sign bit is not used
constexpr int EGT_HIGHBYTE_IDX_1 = 6;

/// Index of second EGT high byte where sign bit is not used
constexpr int EGT_HIGHBYTE_IDX_2 = 7;

/// Number of bits per byte (for field map calculations)
constexpr int BITS_PER_BYTE = 8;

/// Byte mask for extracting single byte (0xFF)
constexpr uint8_t BYTE_MASK = 0xFF;

// ============================================================================
// Metric Scaling
// ============================================================================

/// Divisor for metrics that need to be divided by 10
constexpr int METRIC_SCALE_DIVISOR = 10;

// ============================================================================
// GPS Coordinate Encoding
// ============================================================================

/// Scale denominator for GPS coordinate conversion
/// GPS coordinates are encoded as integer values that need to be divided by this
/// to convert to degrees and decimal minutes format
constexpr int GPS_COORD_SCALE_DENOMINATOR = 6000;

/// Divisor for converting minutes fraction to decimal
constexpr int GPS_MINUTES_DECIMAL_DIVISOR = 100;

// ============================================================================
// Mark Indicators (Special Event Markers)
// ============================================================================

/// Mark code indicating start of marked region
constexpr uint8_t MARK_START = 0x02;

/// Mark code indicating end of marked region
constexpr uint8_t MARK_END = 0x03;

/// Mark code for unknown/other mark type
constexpr uint8_t MARK_UNKNOWN = 0x04;

// ============================================================================
// Engine Configuration
// ============================================================================

/// Cylinder count for typical 4-cylinder single engine
constexpr int SINGLE_ENGINE_CYLINDER_COUNT = 4;

// ============================================================================
// Time Calculation Constants
// ============================================================================

/// Small offset added to hours calculation (purpose: rounding/bias adjustment)
/// TODO: Document the exact purpose of this offset
constexpr float HOURS_ROUNDING_OFFSET = 0.01f;

/// Number of minutes per hour
constexpr int MINUTES_PER_HOUR = 60;

// ============================================================================
// Test/Default Date Values (for parseedmlog)
// ============================================================================

/// Default test year for timestamp calculations
constexpr int TEST_YEAR = 2025;

/// Default test month (0-based: 5 = June)
constexpr int TEST_MONTH = 5;

/// Default test day of month
constexpr int TEST_DAY = 1;

/// Offset for tm_year field (years since 1900)
constexpr int TM_YEAR_BASE = 1900;

} // namespace jpi_edm
