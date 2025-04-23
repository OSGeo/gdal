/******************************************************************************
 *
 * Purpose:  Enumerations, data types and related helpers for the PCIDSK SDK
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_TYPES_H
#define INCLUDE_PCIDSK_TYPES_H

#include "pcidsk_config.h"

namespace PCIDSK
{
    //! Channel pixel data types.
    enum eChanType{
        CHN_8U,     /*!< 8 bit unsigned byte */
        CHN_16S,    /*!< 16 bit signed integer */
        CHN_16U,    /*!< 16 bit unsigned integer */
        CHN_32S,      /*!< 32 bit signed integer */
        CHN_32U,      /*!< 32 bit unsigned integer */
        CHN_32R,    /*!< 32 bit ieee floating point */
        CHN_64S,      /*!< 64 bit signed integer */
        CHN_64U,      /*!< 64 bit unsigned integer */
        CHN_64R,      /*!< 64 bit ieee floating point */
        CHN_C16S,     /*!< 16-bit signed integer, complex */
        CHN_C16U,     /*!< 16-bit unsigned integer, complex */
        CHN_C32S,     /*!< 32-bit signed integer, complex */
        CHN_C32U,     /*!< 32-bit unsigned integer, complex */
        CHN_C32R,     /*!< 32-bit IEEE-754 Float, complex */
        CHN_BIT,      /*!< 1bit unsigned (packed bitmap) */
        CHN_UNKNOWN=99 /*!< unknown channel type */
    };

    //! Segment types.
    enum eSegType {
        SEG_UNKNOWN = -1,

        SEG_BIT = 101,
        SEG_VEC = 116,
        SEG_SIG = 121,
        SEG_TEX = 140,
        SEG_GEO = 150,
        SEG_ORB = 160,
        SEG_LUT = 170,
        SEG_PCT = 171,
        SEG_BLUT = 172,
        SEG_BPCT = 173,
        SEG_BIN = 180,
        SEG_ARR = 181,
        SEG_SYS = 182,
        SEG_GCPOLD = 214,
        SEG_GCP2 = 215
    };

    enum eInterpolationType {
        INTERP_UNKNOWN = -1,
        INTERP_LINEAR = 1
    };

    // Helper functions for working with segments and data types
    int PCIDSK_DLL DataTypeSize( eChanType );
    const char PCIDSK_DLL * DataTypeName( eChanType );
    const char PCIDSK_DLL * SegmentTypeName( int );
    eChanType PCIDSK_DLL GetDataTypeFromName(const char * pszDataType);
    bool PCIDSK_DLL IsDataTypeComplex(eChanType type);

} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_TYPES_H
