# General notes on the format of EDM .JPI or .DAT files

The format of the .JPI or .DAT files is slightly ridiculous, consisting of a 
number textual header records (broken into lines with CRLF ('\r\n'), followed
by binary data dumping each flight's data, and then returning to the text
format for a couple of footer lines.

## Header and footer records

Header and footer records are "$X,....\*NN\r\n" where X is a letter for a record
type. They are just text lines delimited by line feeds. Most are series of
numeric short values just converted to comma delimited ascii.

Header and footer records all end in "\*NN", where the NN is two hex digits. It is a
checksum, computed by a byte which is the XOR of all the bytes in the header
line excluding the initial '$' and trailing '\*NN'.


### Header record types

Fields are in ascii, delimited by commas.
Each field is usually an unsigned integer, represented as text.
Some have a space after the first comma, some do not.

#### $U - tail number

Fields: There is one text field, the tail number of the airplane

    $U, N12345*44

#### $A - configured limits:

There are eight numeric fields.

Occasionally values show up as "999999999". This has been observed in the OilLo
field. It may mean that no measurement was available, but this is a guess.

Fields: VoltsHi\*10,VoltsLo\*10,DIF,CHT,CLD,TIT,OilHi,OilLo

    $A, 305,230,500,415,60,1650,230,90*7F

#### $F - Fuel flow config and limits.

Fields: empty,main_tank_size,aux_tank_size,kfactor,kfactor

The K-factor is the number of pulses expected for every one volumetric unit of
fluid passing through a given flow meter.

    $F,0,999,  0,2950,2950*53

#### $T - timestamp of download, fielded (Times are UTC)

Fields: MM,DD,YY,hh,mm,??

The last value is unknown and may be some kind of sequence number but isn't always
sequential.

    $T, 5,13, 5,23, 2, 2222*65

#### $C - config info

There are two formats.

Old format:

Fields: model#,feature flags lo, feature flags hi, unknown, firmware version

    $C,700,63741, 6193, 1552, 292*58

New format:

Fields: model#,feature flags lo, feature flags hi, unknown1, unknown2, unknown3, firmware version, build version

    $C,930,63743,65041,1560,16610,123,140,2011,6*55


The feature flags is a 32 bit set of flags indicating which features the EDM
supports and reports. As far as I can determine, it is unused in the new format,
except that the flight header's copy must match the one in the $C file header (see the flight
header format description below). It should be read as two 16-bit integers. These are in
little-endian format, with the high word coming after the low word.

The following are notes from the old jpihack.c code about what the bits represent:

    -m-d fpai r2to eeee eeee eccc cccc cc-b

    e = egt (up to 9 cyls)
    c = cht (up to 9 cyls)
    d = probably cld, or maybe engine temps unit (1=F)
    b = bat
    o = oil
    t = tit1
    2 = tit2
    a = OAT
    f = fuel flow
    r = CDT (also CARB - not distinguished in the CSV output)
    i = IAT
    m = MAP
    p = RPM
    *** e and c may be swapped (but always exist in tandem)
    *** d and b may be swapped (but seem to exist in tandem)
    *** m, p and i may be swapped among themselves.

#### $P - Unknown

*Changing this value (and recalculating its checksum) results in EZTrends
reporting the file as corrupt.*

    $P, 2*6E

#### $H - Unknown

    $H,0*54

#### $D - flight info

Fields: flight#, length-of-flight-data-divided-by-two

There can be multiples of these records. Note that since the value
is divided by two and yet stored as an integer, you can't simply
double the value to get the flight's full data size. You don't know if
it is off by one due to the rounding (truncation actually) error.

*(In the original file format, everything was in 2-byte words and this
was simply a count of the number of those. In the new file format this
doesn't quite work because a record may have an odd number of bytes.)*

    $D, 50, 1487*4B
    $D, 51, 879*76
    $D, 52, 13422*75
    $D, 53, 14222*75

#### $L - last header record

The presence of this indicates the end of the text headers.
The value has an unknown meaning.

    $L, 49*4D


### Footer record types

#### $E - End of binary data

There are two of these. One occurs immedately after the flight record data.
The other occurs after the "JPI>" data (see below).

This is always "$E,4\*5D".

    $E,4*5D

#### JPI> data

After the first $E marker is more data beginning with "JPI>". The meaning is unknown.
It is terminated with a second $E marker.

#### $V - version

Finally there is a version line, for example:

    $V,Created by EDM930 VERSION 1.4 BLD 2011 BETA 6*61"


## Binary Data

Binary data follows immediately after the CRLF of the last
header record.

As indicated above there can be multiple flights recorded in a single file. The
$D header gives the ordering and flight number.

Each flight consists of a single header followed by a variable number of
data records. After the last data record there is an extra byte. This is probably
a checksum of some sort, but I haven't confirmed that.

Following that extra byte is the next flight's binary data - its header and then
data records, extra byte, and so on.

### Flight information header

The first flight header follows immediately after the $L record.

#### New format

     7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         flight number         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            flags              |
    |                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[0]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[1]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[2]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[3]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[4]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[5]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[7]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[7]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           interval            |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      year   |  mo   |  day    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   hr    |   min     |  sec/2  |
    +-------------------------------/

#### Old format

     7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         flight number         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            flags              |
    |                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[0]           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           interval            |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      year   |  mo   |  day    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   hr    |   min     |  sec/2  |
    +-------------------------------/


The interval field is in seconds.

### Data records

#### New format

The data records represent measurements of up to 128 different values.
The specific values which are updated by a particular data record can be
represented as a bitmap of all possible values. The bits that are 1's
indicate which values the data record has data.

The values in the 128-bit bitmap are as follows:

*'-' means unknown and not set in the datafiles I looked at*

*'?' means unknown, but it was set*

    byte 0
    [0]   EGT1 (left engine) low byte
    [1]   EGT2 (left engine) low byte
    [2]   EGT3 (left engine) low byte
    [3]   EGT4 (left engine) low byte
    [4]   EGT5 (left engine) low byte
    [5]   EGT6 (left engine) low byte
    [6]   T1?
    [7]   T2?

    byte 1
    [8]   CHT1 (left engine)
    [9]   CHT2 (left engine)
    [10]  CHT3 (left engine)
    [11]  CHT4 (left engine)
    [12]  CHT5 (left engine)
    [13]  CHT6 (left engine)
    [14]  CLD
    [15]  OILT

    byte 2 
    [16]  MARK (see below)
    [17]  Oil Pressure
    [18]  CRB
    [19]  IAT  *maybe (intake air temp?)*
    [20]  Volts
    [21]  OAT
    [22]  USD
    [23]  FF

    byte 3 
    [24]  EGTR1 (right engine) low byte
    [25]  EGTR2 (right engine) low byte
    [26]  EGTR3 (right engine) low byte
    [27]  EGTR4 (right engine) low byte
    [28]  EGTR5 (right engine) low byte
    [29]  EGTR6 (right engine) low byte
    [30]  HP and/or RT1?
    [31]  maybe RT2?

    byte 4 
    [32] - CHTR1 (right engine) 
    [33] - CHTR2 (right engine)
    [34] - CHTR3 (right engine)
    [35] - CHTR4 (right engine)
    [36] - CHTR5 (right engine)
    [37] - CHTR6 (right engine)
    [38] - RCLD (right engine)
    [39] - ROILT (right engine)

    byte 5
    [40]  MAP
    [41]  RPM low byte (left engine)
    [42]  RPM high byte (left engine)
    [43]  RIAT (right engine)
    [44]  - *maybe RPM low byte for right engine?*
    [45]  - *maybe RPM high byte for right engine?*
    [46]  RUSD
    [47]  RFF

    byte 6
    [48]  EGT1 high byte
    [49]  EGT2 high byte
    [50]  EGT3 high byte
    [51]  EGT4 high byte
    [52]  EGT5 high byte
    [53]  EGT6 high byte
    [54]  -
    [55]  -

    byte 7
    [56-63] -

    byte 8
    [64] ?
    [65] -
    [66] -
    [67] ?
    [68] ?
    [69] ?
    [70] -
    [71] ?

    byte 9
    [72] -
    [73] -
    [74] -
    [74] -
    [76] -
    [77] -
    [78] hours low byte
    [79] hours high byte

    byte 10 
    [80] -
    [81] -
    [82] -
    [83] ?
    [84] ?
    [85] Ground Speed
    [86] ?
    [87] ?

    byte 11
    [88-95] -

    byte 12 
    [96-103] -

    byte 13 
    [104-111] -

    byte 14 
    [112-119] -

    byte 15 
    [120-127] -


The MARK value (bit 16) seems to indicate times of which the EDM wants to make a
special note. I have seen the following values:

    0x02 - the timebase switches to 1 second intervals.
          EZTrends exports this as the ascii char '['.
    0x03 - the timebase returns the default interval for the flight (typically 6).
          EZTrends exports this as the ascii char ']'.
    0x04 - I'm not sure what this means. It might be the point that the EDM has
          determined to be LOP or ROP. EZTrends exports this as the ascii char '<'.

##### Data record header

Each record starts with a 5-byte header:

     7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0 |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            popMap[0]          |             popMap[1]          |  repeatCount |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-/

    struct rec_hdr_t {
       uint16_t popMap[2];   // these two values must match
       uint8_t repeatcount; // if 0, just repeat the previous record
    }

popMap[0] *MUST* equal popMap[1] or the record is corrupt.


The bytes in popMap[0] (or popMap[1]) are used to indicate which bytes of the 128-bit
bitmap are active for that data rec. Each bit in the popMap represents a word (two bytes)
of the 128-bit measurement bitmap shown in the previous section.

To do this, convert the popMap to host order, then iterate over it, least bit first.
If the bit is set in the popMap, read a byte from the data stream and map that to the
approprate word in the 128-bit measurement bitmap. I.e. bit 0 in the popMap represents
byte 0 in the bitmap. If that bit it is 1, fill out that byte by reading from the stream
if it is 0, fill byte 1 with 0x00. Then consider bit 1 in the popMap. If it is 1 read
a byte from the stream, and use that to fill out byte 1 in the bitmap. If it is 0, fill
with 0x00.

We do this twice. The first time we fill out a the 128-bit bitmap of values available
(the *fieldMap*). The second time, we fill out a 128-bit bitmap that tells us whether
to add or subtract from the previous set of measurements (the *signMap*).

##### Data record values

Now that we have the two 128-bit bitmaps filled out, the stream just supplies values. Each
bit in the fieldMap indicates which value is next in the stream. Each value bit in the sign
map indicates whether to add (signMap bit is 0) or subtract (signMap bit is 1) from the
previous value.

Each value is read as a unsigned byte (uint8_t).

##### Initial values

For *most* fields, the initial value is 0xF0.

There are some exceptions, which start with 0xFF. These are elements 30, 42, 48, 49, 50, 51, 53, and 79.
There may be others.

##### End-of-Record checksum

At the end of each record there is a checksum. This has one of two forms:

- in very old EDMs, this is an XOR of all the bytes in the record
- in most EDMs, this is the sum of every byte in the record. The sum is then negated.

