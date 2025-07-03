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


The interval field is in seconds.

In the original format, there was only 1 field in the
"unknown" array. In later formats there can be up to 8.

- There is always at least one element in the "unknown" array.
- If it's a 900+, it has at least two extra elements (making 3).
- If it's a 900+ with a build greater than 880, it has one more (making 4).
- At some point after that, it went to 7. Unfortunately, I don't know
when. However, when it has 7, four of the fields are used for initial GPS coordinates:

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
    |       latitude high bits      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |       latitude low bits       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |       longitude high bits     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |       longitude low bits      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          unknown[7])          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |           interval            |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      year   |  mo   |  day    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   hr    |   min     |  sec/2  |
    +-------------------------------+
    |  checksum   |
    +-------------/

If an airplane is not equipped with a GPS (or it's not hooked up to the JPI), the GPS fields
may be filled with junk data. I'm not sure on that.

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

*Elements with '1' after the text part of the descriptor are either the single engine or the left engine in s*
*Elements with '2' after the text part are the right engine in s*

*Elements ending in 'l' are the low byte of a multibyte value (usually EGTs or TITs)*
*Elements ending in 'h' are the low byte of a multibyte value*

    byte 0 (high values are in byte 6)
    [0]   EGT1.l (Exhaust Gas Temperature, single/left engine, cylinder 1, low byte)
    [1]   EGT2.l (Exhaust Gas Temperature, single/left engine, cylinder 2, low byte)
    [2]   EGT3.l (Exhaust Gas Temperature, single/left engine, cylinder 3, low byte)
    [3]   EGT4.l (Exhaust Gas Temperature, single/left engine, cylinder 4, low byte)
    [4]   EGT5.l (Exhaust Gas Temperature, single/left engine, cylinder 5, low byte)
    [5]   EGT6.l (Exhaust Gas Temperature, single/left engine, cylinder 6, low byte)
    [6]   TIT1.l (Turbo Inlet Temperature #1, single/left engine, low byte)
    [7]   TIT2.l (Turbo Inlet Temperature #2, single/left engine, low byte)

    byte 1
    [8]   CHT1 (Cylinder Head Temperature, single/left engine, cylinder 1)
    [9]   CHT2 (Cylinder Head Temperature, single/left engine, cylinder 2)
    [10]  CHT3 (Cylinder Head Temperature, single/left engine, cylinder 3)
    [11]  CHT4 (Cylinder Head Temperature, single/left engine, cylinder 4)
    [12]  CHT5 (Cylinder Head Temperature, single/left engine, cylinder 5)
    [13]  CHT6 (Cylinder Head Temperature, single/left engine, cylinder 6)
    [14]  CLD   (Engine cooling rate in degrees/min, single/left engine ) 
    [15]  OILT  (Oil temperature, single/left engine )

    byte 2 
    [16]  MARK (see below)
    [17]  OILP (oil pressure, single/left engine)
    [18]  CRB  (carb temp or maybe compressor dischange temp)
    [19]  IAT/MAP2  (induction air temp) (right manifold Pressure, 760 only)
    [20]  VOLTS (voltage, single/left engine)
    [21]  OAT (outside air temperature)
    [22]  FUSED1 (fuel used, tank 1?)
    [23]  FF  (Fuel flow, single/left engine)

    byte 3  (high values are in byte 7)
    [24]  EGTR1.l (Exhaust Gas Temperature, right engine, cylinder 1, low byte)
    [25]  EGTR2.l (Exhaust Gas Temperature, right engine, cylinder 2, low byte)
    [26]  EGTR3.l (Exhaust Gas Temperature, right engine, cylinder 3, low byte)
    [27]  EGTR4.l (Exhaust Gas Temperature, right engine, cylinder 4, low byte)
    [28]  EGTR5.l (Exhaust Gas Temperature, right engine, cylinder 5, low byte)
    [29]  EGTR6.l (Exhaust Gas Temperature, right engine, cylinder 6, low byte)
    [30]  HP (single engine), TITR1.l (Turbo Inlet Temperature #1, right engine, low byte)
    [31]  TITR2.l (Turbo Inlet Temperature #2, right engine, low byte))

    byte 4 
    [32] - CHTR1 (Cylinder Head Temperature, right engine, cylinder 1)
    [33] - CHTR2 (Cylinder Head Temperature, right engine, cylinder 2)
    [34] - CHTR3 (Cylinder Head Temperature, right engine, cylinder 3)
    [35] - CHTR4 (Cylinder Head Temperature, right engine, cylinder 4)
    [36] - CHTR5 (Cylinder Head Temperature, right engine, cylinder 5)
    [37] - CHTR6 (Cylinder Head Temperature, right engine, cylinder 6)
    [38] - CLDR   (Engine cooling rate in degrees/min, right engine)
    [39] - OILTR  (Oil Temperature, right engine)

    byte 5
    [40]  MAPR (Manifold pressure, single/left engine)
    [41]  RPM.l (RPM, single/left engine, low byte)
    [42]  RPM.h (RPM, single/left engine, high byte)
    [43]  RPMR.l (RPM, right engine, low byte)
    [44]  RPMR.h, HYDP.2 (RPM, right engine, high byte, or hydraulic pressure of something? maybe gear?)
    [45]  HYDP1 (hydraulic pressure of something? maybe gear?)
    [46]  FUSED2 (fuel used, not sure which tank)
    [47]  FF2 (Fuel flow, not sure what)

    ---------- end of 48-bit (old version) bitmap ---------

    byte 6 (low values are in byte 0)
    [48]  EGT1.h (Exhaust Gas Temperature, single/left engine, cylinder 1, high byte)
    [49]  EGT2.h (Exhaust Gas Temperature, single/left engine, cylinder 2, high byte)
    [50]  EGT3.h (Exhaust Gas Temperature, single/left engine, cylinder 3, high byte)
    [51]  EGT4.h (Exhaust Gas Temperature, single/left engine, cylinder 4, high byte)
    [52]  EGT5.h (Exhaust Gas Temperature, single/left engine, cylinder 5, high byte)
    [53]  EGT6.h (Exhaust Gas Temperature, single/left engine, cylinder 6, high byte)
    [54]  TIT1.h  (Turbo Inlet Temperature #1, single/left engine, high byte)
    [55]  TIT2.h  (Turbo Inlet Temperature #2, single/left engine, high byte)

    byte 7 (low values are in byte 3)
    [56]  EGTR1.h (Exhaust Gas Temperature, right engine, cylinder 1, high byte)
    [57]  EGTR2.h (Exhaust Gas Temperature, right engine, cylinder 2, high byte)
    [58]  EGTR3.h (Exhaust Gas Temperature, right engine, cylinder 3, high byte)
    [59]  EGTR4.h (Exhaust Gas Temperature, right engine, cylinder 4, high byte)
    [60]  EGTR5.h (Exhaust Gas Temperature, right engine, cylinder 5, high byte)
    [61]  EGTR6.h (Exhaust Gas Temperature, right engine, cylinder 6, high byte)
    [62]  HP (single engine), TITR1.h (Turbo Inlet Temperature #1, right engine, high byte)
    [63]  TITR2.h (Turbo Inlet Temperature #2, right engine, high byte)

    byte 8
    [64] AMPS1
    [65] VLT2
    [66] AMP2
    [67] RFL (Right main fuel level)
    [68] LFL (Left main fuel level)
    [69] FP (fuel pressure, single/left engine)
    [70] HP (horsepower, single/left engine)
    [71] LAUX (left aux tank fuel level)

    byte 9
    [72] -
    [73] -
    [74] TORQ (single/left engine torque)
    [76] -
    [77] -
    [78] HOURS.l (single/left engine hours, low byte)
    [79] HOURS.h (single/left engine hours, high byte)

    byte 10 
    [80] -
    [81] -
    [82] -
    [83] ?
    [84] RAUX (right aux tank fuel level)
    [85] Ground Speed in knots
    [86] longitude? (See note below)
    [87] latitude?  (See note below)

    byte 11
    [88] MP2
    [89] HP2
    [90] IAT2
    [91] FLVL21
    [92] FLVL22
    [93] FP2
    [94] OILP2
    [94] FLVL23

    byte 12 
    [96] UNK1.l
    [97] UNK2.l
    [98] TORQUE2
    [99] UNK
    [100] UNK1.h
    [101] UNK2.h
    [102] HOURSR.l (right engine hours, low byte)
    [103] HOURSR.h (right engine hours, high byte)

    byte 13 
    [104] EGT7.l (Exhaust Gas Temperature, single/left engine, cylinder 7, low byte)
    [105] EGT8.l (Exhaust Gas Temperature, single/left engine, cylinder 8, low byte)
    [106] EGT9.l (Exhaust Gas Temperature, single/left engine, cylinder 9, low byte)
    [107] FF3     (Fuel flow, not sure what)
    [108] EGT7.h (Exhaust Gas Temperature, single/left engine, cylinder 7, high byte)
    [109] EGT8.h (Exhaust Gas Temperature, single/left engine, cylinder 8, high byte)
    [110] EGT9.h (Exhaust Gas Temperature, single/left engine, cylinder 9, high byte)
    [111] HYD1 (hydraulic pressure #1, single/left engine)

    byte 14 
    [112] EGTR7.l (Exhaust Gas Temperature, right engine, cylinder 7, low byte)
    [113] EGTR8.l (Exhaust Gas Temperature, right engine, cylinder 8, low byte)
    [114] EGTR9.l (Exhaust Gas Temperature, right engine, cylinder 9, low byte)
    [115] FF4      (Fuel flow, not sure what)
    [116] EGTR7.h (Exhaust Gas Temperature, right engine, cylinder 7, high byte)
    [117] EGTR8.h (Exhaust Gas Temperature, right engine, cylinder 8, high byte)
    [118] EGTR9.h (Exhaust Gas Temperature, right engine, cylinder 9, high byte)
    [119] HYDR1 (hydraulic pressure #1, right engine)

    byte 15 
    [120] CHT7 (Cylinder Head Temperature, single/left engine, cylinder 7)
    [121] CHT8 (Cylinder Head Temperature, single/left engine, cylinder 8)
    [122] CHT9 (Cylinder Head Temperature, single/left engine, cylinder 9)
    [123] HYD2 (hydraulic pressure #2, single/left engine)
    [124] CHTR7 (Cylinder Head Temperature, right engine, cylinder 7)
    [125] CHTR8 (Cylinder Head Temperature, right engine, cylinder 8)
    [126] CHTR9 (Cylinder Head Temperature, right engine, cylinder 9)
    [127] HYDR2 (hydraulic pressure #2, right engine)


The MARK value (bit 16) seems to indicate times of which the EDM wants to make a
special note. I have seen the following values:

    0x02 - the timebase switches to 1 second intervals.
          EZTrends exports this as the ascii char '['.
    0x03 - the timebase returns the default interval for the flight (typically 6).
          EZTrends exports this as the ascii char ']'.
    0x04 - I'm not sure what this means. It might be the point that the EDM has
          determined to be LOP or ROP. EZTrends exports this as the ascii char '<'.

The GPS values appear to be differences from the initial value recorded in the flight
header data fields (fields 3 & 4 for latitude, fields 5 & 6 for longitude). If the
plane has no GPS, or it is not communicating, the JPI will still indicate that there
is GPS data for elements 86 and 87 in the population map, but the elements will always
be 0.

At some point, when it detects that there is a GPS, it will indicate this by setting
elements 86 and 87 to the value 100 (0x64). Element 86 will not have a sign bit set.
Element 87 will.

Once that happens, it can be assumed that the plane's current GPS position is the one
listed in the flight header data. That is used as the initial data point, and all
further measurements are differences from the previous measurement, as usual.


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

