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

#include "EDMFileHeaders.hpp"
#include "EDMFlight.hpp"

namespace jpi_edm {

class EDMFlightFile
{
  public:
    EDMFlightFile(){};
    virtual ~EDMFlightFile(){};

    virtual void setMetaDataCompletionCb(std::function<void(EDMMetaData &)> cb);
    virtual void setFlightHeaderCompletionCb(std::function<void(EDMFlightHeader &)> cb);
    virtual void setFlightRecordCompletionCb(std::function<void(EDMFlightRecord &)> cb);
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
    std::shared_ptr<EDMFlightHeader> parseFlightHeader(std::istream &stream, int flightId, std::streamoff headerSize);
    void parseFlightDataRec(std::istream &stream, int recordId, std::shared_ptr<EDMFlightHeader> &header, bool &isFast);
    void parseFileFooters(std::istream &stream);

  private:
    EDMMetaData m_metaData;
    std::vector<std::pair<int, long>> m_flightDataCounts;
    std::vector<int> m_values; // storage for the previous values so we can diff them
    int m_stdRecs{0};
    int m_fastRecs{0};

    std::function<void(EDMMetaData &)> m_metaDataCompletionCb;
    std::function<void(EDMFlightHeader &)> m_flightHeaderCompletionCb;
    std::function<void(EDMFlightRecord &)> m_flightRecCompletionCb;
    std::function<void(unsigned long, unsigned long)> m_flightCompletionCb;
    std::function<void(void)> m_fileFooterCompletionCb;
};

} // namespace jpi_edm
