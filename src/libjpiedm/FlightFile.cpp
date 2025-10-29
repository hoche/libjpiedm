/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Utilities for parsing a JPI EDM flight file.
 *
 */

#include <algorithm>
#include <bitset>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#define sscanf sscanf_s
#else
#include <arpa/inet.h>
#endif
#include <limits.h>
#include <stdio.h>

#include "FileHeaders.hpp"
#include "Flight.hpp"
#include "FlightFile.hpp"
#include "Metadata.hpp"
#include "MetricId.hpp"
#include "ProtocolConstants.hpp"

namespace jpi_edm {

// #define DEBUG_FLIGHTS
// #define DEBUG_FLIGHT_HEADERS
//  #define DEBUG_PARSE

#if defined(DEBUG_FLIGHTS) && !defined(DEBUG_FLIGHT_HEADERS)
#define DEBUG_FLIGHT_HEADERS
#endif

// Use constant from ProtocolConstants.hpp
const int maxheaderlen = MAX_HEADER_LINE_LENGTH;

// Debugging utility to convert char to hex for printing
struct HexCharStruct {
    unsigned char c;
    HexCharStruct(unsigned char _c) : c(_c) {}
};

inline std::ostream &operator<<(std::ostream &o, const HexCharStruct &hs)
{
    return (o << std::hex << (int)hs.c) << std::dec;
}

inline HexCharStruct hex(unsigned char _c) { return HexCharStruct(_c); }

void FlightFile::setMetadataCompletionCb(std::function<void(std::shared_ptr<Metadata>)> cb)
{
    m_metadataCompletionCb = cb;
}

void FlightFile::setFlightHeaderCompletionCb(std::function<void(std::shared_ptr<FlightHeader>)> cb)
{
    m_flightHeaderCompletionCb = cb;
}

void FlightFile::setFlightRecordCompletionCb(std::function<void(std::shared_ptr<FlightMetricsRecord>)> cb)
{
    m_flightRecCompletionCb = cb;
}

void FlightFile::setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb)
{
    m_flightCompletionCb = cb;
}

void FlightFile::setFileFooterCompletionCb(std::function<void(void)> cb) { m_fileFooterCompletionCb = cb; }

/**
 * @brief split_header_line
 *
 * Given a line that looks like:
 *   $A, 305,230,500,415,60,1650,230,90*7F
 *
 * break it into a vector of unsigned integers, not including
 * either the leading '$A' nor anything after the '*'.
 */
std::vector<unsigned long> FlightFile::split_header_line(int lineno, std::string line)
{
    std::vector<unsigned long> values;
    size_t pos = 0;
    std::string token;

    while ((pos = line.find_first_of(",*")) != std::string::npos) {
        token = line.substr(0, pos);
        if (token[0] != '$') {
            try {
                unsigned long val = std::stoul(token);
                if (val == SPECIAL_VALUE_SENTINEL_A_RECORD) {
                    val = USHRT_MAX; // special case for $A record
                }
                values.push_back(val);
            } catch (const std::invalid_argument &ex) {
                std::stringstream msg;
                msg << "invalid argument in header: line " << lineno << " (" << ex.what() << ")";
                throw std::invalid_argument{msg.str()};
            } catch (const std::out_of_range &ex) {
                std::stringstream msg;
                msg << "out of range value in header: line " << lineno << " (" << ex.what() << ")";
                throw std::out_of_range{msg.str()};
            }
        }
        line.erase(0, pos + 1);
    }

    return values;
}

void FlightFile::validateHeaderChecksum(int lineno, const char *line)
{
    if (!line) {
        std::stringstream msg;
        msg << "invalid header: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    // Convert to std::string for safer operations
    std::string lineStr(line);

    // Find the asterisk separator
    size_t asteriskPos = lineStr.rfind('*');
    if (asteriskPos == std::string::npos || asteriskPos == 0) {
        std::stringstream msg;
        msg << "invalid header: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    // Extract checksum string (after asterisk)
    std::string checksumStr = lineStr.substr(asteriskPos + 1);

    // Parse checksum with bounds checking
    uint16_t testval = 0;
    try {
        size_t pos = 0;
        unsigned long parsed = std::stoul(checksumStr, &pos, 16);
        if (pos != checksumStr.length() || parsed > 0xFF) {
            std::stringstream msg;
            msg << "invalid header checksum format: line " << lineno;
            throw std::invalid_argument{msg.str()};
        }
        testval = static_cast<uint16_t>(parsed);
    } catch (const std::exception&) {
        std::stringstream msg;
        msg << "invalid header checksum format: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    // Calculate checksum (XOR of all bytes between $ and *)
    uint8_t cs = 0;
    for (size_t i = 1; i < asteriskPos; ++i) {
        cs ^= static_cast<uint8_t>(lineStr[i]);
    }

    if (testval != cs) {
        std::stringstream msg;
        msg << "header checksum failed: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }
}

void FlightFile::parseFileHeaders(std::istream &stream)
{
    int lineno = 0;
    bool end_of_headers = false;
    unsigned long theLHeaderVal = 0;
    Metadata metadata;

    // line is terminated with CRLF (\r\n)
    // read to the LF (\n) (getline won't include the LF in the return)
    std::unique_ptr<char[]> buffer(new char[HEADER_BUFFER_SIZE]);
    while (stream.good() && !end_of_headers) {
        lineno++;

        stream.getline(buffer.get(), maxheaderlen);
        if (!stream.good()) {
            std::stringstream msg;
            msg << "Couldn't read stream: line " << lineno;
            throw std::runtime_error{msg.str()};
        }

        char *line = buffer.get();

        // strip off the trailing CR
        auto newEnd = std::strrchr(line, '\r');
        if (newEnd) {
            *newEnd = '\0';
        }

        validateHeaderChecksum(lineno, line);

        // check to see if the line starts with '$'
        if (line[0] != '$') {
            std::stringstream msg;
            msg << "Invalid file format: Was expecting a header line: line " << lineno;
            throw std::runtime_error{msg.str()};
        }

        switch (line[1]) {
        case 'A': // config limits
            metadata.m_configLimits.apply(split_header_line(lineno, line));
            break;
        case 'C': // config info
            metadata.m_configInfo.apply(split_header_line(lineno, line));
            break;
        case 'D': // this repeats and gives the ID and bytes for a flight (but
                  // have to multiply bytes by 2)
        {
            auto flightDataCount = split_header_line(lineno, line);
            if (flightDataCount.size() > 1) {
                m_flightDataCounts.push_back(std::pair<int, long>{flightDataCount[0], flightDataCount[1]});
            }
        } break;
        case 'F': // fuel limits?
            metadata.m_fuelLimits.apply(split_header_line(lineno, line));
            break;
        case 'H': // unknown what this means
            break;
        case 'L': // last header record marker
        {
            auto lheaderdata = split_header_line(lineno, line);
            if (lheaderdata.size() > 0) {
                theLHeaderVal = lheaderdata[0];
            }
            end_of_headers = true;
        } break;
        case 'P': // protocol version
            metadata.m_protoHeader.apply(split_header_line(lineno, line));
            break;
        case 'T': // timestamp
            metadata.m_timeStamp.apply(split_header_line(lineno, line));
            break;
        case 'U': // tailnumber
            line = std::strchr(line, ',');
            if (line && *line) {
                line++;
                while (*line && *line != '*')
                    metadata.m_tailNum.push_back(*line++);
            }
            break;
        default:
        {
            // Unknown header types are not critical errors - they may be future extensions
            // For now, we log to stderr and continue parsing
            std::cerr << "Warning: Unknown header on line " << lineno << std::endl;
        } break;
        }
    }

    m_metadata = std::make_shared<Metadata>(metadata);

    if (m_metadataCompletionCb) {
        m_metadataCompletionCb(m_metadata);
    }
}

void FlightFile::parseFileFooters(std::istream &stream)
{
    if (m_fileFooterCompletionCb) {
        m_fileFooterCompletionCb();
    }
}

bool FlightFile::validateBinaryChecksum(std::istream &stream, std::iostream::off_type startOff,
                                        std::iostream::off_type endOff, unsigned char checksum)
{
    auto curLoc{stream.tellg()};
    if (curLoc == -1) {
        throw std::runtime_error("Failed to get current stream position for checksum validation");
    }

    // checksum - go back to the start of the record and add or xor everything
    // up to the end
    unsigned char checksum_sum{0};
    unsigned char checksum_xor{0};

    stream.seekg(startOff);
    if (!stream) {
        throw std::runtime_error("Failed to seek to checksum start position");
    }

    auto len = endOff - startOff;
    std::vector<char> buffer(len);
    stream.read(buffer.data(), len);

    if (!stream || stream.gcount() != len) {
        std::stringstream msg;
        msg << "Failed to read " << len << " bytes for checksum validation. "
            << "Read " << stream.gcount() << " bytes instead.";
        throw std::runtime_error(msg.str());
    }

    for (const auto &byte : buffer) {
        checksum_sum += static_cast<unsigned char>(byte);
        checksum_xor ^= static_cast<unsigned char>(byte);
    }
    checksum_sum = -checksum_sum;

#ifdef DEBUG_FLIGHTS
    std::cout << "checksum_sum: " << hex(checksum_sum) << "\n";
    std::cout << "checksum_xor: " << hex(checksum_xor) << "\n";
    std::cout << "stream checksum: " << hex(checksum) << "\n";
#endif

    stream.seekg(curLoc, std::ios_base::beg);
    if (!stream) {
        throw std::runtime_error("Failed to restore stream position after checksum validation");
    }
    if (checksum != checksum_sum && checksum != checksum_xor) {
        return false;
    }
    return true;
}

// This scans the stream, adding bytes to it until a checksum matches
std::optional<std::streamoff> FlightFile::detectFlightHeaderSize(std::istream &stream)
{
    auto startOff{stream.tellg()};
    if (startOff == -1) {
        throw std::runtime_error("Failed to get stream position for flight header detection");
    }

    std::streamoff offset;
    unsigned char checksum;
    for (offset = MAX_FLIGHT_HEADER_SIZE; offset >= MIN_FLIGHT_HEADER_SIZE; offset -= HEADER_SIZE_STEP) {
        stream.clear(); // Clear any error flags from previous iterations
        stream.seekg(startOff + offset, std::ios_base::beg);
        if (!stream) {
            // Seek failed (beyond EOF) - try next offset
            continue;
        }

        stream.read(reinterpret_cast<char *>(&checksum), 1);
        if (!stream || stream.gcount() != 1) {
            // Read failed - try next offset
            continue;
        }

        if (validateBinaryChecksum(stream, startOff, startOff + offset, checksum)) {
            // reset the stream and return the found offset
            stream.clear(); // Clear any error flags before final seek
            stream.seekg(startOff, std::ios_base::beg);
            if (!stream) {
                throw std::runtime_error("Failed to reset stream position after finding header size");
            }
            return offset;
        }
    }

    // reset the stream and return nullopt if not found
    stream.clear(); // Clear any error flags before final seek
    stream.seekg(startOff, std::ios_base::beg);
    if (!stream) {
        throw std::runtime_error("Failed to reset stream position after header size detection");
    }
    return std::nullopt;
}

std::shared_ptr<FlightHeader> FlightFile::parseFlightHeader(std::istream &stream, int flightId,
                                                            std::streamoff headerSize)
{
    auto startOff{stream.tellg()};
    if (startOff == -1) {
        throw std::runtime_error("Failed to get stream position for flight header parsing");
    }

#ifdef DEBUG_FLIGHT_HEADERS
    std::cout << "Flight Header start offset: 0x" << std::hex << startOff << std::dec << std::endl;
#endif

    auto flightHeader = std::make_shared<FlightHeader>();

    stream.read(reinterpret_cast<char *>(&flightHeader->flight_num), 2);
    if (!stream || stream.gcount() != 2) {
        throw std::runtime_error("Failed to read flight number from header");
    }
    flightHeader->flight_num = ntohs(flightHeader->flight_num);

    if (flightHeader->flight_num != flightId) {
        std::stringstream msg;
        msg << "Flight IDs don't match. Offset: " << std::hex << (stream.tellg() - static_cast<std::streamoff>(4L));
#ifdef DEBUG_FLIGHT_HEADERS
        std::cout << msg.str() << std::endl;
#endif
        throw std::runtime_error{msg.str()};
    }

    uint16_t flags[2];
    stream.read(reinterpret_cast<char *>(&flags), 4);
    if (!stream || stream.gcount() != 4) {
        throw std::runtime_error("Failed to read flags from flight header");
    }
    flightHeader->flags = htons(flags[0]) | (htons(flags[1]) << 16);

#ifdef DEBUG_FLIGHT_HEADERS
    std::cout << "flags: 0x" << std::hex << flightHeader->flags << std::dec << "\n";
#endif

    std::streamoff intervalOffset = startOff + headerSize - std::streamoff(INTERVAL_FIELD_TRAILING_BYTES);
    if (headerSize >= MAX_FLIGHT_HEADER_SIZE) {
        // big header, with at least seven data fields before the interval field
        // This potentially has GPS data in fields 3,4 and 5,6.
        uint32_t latlng{0};
        for (int i = 0; stream.tellg() < intervalOffset; ++i) {
            uint16_t val;
            stream.read(reinterpret_cast<char *>(&val), 2);
            if (!stream || stream.gcount() != 2) {
                throw std::runtime_error("Failed to read GPS data field from flight header");
            }
            val = ntohs(val);
            switch (i) {
            case HEADER_DATA_GPS_LAT_HIGH_IDX:
                latlng = static_cast<uint32_t>(val << 16);
                break;
            case HEADER_DATA_GPS_LAT_LOW_IDX:
                flightHeader->startLat = static_cast<int32_t>(latlng | val);
#ifdef DEBUG_FLIGHT_HEADERS
                std::cout << "Starting latitude: " << std::setprecision(6)
                          << (static_cast<float>(flightHeader->startLat) /
                              static_cast<float>(GPS_COORD_SCALE_DENOMINATOR))
                          << "\n";
#endif
                break;
            case HEADER_DATA_GPS_LNG_HIGH_IDX:
                latlng = static_cast<uint32_t>(val << 16);
                break;
            case HEADER_DATA_GPS_LNG_LOW_IDX:
                flightHeader->startLng = static_cast<int32_t>(latlng | val);
#ifdef DEBUG_FLIGHT_HEADERS
                std::cout << "Starting longitude: " << std::setprecision(6)
                          << (static_cast<float>(flightHeader->startLng) /
                              static_cast<float>(GPS_COORD_SCALE_DENOMINATOR))
                          << "\n";
#endif
                break;
#ifdef DEBUG_FLIGHT_HEADERS
            default:
                std::cout << "unknown[" << i << "]: 0x" << std::hex << val << std::dec << "\n";
#endif
            }
        }
    } else {
        // small header. just skip the data block
        stream.seekg(intervalOffset, std::ios_base::beg);
        if (!stream) {
            throw std::runtime_error("Failed to seek to interval field in flight header");
        }
    }

    stream.read(reinterpret_cast<char *>(&flightHeader->interval), 2);
    if (!stream || stream.gcount() != 2) {
        throw std::runtime_error("Failed to read interval from flight header");
    }
    flightHeader->interval = ntohs(flightHeader->interval);

    uint16_t dt;
    stream.read(reinterpret_cast<char *>(&dt), 2);
    if (!stream || stream.gcount() != 2) {
        throw std::runtime_error("Failed to read date from flight header");
    }
    dt = ntohs(dt);
    flightHeader->startDate.tm_mday = (dt & DATE_MDAY_MASK);
    flightHeader->startDate.tm_mon = ((dt & DATE_MONTH_MASK) >> DATE_MONTH_SHIFT) - 1;
    flightHeader->startDate.tm_year = (dt >> DATE_YEAR_SHIFT) + DATE_YEAR_OFFSET;

    uint16_t tm;
    stream.read(reinterpret_cast<char *>(&tm), 2);
    if (!stream || stream.gcount() != 2) {
        throw std::runtime_error("Failed to read time from flight header");
    }
    tm = ntohs(tm);
    flightHeader->startDate.tm_sec = (tm & TIME_SECONDS_MASK) * TIME_SECONDS_SCALE;
    flightHeader->startDate.tm_min = (tm & TIME_MINUTES_MASK) >> TIME_MINUTES_SHIFT;
    flightHeader->startDate.tm_hour = (tm >> TIME_HOURS_SHIFT);

#ifdef DEBUG_FLIGHT_HEADERS
    std::cout << "date: 0x" << std::hex << dt << std::dec << "\n";
    std::cout << "time: 0x" << std::hex << tm << std::dec << "\n";
    std::cout << "Start date:\n"
              << "  tm_sec: " << flightHeader->startDate.tm_sec << "  tm_min: " << flightHeader->startDate.tm_min
              << "  tm_hour: " << flightHeader->startDate.tm_hour << "  tm_mday: " << flightHeader->startDate.tm_mday
              << "  tm_mon: " << flightHeader->startDate.tm_mon << "  tm_year: " << flightHeader->startDate.tm_year
              << "  tm_wday: " << flightHeader->startDate.tm_wday << "  tm_yday: " << flightHeader->startDate.tm_yday
              << "  tm_isdst: " << flightHeader->startDate.tm_isdst << "\n";
#endif

    auto endOff{stream.tellg()};
    if (endOff == -1) {
        throw std::runtime_error("Failed to get stream position after reading flight header");
    }

#ifdef DEBUG_FLIGHT_HEADERS
    std::cout << "\n";
    std::cout << "Flight Header end offset: " << std::hex << endOff << std::dec << "\n";
    std::cout << std::flush;
#endif

    unsigned char checksum;
    stream.read(reinterpret_cast<char *>(&checksum), 1);
    if (!stream || stream.gcount() != 1) {
        throw std::runtime_error("Failed to read checksum from flight header");
    }
    if (!validateBinaryChecksum(stream, startOff, endOff, checksum)) {
        std::stringstream msg;
        msg << "checksum failure in flight header ";
#ifdef DEBUG_FLIGHTS
        std::cout << msg.str() << std::endl;
#endif
        throw std::runtime_error{msg.str()};
    }

    if (m_flightHeaderCompletionCb) {
        m_flightHeaderCompletionCb(flightHeader);
    }
    return flightHeader;
}

void FlightFile::parseFlightDataRec(std::istream &stream, const std::shared_ptr<Flight> &flight)
{
    int oldFormat = false; // NOT ACTIVE YET

    flight->incrementSequence();

    int maskSize = oldFormat ? 1 : 2;

    auto startOff{stream.tellg()};
    if (startOff == -1) {
        throw std::runtime_error("Failed to get stream position for flight data record");
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "-----------------------------------\n";
    std::cout << "recordSeq: " << flight->m_recordSeq << "\n";
    std::cout << "start offset: " << std::hex << startOff << std::dec << "\n";
#endif

    // A pair of bitmaps, which should be identical
    // They indicate which bytes of the data bitmap are populated
    std::vector<std::uint16_t> bmPopMap{0, 0};
    stream.read(reinterpret_cast<char *>(&bmPopMap[0]), maskSize);
    if (!stream || stream.gcount() != maskSize) {
        std::stringstream msg;
        msg << "Failed to read bmPopMap[0] in flight data record " << flight->m_recordSeq;
        throw std::runtime_error(msg.str());
    }

    stream.read(reinterpret_cast<char *>(&bmPopMap[1]), maskSize);
    if (!stream || stream.gcount() != maskSize) {
        std::stringstream msg;
        msg << "Failed to read bmPopMap[1] in flight data record " << flight->m_recordSeq;
        throw std::runtime_error(msg.str());
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "bmPopMap[0]: " << std::hex << bmPopMap[0] << "   bmPopMap[1]: " << bmPopMap[1] << std::dec << "\n";
    std::cout << std::flush;
#endif

    if (bmPopMap[0] != bmPopMap[1]) {
        std::stringstream msg;
        msg << "bmPopMaps don't match (record: " << std::dec << flight->m_recordSeq << " offset: " << std::hex
            << (stream.tellg() - static_cast<std::streamoff>(4L));
#ifdef DEBUG_FLIGHTS
        std::cout << msg.str() << std::endl;
#endif
        throw std::runtime_error{msg.str()};
    }

    // convert the population map to a bitset for ease of access
    std::bitset<16> flags{(maskSize == 1) ? bmPopMap[0] : ntohs(bmPopMap[0])};

    char repeatCount;
    stream.read(&repeatCount, 1);
    if (!stream || stream.gcount() != 1) {
        std::stringstream msg;
        msg << "Failed to read repeat count in flight data record " << flight->m_recordSeq;
        throw std::runtime_error(msg.str());
    }

    // The next few bytes indicate which measurements are available
    const int mapBytes = maskSize * BITS_PER_BYTE;

    std::bitset<MAX_METRIC_FIELDS> fieldMap;
    for (int i = 0; i < mapBytes; ++i) {
        if (flags[i]) {
            char val;
            stream.read(&val, 1);
            if (!stream || stream.gcount() != 1) {
                std::stringstream msg;
                msg << "Failed to read field map byte " << i << " in flight data record " << flight->m_recordSeq;
                throw std::runtime_error(msg.str());
            }
            for (int k = 0; k < BITS_PER_BYTE; ++k) {
                fieldMap.set(i * BITS_PER_BYTE + k, val & (1 << k)); // set the proper bit to 1
            }
        }
    }

    // The measurements are differences from the previous value. This indicates
    // whether it should be added to or subtracted from the previous value.
    // Note that we skip bytes 6 & 7 - they are the high bytes of the EGTs and
    // the sign bits aren't used (they use the sign bits from the low bytes).
    std::bitset<MAX_METRIC_FIELDS> signMap;
    for (int i = 0; i < mapBytes; ++i) {
        if (flags[i] && (i != EGT_HIGHBYTE_IDX_1 && i != EGT_HIGHBYTE_IDX_2)) {
            char val;
            stream.read(&val, 1);
            if (!stream || stream.gcount() != 1) {
                std::stringstream msg;
                msg << "Failed to read sign map byte " << i << " in flight data record " << flight->m_recordSeq;
                throw std::runtime_error(msg.str());
            }
            for (int k = 0; k < BITS_PER_BYTE; ++k) {
                signMap.set(i * BITS_PER_BYTE + k, val & (1 << k)); // set the proper bit to 1
            }
        }
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "repeatCount: " << hex(repeatCount) << "\n";
    {
        std::cout << "          ";
        if (fieldMap.size() > 0) {
            for (size_t count = 0, i = fieldMap.size() / 8; i > 0; --i) {
                std::cout << " Byte " << hex(i - 1) << "  ";
            }
        }
        std::cout << "\n";
        std::cout << "fieldMap: ";
        if (fieldMap.size() > 0) {
            for (size_t count = 0, i = fieldMap.size(); i > 0; --i) {
                std::cout << fieldMap[i - 1];
                if (++count == 8) {
                    std::cout << " ";
                    count = 0;
                }
            }
        }
        std::cout << "\n";
        std::cout << " signMap: ";
        if (signMap.size() > 0) {
            for (size_t count = 0, i = signMap.size(); i > 0; --i) {
                std::cout << signMap[i - 1];
                if (++count == 8) {
                    std::cout << " ";
                    count = 0;
                }
            }
        }
        std::cout << "\n";
    }
    std::cout << "values to read: " << std::dec << fieldMap.count() << "\n";
#endif

#ifdef DEBUG_FLIGHTS
    std::cout << "values start offset: " << std::hex << stream.tellg() << std::dec << "\n";
    int printCount = 0;
    std::cout << "raw values:\n";
    std::cout << "[idx]\thexval\tintval\tsign\tfinalval\n";
#endif

    std::map<int, int> values;
    for (size_t metricIdx = 0; metricIdx < fieldMap.size(); ++metricIdx) {
        if (fieldMap[metricIdx]) {
            unsigned char byte;
            stream.read(reinterpret_cast<char *>(&byte), 1);
            if (!stream || stream.gcount() != 1) {
                std::stringstream msg;
                msg << "Failed to read metric value byte at index " << metricIdx
                    << " in flight data record " << flight->m_recordSeq;
                throw std::runtime_error(msg.str());
            }
            int val = byte; // promote to int
            if (signMap[metricIdx]) {
                val = -val;
            };

#ifdef DEBUG_FLIGHTS
            std::cout << "[" << metricIdx << "]\t0x" << hex(byte) << "\t(" << int(byte) << ")\t"
                      << (signMap[metricIdx] ? "-" : "+") << "   =>\t" << val << "\n";
            if (++printCount % 16 == 0) {
                std::cout << "\n";
            }
#endif
            values[metricIdx] = val;

            if (metricIdx == MARK_IDX) {
                switch (val) {
                case 2:
                    flight->setFastFlag(true);
                    break;
                case 3:
                    flight->setFastFlag(false);
                    break;
                }
            }
        }
    }

    flight->updateMetrics(values);

    auto endOff{stream.tellg()};
    if (endOff == -1) {
        throw std::runtime_error("Failed to get stream position after reading flight data values");
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "\n";
    std::cout << "end offset: " << std::hex << endOff << std::dec << "\n";
    std::cout << std::flush;
#endif

    unsigned char checksum;
    stream.read(reinterpret_cast<char *>(&checksum), 1);
    if (!stream || stream.gcount() != 1) {
        std::stringstream msg;
        msg << "Failed to read checksum from flight data record " << flight->m_recordSeq;
        throw std::runtime_error(msg.str());
    }
    if (!validateBinaryChecksum(stream, startOff, endOff, checksum)) {
        std::stringstream msg;
        msg << "checksum failure in record " << std::dec << flight->m_recordSeq;
#ifdef DEBUG_FLIGHTS
        std::cout << msg.str() << std::endl;
#endif
        throw std::runtime_error{msg.str()};
    }

    if (m_flightRecCompletionCb) {
        m_flightRecCompletionCb(flight->getFlightMetricsRecord());
    }
}

void FlightFile::parseFlights(std::istream &stream)
{
    // If there are no flights to parse, return early
    if (m_flightDataCounts.empty()) {
        return;
    }

    auto headerSizeOpt = detectFlightHeaderSize(stream);

    if (!headerSizeOpt.has_value()) {
        throw std::runtime_error("Failed to detect flight header size - invalid file format");
    }

    std::streamoff headerSize = headerSizeOpt.value();

#ifdef DEBUG_FLIGHT_HEADERS
    std::cout << "Detected flight header size: " << headerSize << std::endl;
#endif

    for (auto &&flightDataCount : m_flightDataCounts) {
        auto startOff{stream.tellg()};
        if (startOff == -1) {
            throw std::runtime_error("Failed to get stream position before reading flight data");
        }

#ifdef DEBUG_PARSE
        std::cout << "======== startOff: " << std::hex << startOff << std::dec << "\n";
#endif

        // Validate flight data count is reasonable (prevent integer overflow)
        const std::streamoff MAX_FLIGHT_RECORDS = 1000000; // 1 million records max
        if (flightDataCount.second < 1 || flightDataCount.second > MAX_FLIGHT_RECORDS) {
            std::stringstream msg;
            msg << "Invalid flight data count: " << flightDataCount.second
                << " (must be between 1 and " << MAX_FLIGHT_RECORDS << ")";
            throw std::runtime_error(msg.str());
        }

        // Calculate total bytes to read with overflow checking
        std::streamoff recordCount = flightDataCount.second - 1L;
        std::streamoff totalBytes;
        if (recordCount > std::numeric_limits<std::streamoff>::max() / 2) {
            throw std::runtime_error("Flight data count too large - would cause integer overflow");
        }
        totalBytes = recordCount * 2;

        auto flight = std::make_shared<Flight>(m_metadata);
        flight->m_flightHeader = parseFlightHeader(stream, flightDataCount.first, headerSize);

        if (!stream.good()) {
            throw std::runtime_error("Stream error after reading flight header");
        }

        while ((stream.tellg() - startOff) < totalBytes) {
            if (!stream.good()) {
                std::stringstream msg;
                msg << "Stream error while reading flight data records at position " << stream.tellg();
                throw std::runtime_error(msg.str());
            }

            parseFlightDataRec(stream, flight);

#ifdef DEBUG_PARSE
            auto bytesRead = stream.tellg() - startOff;
            std::cout << "---> " << std::dec << bytesRead << "    streamnext: " << std::hex << stream.tellg()
                      << std::dec << "    totalBytes: " << totalBytes << "\n" << std::flush;
#endif
        }

        if (!stream.good()) {
            throw std::runtime_error("Stream error after reading flight data");
        }

        if (m_flightCompletionCb) {
            m_flightCompletionCb(flight->m_stdRecCount, flight->m_fastRecCount);
        }
    }
}

void FlightFile::parse(std::istream &stream)
{
    stream.seekg(0);

    parseFileHeaders(stream);
    parseFlights(stream);
    parseFileFooters(stream);
}

void FlightFile::processFile(std::istream &stream) { parse(stream); }

} // namespace jpi_edm
