/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-NC-4.0
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

    // The stream should already be positioned correctly by FlightRange::begin() for index 0.
    // For index > 0, we would need to calculate the offset, but iterators are normally
    // created with index 0 and incremented, so advance() maintains position.
    // Just ensure stream is in a good state - don't reposition it
    m_stream->clear();

    // Parse the flight at m_index
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

        // Validate flight data count - be more lenient for legacy models
        const std::streamoff MAX_FLIGHT_RECORDS = 1000000;
        bool isLegacyModel =
            (m_metadata && m_metadata->m_configInfo.edm_model > 0 && m_metadata->m_configInfo.edm_model < 800);

        // For legacy models, allow 0 (header only, no data records)
        if (flightDataCount.second < 0 || flightDataCount.second > MAX_FLIGHT_RECORDS) {
            if (!isLegacyModel || flightDataCount.second < 0) {
                throw std::runtime_error("Invalid flight data count");
            }
        }

        // Calculate flight data section size
        // flightDataCount.second gives the size in "byte pairs" (need to multiply by 2)
        // This represents the TOTAL size of the flight data section
        // The callback API uses (flightDataCount.second - 1) * 2 as a loop limit,
        // but the actual flight size is flightDataCount.second * 2
        std::streamoff flightDataSize;
        if (flightDataCount.second > std::numeric_limits<std::streamoff>::max() / 2) {
            throw std::runtime_error("Flight data count too large");
        }
        flightDataSize = flightDataCount.second * 2;

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
        // For FlightView, we still use the callback API's calculation for data size
        std::streamoff recordCount = (flightDataCount.second > 0) ? (flightDataCount.second - 1L) : 0L;
        std::streamoff totalBytes = recordCount * 2;
        m_currentFlight = FlightView(m_stream, m_parser, flightHeader, flight, dataStartOff, totalBytes);

        // Read all data records to find where this flight ends (matching callback API behavior)
        // The callback API loop: while ((stream.tellg() - startOff) < totalBytes) { parseFlightDataRec(...); }
        // We must do the same to ensure stream position matches
        while ((m_stream->tellg() - startOff) < totalBytes) {
            if (!m_stream->good()) {
                throw std::runtime_error("Stream error while skipping flight data records");
            }
            m_parser->parseFlightDataRec(*m_stream, flight);
        }

        // Stream is now positioned at the start of the next flight
        if (!m_stream->good()) {
            throw std::runtime_error("Stream error after reading flight data");
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
                         const std::vector<std::pair<int, long>> *flightDataCounts, std::streamoff headerSize,
                         std::streamoff flightDataStartPos)
    : m_stream(stream), m_parser(parser), m_metadata(metadata), m_flightDataCounts(flightDataCounts),
      m_headerSize(headerSize), m_flightDataStartPos(flightDataStartPos)
{
}

FlightIterator FlightRange::begin() const
{
    if (!m_stream || !m_parser || !m_metadata || !m_flightDataCounts || m_flightDataCounts->empty()) {
        return FlightIterator(); // Return end iterator
    }
    // Position stream at start of flight data
    m_stream->clear();
    m_stream->seekg(m_flightDataStartPos, std::ios::beg);
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
