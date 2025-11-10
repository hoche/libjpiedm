/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * @brief Implementation of streaming iterator-based API for JPI EDM flight data.
 */

#include "FlightIterator.hpp"
#include "FlightFile.hpp"

#include <limits>
#include <stdexcept>

namespace jpi_edm {

// =============================================================================
// FlightView::RecordIterator Implementation
// =============================================================================

FlightView::RecordIterator::RecordIterator(std::istream *stream, FlightFile *parser, std::shared_ptr<Flight> flight,
                                           std::streamoff startOffset, std::streamoff totalBytes)
    : m_stream(stream), m_parser(parser), m_flight(flight), m_startOffset(startOffset), m_totalBytes(totalBytes),
      m_currentOffset(0), m_isEnd(false)
{
    if (!m_stream || !m_parser || !m_flight) {
        m_isEnd = true;
        return;
    }

    // Position stream at start of flight data records
    m_stream->clear();
    m_stream->seekg(m_startOffset, std::ios::beg);
    if (!m_stream->good()) {
        m_isEnd = true;
        return;
    }

    // Parse the first record
    advance();
}

void FlightView::RecordIterator::advance()
{
    if (m_isEnd || !m_stream || !m_parser || !m_flight) {
        m_isEnd = true;
        m_currentRecord.reset();
        return;
    }

    // Check if we've read all the data for this flight
    auto currentPos = m_stream->tellg();
    if (currentPos == -1 || (currentPos - m_startOffset) >= m_totalBytes) {
        m_isEnd = true;
        m_currentRecord.reset();
        return;
    }

    try {
        // Parse the next record
        m_parser->parseFlightDataRec(*m_stream, m_flight);
        m_currentRecord = m_flight->getFlightMetricsRecord();
    } catch (const std::exception &) {
        // If parsing fails, mark as end
        m_isEnd = true;
        m_currentRecord.reset();
        throw;
    }
}

FlightView::RecordIterator::reference FlightView::RecordIterator::operator*() const
{
    if (m_isEnd || !m_currentRecord) {
        throw std::out_of_range("RecordIterator: dereferencing end iterator");
    }
    return m_currentRecord;
}

FlightView::RecordIterator::pointer FlightView::RecordIterator::operator->() const
{
    if (m_isEnd || !m_currentRecord) {
        throw std::out_of_range("RecordIterator: dereferencing end iterator");
    }
    return &m_currentRecord;
}

FlightView::RecordIterator &FlightView::RecordIterator::operator++()
{
    advance();
    return *this;
}

FlightView::RecordIterator FlightView::RecordIterator::operator++(int)
{
    RecordIterator temp = *this;
    advance();
    return temp;
}

bool FlightView::RecordIterator::operator==(const RecordIterator &other) const
{
    // Two end iterators are equal
    if (m_isEnd && other.m_isEnd) {
        return true;
    }
    // End iterator is not equal to non-end iterator
    if (m_isEnd != other.m_isEnd) {
        return false;
    }
    // Both are valid - compare stream position and flight
    return (m_stream == other.m_stream) && (m_flight == other.m_flight) &&
           (m_stream->tellg() == other.m_stream->tellg());
}

bool FlightView::RecordIterator::operator!=(const RecordIterator &other) const { return !(*this == other); }

// =============================================================================
// FlightView Implementation
// =============================================================================

FlightView::FlightView(std::istream *stream, FlightFile *parser, std::shared_ptr<FlightHeader> header,
                       std::shared_ptr<Flight> flight, std::streamoff startOffset, std::streamoff totalBytes)
    : m_stream(stream), m_parser(parser), m_header(header), m_flight(flight), m_startOffset(startOffset),
      m_totalBytes(totalBytes)
{
    if (!m_header) {
        throw std::invalid_argument("FlightView: header cannot be null");
    }
}

FlightView::RecordIterator FlightView::begin() const
{
    if (!m_stream || !m_parser || !m_flight) {
        return RecordIterator(); // Return end iterator
    }
    return RecordIterator(m_stream, m_parser, m_flight, m_startOffset, m_totalBytes);
}

FlightView::RecordIterator FlightView::end() const
{
    return RecordIterator(); // Default-constructed is end iterator
}

// =============================================================================
// FlightIterator Implementation
// =============================================================================

FlightIterator::FlightIterator(std::istream *stream, FlightFile *parser, std::shared_ptr<Metadata> metadata,
                               const std::vector<std::pair<int, long>> *flightDataCounts, std::streamoff headerSize,
                               size_t index)
    : m_stream(stream), m_parser(parser), m_metadata(metadata), m_flightDataCounts(flightDataCounts),
      m_headerSize(headerSize), m_index(index), m_isEnd(false)
{
    if (!m_stream || !m_parser || !m_metadata || !m_flightDataCounts) {
        m_isEnd = true;
        return;
    }

    if (m_index >= m_flightDataCounts->size()) {
        m_isEnd = true;
        return;
    }

    // Parse the first flight
    advance();
}

void FlightIterator::advance()
{
    if (m_isEnd || !m_stream || !m_parser || !m_flightDataCounts) {
        m_isEnd = true;
        return;
    }

    if (m_index >= m_flightDataCounts->size()) {
        m_isEnd = true;
        return;
    }

    try {
        const auto &flightDataCount = (*m_flightDataCounts)[m_index];

        auto startOff = m_stream->tellg();
        if (startOff == -1) {
            throw std::runtime_error("Failed to get stream position before reading flight data");
        }

        // Validate flight data count
        const std::streamoff MAX_FLIGHT_RECORDS = 1000000;
        if (flightDataCount.second < 1 || flightDataCount.second > MAX_FLIGHT_RECORDS) {
            throw std::runtime_error("Invalid flight data count");
        }

        // Calculate total bytes
        std::streamoff recordCount = flightDataCount.second - 1L;
        std::streamoff totalBytes;
        if (recordCount > std::numeric_limits<std::streamoff>::max() / 2) {
            throw std::runtime_error("Flight data count too large");
        }
        totalBytes = recordCount * 2;

        // Create flight object
        auto flight = std::make_shared<Flight>(m_metadata);

        // Parse flight header
        auto flightHeader = m_parser->parseFlightHeader(*m_stream, flightDataCount.first, m_headerSize);
        flight->m_flightHeader = flightHeader;

        if (!m_stream->good()) {
            throw std::runtime_error("Stream error after reading flight header");
        }

        // Record position after header for data records
        auto dataStartOff = m_stream->tellg();
        if (dataStartOff == -1) {
            throw std::runtime_error("Failed to get stream position after flight header");
        }

        // Create FlightView (records will be parsed lazily)
        m_currentFlight = FlightView(m_stream, m_parser, flightHeader, flight, dataStartOff, totalBytes);

        // Skip to end of this flight's data for next iteration
        m_stream->clear();
        m_stream->seekg(dataStartOff + totalBytes, std::ios::beg);
        if (!m_stream->good()) {
            throw std::runtime_error("Failed to skip to end of flight data");
        }

    } catch (const std::exception &) {
        m_isEnd = true;
        throw;
    }
}

FlightIterator::reference FlightIterator::operator*() const
{
    if (m_isEnd) {
        throw std::out_of_range("FlightIterator: dereferencing end iterator");
    }
    return m_currentFlight;
}

FlightIterator::pointer FlightIterator::operator->() const
{
    if (m_isEnd) {
        throw std::out_of_range("FlightIterator: dereferencing end iterator");
    }
    return &m_currentFlight;
}

FlightIterator &FlightIterator::operator++()
{
    ++m_index;
    advance();
    return *this;
}

FlightIterator FlightIterator::operator++(int)
{
    FlightIterator temp = *this;
    ++m_index;
    advance();
    return temp;
}

bool FlightIterator::operator==(const FlightIterator &other) const
{
    // Two end iterators are equal
    if (m_isEnd && other.m_isEnd) {
        return true;
    }
    // End iterator is not equal to non-end iterator
    if (m_isEnd != other.m_isEnd) {
        return false;
    }
    // Both are valid - compare stream and index
    return (m_stream == other.m_stream) && (m_index == other.m_index);
}

bool FlightIterator::operator!=(const FlightIterator &other) const { return !(*this == other); }

// =============================================================================
// FlightRange Implementation
// =============================================================================

FlightRange::FlightRange(std::istream *stream, FlightFile *parser, std::shared_ptr<Metadata> metadata,
                         const std::vector<std::pair<int, long>> *flightDataCounts, std::streamoff headerSize)
    : m_stream(stream), m_parser(parser), m_metadata(metadata), m_flightDataCounts(flightDataCounts),
      m_headerSize(headerSize)
{
}

FlightIterator FlightRange::begin() const
{
    if (!m_stream || !m_parser || !m_metadata || !m_flightDataCounts || m_flightDataCounts->empty()) {
        return FlightIterator(); // Return end iterator
    }
    return FlightIterator(m_stream, m_parser, m_metadata, m_flightDataCounts, m_headerSize, 0);
}

FlightIterator FlightRange::end() const
{
    if (!m_stream || !m_parser || !m_metadata || !m_flightDataCounts) {
        return FlightIterator(); // Return end iterator
    }
    return FlightIterator(m_stream, m_parser, m_metadata, m_flightDataCounts, m_headerSize, m_flightDataCounts->size());
}

} // namespace jpi_edm
