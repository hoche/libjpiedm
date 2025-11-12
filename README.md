# libjpiedm

libjpiedm is a C++ library for parsing data files from JPI EDM Engine Monitors.

It is released under the Creative Commons "CC BY" license (except where otherwise
noted).

## Building

To build (on every platform):

    mkdir build
    cd build
    cmake ..
    cmake --build . -j

## Using

libjpiedm streams data straight from an `std::istream`. It never loads an
entire flight into memory; instead it offers two complementary APIs that parse
on-demand:

- **Callback API** – legacy friendly, ideal when you already have processing
  code built around callbacks or want tight control over streaming.
- **Iterator API** – modern C++ range interface that cooperates with the
  callbacks but hides most of the plumbing.

You can freely mix both styles in the same application.

### Callback API

The callback API is built around `jpi_edm::FlightFile`. Register the callbacks
you care about, then call `processFile()`. Callbacks fire in this order:

1. `setMetadataCompletionCb` – once, after all ASCII `$` headers are parsed.
2. For each flight:
   - `setFlightHeaderCompletionCb`
   - Zero or more `setFlightRecordCompletionCb` invocations (one per data
     record; the supplied `FlightMetricsRecord` includes the `m_isFast` flag).
   - `setFlightCompletionCb` (standard and fast record counts).
3. `setFileFooterCompletionCb` – once, if a footer is present.

```cpp
#include "FlightFile.hpp"

using namespace jpi_edm;

FlightFile parser;
parser.setMetadataCompletionCb([](std::shared_ptr<Metadata> md) {
    std::cout << "Tail number: " << md->m_tailNum << "\n";
});
parser.setFlightHeaderCompletionCb([](std::shared_ptr<FlightHeader> hdr) {
    std::cout << "Flight #" << hdr->flight_num << " interval: " << hdr->interval << "s\n";
});
parser.setFlightRecordCompletionCb([](std::shared_ptr<FlightMetricsRecord> rec) {
    // Records arrive sequentially – ideal for logging or streaming to a DB.
});
parser.setFlightCompletionCb([](unsigned long stdRecs, unsigned long fastRecs) {
    std::cout << "Completed flight (" << stdRecs << " standard, "
              << fastRecs << " fast records)\n";
});

std::ifstream stream("data.jpi", std::ios::binary);
parser.processFile(stream);           // Parse every flight
// parser.processFile(stream, 60);    // Or just flight #60 using the single-flight fast path
```

Need to inspect available flights before parsing? `detectFlights()` scans only
the `$D` header records:

```cpp
auto flights = parser.detectFlights(stream);
for (const auto &flight : flights) {
    std::cout << "Flight " << flight.flightNumber
              << " ~" << flight.recordCount << " records\n";
}
```

### Iterator API

If you prefer range-based loops, use `FlightFile::flights(stream)` which
returns a lightweight `FlightRange`. Each `FlightView` lazily parses its
records; you can still register callbacks if you want to reuse existing logic.

```cpp
FlightFile file;
std::ifstream stream("data.jpi", std::ios::binary);

for (const auto &flight : file.flights(stream)) {
    const auto &hdr = flight.getHeader();
    std::cout << "Flight #" << hdr.flight_num << " started "
              << hdr.startDate.tm_mon + 1 << "/"
              << hdr.startDate.tm_mday << "\n";

    for (const auto &record : flight) {
        // record is std::shared_ptr<FlightMetricsRecord>
        // Iterate lazily; no buffering of the entire flight.
    }
}
```

Key iterator API types:

- `FlightRange` – returned by `flights(stream)`, supports `begin()/end()`.
- `FlightView` – represents one flight; exposes header metadata plus
  `begin()/end()` for records.
- `FlightView::RecordIterator` – streaming iterator over the metrics records.

The iterator API and callbacks share the same underlying streaming mechanics,
so using one does not force you to abandon the other – you can, for example,
register just the metadata callback and then iterate over flights.

See `examples/single_flight_example.cpp` and `examples/iterator_example.cpp`
for complete walk-throughs.

## Platforms

Tested and running on Linux (x86 and ARM), OSX (x86_64 and arm_64), Windows (x86), as well as a Big-Endian
NetBSD system just to make sure I got the byte ordering right.

There is are github "actions" to check the build and do a basic set of tests every time a code
change is submitted. These are currently set up to run on Linux, Windows, and OSX (x86_64 and arm_64).

## Known Issues

* It doesn't parse the old-style file formats.
* The multiengine values are just guesses.

## Latest Updates

November 2025
* Bunch of bug fixes.
* Added ability to jump directly to a specific flight by ID without parsing the entire file.
* `processFile()` now accepts an optional flight ID parameter.
* New `detectFlights()` method for lightweight flight enumeration.
* Added iterator API
* parseedmlog output nearly mirrors EZTrends output, except for twins. More work to be done there.
* Got the GPS lat/lng calculations working!

July 2025
* Figured out a number of the remaining parameters and documented them.
* Figured out the Lat/Long calculations, at least in part (more work to be done here).

## Credits

Some information, particularly about the headers and the old file data
packing scheme are derived from the uncredited and unlicenced code found
at http://www.geocities.ws/jpihack/jpihackcpp.txt

The idea for handling the Metrics array is directly from Keith Wannamaker's
edmtools project, which can be found at https://github.com/wannamak/edmtools.



