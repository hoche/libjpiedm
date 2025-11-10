# Iterator-Based API Documentation

## Overview

The libjpiedm library provides both a callback-based API (original) and a modern C++ iterator-based API for parsing JPI EDM flight data files. The iterator API offers a more intuitive interface while maintaining the library's memory-efficient streaming architecture.

## Key Features

- **Lazy Evaluation**: Flights and records are parsed on-demand as you iterate
- **Memory Efficient**: Does NOT load the entire file into memory
- **Modern C++**: Supports range-based for loops and standard iterator operations
- **Compatible**: Works alongside the existing callback-based API

## Quick Start

```cpp
#include "FlightFile.hpp"
#include "FlightIterator.hpp"
#include <fstream>
#include <iostream>

using namespace jpi_edm;

int main() {
    std::ifstream stream("data.jpi", std::ios::binary);
    FlightFile parser;

    // Iterate through flights (lazy evaluation)
    for (const auto& flight : parser.flights(stream)) {
        std::cout << "Flight " << flight.getHeader().flight_num << "\n";
        std::cout << "  Total records: " << flight.getTotalRecordCount() << "\n";

        // Iterate through records in this flight (also lazy)
        for (const auto& record : flight) {
            std::cout << "    Record " << record->m_recordSeq << "\n";
        }
    }

    return 0;
}
```

## API Reference

### `FlightFile::flights()`

```cpp
FlightRange flights(std::istream& stream);
```

Returns an iterable range of flights. This method:
- Parses file headers immediately
- Returns a range object for iteration
- Does NOT parse flight data until you iterate

**Parameters:**
- `stream`: Input stream containing the EDM file (must remain valid during iteration)

**Returns:** `FlightRange` object that can be used in range-based for loops

**Throws:**
- `std::runtime_error` if file headers cannot be parsed
- `std::runtime_error` if flight header size detection fails (when flights exist)

### `FlightView` Class

Represents a single flight with its header and iterable records.

**Methods:**
```cpp
const FlightHeader& getHeader() const;
std::shared_ptr<FlightHeader> getHeaderPtr() const;
unsigned long getStandardRecordCount() const;
unsigned long getFastRecordCount() const;
unsigned long getTotalRecordCount() const;

// Iterator interface for records
FlightView::RecordIterator begin() const;
FlightView::RecordIterator end() const;
```

### `FlightIterator`

Input iterator for flights in the file.

**Iterator traits:**
- `iterator_category`: `std::input_iterator_tag`
- `value_type`: `FlightView`

**Supported operations:**
- Dereference: `*it`, `it->`
- Increment: `++it`, `it++`
- Comparison: `it1 == it2`, `it1 != it2`

### `FlightView::RecordIterator`

Input iterator for metric records within a flight.

**Iterator traits:**
- `iterator_category`: `std::input_iterator_tag`
- `value_type`: `std::shared_ptr<FlightMetricsRecord>`

**Supported operations:**
- Dereference: `*it`, `it->`
- Increment: `++it`, `it++`
- Comparison: `it1 == it2`, `it1 != it2`

## Usage Examples

### Example 1: Count Total Flights

```cpp
FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

int flightCount = 0;
for (const auto& flight : parser.flights(stream)) {
    ++flightCount;
}

std::cout << "Total flights: " << flightCount << "\n";
```

###Example 2: Find Flights with High Record Counts

```cpp
FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

const unsigned long MIN_RECORDS = 1000;

for (const auto& flight : parser.flights(stream)) {
    if (flight.getTotalRecordCount() >= MIN_RECORDS) {
        std::cout << "Flight " << flight.getHeader().flight_num
                  << " has " << flight.getTotalRecordCount() << " records\n";
    }
}
```

### Example 3: Process First N Records from Each Flight

```cpp
FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

const int RECORDS_TO_PROCESS = 10;

for (const auto& flight : parser.flights(stream)) {
    int count = 0;

    for (const auto& record : flight) {
        if (count++ >= RECORDS_TO_PROCESS) {
            break;  // Early termination - remaining records not parsed
        }

        // Process record
        std::cout << "Processing record " << record->m_recordSeq << "\n";
    }
}
```

### Example 4: Access Flight Header Information

```cpp
FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

for (const auto& flight : parser.flights(stream)) {
    const FlightHeader& header = flight.getHeader();

    std::cout << "Flight #" << header.flight_num << "\n";
    std::cout << "  Interval: " << header.interval << " seconds\n";
    std::cout << "  Flags: 0x" << std::hex << header.flags << std::dec << "\n";

    if (header.startLat != 0 || header.startLng != 0) {
        std::cout << "  Starting position: "
                  << (header.startLat / 10000000.0) << ", "
                  << (header.startLng / 10000000.0) << "\n";
    }
}
```

### Example 5: Manual Iterator Usage

```cpp
FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

auto flightRange = parser.flights(stream);
auto it = flightRange.begin();
auto end = flightRange.end();

while (it != end) {
    const FlightView& flight = *it;

    // Process flight
    std::cout << "Flight: " << flight.getHeader().flight_num << "\n";

    ++it;
}
```

## Important Notes

### Stream Lifetime

The input stream must remain valid for the lifetime of the range and all active iterators:

```cpp
// ✓ CORRECT: Stream lives long enough
std::ifstream stream("data.jpi", std::ios::binary);
FlightFile parser;

for (const auto& flight : parser.flights(stream)) {
    // Stream is still valid here
}
// Stream goes out of scope after iteration completes

// ✗ WRONG: Stream destroyed before iteration
FlightRange getFlights() {
    std::ifstream stream("data.jpi", std::ios::binary);
    FlightFile parser;
    return parser.flights(stream);  // BAD: stream will be destroyed
}
```

### Iterator Invalidation

Iterators are invalidated when:
- The underlying stream is modified or closed
- You seek to a different position in the stream
- The FlightFile object is destroyed

### Error Handling

The iterator will throw exceptions on parse errors:

```cpp
try {
    FlightFile parser;
    std::ifstream stream("data.jpi", std::ios::binary);

    for (const auto& flight : parser.flights(stream)) {
        for (const auto& record : flight) {
            // Process record
        }
    }
} catch (const std::runtime_error& ex) {
    std::cerr << "Parse error: " << ex.what() << "\n";
} catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
}
```

### Performance Characteristics

- **Memory Usage**: O(1) - only current flight/record in memory
- **Time Complexity**: O(n) where n is the number of flights/records accessed
- **Lazy Evaluation**: Unaccessed flights/records are never parsed

## Comparison with Callback API

Both the iterator-based API and callback-based API are available in the library. Here's a side-by-side comparison:

### Iterator API (New)
```cpp
#include "FlightFile.hpp"
#include "FlightIterator.hpp"

FlightFile parser;
std::ifstream stream("data.jpi", std::ios::binary);

// Sequential, intuitive processing
for (const auto& flight : parser.flights(stream)) {
    std::cout << "Flight " << flight.getHeader().flight_num << "\n";

    for (const auto& record : flight) {
        // Process each record
        std::cout << "  Record " << record->m_recordSeq << "\n";
    }
}
```

**Advantages:**
- Intuitive, familiar syntax
- Natural flow control (break, continue, early exit)
- Scoped processing - easy to manage state per flight
- Type-safe access to flight header before processing records

### Callback API (Original)
```cpp
#include "FlightFile.hpp"

int flightCount = 0;
int recordCount = 0;

FlightFile parser;

// Register callbacks for events
parser.setMetadataCompletionCb([](std::shared_ptr<Metadata> meta) {
    std::cout << "Protocol: " << meta->ProtoVersion() << "\n";
});

parser.setFlightHeaderCompletionCb([&](std::shared_ptr<FlightHeader> hdr) {
    flightCount++;
    std::cout << "Flight " << hdr->flight_num << "\n";
});

parser.setFlightRecordCompletionCb([&](std::shared_ptr<FlightMetricsRecord> rec) {
    recordCount++;
    std::cout << "  Record " << rec->m_recordSeq << "\n";
});

parser.setFlightCompletionCb([](unsigned long std, unsigned long fast) {
    std::cout << "Flight complete: " << std << " + " << fast << " records\n";
});

std::ifstream stream("data.jpi", std::ios::binary);
parser.processFile(stream);

std::cout << "Total: " << flightCount << " flights, "
          << recordCount << " records\n";
```

**Advantages:**
- Event-driven architecture
- Separate concerns (each callback handles one event)
- Can easily skip processing certain types of data
- More granular control (separate flight completion event)

### When to Use Each API

| Use Case | Recommended API |
|----------|----------------|
| Sequential processing of flights | Iterator API |
| Early termination based on conditions | Iterator API |
| Processing only specific flights | Iterator API |
| Need to manage state per flight | Iterator API |
| Event-driven architecture | Callback API |
| Integration with async frameworks | Callback API |
| Need flight completion notification | Callback API |
| Processing metadata separately | Callback API |

### Full Working Examples

See the `examples/` directory for complete working examples:
- **[iterator_example.cpp](examples/iterator_example.cpp)** - Iterator API demonstration
- **[callback_example.cpp](examples/callback_example.cpp)** - Callback API demonstration

Both examples process the same data and produce similar output, showing how the two APIs can be used interchangeably.

## Building Examples

The library includes two complete examples demonstrating both APIs:

```bash
# First, build the library
cd build
cmake --build . -j
cd ..

# Run the included parseedmlog tool (uses callback API internally)
./build/parseedmlog <file.jpi>

# Build the iterator API example
g++ -std=c++17 -I src/libjpiedm examples/iterator_example.cpp \
    -L build -ljpiedm -o iterator_example
./iterator_example <file.jpi>

# Build the callback API example
g++ -std=c++17 -I src/libjpiedm examples/callback_example.cpp \
    -L build -ljpiedm -o callback_example
./callback_example <file.jpi>
```

Both examples will produce similar output, demonstrating how the two APIs provide different interfaces to the same underlying functionality.

## See Also

### Header Files
- `FlightFile.hpp` - Main parser class with both APIs
- `FlightIterator.hpp` - Iterator classes and range adaptors
- `Flight.hpp` - Flight data structures (FlightHeader, FlightMetricsRecord)
- `Metadata.hpp` - File metadata structure

### Examples
- `examples/iterator_example.cpp` - Complete iterator API example
- `examples/callback_example.cpp` - Complete callback API example
- `src/parseedmlog/main.cpp` - Real-world usage (callback API)

### Documentation
- `README.md` - Library overview and building instructions
- `JPI_Logfile_Format.md` - EDM file format specification
