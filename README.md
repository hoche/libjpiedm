# libjpiedm

C++ library for reading data from JPI EDM Engine Monitors.

Released under the Creative Commons "CC BY" license (except where otherwise noted).

Some information, particularly about the headers and the old file data
packing scheme are derived from the uncredited and unlicenced code found
at http://www.geocities.ws/jpihack/jpihackcpp.txt

## Building

To build:

    mkdir build
    cd build
    cmake ..
    cmake --build . -j

## Using

The library works on a stream basis. The objects get created and destroy as the
library works its way through the file. The main reason for doing is so that the
library doesn't have to keep the entire structure in memory. Instead, apps
using the library must register callbacks. The callbacks get invoked as the
library parses the file. The apps can then copy or discard the data as they see
fit.

Usage:

1. Create an EDMFlightFile object (See EDMFlightFile.hpp).
2.  Register one or more callbacks with it:

    virtual void setFileHeaderCompletionCb(std::function<void(EDMFileHeaderSet)> cb);

    virtual void setFlightHeaderCompletionCb(std::function<void(EDMFlightHeader)> cb);

    virtual void setFlightRecordCompletionCb(std::function<void(EDMFlightRecord)> cb);

    virtual void setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb);
   
    virtual void setFileFooterCompletionCb(std::function<void(void)> cb);

4. Open a JPI file as an iostream.
5. Call EDMFlightFile's processFile().

As the file is processed, the callbacks will be invoked.

EDMFileHeaderSet is a fairly transparent container for a number of classes that represent
the data found in the file header (the ascii portion that has lines that start it $A, $C, etc).
The classes are currently pretty much just POD (plain old data) structs, with public members
that can be accessed.

EDMFlightHeader is similar, but contains the header information for a particular flight,
including what time it was started and the sample rate. Note that the sample rate can vary
throughout the flight - occasionally the EDM goes into 1-second-per-sample mode for awhile.
The library will detect this and mark a particular EDMFlightRecord as "isFast" as appropriate.
It will also report the number of standard vs fast samples when it invokes the flight-completion
callback.

EDMFlightRecords is an (ordered) map of the data that's in each sample. I've figured out a
bunch of the fields by cross-indexing with EZTrends, but there're still a bunch I don't know.

At the moment, the best way to see how this works is to look at the sample app provided.
It's not very pretty, but should serve as a starting point.

Note: because of the way JPI structures their file, it's difficult to jump directly to a
particular flight. The $D records in the header give approximate locations, but they're not
always correct and I haven't figured out a reliable way to use them to jump.

## Platforms

Tested and running on Linux (x86 and ARM), OSX (x86), Windows (x86), as well as a Big-Endian
NetBSD system just to make sure I got the byte ordering right.


## Known Issues

* It doesn't detect old-style (short) records at all. A fix will be coming...uh. soonish.
* The multiengine values are just guesses.

