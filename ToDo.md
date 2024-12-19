# To Do list

- parseedmlog improvements
  - ~~make output format closer to JPI export format~~
  - ~~output to a file~~
  - ~~select a single flight to output~~
- ~~check against other files~~
- ~~check bigendian vs littleendian~~
- ~~exception handling~~
- documentation
    - ~~top-level How To Use~~
    - code - add doxygen docs
    - ~~log file format~~
- ~~set NameSpace~~
- ~~mapping function for vector of values to named values~~
- ~~get flights available~~
- add in old/alternate format parser
    - ~~detect old file format~~
    - ~~parse flight headers correctly~~
    - fix main data record parser
- handle protocol version
- refactor
    - File header parser can be separate from FileStream object
    - Skip function - had a hard time with this, but should be able to
    figure it out by jumping to flightWordsx2 or flightWordsx2 + 1 and then
    looking for the flight number.
    - Flight parser, which is composed of:
        - Flight Header parser - separate from FileStream object
        - Data Record parser class - two versions, for old and new
    - bulk read when able (but don't prematurely optimize)
- other language bindings
    - cgo
    - cpython

- Find misc pieces of data:
    - ~~what causes the sample rate change~~
    - GPS
    - LOP find start/end
    - ROP find start/end
