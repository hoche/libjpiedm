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

#### $P - Version?

This may be a protocol version. Old-format files do not seem to have this;
new ones always have it set to the value "2".

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

     7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         flight number         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            flags              |
    |                               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[0]           |
    +      (variable length, up     +
    |         to unknown[7])        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           interval            |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      year   |  mo   |  day    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   hr    |   min     |  sec/2  |
    +-------------------------------+
    |  checksum   |
    +-------------/

In the original format, there was only 1 field in the
"unknown" array. In later formats there can be up to 8.

- There is always at least one element in the "unknown" array.
- If it's a 900+, it has at least two extra elements (making 3).
- If it's a 900+ with a build greater than 880, it has one more (making 4).
- At some point after that, it went to 7. Unfortunately, I don't know
when.

The interval field is in seconds.

The checksum is the same checksum used in the data records:

- in very old EDMs, this is an XOR of all the bytes in the record
- in most EDMs, this is the sum of every byte in the record. The sum is then negated.

### Data records

The data records represent measurements of up to either 48 different values
(old format) or 128 different values (new format).

The specific values which are updated by a particular data record can be
represented as a bitmap of all possible values. The bits that are 1's
indicate which values the data record has data.

The bits in the bitmap represent the following measurements:

*'-' means unknown and not set in the datafiles I looked at*

*'?' means unknown, but it was set*

*Elements with '1' after the text part of the descriptor are either the single engine or the left engine in twins*
*Elements with '2' after the text part are the right engine in twins*

*Elements ending in 'l' are the low byte of a multibyte value (usually EGTs or TITs)*
*Elements ending in 'h' are the low byte of a multibyte value*

    byte 0 (high values are in byte 6)
    [0]   EGT1.1.l (left engine, low byte)
    [1]   EGT1.2.l
    [2]   EGT1.3.l
    [3]   EGT1.4.l
    [4]   EGT1.5.l
    [5]   EGT1.6.l
    [6]   TIT1.1.l
    [7]   TIT1.2.l

    byte 1
    [8]   CHT1.1
    [9]   CHT1.2
    [10]  CHT1.3
    [11]  CHT1.4
    [12]  CHT1.5
    [13]  CHT1.6
    [14]  CLD1 
    [15]  OILT1

    byte 2 
    [16]  MARK (see below)
    [17]  OILP1 (oil pressure)
    [18]  CRB1  (carb temp or maybe compressor dischange temp)
    [19]  IAT1/MP2  (induction air temp) (right manifold Pressure, 760 only)
    [20]  VOLTS1
    [21]  OAT
    [22]  FUSED1 (fuel used)
    [23]  FF1 (fuel flow)

    byte 3  (high values are in byte 7)
    [24]  EGT2.1.l (right engine, low byte)
    [25]  EGT2.2.l
    [26]  EGT2.3.l
    [27]  EGT2.4.l
    [28]  EGT2.5.l
    [29]  EGT2.6.l
    [30]  HP (single engine), TIT2.1.l (twin)
    [31]  TIT2.2.l

    byte 4 
    [32] - CHT2.1
    [33] - CHT2.2
    [34] - CHT2.3
    [35] - CHT2.4
    [36] - CHT2.5
    [37] - CHT2.6
    [38] - CLD2
    [39] - OILT2

    byte 5
    [40]  MAP1
    [41]  RPM1.l
    [42]  RPM1.h
    [43]  RPM2.l
    [44]  RPM2.h, HYDP12
    [45]  HYDP11
    [46]  FUSED1.2
    [47]  FF2.1

    ---------- end of 48-bit (old version) bitmap ---------

    byte 6 (low values are in byte 0)
    [48]   EGT11.h (left engine) high byte
    [49]   EGT12.h (left engine) high byte
    [50]   EGT13.h (left engine) high byte
    [51]   EGT14.h (left engine) high byte
    [52]   EGT15.h (left engine) high byte
    [53]   EGT16.h (left engine) high byte
    [54]   TIT11.h (left engine) high byte
    [55]   TIT12.h (left engine) high byte

    byte 7 (low values are in byte 3)
    [56]  EGT21.h (right engine, high byte)
    [57]  EGT22.h
    [58]  EGT23.h
    [59]  EGT24.h
    [60]  EGT25.h
    [61]  EGT26.h
    [62]  HP (single engine), TIT21.l (twin)
    [63]  TIT22.l

    byte 8
    [64] AMPS1
    [65] VOLTS2
    [66] AMPS2
    [67] FLVL11
    [68] FLVL12
    [69] FP1
    [70] HP1
    [71] FLVL13

    byte 9
    [72] -
    [73] -
    [74] -
    [74] TORQUE1
    [76] -
    [77] -
    [78] HOURS1.l
    [79] HOURS1.h

    byte 10 
    [80] -
    [81] -
    [82] -
    [83] ?
    [84] FLVL14?
    [85] Ground Speed?
    [86] ?
    [87] ?

    byte 11
    [88] MP2
    [89] HP2
    [90] IAT2
    [91] FLVL21
    [92] FLVL21
    [93] FP2
    [94] OILP2
    [94] FLVL23

    byte 12 
    [96] UNK1.l
    [97] UNK2.l
    [98] TORQUE2
    [99] TIT2
    [100] UNK1.h
    [101] UNK2.h
    [102] HOURS2.l
    [103] HOURS2.h

    byte 13 
    [104] EGT17.l
    [105] EGT18.l
    [106] EGT19.l
    [107] FF22
    [108] EGT17.h
    [109] EGT18.h
    [110] EGT19.h
    [111] HYDP11

    byte 14 
    [112] EGT27.l
    [113] EGT28.l
    [114] EGT29.l
    [115] FF23
    [116] EGT27.h
    [117] EGT28.h
    [118] EGT29.h
    [119] HYDP21

    byte 15 
    [120] CHT17
    [121] CHT18
    [122] CHT19
    [123] HYDP12
    [124] CHT27
    [125] CHT28
    [126] CHT29
    [127] HYDP22


The MARK value (bit 16) seems to indicate times of which the EDM wants to make a
special note. I have seen the following values:

    0x02 - the timebase switches to 1 second intervals.
          EZTrends exports this as the ascii char '['.
    0x03 - the timebase returns the default interval for the flight (typically 6).
          EZTrends exports this as the ascii char ']'.
    0x04 - I'm not sure what this means. It might be the point that the EDM has
          determined to be LOP or ROP. EZTrends exports this as the ascii char '<'.

#### Data record header

##### New version

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

The bytes in popMap[0] (or popMap[1]) are a used to indicate which bytes of the 128-bit
bitmap are populated for that data rec. Each bit in the popMap represents a word (two bytes)
of the 128-bit measurement bitmap shown in the previous section.

To create the measurement bitmap, convert the popMap to host order, then iterate over it,
least bit first. If the bit is set in the popMap, read a byte from the data stream and map
that to the approprate word in the 128-bit measurement bitmap. I.e. bit 0 in the popMap
represents byte 0 in the bitmap. If that bit it is 1, fill out that byte by reading from the
stream if it is 0, fill byte 1 with 0x00. Then consider bit 1 in the popMap, and repeat.

We do this twice. The first time we fill out a the 128-bit bitmap of values available
(the *fieldMap*). The second time, we fill out a 128-bit bitmap that tells us whether
to add or subtract from the previous set of measurements (the *signMap*).

Now that we have the two 128-bit bitmaps filled out, the stream just supplies values. Each
bit in the fieldMap indicates which value is next in the stream. Each value bit in the sign
map indicates whether to add (signMap bit is 0) or subtract (signMap bit is 1) from the
previous value.

Each value is read as a unsigned byte (uint8_t).

##### Old version

Each record starts with a 3-byte header:

     7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   popMap[0]   |   popMap[1]   |  repeatCount  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-/

    struct rec_hdr_t {
       uint8_t popMap[2];   // these two values must match
       uint8_t repeatcount; // if 0, just repeat the previous record
    }

popMap[0] *MUST* equal popMap[1] or the record is corrupt.

The initial unpacking process is similar to the new version, except that there are only
8 bytes (48 bits) to deal with, and instead of 16-bit values, we're reading 8-bit values.

The bytes in popMap[0] (or popMap[1]) are used to indicate which bytes of the 48-bit
bitmap are populated for that data rec. Each bit in the popMap represents a byte of the
48-bit measurement bitmap shown in the previous section.

As before, to create the measurement bitmap, convert the popMap to host order, then iterate over it,
least bit first. If the bit is set in the popMap, read a byte from the data stream and map
that to the approprate word in the 48-bit measurement bitmap. I.e. bit 0 in the popMap
represents byte 0 in the bitmap. If that bit it is 1, fill out that byte by reading from the stream
if it is 0, fill byte 1 with 0x00. Then consider bit 1 in the popMap and repeat.

The next steps are where it varies from the 128-bit version.

Only the first 6 bits of the popMap represent fieldFlags that need to be populated.
The last two (the two high bits) represent scaleFlags that need to be populated.

Then the first 6 bits are again used, but this time to populate the signFlags.
When populating the signFlags, the last two bits of the popMap are ignored.

The data is then applied in the same way as in the 128-bit version: values are read from the
stream and the signFlags are used to decide whether to add to or subtract from the previous
value of that field.

One further difference: The scaleFlags only apply to EGTs, and the bits are used to decide
whether the value is used to apply to the first byte or the second byte of the previous value.
So depending on how you're storing them, if a scaleFlag for an EGT is set to 1, it's either
a bitshift or a multiply-by-256 before you add to or subtract from the previous value.
The scaleFlags only apply to the first 8 EGT values. In a single-engine, the high byte of
scaleFlags will always be 0. (I'm not sure how it works for a 9-cyl radial.)

#### Initial values

For *most* fields, the initial value is 0xF0.

There are some exceptions, which start with 0xFF. These are elements 30, 42, 48, 49, 50, 51, 53, and 79.
There may be others.

#### End-of-Record checksum

At the end of each record there is a checksum. This has one of two forms:

- in very old EDMs, this is an XOR of all the bytes in the record
- in most EDMs, this is the sum of every byte in the record. The sum is then negated.

