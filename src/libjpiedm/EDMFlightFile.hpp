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

    virtual void setFileHeaderCompletionCb(std::function<void(EDMFileHeaderSet&)> cb);
    virtual void setFlightHeaderCompletionCb(std::function<void(EDMFlightHeader&)> cb);
    virtual void setFlightRecordCompletionCb(std::function<void(EDMFlightRecord&)> cb);
    virtual void setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb);
    virtual void setFileFooterCompletionCb(std::function<void(void)> cb);

    virtual bool processFile(std::istream &stream);

  private:
    /**
     * Parse a header into a vector of unsigned longs.
     *
     * Throws an exception if the header line was invalid or couldn't be parsed.
     */
    virtual std::vector<unsigned long> split_header_line(int lineno, std::string line);

    /**
     * Validate a header checksum.
     *
     * Throws an exception if the header line was invalid or if the checksum
     * didn't match.
     */
    virtual void validate_header_checksum(int lineno, const char *line);

    virtual bool parse(std::istream &stream);

    virtual void parseFileHeaders(std::istream &stream);
    virtual void parseFlightHeader(std::istream &stream, int flightId);
    virtual void parseFlightDataRec(std::istream &stream, int recordId, bool& isFast);
    virtual void parseFileFooters(std::istream &stream);

  private:
    EDMFileHeaderSet m_fileHeaderSet;
    std::vector<std::pair<int, long>> m_flightDataCounts;
    std::vector<int> m_values; // storage for the previous values so we can diff them
    int m_stdRecs;
    int m_fastRecs;

    std::function<void(EDMFileHeaderSet&)> m_fileHeaderCompletionCb;
    std::function<void(EDMFlightHeader&)> m_flightHeaderCompletionCb;
    std::function<void(EDMFlightRecord&)> m_flightRecCompletionCb;
    std::function<void(unsigned long, unsigned long)> m_flightCompletionCb;
    std::function<void(void)> m_fileFooterCompletionCb;
};

} // namespace jpi_edm
