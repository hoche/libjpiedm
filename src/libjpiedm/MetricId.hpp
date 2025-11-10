#pragma once

namespace jpi_edm {

enum MetricId {
    EGT11,
    EGT12,
    EGT13,
    EGT14,
    EGT15,
    EGT16,
    EGT17,
    EGT18,
    EGT19,

    EGT21,
    EGT22,
    EGT23,
    EGT24,
    EGT25,
    EGT26,
    EGT27,
    EGT28,
    EGT29,

    CHT11,
    CHT12,
    CHT13,
    CHT14,
    CHT15,
    CHT16,
    CHT17,
    CHT18,
    CHT19,

    CHT21,
    CHT22,
    CHT23,
    CHT24,
    CHT25,
    CHT26,
    CHT27,
    CHT28,
    CHT29,

    CLD1,
    CLD2,

    TIT11,
    TIT12,
    TIT21,
    TIT22,

    OILT1,
    OILT2,

    OILP1,
    OILP2,

    CRB1,
    CRB2,

    IAT1,
    IAT2,

    MAP1,
    MAP2,

    VOLT1,
    VOLT2,

    AMP1,
    AMP2,

    FF11,
    FF12,
    FF21,
    FF22,

    FLVL11,
    FLVL12,
    FLVL13,
    FLVL21,
    FLVL22,
    FLVL23,

    FUSD11,
    FUSD12,
    FUSD21,
    FUSD22,

    FP1,
    FP2,

    HP1,
    HP2,

    RPM1,
    RPM2,

    HRS1,
    HRS2,

    TORQ1,
    TORQ2,

    LMAIN,
    RMAIN,

    LAUX,
    RAUX,

    HYDP11,
    HYDP12,
    HYDP21,
    HYDP22,

    MARK,
    OAT,
    SPD,
    ALT,
    LAT,
    LNG,

    DIF1, // temp diff between hottest and coldest EGT, engine 1
    DIF2, // temp diff between hottest and coldest EGT, engine 2
};

} // namespace jpi_edm
