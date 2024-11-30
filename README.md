# libjpiedm

C++ library for reading data from JPI EDM Engine Monitors.

Released under the Creative Commons "CC BY" license (except where otherwise noted).

Some information, particularly about the headers and the old file data
packing scheme are derived from the uncredited and unlicenced code found
at http://www.geocities.ws/jpihack/jpihackcpp.txt

## Usage

To build:

    mkdir build
    cd build
    cmake ..
    cmake --build . -j

To use:

1. Create an EDMFlightFile object (See EDMFlightFile.hpp).
2.  Register one or more callbacks with it:

     virtual void setFileHeaderCompletionCb(std::function<void(EDMFileHeaderSet)> cb);
     virtual void setFlightHeaderCompletionCb(std::function<void(EDMFlightHeader)> cb);
     virtual void setFlightRecordCompletionCb(std::function<void(EDMFlightRecord)> cb);
     virtual void setFlightCompletionCb(std::function<void(unsigned long, unsigned long)> cb);
     virtual void setFileFooterCompletionCb(std::function<void(void)> cb);

3. Open a JPI file as an iostream.
4. Call EDMFlightFile's processFile().

As the file is processed, the callbacks will be invoked.


EDMFileHeaderSet is a fairly transparent container for a number of classes that represent
the data found in the file header (the ascii portion that has lines that start it $A, $C, etc).
The classes are currently pretty much just POD (plain old data) structs, with public members
that can be accessed.

EDMFlightHeader is similar, but contains the header information for a particular flight,
including what time it was started and the sample rate. Note that the sample rate can vary
throughout the flight - occasionally the EDM goes into 1-second-per-sample mode for awhile.
I'm still figuring out how to detect that.

EDMFlightRecords is an (ordered) map of the data that's in each sample. I've figured out a
bunch of the fields by cross-indexing with EZTrends, but there're still a bunch I don't know.


At the moment, the best way to see how this works is to look at the sample_app provided.

## Platforms

Tested and running on Linux (x86 and ARM), OSX (x86), Windows (x86), as well as a Big-Endian
NetBSD system just to make sure I got the byte ordering right.

