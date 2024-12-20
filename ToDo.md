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
    - ~~fix main data record parser~~
    - figure out why NineCyl sample isn't reading right
- ~~handle protocol version~~
- Skip function - had a hard time with this, but should be able to
  figure it out by jumping to flightWordsx2 or flightWordsx2 + 1 and then
  looking for the flight number.
- refactor
    - File header parser can be separate from FileStream object
    - Flight parser, which is composed of:
        - Flight Header parser - separate from FileStream object
        - Data Record parser class - two versions, for old and new
    - Distinguish from all available parameters and ones actually given
      by a particular file - e.g. single engine vs multi, older file format
      vs new, etc.
      - Hash table lookup for all available. Need canonical names.
      - Mapping to hashkeys based on file version.
      - Maybe a getAvailableParameters() function to get list of keys?
      - Hash values should be the composed/normalized version of the value,
        i.e. low/high bytes combined together, things that need multiplying
        by 10 done, etc. Conversion from C to F (or vice versa) should not
        be done here.
- sanity check results
    - graph in excel just to look
- Find misc pieces of data:
    - ~~what causes the sample rate change~~
    - GPS
    - LOP find start/end
    - ROP find start/end
- other language bindings
    - cgo
    - cpython
    - java
    - rust?

