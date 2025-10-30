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
#include <optional>
#include <string>
#include <vector>

#include "FileHeaders.hpp"
#include "Flight.hpp"
#include "Metadata.hpp"

namespace jpi_edm {

// Forward declarations for iterator API
class FlightRange;

class FlightFile
{
  public:
    FlightFile() = default;
    virtual ~FlightFile() = default;

    // Explicitly handle copy and move operations
    FlightFile(const FlightFile &) = delete;
    FlightFile &operator=(const FlightFile &) = delete;
    FlightFile(FlightFile &&) = default;
    FlightFile &operator=(FlightFile &&) = default;

    // =========================================================================
    // Callback-based API (original streaming interface)
    // =========================================================================

    virtual void setMetadataCompletionCb(std::function<void(std::shared_ptr<Metadata>)> cb);
    virtual void setFlightHeaderCompletionCb(std::function<void(std::shared_ptr<FlightHeader>)> cb);
    virtual void setFlightRecordCompletionCb(std::function<void(std::shared_ptr<FlightMetricsRecord>)> cb);
    virtual void setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb);
    virtual void setFileFooterCompletionCb(std::function<void(void)> cb);

    virtual void processFile(std::istream &stream);

    // =========================================================================
    // Iterator-based API (modern C++ interface with lazy evaluation)
    // =========================================================================

    /**
     * @brief Get an iterable range of flights for streaming iteration.
     *
     * This method provides a lazy-evaluation iterator interface that parses
     * flights on-demand as you iterate. It does NOT load all flights into
     * memory, maintaining the streaming architecture of the library.
     *
     * The stream must remain valid for the lifetime of the returned range
     * and any active iterators.
     *
     * @param stream Input stream containing the EDM file
     * @return FlightRange that can be used in range-based for loops
     * @throws std::runtime_error if the file headers cannot be parsed
     *
     * Example:
     * @code
     *   FlightFile file;
     *   std::ifstream stream("data.jpi", std::ios::binary);
     *
     *   // Flights are parsed on-demand as you iterate
     *   for (const auto& flight : file.flights(stream)) {
     *       std::cout << "Flight " << flight.getHeader().flight_num << "\n";
     *
     *       // Records within each flight are also parsed on-demand
     *       for (const auto& record : flight) {
     *           std::cout << "  Record " << record->m_recordSeq << "\n";
     *       }
     *   }
     * @endcode
     */
    [[nodiscard]] FlightRange flights(std::istream &stream);

  private:
    // Make parseFlightHeader and parseFlightDataRec accessible to iterator
    friend class FlightIterator;
    friend class FlightView;
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
    [[nodiscard]] bool validateBinaryChecksum(std::istream &stream, std::iostream::off_type startOff,
                                              std::iostream::off_type endOff, unsigned char checksum);
    [[nodiscard]] std::optional<std::streamoff> detectFlightHeaderSize(std::istream &stream);

    void parse(std::istream &stream);

    void parseFileHeaders(std::istream &stream);
    [[nodiscard]] std::shared_ptr<FlightHeader> parseFlightHeader(std::istream &stream, int flightId,
                                                                  std::streamoff headerSize);
    void parseFlightDataRec(std::istream &stream, const std::shared_ptr<Flight> &flight);
    void parseFlights(std::istream &stream);
    void parseFileFooters(std::istream &stream);

  private:
    std::shared_ptr<Metadata> m_metadata;
    std::vector<std::pair<int, long>> m_flightDataCounts;

    std::function<void(std::shared_ptr<Metadata>)> m_metadataCompletionCb;
    std::function<void(std::shared_ptr<FlightHeader>)> m_flightHeaderCompletionCb;
    std::function<void(std::shared_ptr<FlightMetricsRecord>)> m_flightRecCompletionCb;
    std::function<void(unsigned long, unsigned long)> m_flightCompletionCb;
    std::function<void(void)> m_fileFooterCompletionCb;
};

} // namespace jpi_edm
