/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Class that parses a JPI EDM flight file.
 *
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "FileHeaders.hpp"
#include "Metadata.hpp"
#include "Flight.hpp"

namespace jpi_edm {

class FlightFile
{
  public:
    FlightFile(){};
    virtual ~FlightFile(){};

    virtual void setMetadataCompletionCb(std::function<void(Metadata &)> cb);
    virtual void setFlightHeaderCompletionCb(std::function<void(FlightHeader &)> cb);
    virtual void setFlightRecordCompletionCb(std::function<void(FlightRecord &)> cb);
    virtual void setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb);
    virtual void setFileFooterCompletionCb(std::function<void(void)> cb);

    virtual bool processFile(std::istream &stream);

  private:
    /**
     * Parse a header into a vector of unsigned longs.
     *
     * Throws an exception if the header line was invalid or couldn't be parsed.
     */
    std::vector<unsigned long> split_header_line(int lineno, std::string line);

    /**
     * Validate a header checksum.
     *
     * Throws an exception if the header line was invalid or if the checksum
     * didn't match.
     */
    void validateHeaderChecksum(int lineno, const char *line);
    bool validateBinaryChecksum(std::istream &stream, std::iostream::off_type startOff, std::iostream::off_type endOff,
                                unsigned char checksum);
    std::streamoff detectFlightHeaderSize(std::istream &stream);

    bool parse(std::istream &stream);

    void parseFileHeaders(std::istream &stream);
    std::shared_ptr<FlightHeader> parseFlightHeader(std::istream &stream, int flightId, std::streamoff headerSize);
    void parseFlightDataRec(std::istream &stream, int recordId, std::shared_ptr<FlightHeader> &header, bool &isFast);
    void parseFileFooters(std::istream &stream);

  private:
    Metadata m_metadata;
    std::vector<std::pair<int, long>> m_flightDataCounts;
    std::vector<int> m_values; // storage for the previous values so we can diff them
    int m_stdRecs{0};
    int m_fastRecs{0};

    std::function<void(Metadata &)> m_metadataCompletionCb;
    std::function<void(FlightHeader &)> m_flightHeaderCompletionCb;
    std::function<void(FlightRecord &)> m_flightRecCompletionCb;
    std::function<void(unsigned long, unsigned long)> m_flightCompletionCb;
    std::function<void(void)> m_fileFooterCompletionCb;
};

} // namespace jpi_edm
