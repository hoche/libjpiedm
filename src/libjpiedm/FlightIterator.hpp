/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * @brief Streaming iterator-based API for accessing JPI EDM flight data records.
 *
 * This provides a modern C++ iterator interface that works alongside the
 * callback-based API. It uses lazy evaluation and does NOT load the entire
 * file into memory - instead it parses on-demand as you iterate.
 *
 * Key features:
 * - Lazy evaluation: only parses what you access
 * - Memory efficient: maintains streaming architecture
 * - Range-based for loop support
 * - Standard iterator operations
 */

#pragma once

#include <iostream>
#include <iterator>
#include <memory>
#include <optional>

#include "Flight.hpp"
#include "Metadata.hpp"

namespace jpi_edm {

// Forward declaration
class FlightFile;

/**
 * @brief Represents a single flight with header and metrics records.
 *
 * This is a lightweight wrapper that provides access to flight data
 * without storing all records in memory. Records are accessed through
 * an inner iterator that parses on-demand.
 */
class FlightView
{
  public:
    /**
     * @brief Iterator for metric records within a flight.
     *
     * This is a streaming iterator that parses flight data records
     * on-demand as you iterate. It does NOT load all records into memory.
     */
    class RecordIterator
    {
      public:
        // Iterator traits
        using iterator_category = std::input_iterator_tag;
        using value_type = std::shared_ptr<FlightMetricsRecord>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type *;
        using reference = const value_type &;

        RecordIterator() = default;

        // Construct begin iterator
        RecordIterator(std::istream *stream, FlightFile *parser, std::shared_ptr<Flight> flight,
                       std::streamoff startOffset, std::streamoff totalBytes);

        // Iterator operations
        reference operator*() const;
        pointer operator->() const;
        RecordIterator &operator++();
        RecordIterator operator++(int);

        bool operator==(const RecordIterator &other) const;
        bool operator!=(const RecordIterator &other) const;

      private:
        void advance();

        std::istream *m_stream{nullptr};
        FlightFile *m_parser{nullptr};
        std::shared_ptr<Flight> m_flight;
        std::streamoff m_startOffset{0};
        std::streamoff m_totalBytes{0};
        std::streamoff m_currentOffset{0};
        std::shared_ptr<FlightMetricsRecord> m_currentRecord;
        bool m_isEnd{true};
    };

    FlightView() = default;

    FlightView(std::istream *stream, FlightFile *parser, std::shared_ptr<FlightHeader> header,
               std::shared_ptr<Flight> flight, std::streamoff startOffset, std::streamoff totalBytes);

    // Access to flight metadata
    [[nodiscard]] const FlightHeader &getHeader() const { return *m_header; }
    [[nodiscard]] std::shared_ptr<FlightHeader> getHeaderPtr() const { return m_header; }
    [[nodiscard]] unsigned long getStandardRecordCount() const { return (m_flight) ? m_flight->m_stdRecCount : 0UL; }
    [[nodiscard]] unsigned long getFastRecordCount() const { return (m_flight) ? m_flight->m_fastRecCount : 0UL; }
    [[nodiscard]] unsigned long getTotalRecordCount() const { return getStandardRecordCount() + getFastRecordCount(); }

    // Iterator interface for records (lazy parsing)
    [[nodiscard]] RecordIterator begin() const;
    [[nodiscard]] RecordIterator end() const;

  private:
    std::istream *m_stream{nullptr};
    FlightFile *m_parser{nullptr};
    std::shared_ptr<FlightHeader> m_header;
    std::shared_ptr<Flight> m_flight;
    std::streamoff m_startOffset{0};
    std::streamoff m_totalBytes{0};
};

/**
 * @brief Streaming iterator over flights in an EDM file.
 *
 * This iterator lazily parses flights as you iterate through them,
 * maintaining the memory-efficient streaming architecture of the library.
 *
 * Example usage:
 * @code
 *   FlightFile file;
 *   std::ifstream stream("data.jpi", std::ios::binary);
 *
 *   for (const auto& flight : file.flights(stream)) {
 *       std::cout << "Flight " << flight.getHeader().flight_num << "\n";
 *       for (const auto& record : flight) {
 *           // Process each record on-demand
 *           std::cout << "  Record " << record->m_recordSeq << "\n";
 *       }
 *   }
 * @endcode
 */
class FlightIterator
{
  public:
    // Iterator traits
    using iterator_category = std::input_iterator_tag;
    using value_type = FlightView;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *;
    using reference = const value_type &;

    FlightIterator() = default;

    // Construct begin iterator
    FlightIterator(std::istream *stream, FlightFile *parser, std::shared_ptr<Metadata> metadata,
                   const std::vector<std::pair<int, long>> *flightDataCounts, std::streamoff headerSize,
                   size_t index = 0);

    // Iterator operations
    reference operator*() const;
    pointer operator->() const;
    FlightIterator &operator++();
    FlightIterator operator++(int);

    bool operator==(const FlightIterator &other) const;
    bool operator!=(const FlightIterator &other) const;

  private:
    void advance();

    std::istream *m_stream{nullptr};
    FlightFile *m_parser{nullptr};
    std::shared_ptr<Metadata> m_metadata;
    const std::vector<std::pair<int, long>> *m_flightDataCounts{nullptr};
    std::streamoff m_headerSize{0};
    size_t m_index{0};
    FlightView m_currentFlight;
    bool m_isEnd{true};
};

/**
 * @brief Range adaptor for iterating over flights in an EDM file.
 *
 * This is a lightweight proxy object that provides begin() and end()
 * methods for range-based for loops. It does NOT store flight data.
 */
class FlightRange
{
  public:
    FlightRange(std::istream *stream, FlightFile *parser, std::shared_ptr<Metadata> metadata,
                const std::vector<std::pair<int, long>> *flightDataCounts, std::streamoff headerSize);

    [[nodiscard]] FlightIterator begin() const;
    [[nodiscard]] FlightIterator end() const;

  private:
    std::istream *m_stream;
    FlightFile *m_parser;
    std::shared_ptr<Metadata> m_metadata;
    const std::vector<std::pair<int, long>> *m_flightDataCounts;
    std::streamoff m_headerSize;
};

} // namespace jpi_edm
