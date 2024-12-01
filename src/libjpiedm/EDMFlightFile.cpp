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
#include <sstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <limits.h>

#include "EDMFileHeaders.hpp"
#include "EDMFlight.hpp"
#include "EDMFlightFile.hpp"

namespace jpi_edm {

//#define DEBUG_FLIGHTS
//#define DEBUG_PARSE

const int maxheaderlen = 256;

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

void EDMFlightFile::setFileHeaderCompletionCb(std::function<void(EDMFileHeaderSet)> cb)
{
    m_fileHeaderCompletionCb = cb;
}

void EDMFlightFile::setFlightHeaderCompletionCb(std::function<void(EDMFlightHeader)> cb)
{
    m_flightHeaderCompletionCb = cb;
}

void EDMFlightFile::setFlightRecordCompletionCb(std::function<void(EDMFlightRecord)> cb)
{
    m_flightRecCompletionCb = cb;
}

void EDMFlightFile::setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb)
{
    m_flightCompletionCb = cb;
}

void EDMFlightFile::setFileFooterCompletionCb(std::function<void(void)> cb)
{
    m_fileFooterCompletionCb = cb;
}

/**
 * @brief split_header_line
 *
 * Given a line that looks like:
 *   $A, 305,230,500,415,60,1650,230,90*7F
 *
 * break it into a vector of unsigned integers, not including
 * either the leading '$A' nor anything after the '*'.
 */
std::vector<unsigned long> EDMFlightFile::split_header_line(int lineno, std::string line)
{
    std::vector<unsigned long> values;
    size_t pos = 0;
    std::string token;

    while ((pos = line.find_first_of(",*")) != std::string::npos) {
        token = line.substr(0, pos);
        if (token[0] != '$') {
            try {
                unsigned long val = std::strtoul(token.c_str(), NULL, 10);
                if (val == 999999999) {
                    val = USHRT_MAX; // special case for $A record
                }
                values.push_back(val);
            } catch ([[maybe_unused]] std::invalid_argument const &ex) {
                std::stringstream msg;
                msg << "invalid argument in header: line " << lineno;
                throw std::invalid_argument{msg.str()};
            } catch ([[maybe_unused]] std::out_of_range const &ex) {
                std::stringstream msg;
                msg << "invalid header: line " << lineno;
                throw std::out_of_range{msg.str()};
            }
        }
        line.erase(0, pos + 1);
    }

    return values;
}

void EDMFlightFile::validate_header_checksum(int lineno, const char *line)
{
    if (!line) {
        std::stringstream msg;
        msg << "invalid header: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    const char *endp = strrchr(line, '*');
    const char *p = line + 1;

    if (!endp || *endp != '*') {
        std::stringstream msg;
        msg << "invalid header: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    uint16_t testval;
    uint8_t cs = 0;
    if (sscanf(endp + 1, "%hx", &testval) != 1) {
        std::stringstream msg;
        msg << "invalid header checksum format: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }

    while (p < endp)
        cs ^= *p++;

    if (testval != cs) {
        std::stringstream msg;
        msg << "header checksum failed: line " << lineno;
        throw std::invalid_argument{msg.str()};
    }
}

void EDMFlightFile::parseFileHeaders(std::istream &stream)
{
    int lineno = 0;
    bool end_of_headers = false;
    unsigned long theLHeaderVal = 0;
    EDMFileHeaderSet headerSet;

    // line is terminated with CRLF (\r\n)
    // read to the LF (\n) (getline won't include the LF in the return)
    std::unique_ptr<char[]> buffer(new char[1024]);
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
        *newEnd = '\0';

        // validate the checksum
        validate_header_checksum(lineno, line);

        // check to see if the line starts with '$'
        if (line[0] != '$') {
            std::stringstream msg;
            msg << "Invalid file format: Was expecting a header line: line " << lineno;
            throw std::runtime_error{msg.str()};
        }

        switch (line[1]) {
        case 'A': // config limits
            headerSet.m_configLimits.apply(split_header_line(lineno, line));
            break;
        case 'C': // config info
            headerSet.m_configInfo.apply(split_header_line(lineno, line));
            break;
        case 'D': // this repeats and gives the ID and bytes for a flight (but
                  // have to multiply bytes by 2)
        {
            auto flightDataCount = split_header_line(lineno, line);
            if (flightDataCount.size() > 1) {
                m_flightDataCounts.push_back(
                    std::pair<int, long>{flightDataCount[0], flightDataCount[1]});
            }
        } break;
        case 'F': // fuel limits?
            headerSet.m_fuelLimits.apply(split_header_line(lineno, line));
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
        case 'P': // unknown header
            headerSet.m_PHeader.apply(split_header_line(lineno, line));
            break;
        case 'T': // timestamp
            headerSet.m_timeStamp.apply(split_header_line(lineno, line));
            break;
        case 'U': // tailnumber
            line = std::strchr(line, ',');
            if (line && *line) {
                line++;
                while (*line && *line != '*')
                    headerSet.m_tailNum.push_back(*line++);
            }
            break;
        default:
        {
            std::stringstream msg;
            msg << "Invalid file format:  Unknown header on line: line " << lineno;
            // this isn't really a critical error, but should get noted somehow
            // throw std::runtime_error{msg.str()};
            // LOG_MESSAGE{msg}
        } break;
        }
    }

    m_fileHeaderSet = headerSet;

    if (m_fileHeaderCompletionCb) {
        m_fileHeaderCompletionCb(headerSet);
    }
}

void EDMFlightFile::parseFlightHeader(std::istream &stream, int flightId)
{
    const bool newVersion = true; // XXX Left here to support the old version

#ifdef DEBUG_FLIGHTS
    std::cout << "Flight Header start: " << std::hex << stream.tellg() << std::dec << "\n";
#endif

    int readlen = newVersion ? 14 : 7;
    std::vector<std::uint16_t> v(readlen);
    stream.read(reinterpret_cast<char *>(v.data()), readlen * sizeof(uint16_t));

#ifdef DEBUG_FLIGHTS
    std::cout << "Flight Header end: " << std::hex << stream.tellg() << std::dec << "\n";
#endif

    EDMFlightHeader flightHeader;

    flightHeader.flight_num = ntohs(v[0]);
    if (flightHeader.flight_num != flightId) {
        std::stringstream msg;
        msg << "Flight IDs don't match. Offset: " << std::hex << (stream.tellg() - static_cast<std::streamoff>(4L));
        throw std::runtime_error{msg.str()};
    }

    flightHeader.flags = htons(v[1]) | (htons(v[2]) << 16);

    int offset = newVersion ? 11 : 7;
    flightHeader.interval = ntohs(v[offset++]);
    uint16_t dt = ntohs(v[offset++]);

    flightHeader.startDate.tm_mday = (dt & 0x1f);
    flightHeader.startDate.tm_mon = ((dt & 0x01e0) >> 5) - 1;
    flightHeader.startDate.tm_year = ((dt & 0xfe00) >> 9) + 100;

    uint16_t tm = ntohs(v[offset++]);
    flightHeader.startDate.tm_sec = (tm & 0x1f) * 2;
    flightHeader.startDate.tm_min = (tm & 0x07e0) >> 5;
    flightHeader.startDate.tm_hour = ((tm & 0xf800) >> 11) - 1;

    if (m_flightHeaderCompletionCb) {
        m_flightHeaderCompletionCb(flightHeader);
    }
}

void EDMFlightFile::parseFlightDataRec(std::istream &stream, int recordSeq, bool& isFast)
{
#ifdef DEBUG_FLIGHTS
    std::cout << "-----------------------------------\n";
    std::cout << "recordSeq: " << recordSeq << "\n";
    std::cout << "start offset: " << std::hex << stream.tellg() << std::dec << "\n";
#endif

    // Unknown byte
    char unknown;
    stream.read(&unknown, 1);

    // A pair of bitmaps, which should be identical
    // They indicate which bytes of the data bitmap are populated
    std::vector<std::uint16_t> bmPopMap(2);
    stream.read(reinterpret_cast<char *>(bmPopMap.data()), 2 * sizeof(uint16_t));

#ifdef DEBUG_FLIGHTS
    std::cout << "unknown: " << hex(unknown) << "\n";
    std::cout << "bmPopMap[0]: " << std::hex << bmPopMap[0] << "   bmPopMap[1]: " << bmPopMap[1]
              << std::dec << "\n";
    std::cout << std::flush;
#endif

    if (bmPopMap[0] != bmPopMap[1]) {
        std::stringstream msg;
        msg << "bmPopMaps don't match (record: " << std::dec << recordSeq << " offset: " << std::hex
            << (stream.tellg() - static_cast<std::streamoff>(4L));
        throw std::runtime_error{msg.str()};
    }
    std::bitset<16> flags{ntohs(bmPopMap[0])};

    // a byte indicating whether we should just repeat the previous data record
    // we don't have to do anything.
    char repeatCount;
    stream.read(&repeatCount, 1);
    if (repeatCount) {
        return;
    }

    // The next few bytes indicate which measurements are available
    std::bitset<128> fieldMap;
    for (int i = 0; i < 16; ++i) {
        if (flags[i]) {
            char val;
            stream.read(&val, 1);
            for (int k = 0; k < 8; ++k) {
                fieldMap.set(i * 8 + k,
                             val & (1 << k)); // set the proper bit to 1
            }
        }
    }

    // The measurements are differences from the previous value. This indicates
    // whether it should be added to or subtracted from the previous value.
    std::bitset<128> signMap;
    for (int i = 0; i < 16; ++i) {
        if (flags[i] && (i != 6 && i != 7)) { // XXX I don't know why we skip bytes 6 and 7
            char val;
            stream.read(&val, 1);
            for (int k = 0; k < 8; ++k) {
                signMap.set(i * 8 + k,
                            val & (1 << k)); // set the proper bit to 1
            }
        }
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "repeatCount: " << hex(repeatCount) << "\n";
    {
        //std::cout << "fieldMap: b" << fieldMap << "\n";
        for (int count = 0, i = fieldMap.size()/8-1; i>=0; i--) {
            std::cout << " Byte " << hex(i) << "  ";
        }
        std::cout << "\n";
        for(int count = 0, i = fieldMap.size()-1; i>=0; i--) {
            std::cout << fieldMap[i];
            if (++count == 8) {std::cout << " "; count = 0;}
        }
        std::cout << "\n";
        //std::cout << "signMap: b" << signMap << "\n";
        for(int count = 0, i = signMap.size()-1; i>=0; i--) {
            std::cout << signMap[i];
            if (++count == 8) {std::cout << " "; count = 0;}
        }
        std::cout << "\n";
    }
    std::cout << "values to read: " << std::dec << fieldMap.count() << "\n";
#endif

    if (recordSeq == 0) {
        m_stdRecs = 0;
        m_fastRecs = 0;
        std::vector<int> default_values(128, 0xF0);
        std::vector<int> specialdefaults{30, 42, 48, 49, 50, 51, 52, 53, 79};
        for (auto i : specialdefaults)
            default_values[i] = 0; // special cases with defaults of 0x00 instead of 0xF0

        m_values = default_values;
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "values start offset: " << std::hex << stream.tellg() << std::dec << "\n";
    int printCount = 0;
#endif

    for (int k = 0; k < fieldMap.size(); ++k) {
        if (fieldMap[k]) {
            unsigned char diff;
            stream.read(reinterpret_cast<char *>(&diff), 1);
#ifdef DEBUG_FLIGHTS
            std::cout << "[" << k << ":0x" << hex(diff) << "]";
            if (++printCount%16 == 0) {
                std::cout << "\n";
            }
#endif
            signMap[k] ? m_values[k] -= diff : m_values[k] += diff;
        } else if (recordSeq == 0) {
            m_values[k] = 0;
        }
    }

#ifdef DEBUG_FLIGHTS
    std::cout << "\n";
    std::cout << "end offset: " << std::hex << (stream.tellg() - 1L) << std::dec << "\n";
    std::cout << std::flush;
#endif

    // special test for the MARK value.
    switch (m_values[EDMFlightRecord::MARK_IDX]) {
        case 0x02:
            isFast = true;
            break;
        case 0x03:
            isFast = false;
            break;
    }


    if (m_flightRecCompletionCb) {
        EDMFlightRecord fr(recordSeq, isFast);
        fr.apply(m_values);
        m_flightRecCompletionCb(fr);
    }
}

void EDMFlightFile::parseFileFooters(std::istream &stream)
{
    if (m_fileFooterCompletionCb) {
        m_fileFooterCompletionCb();
    }
}

bool EDMFlightFile::parse(std::istream &stream)
{
    stream.seekg(0);
    parseFileHeaders(stream);
    for (auto &&flightDataCount : m_flightDataCounts) {
        auto startOff{stream.tellg()};
#ifdef DEBUG_PARSE
        std::cout << "======== startOff: " << std::hex << startOff << std::dec << "\n";
#endif

        parseFlightHeader(stream, flightDataCount.first);

        std::vector<int> values{128};
        unsigned long recordSeq{0};
        unsigned long stdRecCount{0};
        unsigned long fastRecCount{0};
        bool isFast{false};

        for (; (stream.tellg() - startOff) < ((flightDataCount.second - 1) * 2); ++recordSeq) {
            parseFlightDataRec(stream, recordSeq, isFast);
#ifdef DEBUG_PARSE
            std::cout << "---> " << std::dec << bytesRead << "    streamnext: " << std::hex
                      << stream.tellg() << std::dec
                      << "    (flightDataCount.second-1)*2): " << ((flightDataCount.second - 1) * 2)
                      << "\n"
                      << std::flush;
#endif
            if (isFast) {
                ++fastRecCount;
            } else {
                ++stdRecCount;
            }

        }
        stream.get(); // pad byte? checksum?
        if (m_flightCompletionCb) {
            m_flightCompletionCb(stdRecCount, fastRecCount);
        }
    }

    parseFileFooters(stream);
    return true;
}

bool EDMFlightFile::processFile(std::istream &stream)
{
    return parse(stream);
}


} // namespace jpi_edm
