/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Free standing functions for GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal.h"

#include <cctype>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_mdreader.h"
#include "gdal_priv.h"
#include "gdal_version.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

#if defined(WIN32) && _MSC_VER < 1800
inline double round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}
#endif

static int GetMinBitsForPair(
    const bool pabSigned[], const bool pabFloating[], const int panBits[])
{
    if(pabFloating[0] != pabFloating[1])
    {
        const int nNotFloatingTypeIndex = pabFloating[0] ? 1 : 0;
        const int nFloatingTypeIndex = pabFloating[0] ? 0 : 1;

        return std::max(panBits[nFloatingTypeIndex],
                        2 * panBits[nNotFloatingTypeIndex]);
    }

    if( pabSigned[0] != pabSigned[1] )
    {
        const int nUnsignedTypeIndex = pabSigned[0] ? 1 : 0;
        const int nSignedTypeIndex = pabSigned[0] ? 0 : 1;

        return std::max(panBits[nSignedTypeIndex],
                        2 * panBits[nUnsignedTypeIndex]);
    }

    return std::max(panBits[0], panBits[1]);
}

static int GetDataTypeElementSizeBits( GDALDataType eDataType )
{
    switch( eDataType )
    {
      case GDT_Byte:
        return 8;

      case GDT_UInt16:
      case GDT_Int16:
      case GDT_CInt16:
        return 16;

      case GDT_UInt32:
      case GDT_Int32:
      case GDT_Float32:
      case GDT_CInt32:
      case GDT_CFloat32:
        return 32;

      case GDT_Float64:
      case GDT_CFloat64:
        return 64;

      default:
        return 0;
    }
}

/************************************************************************/
/*                         GDALDataTypeUnion()                          */
/************************************************************************/

/**
 * \brief Return the smallest data type that can fully express both input data
 * types.
 *
 * @param eType1 first data type.
 * @param eType2 second data type.
 *
 * @return a data type able to express eType1 and eType2.
 */

GDALDataType CPL_STDCALL
GDALDataTypeUnion( GDALDataType eType1, GDALDataType eType2 )

{
    const int panBits[] = { 
        GetDataTypeElementSizeBits( eType1 ), 
        GetDataTypeElementSizeBits( eType2 ) 
    };

    if( panBits[0] == 0 || panBits[1] == 0 )
        return GDT_Unknown;

    const bool pabSigned[] = { 
        CPL_TO_BOOL( GDALDataTypeIsSigned( eType1 ) ),
        CPL_TO_BOOL( GDALDataTypeIsSigned( eType2 ) )
    };

    const bool bSigned = pabSigned[0] || pabSigned[1];
    const bool pabFloating[] = {
        CPL_TO_BOOL(GDALDataTypeIsFloating(eType1)),
        CPL_TO_BOOL(GDALDataTypeIsFloating(eType2))
    };
    const bool bFloating = pabFloating[0] || pabFloating[1];
    const bool bComplex = 
        CPL_TO_BOOL( GDALDataTypeIsComplex( eType1 ) ) ||
        CPL_TO_BOOL( GDALDataTypeIsComplex( eType2 ) );

    const int nBits = GetMinBitsForPair(pabSigned, pabFloating, panBits);

    return GDALFindDataType(nBits, bSigned, bFloating, bComplex);
}

/************************************************************************/
/*                        GDALDataTypeUnionWithValue()                  */
/************************************************************************/

/**
 * \brief Union a data type with the one found for a value
 *
 * @param eDT the first data type
 * @param dValue the value for which to find a data type and union with eDT
 * @param bComplex if the value is complex
 *
 * @return a data type able to express eDT and dValue.
 * @since GDAL 2.3
 */
GDALDataType CPL_STDCALL GDALDataTypeUnionWithValue(
    GDALDataType eDT, double dValue, int bComplex )
{
    const GDALDataType eDT2 = GDALFindDataTypeForValue(dValue, bComplex);
    return GDALDataTypeUnion(eDT, eDT2);
}

/************************************************************************/
/*                        GetMinBitsForValue()                          */
/************************************************************************/
static int GetMinBitsForValue(double dValue)
{
    if( round(dValue) == dValue )
    {
        if( dValue <= std::numeric_limits<GByte>::max() &&
            dValue >= std::numeric_limits<GByte>::min() )
            return 8;

        if( dValue <= std::numeric_limits<GInt16>::max() &&
            dValue >= std::numeric_limits<GInt16>::min() )
            return 16;

        if( dValue <= std::numeric_limits<GUInt16>::max() &&
            dValue >= std::numeric_limits<GUInt16>::min() )
            return 16;

        if( dValue <= std::numeric_limits<GInt32>::max() &&
            dValue >= std::numeric_limits<GInt32>::min() )
            return 32;

        if( dValue <= std::numeric_limits<GUInt32>::max() &&
            dValue >= std::numeric_limits<GUInt32>::min() )
            return 32;
    }
    else if( ((float)dValue) == dValue )
    {
        return 32;
    }

    return 64;
}

/************************************************************************/
/*                        GDALFindDataType()                            */
/************************************************************************/

/**
 * \brief Finds the smallest data type able to support the given
 *  requirements
 *
 * @param nBits number of bits necessary
 * @param bSigned if negative values are necessary
 * @param bFloating if non-integer values necessary
 * @param bComplex if complex values are necessary
 *
 * @return a best fit GDALDataType for supporting the requirements
 * @since GDAL 2.3
 */
GDALDataType CPL_STDCALL GDALFindDataType(
    int nBits, int bSigned, int bFloating, int bComplex )
{
    if( bSigned ) { nBits = std::max(nBits, 16); }
    if( bComplex ) { nBits = std::max(nBits, !bSigned ? 32 : 16); }
    if( bFloating ) { nBits = std::max(nBits, !bSigned ? 64 : 32); }

    if( nBits <= 8 ) { return GDT_Byte; }

    if( nBits <= 16 )
    {
        if( bComplex ) return GDT_CInt16;
        if( bSigned ) return GDT_Int16;
        return GDT_UInt16;
    }

    if( nBits <= 32 )
    {
        if( bFloating )
        {
            if( bComplex ) return GDT_CFloat32;
            return GDT_Float32;
        }

        if( bComplex ) return GDT_CInt32;
        if( bSigned ) return GDT_Int32;
        return GDT_UInt32;
    }

    if( bComplex )
        return GDT_CFloat64;

    return GDT_Float64;
}

/************************************************************************/
/*                        GDALFindDataTypeForValue()                    */
/************************************************************************/

/**
 * \brief Finds the smallest data type able to support the provided value
 *
 * @param dValue value to support
 * @param bComplex is the value complex
 *
 * @return a best fit GDALDataType for supporting the value
 * @since GDAL 2.3
 */
GDALDataType CPL_STDCALL GDALFindDataTypeForValue(
    double dValue, int bComplex )
{
    const bool bFloating = round(dValue) != dValue;
    const bool bSigned = bFloating || dValue < 0;
    const int nBits = GetMinBitsForValue(dValue);

    return GDALFindDataType(nBits, bSigned, bFloating, bComplex);
}

/************************************************************************/
/*                        GDALGetDataTypeSizeBytes()                    */
/************************************************************************/

/**
 * \brief Get data type size in <b>bytes</b>.
 *
 * Returns the size of a GDT_* type in bytes.  In contrast,
 * GDALGetDataTypeBits() returns the size in <b>bits</b>.
 *
 * @param eDataType type, such as GDT_Byte.
 * @return the number of bytes or zero if it is not recognised.
 */

int CPL_STDCALL GDALGetDataTypeSizeBytes( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Byte:
        return 1;

      case GDT_UInt16:
      case GDT_Int16:
        return 2;

      case GDT_UInt32:
      case GDT_Int32:
      case GDT_Float32:
      case GDT_CInt16:
        return 4;

      case GDT_Float64:
      case GDT_CInt32:
      case GDT_CFloat32:
        return 8;

      case GDT_CFloat64:
        return 16;

      default:
        return 0;
    }
}

/************************************************************************/
/*                        GDALGetDataTypeSizeBits()                     */
/************************************************************************/

/**
 * \brief Get data type size in <b>bits</b>.
 *
 * Returns the size of a GDT_* type in bits, <b>not bytes</b>!  Use
 * GDALGetDataTypeSizeBytes() for bytes.
 *
 * @param eDataType type, such as GDT_Byte.
 * @return the number of bits or zero if it is not recognised.
 */

int CPL_STDCALL GDALGetDataTypeSizeBits( GDALDataType eDataType )

{
    return GDALGetDataTypeSizeBytes( eDataType ) * 8;
}

/************************************************************************/
/*                        GDALGetDataTypeSize()                         */
/************************************************************************/

/**
 * \brief Get data type size in bits.  <b>Deprecated</b>.
 *
 * Returns the size of a GDT_* type in bits, <b>not bytes</b>!
 *
 * Use GDALGetDataTypeSizeBytes() for bytes.
 * Use GDALGetDataTypeSizeBits() for bits.
 *
 * @param eDataType type, such as GDT_Byte.
 * @return the number of bits or zero if it is not recognised.
 */

int CPL_STDCALL GDALGetDataTypeSize( GDALDataType eDataType )

{
    return GDALGetDataTypeSizeBytes( eDataType ) * 8;
}

/************************************************************************/
/*                       GDALDataTypeIsComplex()                        */
/************************************************************************/

/**
 * \brief Is data type complex?
 *
 * @return TRUE if the passed type is complex (one of GDT_CInt16, GDT_CInt32,
 * GDT_CFloat32 or GDT_CFloat64), that is it consists of a real and imaginary
 * component.
 */

int CPL_STDCALL GDALDataTypeIsComplex( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_CInt16:
      case GDT_CInt32:
      case GDT_CFloat32:
      case GDT_CFloat64:
        return TRUE;

      default:
        return FALSE;
    }
}

/************************************************************************/
/*                       GDALDataTypeIsFloating()                       */
/************************************************************************/

/**
 * \brief Is data type floating?
 *
 * @return TRUE if the passed type is floating
 * @since GDAL 2.3
 */

int CPL_STDCALL GDALDataTypeIsFloating( GDALDataType eDataType )
{
    switch( eDataType )
    {
      case GDT_Float32:
      case GDT_Float64:
      case GDT_CFloat32:
      case GDT_CFloat64:
        return TRUE;

      default:
        return FALSE;
    }
}

/************************************************************************/
/*                       GDALDataTypeIsSigned()                         */
/************************************************************************/

/**
 * \brief Is data type signed?
 *
 * @return TRUE if the passed type is signed.
 * @since GDAL 2.3
 */

int CPL_STDCALL GDALDataTypeIsSigned( GDALDataType eDataType )
{
    switch( eDataType )
    {
      case GDT_Byte:
      case GDT_UInt16:
      case GDT_UInt32:
        return FALSE;

      default:
        return TRUE;
    }
}

/************************************************************************/
/*                        GDALGetDataTypeName()                         */
/************************************************************************/

/**
 * \brief Get name of data type.
 *
 * Returns a symbolic name for the data type.  This is essentially the
 * the enumerated item name with the GDT_ prefix removed.  So GDT_Byte returns
 * "Byte".  The returned strings are static strings and should not be modified
 * or freed by the application.  These strings are useful for reporting
 * datatypes in debug statements, errors and other user output.
 *
 * @param eDataType type to get name of.
 * @return string corresponding to existing data type
 *         or NULL pointer if invalid type given.
 */

const char * CPL_STDCALL GDALGetDataTypeName( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Unknown:
        return "Unknown";

      case GDT_Byte:
        return "Byte";

      case GDT_UInt16:
        return "UInt16";

      case GDT_Int16:
        return "Int16";

      case GDT_UInt32:
        return "UInt32";

      case GDT_Int32:
        return "Int32";

      case GDT_Float32:
        return "Float32";

      case GDT_Float64:
        return "Float64";

      case GDT_CInt16:
        return "CInt16";

      case GDT_CInt32:
        return "CInt32";

      case GDT_CFloat32:
        return "CFloat32";

      case GDT_CFloat64:
        return "CFloat64";

      default:
        return NULL;
    }
}

/************************************************************************/
/*                        GDALGetDataTypeByName()                       */
/************************************************************************/

/**
 * \brief Get data type by symbolic name.
 *
 * Returns a data type corresponding to the given symbolic name. This
 * function is opposite to the GDALGetDataTypeName().
 *
 * @param pszName string containing the symbolic name of the type.
 *
 * @return GDAL data type.
 */

GDALDataType CPL_STDCALL GDALGetDataTypeByName( const char *pszName )

{
    VALIDATE_POINTER1( pszName, "GDALGetDataTypeByName", GDT_Unknown );

    for( int iType = 1; iType < GDT_TypeCount; iType++ )
    {
        if( GDALGetDataTypeName((GDALDataType)iType) != NULL
            && EQUAL(GDALGetDataTypeName((GDALDataType)iType), pszName) )
        {
            return (GDALDataType)iType;
        }
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                      GDALAdjustValueToDataType()                     */
/************************************************************************/

template<class T> static inline void ClampAndRound(
    double& dfValue, bool& bClamped, bool &bRounded )
{
    // TODO(schwehr): Rework this template.  ::min() versus ::lowest.

    if (dfValue < std::numeric_limits<T>::min())
    {
        bClamped = true;
        dfValue = static_cast<double>(std::numeric_limits<T>::min());
    }
    else if (dfValue > std::numeric_limits<T>::max())
    {
        bClamped = true;
        dfValue = static_cast<double>(std::numeric_limits<T>::max());
    }
    else if (dfValue != static_cast<double>(static_cast<T>(dfValue)))
    {
        bRounded = true;
        dfValue = static_cast<double>(static_cast<T>(floor(dfValue + 0.5)));
    }
}

/**
 * \brief Adjust a value to the output data type
 *
 * Adjustment consist in clamping to minimum/maxmimum values of the data type
 * and rounding for integral types.
 *
 * @param eDT target data type.
 * @param dfValue value to adjust.
 * @param pbClamped pointer to a integer(boolean) to indicate if clamping has been made, or NULL
 * @param pbRounded pointer to a integer(boolean) to indicate if rounding has been made, or NULL
 *
 * @return adjusted value
 * @since GDAL 2.1
 */

double GDALAdjustValueToDataType(
    GDALDataType eDT, double dfValue, int* pbClamped, int* pbRounded )
{
    bool bClamped = false;
    bool bRounded = false;
    switch(eDT)
    {
        case GDT_Byte:
            ClampAndRound<GByte>(dfValue, bClamped, bRounded);
            break;
        case GDT_Int16:
            ClampAndRound<GInt16>(dfValue, bClamped, bRounded);
            break;
        case GDT_UInt16:
            ClampAndRound<GUInt16>(dfValue, bClamped, bRounded);
            break;
        case GDT_Int32:
            ClampAndRound<GInt32>(dfValue, bClamped, bRounded);
            break;
        case GDT_UInt32:
            ClampAndRound<GUInt32>(dfValue, bClamped, bRounded);
            break;
        case GDT_Float32:
        {
            // TODO(schwehr): ::min() versus ::lowest.
            // Use ClampAndRound after it has been fixed.
            if ( dfValue < -std::numeric_limits<float>::max())
            {
                bClamped = TRUE;
                dfValue = static_cast<double>(
                    -std::numeric_limits<float>::max());
            }
            else if (dfValue > std::numeric_limits<float>::max())
            {
                bClamped = TRUE;
                dfValue = static_cast<double>(
                    std::numeric_limits<float>::max());
            }
            else
            {
                // Intentionaly loose precision.
                // TODO(schwehr): Is the double cast really necessary?
                // If so, why?  What will fail?
                dfValue = static_cast<double>(static_cast<float>(dfValue));
            }
            break;
        }
        default:
            break;
    }
    if( pbClamped )
        *pbClamped = bClamped;
    if( pbRounded )
        *pbRounded = bRounded;
    return dfValue;
}

/************************************************************************/
/*                        GDALGetNonComplexDataType()                */
/************************************************************************/
/**
 * \brief Return the base data type for the specified input.
 *
 * If the input data type is complex this function returns the base type
 * i.e. the data type of the real and imaginary parts (non-complex).
 * If the input data type is already non-complex, then it is returned
 * unchanged.
 *
 * @param eDataType type, such as GDT_CFloat32.
 *
 * @return GDAL data type.
 */
GDALDataType CPL_STDCALL GDALGetNonComplexDataType( GDALDataType eDataType )
{
    switch( eDataType )
    {
      case GDT_CInt16:
        return GDT_Int16;
      case GDT_CInt32:
        return GDT_Int32;
      case GDT_CFloat32:
        return GDT_Float32;
      case GDT_CFloat64:
        return GDT_Float64;
      default:
        return eDataType;
    }
}

/************************************************************************/
/*                        GDALGetAsyncStatusTypeByName()                */
/************************************************************************/
/**
 * Get AsyncStatusType by symbolic name.
 *
 * Returns a data type corresponding to the given symbolic name. This
 * function is opposite to the GDALGetAsyncStatusTypeName().
 *
 * @param pszName string containing the symbolic name of the type.
 *
 * @return GDAL AsyncStatus type.
 */
GDALAsyncStatusType CPL_DLL CPL_STDCALL GDALGetAsyncStatusTypeByName(
    const char *pszName )
{
    VALIDATE_POINTER1( pszName, "GDALGetAsyncStatusTypeByName", GARIO_ERROR);

    for( int iType = 1; iType < GARIO_TypeCount; iType++ )
    {
        if( GDALGetAsyncStatusTypeName((GDALAsyncStatusType)iType) != NULL
            && EQUAL(GDALGetAsyncStatusTypeName((GDALAsyncStatusType)iType),
                     pszName) )
        {
            return (GDALAsyncStatusType)iType;
        }
    }

    return GARIO_ERROR;
}

/************************************************************************/
/*                        GDALGetAsyncStatusTypeName()                 */
/************************************************************************/

/**
 * Get name of AsyncStatus data type.
 *
 * Returns a symbolic name for the AsyncStatus data type.  This is essentially the
 * the enumerated item name with the GARIO_ prefix removed.  So GARIO_COMPLETE returns
 * "COMPLETE".  The returned strings are static strings and should not be modified
 * or freed by the application.  These strings are useful for reporting
 * datatypes in debug statements, errors and other user output.
 *
 * @param eAsyncStatusType type to get name of.
 * @return string corresponding to type.
 */

 const char * CPL_STDCALL GDALGetAsyncStatusTypeName(
     GDALAsyncStatusType eAsyncStatusType )

{
    switch( eAsyncStatusType )
    {
      case GARIO_PENDING:
        return "PENDING";

      case GARIO_UPDATE:
        return "UPDATE";

      case GARIO_ERROR:
        return "ERROR";

      case GARIO_COMPLETE:
        return "COMPLETE";

      default:
        return NULL;
    }
}

/************************************************************************/
/*                  GDALGetPaletteInterpretationName()                  */
/************************************************************************/

/**
 * \brief Get name of palette interpretation
 *
 * Returns a symbolic name for the palette interpretation.  This is the
 * the enumerated item name with the GPI_ prefix removed.  So GPI_Gray returns
 * "Gray".  The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param eInterp palette interpretation to get name of.
 * @return string corresponding to palette interpretation.
 */

const char *GDALGetPaletteInterpretationName( GDALPaletteInterp eInterp )

{
    switch( eInterp )
    {
      case GPI_Gray:
        return "Gray";

      case GPI_RGB:
        return "RGB";

      case GPI_CMYK:
        return "CMYK";

      case GPI_HLS:
        return "HLS";

      default:
        return "Unknown";
    }
}

/************************************************************************/
/*                   GDALGetColorInterpretationName()                   */
/************************************************************************/

/**
 * \brief Get name of color interpretation
 *
 * Returns a symbolic name for the color interpretation.  This is derived from
 * the enumerated item name with the GCI_ prefix removed, but there are some
 * variations. So GCI_GrayIndex returns "Gray" and GCI_RedBand returns "Red".
 * The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param eInterp color interpretation to get name of.
 * @return string corresponding to color interpretation
 *         or NULL pointer if invalid enumerator given.
 */

const char *GDALGetColorInterpretationName( GDALColorInterp eInterp )

{
    switch( eInterp )
    {
      case GCI_Undefined:
        return "Undefined";

      case GCI_GrayIndex:
        return "Gray";

      case GCI_PaletteIndex:
        return "Palette";

      case GCI_RedBand:
        return "Red";

      case GCI_GreenBand:
        return "Green";

      case GCI_BlueBand:
        return "Blue";

      case GCI_AlphaBand:
        return "Alpha";

      case GCI_HueBand:
        return "Hue";

      case GCI_SaturationBand:
        return "Saturation";

      case GCI_LightnessBand:
        return "Lightness";

      case GCI_CyanBand:
        return "Cyan";

      case GCI_MagentaBand:
        return "Magenta";

      case GCI_YellowBand:
        return "Yellow";

      case GCI_BlackBand:
        return "Black";

      case GCI_YCbCr_YBand:
        return "YCbCr_Y";

      case GCI_YCbCr_CbBand:
        return "YCbCr_Cb";

      case GCI_YCbCr_CrBand:
        return "YCbCr_Cr";

      default:
        return "Unknown";
    }
}

/************************************************************************/
/*                GDALGetColorInterpretationByName()                    */
/************************************************************************/

/**
 * \brief Get color interpretation by symbolic name.
 *
 * Returns a color interpretation corresponding to the given symbolic name. This
 * function is opposite to the GDALGetColorInterpretationName().
 *
 * @param pszName string containing the symbolic name of the color
 * interpretation.
 *
 * @return GDAL color interpretation.
 *
 * @since GDAL 1.7.0
 */

GDALColorInterp GDALGetColorInterpretationByName( const char *pszName )

{
    VALIDATE_POINTER1( pszName, "GDALGetColorInterpretationByName",
                       GCI_Undefined );

    for( int iType = 0; iType <= GCI_Max; iType++ )
    {
        if( EQUAL(GDALGetColorInterpretationName((GDALColorInterp)iType),
                  pszName) )
        {
            return (GDALColorInterp)iType;
        }
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                     GDALGetRandomRasterSample()                      */
/************************************************************************/

/** Undocumented
 * @param hBand undocumented.
 * @param nSamples undocumented.
 * @param pafSampleBuf undocumented.
 * @return undocumented
 */
int CPL_STDCALL
GDALGetRandomRasterSample( GDALRasterBandH hBand, int nSamples,
                           float *pafSampleBuf )

{
    VALIDATE_POINTER1( hBand, "GDALGetRandomRasterSample", 0 );

    GDALRasterBand *poBand;

    poBand = (GDALRasterBand *) GDALGetRasterSampleOverview( hBand, nSamples );
    CPLAssert( NULL != poBand );

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
    int bGotNoDataValue = FALSE;

    double dfNoDataValue = poBand->GetNoDataValue( &bGotNoDataValue );

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    const int nBlocksPerRow =
        (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    const int nBlocksPerColumn =
        (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;

    const int nBlockPixels = nBlockXSize * nBlockYSize;
    const int nBlockCount = nBlocksPerRow * nBlocksPerColumn;

    if( nBlocksPerRow == 0 || nBlocksPerColumn == 0 || nBlockPixels == 0
        || nBlockCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GDALGetRandomRasterSample(): returning because band"
                  " appears degenerate." );

        return FALSE;
    }

    int nSampleRate = static_cast<int>(
        std::max(1.0, sqrt( static_cast<double>(nBlockCount) ) - 2.0));

    if( nSampleRate == nBlocksPerRow && nSampleRate > 1 )
        nSampleRate--;

    while( nSampleRate > 1
           && ((nBlockCount-1) / nSampleRate + 1) * nBlockPixels < nSamples )
        nSampleRate--;

    int nBlockSampleRate = 1;

    if ((nSamples / ((nBlockCount-1) / nSampleRate + 1)) != 0)
        nBlockSampleRate =
            std::max( 1,
                 nBlockPixels /
                 (nSamples / ((nBlockCount-1) / nSampleRate + 1)));

    int nActualSamples = 0;

    for( int iSampleBlock = 0;
         iSampleBlock < nBlockCount;
         iSampleBlock += nSampleRate )
    {

        const int iYBlock = iSampleBlock / nBlocksPerRow;
        const int iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

        GDALRasterBlock * const poBlock =
            poBand->GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;
        void* pDataRef = poBlock->GetDataRef();

        int iXValid = nBlockXSize;
        if( (iXBlock + 1) * nBlockXSize > poBand->GetXSize() )
            iXValid = poBand->GetXSize() - iXBlock * nBlockXSize;

        int iYValid = nBlockYSize;
        if( (iYBlock + 1) * nBlockYSize > poBand->GetYSize() )
            iYValid = poBand->GetYSize() - iYBlock * nBlockYSize;

        int iRemainder = 0;

        for( int iY = 0; iY < iYValid; iY++ )
        {
            int iX = iRemainder;  // Used after for.
            for( ; iX < iXValid; iX += nBlockSampleRate )
            {
                double dfValue = 0.0;
                const int iOffset = iX + iY * nBlockXSize;

                switch( poBlock->GetDataType() )
                {
                  case GDT_Byte:
                    dfValue = ((GByte *) pDataRef)[iOffset];
                    break;
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *) pDataRef)[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *) pDataRef)[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *) pDataRef)[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *) pDataRef)[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *) pDataRef)[iOffset];
                    break;
                  case GDT_Float64:
                    dfValue = ((double *) pDataRef)[iOffset];
                    break;
                  case GDT_CInt16:
                  {
                    // TODO(schwehr): Clean up casts.
                    const double dfReal = ((GInt16 *) pDataRef)[iOffset*2];
                    const double dfImag = ((GInt16 *) pDataRef)[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  }
                  case GDT_CInt32:
                  {
                    const double dfReal = ((GInt32 *) pDataRef)[iOffset*2];
                    const double dfImag = ((GInt32 *) pDataRef)[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  }
                  case GDT_CFloat32:
                  {
                    const double dfReal = ((float *) pDataRef)[iOffset*2];
                    const double dfImag = ((float *) pDataRef)[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  }
                  case GDT_CFloat64:
                  {
                    const double dfReal = ((double *) pDataRef)[iOffset*2];
                    const double dfImag = ((double *) pDataRef)[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  }
                  default:
                    CPLAssert( false );
                }

                if( bGotNoDataValue && dfValue == dfNoDataValue )
                    continue;

                if( nActualSamples < nSamples )
                    pafSampleBuf[nActualSamples++] = (float) dfValue;
            }

            iRemainder = iX - iXValid;
        }

        poBlock->DropLock();
    }

    return nActualSamples;
}

/************************************************************************/
/*                            GDALInitGCPs()                            */
/************************************************************************/

/** Initialize an array of GCPs.
 *
 * Numeric values are initialized to 0 and strings to the empty string ""
 * allocated with CPLStrdup()
 * An array initialized with GDALInitGCPs() must be de-initialized with
 * GDALDeinitGCPs().
 *
 * @param nCount number of GCPs in psGCP
 * @param psGCP array of GCPs of size nCount.
 */
void CPL_STDCALL GDALInitGCPs( int nCount, GDAL_GCP *psGCP )

{
    if( nCount > 0 )
    {
        VALIDATE_POINTER0( psGCP, "GDALInitGCPs" );
    }

    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        memset( psGCP, 0, sizeof(GDAL_GCP) );
        psGCP->pszId = CPLStrdup("");
        psGCP->pszInfo = CPLStrdup("");
        psGCP++;
    }
}

/************************************************************************/
/*                           GDALDeinitGCPs()                           */
/************************************************************************/

/** De-initialize an array of GCPs (initialized with GDALInitGCPs())
 *
 * @param nCount number of GCPs in psGCP
 * @param psGCP array of GCPs of size nCount.
 */
void CPL_STDCALL GDALDeinitGCPs( int nCount, GDAL_GCP *psGCP )

{
    if ( nCount > 0 )
    {
        VALIDATE_POINTER0( psGCP, "GDALDeinitGCPs" );
    }

    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        CPLFree( psGCP->pszId );
        CPLFree( psGCP->pszInfo );
        psGCP++;
    }
}

/************************************************************************/
/*                         GDALDuplicateGCPs()                          */
/************************************************************************/

/** Duplicate an array of GCPs
 *
 * The return must be freed with GDALDeinitGCPs() followed by CPLFree()
 *
 * @param nCount number of GCPs in psGCP
 * @param pasGCPList array of GCPs of size nCount.
 */
GDAL_GCP * CPL_STDCALL GDALDuplicateGCPs( int nCount, const GDAL_GCP *pasGCPList )

{
    GDAL_GCP *pasReturn = static_cast<GDAL_GCP *>(
        CPLMalloc(sizeof(GDAL_GCP) * nCount) );
    GDALInitGCPs( nCount, pasReturn );

    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        CPLFree( pasReturn[iGCP].pszId );
        pasReturn[iGCP].pszId = CPLStrdup( pasGCPList[iGCP].pszId );

        CPLFree( pasReturn[iGCP].pszInfo );
        pasReturn[iGCP].pszInfo = CPLStrdup( pasGCPList[iGCP].pszInfo );

        pasReturn[iGCP].dfGCPPixel = pasGCPList[iGCP].dfGCPPixel;
        pasReturn[iGCP].dfGCPLine = pasGCPList[iGCP].dfGCPLine;
        pasReturn[iGCP].dfGCPX = pasGCPList[iGCP].dfGCPX;
        pasReturn[iGCP].dfGCPY = pasGCPList[iGCP].dfGCPY;
        pasReturn[iGCP].dfGCPZ = pasGCPList[iGCP].dfGCPZ;
    }

    return pasReturn;
}

/************************************************************************/
/*                       GDALFindAssociatedFile()                       */
/************************************************************************/

/**
 * \fn GDALFindAssociatedFile(const char*, const char*, char**, int)
 * Find file with alternate extension.
 *
 * Finds the file with the indicated extension, substituting it in place
 * of the extension of the base filename.  Generally used to search for
 * associated files like world files .RPB files, etc.  If necessary, the
 * extension will be tried in both upper and lower case.  If a sibling file
 * list is available it will be used instead of doing VSIStatExL() calls to
 * probe the file system.
 *
 * Note that the result is a dynamic CPLString so this method should not
 * be used in a situation where there could be cross heap issues.  It is
 * generally imprudent for application built on GDAL to use this function
 * unless they are sure they will always use the same runtime heap as GDAL.
 *
 * @param pszBaseFilename the filename relative to which to search.
 * @param pszExt the target extension in either upper or lower case.
 * @param papszSiblingFiles the list of files in the same directory as
 * pszBaseFilename or NULL if they are not known.
 * @param nFlags special options controlling search.  None defined yet, just
 * pass 0.
 *
 * @return an empty string if the target is not found, otherwise the target
 * file with similar path style as the pszBaseFilename.
 */

/**/
/**/

CPLString GDALFindAssociatedFile( const char *pszBaseFilename,
                                  const char *pszExt,
                                  char **papszSiblingFiles,
                                  int /* nFlags */ )

{
    CPLString osTarget = CPLResetExtension( pszBaseFilename, pszExt );

    if( papszSiblingFiles == NULL )
    {
        VSIStatBufL sStatBuf;

        if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
        {
            CPLString osAltExt = pszExt;

            if( islower( pszExt[0] ) )
                osAltExt.toupper();
            else
                osAltExt.tolower();

            osTarget = CPLResetExtension( pszBaseFilename, osAltExt );

            if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
                return "";
        }
    }
    else
    {
        const int iSibling = CSLFindString( papszSiblingFiles,
                                            CPLGetFilename(osTarget) );
        if( iSibling < 0 )
            return "";

        osTarget.resize(osTarget.size() - strlen(papszSiblingFiles[iSibling]));
        osTarget += papszSiblingFiles[iSibling];
    }

    return osTarget;
}

/************************************************************************/
/*                         GDALLoadOziMapFile()                         */
/************************************************************************/

/** Helper function for translator implementer wanting support for OZI .map
 *
 * @param pszFilename filename of .tab file
 * @param padfGeoTransform output geotransform. Must hold 6 doubles.
 * @param ppszWKT output pointer to a string that will be allocated with CPLMalloc().
 * @param pnGCPCount output pointer to GCP count.
 * @param ppasGCPs outputer pointer to an array of GCPs.
 * @return TRUE in case of success, FALSE otherwise.
 */
int CPL_STDCALL GDALLoadOziMapFile( const char *pszFilename,
                                    double *padfGeoTransform, char **ppszWKT,
                                    int *pnGCPCount, GDAL_GCP **ppasGCPs )

{
    VALIDATE_POINTER1( pszFilename, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( pnGCPCount, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( ppasGCPs, "GDALLoadOziMapFile", FALSE );

    char **papszLines = CSLLoad2( pszFilename, 1000, 200, NULL );

    if ( !papszLines )
        return FALSE;

    int nLines = CSLCount( papszLines );

    // Check the OziExplorer Map file signature
    if ( nLines < 5
         || !STARTS_WITH_CI(papszLines[0], "OziExplorer Map Data File Version ") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
        "GDALLoadOziMapFile(): file \"%s\" is not in OziExplorer Map format.",
                  pszFilename );
        CSLDestroy( papszLines );
        return FALSE;
    }

    OGRSpatialReference oSRS;
    OGRErr eErr = OGRERR_NONE;

    /* The Map Scale Factor has been introduced recently on the 6th line */
    /* and is a trick that is used to just change that line without changing */
    /* the rest of the MAP file but providing an imagery that is smaller or larger */
    /* so we have to correct the pixel/line values read in the .MAP file so they */
    /* match the actual imagery dimension. Well, this is a bad summary of what */
    /* is explained at http://tech.groups.yahoo.com/group/OziUsers-L/message/12484 */
    double dfMSF = 1;

    for ( int iLine = 5; iLine < nLines; iLine++ )
    {
        if ( STARTS_WITH_CI(papszLines[iLine], "MSF,") )
        {
            dfMSF = CPLAtof(papszLines[iLine] + 4);
            if (dfMSF <= 0.01) /* Suspicious values */
            {
                CPLDebug("OZI", "Suspicious MSF value : %s", papszLines[iLine]);
                dfMSF = 1;
            }
        }
    }

    eErr = oSRS.importFromOzi( papszLines );
    if ( eErr == OGRERR_NONE )
    {
        if ( ppszWKT != NULL )
            oSRS.exportToWkt( ppszWKT );
    }

    int nCoordinateCount = 0;
    // TODO(schwehr): Initialize asGCPs.
    GDAL_GCP asGCPs[30];

    // Iterate all lines in the MAP-file
    for ( int iLine = 5; iLine < nLines; iLine++ )
    {
        char **papszTok = CSLTokenizeString2( papszLines[iLine], ",",
                                              CSLT_ALLOWEMPTYTOKENS
                                              | CSLT_STRIPLEADSPACES
                                              | CSLT_STRIPENDSPACES );

        if ( CSLCount(papszTok) < 12 )
        {
            CSLDestroy(papszTok);
            continue;
        }

        if ( CSLCount(papszTok) >= 17
             && STARTS_WITH_CI(papszTok[0], "Point")
             && !EQUAL(papszTok[2], "")
             && !EQUAL(papszTok[3], "")
             && nCoordinateCount <
             static_cast<int>(CPL_ARRAYSIZE(asGCPs)) )
        {
            bool bReadOk = false;
            double dfLon = 0.0;
            double dfLat = 0.0;

            if ( !EQUAL(papszTok[6], "")
                 && !EQUAL(papszTok[7], "")
                 && !EQUAL(papszTok[9], "")
                 && !EQUAL(papszTok[10], "") )
            {
                // Set geographical coordinates of the pixels
                dfLon = CPLAtofM(papszTok[9]) + CPLAtofM(papszTok[10]) / 60.0;
                dfLat = CPLAtofM(papszTok[6]) + CPLAtofM(papszTok[7]) / 60.0;
                if ( EQUAL(papszTok[11], "W") )
                    dfLon = -dfLon;
                if ( EQUAL(papszTok[8], "S") )
                    dfLat = -dfLat;

                // Transform from the geographical coordinates into projected
                // coordinates.
                if ( eErr == OGRERR_NONE )
                {
                    OGRSpatialReference *poLatLong = oSRS.CloneGeogCS();

                    if ( poLatLong )
                    {
                        OGRCoordinateTransformation *poTransform =
                            OGRCreateCoordinateTransformation( poLatLong,
                                                               &oSRS );
                        if ( poTransform )
                        {
                            bReadOk = CPL_TO_BOOL(
                                poTransform->Transform( 1, &dfLon, &dfLat ));
                            delete poTransform;
                        }
                        delete poLatLong;
                    }
                }
            }
            else if ( !EQUAL(papszTok[14], "")
                      && !EQUAL(papszTok[15], "") )
            {
                // Set cartesian coordinates of the pixels.
                dfLon = CPLAtofM(papszTok[14]);
                dfLat = CPLAtofM(papszTok[15]);
                bReadOk = true;

                //if ( EQUAL(papszTok[16], "S") )
                //    dfLat = -dfLat;
            }

            if ( bReadOk )
            {
                GDALInitGCPs( 1, asGCPs + nCoordinateCount );

                // Set pixel/line part
                asGCPs[nCoordinateCount].dfGCPPixel =
                    CPLAtofM(papszTok[2]) / dfMSF;
                asGCPs[nCoordinateCount].dfGCPLine =
                    CPLAtofM(papszTok[3]) / dfMSF;

                asGCPs[nCoordinateCount].dfGCPX = dfLon;
                asGCPs[nCoordinateCount].dfGCPY = dfLat;

                nCoordinateCount++;
            }
        }

        CSLDestroy( papszTok );
    }

    CSLDestroy( papszLines );

    if ( nCoordinateCount == 0 )
    {
        CPLDebug( "GDAL", "GDALLoadOziMapFile(\"%s\") did read no GCPs.",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to convert the GCPs into a geotransform definition, if      */
/*      possible.  Otherwise we will need to use them as GCPs.          */
/* -------------------------------------------------------------------- */
    if( !GDALGCPsToGeoTransform(
            nCoordinateCount, asGCPs, padfGeoTransform,
            CPLTestBool(CPLGetConfigOption("OZI_APPROX_GEOTRANSFORM", "NO")) ) )
    {
        if ( pnGCPCount && ppasGCPs )
        {
            CPLDebug( "GDAL",
                "GDALLoadOziMapFile(%s) found file, was not able to derive a\n"
                "first order geotransform.  Using points as GCPs.",
                pszFilename );

            *ppasGCPs = static_cast<GDAL_GCP *>(
                CPLCalloc( sizeof(GDAL_GCP), nCoordinateCount ) );
            memcpy( *ppasGCPs, asGCPs, sizeof(GDAL_GCP) * nCoordinateCount );
            *pnGCPCount = nCoordinateCount;
        }
    }
    else
    {
        GDALDeinitGCPs( nCoordinateCount, asGCPs );
    }

    return TRUE;
}

/************************************************************************/
/*                       GDALReadOziMapFile()                           */
/************************************************************************/

/** Helper function for translator implementer wanting support for OZI .map
 *
 * @param pszBaseFilename filename whose basename will help building the .map filename.
 * @param padfGeoTransform output geotransform. Must hold 6 doubles.
 * @param ppszWKT output pointer to a string that will be allocated with CPLMalloc().
 * @param pnGCPCount output pointer to GCP count.
 * @param ppasGCPs outputer pointer to an array of GCPs.
 * @return TRUE in case of success, FALSE otherwise.
 */
int CPL_STDCALL GDALReadOziMapFile( const char * pszBaseFilename,
                                    double *padfGeoTransform, char **ppszWKT,
                                    int *pnGCPCount, GDAL_GCP **ppasGCPs )

{
/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */
    const char *pszOzi = CPLResetExtension( pszBaseFilename, "map" );

    VSILFILE *fpOzi = VSIFOpenL( pszOzi, "rt" );

    if ( fpOzi == NULL && VSIIsCaseSensitiveFS(pszOzi) )
    {
        pszOzi = CPLResetExtension( pszBaseFilename, "MAP" );
        fpOzi = VSIFOpenL( pszOzi, "rt" );
    }

    if ( fpOzi == NULL )
        return FALSE;

    CPL_IGNORE_RET_VAL(VSIFCloseL( fpOzi ));

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    return GDALLoadOziMapFile( pszOzi, padfGeoTransform, ppszWKT,
                               pnGCPCount, ppasGCPs );
}

/************************************************************************/
/*                         GDALLoadTabFile()                            */
/*                                                                      */
/************************************************************************/

/** Helper function for translator implementer wanting support for MapInfo
 * .tab files.
 *
 * @param pszFilename filename of .tab
 * @param padfGeoTransform output geotransform. Must hold 6 doubles.
 * @param ppszWKT output pointer to a string that will be allocated with CPLMalloc().
 * @param pnGCPCount output pointer to GCP count.
 * @param ppasGCPs outputer pointer to an array of GCPs.
 * @return TRUE in case of success, FALSE otherwise.
 */
int CPL_STDCALL GDALLoadTabFile( const char *pszFilename,
                                 double *padfGeoTransform, char **ppszWKT,
                                 int *pnGCPCount, GDAL_GCP **ppasGCPs )

{
    char **papszLines = CSLLoad2( pszFilename, 1000, 200, NULL );

    if ( !papszLines )
        return FALSE;

    char **papszTok = NULL;
    bool bTypeRasterFound = false;
    bool bInsideTableDef = false;
    int nCoordinateCount = 0;
    GDAL_GCP asGCPs[256];  // TODO(schwehr): Initialize.
    const int numLines = CSLCount(papszLines);

    // Iterate all lines in the TAB-file
    for( int iLine=0; iLine<numLines; iLine++ )
    {
        CSLDestroy(papszTok);
        papszTok = CSLTokenizeStringComplex(papszLines[iLine], " \t(),;",
                                            TRUE, FALSE);

        if (CSLCount(papszTok) < 2)
            continue;

        // Did we find table definition
        if (EQUAL(papszTok[0], "Definition") && EQUAL(papszTok[1], "Table") )
        {
            bInsideTableDef = TRUE;
        }
        else if (bInsideTableDef && (EQUAL(papszTok[0], "Type")) )
        {
            // Only RASTER-type will be handled
            if (EQUAL(papszTok[1], "RASTER"))
            {
                bTypeRasterFound = true;
            }
            else
            {
                CSLDestroy(papszTok);
                CSLDestroy(papszLines);
                return FALSE;
            }
        }
        else if (bTypeRasterFound && bInsideTableDef
                 && CSLCount(papszTok) > 4
                 && EQUAL(papszTok[4], "Label")
                 && nCoordinateCount <
                 static_cast<int>(CPL_ARRAYSIZE(asGCPs)) )
        {
            GDALInitGCPs( 1, asGCPs + nCoordinateCount );

            asGCPs[nCoordinateCount].dfGCPPixel = CPLAtofM(papszTok[2]);
            asGCPs[nCoordinateCount].dfGCPLine = CPLAtofM(papszTok[3]);
            asGCPs[nCoordinateCount].dfGCPX = CPLAtofM(papszTok[0]);
            asGCPs[nCoordinateCount].dfGCPY = CPLAtofM(papszTok[1]);
            if( papszTok[5] != NULL )
            {
                CPLFree( asGCPs[nCoordinateCount].pszId );
                asGCPs[nCoordinateCount].pszId = CPLStrdup(papszTok[5]);
            }

            nCoordinateCount++;
        }
        else if( bTypeRasterFound && bInsideTableDef
                 && EQUAL(papszTok[0],"CoordSys")
                 && ppszWKT != NULL )
        {
            OGRSpatialReference oSRS;

            if( oSRS.importFromMICoordSys( papszLines[iLine] ) == OGRERR_NONE )
                oSRS.exportToWkt( ppszWKT );
        }
        else if( EQUAL(papszTok[0],"Units")
                 && CSLCount(papszTok) > 1
                 && EQUAL(papszTok[1],"degree") )
        {
            /*
            ** If we have units of "degree", but a projected coordinate
            ** system we need to convert it to geographic.  See to01_02.TAB.
            */
            if( ppszWKT != NULL && *ppszWKT != NULL
                && STARTS_WITH_CI(*ppszWKT, "PROJCS") )
            {
                char *pszSrcWKT = *ppszWKT;

                OGRSpatialReference oSRS;
                oSRS.importFromWkt( &pszSrcWKT );

                OGRSpatialReference oSRSGeogCS;
                oSRSGeogCS.CopyGeogCSFrom( &oSRS );
                CPLFree( *ppszWKT );

                oSRSGeogCS.exportToWkt( ppszWKT );
            }
        }
    }

    CSLDestroy(papszTok);
    CSLDestroy(papszLines);

    if( nCoordinateCount == 0 )
    {
        CPLDebug( "GDAL", "GDALLoadTabFile(%s) did not get any GCPs.",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to convert the GCPs into a geotransform definition, if      */
/*      possible.  Otherwise we will need to use them as GCPs.          */
/* -------------------------------------------------------------------- */
    if( !GDALGCPsToGeoTransform(
            nCoordinateCount, asGCPs, padfGeoTransform,
            CPLTestBool(CPLGetConfigOption("TAB_APPROX_GEOTRANSFORM", "NO")) ) )
    {
        if (pnGCPCount && ppasGCPs)
        {
            CPLDebug( "GDAL",
                "GDALLoadTabFile(%s) found file, was not able to derive a "
                "first order geotransform.  Using points as GCPs.",
                pszFilename );

            *ppasGCPs = static_cast<GDAL_GCP *>(
                CPLCalloc( sizeof(GDAL_GCP), nCoordinateCount ) );
            memcpy( *ppasGCPs, asGCPs, sizeof(GDAL_GCP) * nCoordinateCount );
            *pnGCPCount = nCoordinateCount;
        }
    }
    else
    {
        GDALDeinitGCPs( nCoordinateCount, asGCPs );
    }

    return TRUE;
}

/************************************************************************/
/*                         GDALReadTabFile()                            */
/************************************************************************/

/** Helper function for translator implementer wanting support for MapInfo
 * .tab files.
 *
 * @param pszBaseFilename filename whose basename will help building the .tab filename.
 * @param padfGeoTransform output geotransform. Must hold 6 doubles.
 * @param ppszWKT output pointer to a string that will be allocated with CPLMalloc().
 * @param pnGCPCount output pointer to GCP count.
 * @param ppasGCPs outputer pointer to an array of GCPs.
 * @return TRUE in case of success, FALSE otherwise.
 */
int CPL_STDCALL GDALReadTabFile( const char * pszBaseFilename,
                                 double *padfGeoTransform, char **ppszWKT,
                                 int *pnGCPCount, GDAL_GCP **ppasGCPs )

{
    return GDALReadTabFile2( pszBaseFilename, padfGeoTransform,
                             ppszWKT, pnGCPCount, ppasGCPs,
                             NULL, NULL );
}

int GDALReadTabFile2( const char * pszBaseFilename,
                      double *padfGeoTransform, char **ppszWKT,
                      int *pnGCPCount, GDAL_GCP **ppasGCPs,
                      char** papszSiblingFiles, char** ppszTabFileNameOut )
{
    if (ppszTabFileNameOut)
        *ppszTabFileNameOut = NULL;

    if( !GDALCanFileAcceptSidecarFile(pszBaseFilename) )
        return FALSE;

    const char *pszTAB = CPLResetExtension( pszBaseFilename, "tab" );

    if (papszSiblingFiles)
    {
        int iSibling = CSLFindString(papszSiblingFiles, CPLGetFilename(pszTAB));
        if (iSibling >= 0)
        {
            CPLString osTabFilename = pszBaseFilename;
            osTabFilename.resize(strlen(pszBaseFilename) -
                                 strlen(CPLGetFilename(pszBaseFilename)));
            osTabFilename += papszSiblingFiles[iSibling];
            if ( GDALLoadTabFile(osTabFilename, padfGeoTransform, ppszWKT,
                                 pnGCPCount, ppasGCPs ) )
            {
                if (ppszTabFileNameOut)
                    *ppszTabFileNameOut = CPLStrdup(osTabFilename);
                return TRUE;
            }
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */

    VSILFILE *fpTAB = VSIFOpenL( pszTAB, "rt" );

    if( fpTAB == NULL && VSIIsCaseSensitiveFS(pszTAB) )
    {
        pszTAB = CPLResetExtension( pszBaseFilename, "TAB" );
        fpTAB = VSIFOpenL( pszTAB, "rt" );
    }

    if( fpTAB == NULL )
        return FALSE;

    CPL_IGNORE_RET_VAL(VSIFCloseL( fpTAB ));

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    if (GDALLoadTabFile( pszTAB, padfGeoTransform, ppszWKT,
                         pnGCPCount, ppasGCPs ) )
    {
        if (ppszTabFileNameOut)
            *ppszTabFileNameOut = CPLStrdup(pszTAB);
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALLoadWorldFile()                          */
/************************************************************************/

/**
 * \brief Read ESRI world file.
 *
 * This function reads an ESRI style world file, and formats a geotransform
 * from its contents.
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array.
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszFilename the world file name.
 * @param padfGeoTransform the six double array into which the
 * geotransformation should be placed.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL
GDALLoadWorldFile( const char *pszFilename, double *padfGeoTransform )

{
    VALIDATE_POINTER1( pszFilename, "GDALLoadWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALLoadWorldFile", FALSE );

    char **papszLines = CSLLoad2( pszFilename, 100, 100, NULL );

    if ( !papszLines )
        return FALSE;

    double world[6] = { 0.0 };
    // reads the first 6 non-empty lines
    int nLines = 0;
    const int nLinesCount = CSLCount(papszLines);
    for( int i = 0;
         i < nLinesCount && nLines < static_cast<int>(CPL_ARRAYSIZE(world));
         ++i )
    {
        CPLString line(papszLines[i]);
        if( line.Trim().empty() )
          continue;

        world[nLines] = CPLAtofM(line);
        ++nLines;
    }

    if( nLines == 6
        && (world[0] != 0.0 || world[2] != 0.0)
        && (world[3] != 0.0 || world[1] != 0.0) )
    {
        padfGeoTransform[0] = world[4];
        padfGeoTransform[1] = world[0];
        padfGeoTransform[2] = world[2];
        padfGeoTransform[3] = world[5];
        padfGeoTransform[4] = world[1];
        padfGeoTransform[5] = world[3];

        // correct for center of pixel vs. top left of pixel
        padfGeoTransform[0] -= 0.5 * padfGeoTransform[1];
        padfGeoTransform[0] -= 0.5 * padfGeoTransform[2];
        padfGeoTransform[3] -= 0.5 * padfGeoTransform[4];
        padfGeoTransform[3] -= 0.5 * padfGeoTransform[5];

        CSLDestroy(papszLines);

        return TRUE;
    }
    else
    {
        CPLDebug( "GDAL",
                  "GDALLoadWorldFile(%s) found file, but it was corrupt.",
                  pszFilename );
        CSLDestroy(papszLines);
        return FALSE;
    }
}

/************************************************************************/
/*                         GDALReadWorldFile()                          */
/************************************************************************/

/**
 * \brief Read ESRI world file.
 *
 * This function reads an ESRI style world file, and formats a geotransform
 * from its contents.  It does the same as GDALLoadWorldFile() function, but
 * it will form the filename for the worldfile from the filename of the raster
 * file referred and the suggested extension.  If no extension is provided,
 * the code will internally try the unix style and windows style world file
 * extensions (eg. for .tif these would be .tfw and .tifw).
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array.
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszBaseFilename the target raster file.
 * @param pszExtension the extension to use (i.e. ".wld") or NULL to derive it
 * from the pszBaseFilename
 * @param padfGeoTransform the six double array into which the
 * geotransformation should be placed.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL
GDALReadWorldFile( const char *pszBaseFilename, const char *pszExtension,
                   double *padfGeoTransform )

{
    return GDALReadWorldFile2( pszBaseFilename, pszExtension,
                               padfGeoTransform, NULL, NULL );
}

int GDALReadWorldFile2( const char *pszBaseFilename, const char *pszExtension,
                        double *padfGeoTransform, char** papszSiblingFiles,
                        char** ppszWorldFileNameOut )
{
    VALIDATE_POINTER1( pszBaseFilename, "GDALReadWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALReadWorldFile", FALSE );

    if (ppszWorldFileNameOut)
        *ppszWorldFileNameOut = NULL;

    if( !GDALCanFileAcceptSidecarFile(pszBaseFilename) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      If we aren't given an extension, try both the unix and          */
/*      windows style extensions.                                       */
/* -------------------------------------------------------------------- */
    if( pszExtension == NULL )
    {
        const std::string oBaseExt = CPLGetExtension( pszBaseFilename );

        if( oBaseExt.length() < 2 )
            return FALSE;

        // windows version - first + last + 'w'
        char szDerivedExtension[100] = { '\0' };
        szDerivedExtension[0] = oBaseExt[0];
        szDerivedExtension[1] = oBaseExt[oBaseExt.length()-1];
        szDerivedExtension[2] = 'w';
        szDerivedExtension[3] = '\0';

        if( GDALReadWorldFile2( pszBaseFilename, szDerivedExtension,
                                padfGeoTransform, papszSiblingFiles,
                                ppszWorldFileNameOut ) )
            return TRUE;

        // unix version - extension + 'w'
        if( oBaseExt.length() > sizeof(szDerivedExtension)-2 )
            return FALSE;

        snprintf( szDerivedExtension, sizeof(szDerivedExtension),
                  "%sw", oBaseExt.c_str() );
        return GDALReadWorldFile2( pszBaseFilename, szDerivedExtension,
                                   padfGeoTransform, papszSiblingFiles,
                                   ppszWorldFileNameOut );
    }

/* -------------------------------------------------------------------- */
/*      Skip the leading period in the extension if there is one.       */
/* -------------------------------------------------------------------- */
    if( *pszExtension == '.' )
        pszExtension++;

/* -------------------------------------------------------------------- */
/*      Generate upper and lower case versions of the extension.        */
/* -------------------------------------------------------------------- */
    char szExtUpper[32] = { '\0' };
    char szExtLower[32] = { '\0' };
    CPLStrlcpy( szExtUpper, pszExtension, sizeof(szExtUpper) );
    CPLStrlcpy( szExtLower, pszExtension, sizeof(szExtLower) );

    for( int i = 0; szExtUpper[i] != '\0'; i++ )
    {
        szExtUpper[i] = static_cast<char>( toupper(szExtUpper[i]) );
        szExtLower[i] = static_cast<char>( tolower(szExtLower[i]) );
    }

    const char *pszTFW = CPLResetExtension( pszBaseFilename, szExtLower );

    if (papszSiblingFiles)
    {
        const int iSibling =
            CSLFindString(papszSiblingFiles, CPLGetFilename(pszTFW));
        if (iSibling >= 0)
        {
            CPLString osTFWFilename = pszBaseFilename;
            osTFWFilename.resize(strlen(pszBaseFilename) -
                                 strlen(CPLGetFilename(pszBaseFilename)));
            osTFWFilename += papszSiblingFiles[iSibling];
            if (GDALLoadWorldFile( osTFWFilename, padfGeoTransform ))
            {
                if (ppszWorldFileNameOut)
                    *ppszWorldFileNameOut = CPLStrdup(osTFWFilename);
                return TRUE;
            }
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */

    VSIStatBufL sStatBuf;
    bool bGotTFW = VSIStatExL( pszTFW, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0;

    if( !bGotTFW  && VSIIsCaseSensitiveFS(pszTFW) )
    {
        pszTFW = CPLResetExtension( pszBaseFilename, szExtUpper );
        bGotTFW = VSIStatExL( pszTFW, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0;
    }

    if( !bGotTFW )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    if (GDALLoadWorldFile( pszTFW, padfGeoTransform ))
    {
        if (ppszWorldFileNameOut)
            *ppszWorldFileNameOut = CPLStrdup(pszTFW);
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALWriteWorldFile()                         */
/*                                                                      */
/*      Helper function for translator implementer wanting              */
/*      support for ESRI world files.                                   */
/************************************************************************/

/**
 * \brief Write ESRI world file.
 *
 * This function writes an ESRI style world file from the passed geotransform.
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array.
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszBaseFilename the target raster file.
 * @param pszExtension the extension to use (i.e. ".wld"). Must not be NULL
 * @param padfGeoTransform the six double array from which the
 * geotransformation should be read.
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL
GDALWriteWorldFile( const char * pszBaseFilename, const char *pszExtension,
                    double *padfGeoTransform )

{
    VALIDATE_POINTER1( pszBaseFilename, "GDALWriteWorldFile", FALSE );
    VALIDATE_POINTER1( pszExtension, "GDALWriteWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALWriteWorldFile", FALSE );

/* -------------------------------------------------------------------- */
/*      Prepare the text to write to the file.                          */
/* -------------------------------------------------------------------- */
    CPLString osTFWText;

    osTFWText.Printf( "%.10f\n%.10f\n%.10f\n%.10f\n%.10f\n%.10f\n",
                      padfGeoTransform[1],
                      padfGeoTransform[4],
                      padfGeoTransform[2],
                      padfGeoTransform[5],
                      padfGeoTransform[0]
                      + 0.5 * padfGeoTransform[1]
                      + 0.5 * padfGeoTransform[2],
                      padfGeoTransform[3]
                      + 0.5 * padfGeoTransform[4]
                      + 0.5 * padfGeoTransform[5] );

/* -------------------------------------------------------------------- */
/*      Update extension, and write to disk.                            */
/* -------------------------------------------------------------------- */
    const char *pszTFW = CPLResetExtension( pszBaseFilename, pszExtension );
    VSILFILE * const fpTFW = VSIFOpenL( pszTFW, "wt" );
    if( fpTFW == NULL )
        return FALSE;

    const int bRet =
        VSIFWriteL( (void *) osTFWText.c_str(), osTFWText.size(), 1, fpTFW )
        == 1;
    if( VSIFCloseL( fpTFW ) != 0 )
        return FALSE;

    return bRet;
}

/************************************************************************/
/*                          GDALVersionInfo()                           */
/************************************************************************/

/**
 * \brief Get runtime version information.
 *
 * Available pszRequest values:
 * <ul>
 * <li> "VERSION_NUM": Returns GDAL_VERSION_NUM formatted as a string.  i.e. "1170"
 *      Note: starting with GDAL 1.10, this string will be longer than 4 characters.
 * <li> "RELEASE_DATE": Returns GDAL_RELEASE_DATE formatted as a string.
 * i.e. "20020416".
 * <li> "RELEASE_NAME": Returns the GDAL_RELEASE_NAME. ie. "1.1.7"
 * <li> "--version": Returns one line version message suitable for use in
 * response to --version requests.  i.e. "GDAL 1.1.7, released 2002/04/16"
 * <li> "LICENSE": Returns the content of the LICENSE.TXT file from the GDAL_DATA directory.
 *      Before GDAL 1.7.0, the returned string was leaking memory but this is now resolved.
 *      So the result should not been freed by the caller.
 * <li> "BUILD_INFO": List of NAME=VALUE pairs separated by newlines with
 * information on build time options.
 * </ul>
 *
 * @param pszRequest the type of version info desired, as listed above.
 *
 * @return an internal string containing the requested information.
 */

const char * CPL_STDCALL GDALVersionInfo( const char *pszRequest )

{
/* -------------------------------------------------------------------- */
/*      Try to capture as much build information as practical.          */
/* -------------------------------------------------------------------- */
    if( pszRequest != NULL && EQUAL(pszRequest,"BUILD_INFO") )
    {
        CPLString osBuildInfo;

#ifdef ESRI_BUILD
        osBuildInfo += "ESRI_BUILD=YES\n";
#endif
#ifdef PAM_ENABLED
        osBuildInfo += "PAM_ENABLED=YES\n";
#endif
        osBuildInfo += "OGR_ENABLED=YES\n";  // Deprecated.  Always yes.

        CPLFree(CPLGetTLS(CTLS_VERSIONINFO));
        CPLSetTLS(CTLS_VERSIONINFO, CPLStrdup(osBuildInfo), TRUE );
        return static_cast<char *>( CPLGetTLS(CTLS_VERSIONINFO) );
    }

/* -------------------------------------------------------------------- */
/*      LICENSE is a special case. We try to find and read the          */
/*      LICENSE.TXT file from the GDAL_DATA directory and return it     */
/* -------------------------------------------------------------------- */
    if( pszRequest != NULL && EQUAL(pszRequest,"LICENSE") )
    {
        char* pszResultLicence = reinterpret_cast<char *>(
            CPLGetTLS( CTLS_VERSIONINFO_LICENCE ) );
        if( pszResultLicence != NULL )
        {
            return pszResultLicence;
        }

        const char *pszFilename = CPLFindFile( "etc", "LICENSE.TXT" );
        VSILFILE *fp = NULL;

        if( pszFilename != NULL )
            fp = VSIFOpenL( pszFilename, "r" );

        if( fp != NULL )
        {
            if( VSIFSeekL( fp, 0, SEEK_END ) == 0 )
            {
                // TODO(schwehr): Handle if VSITellL returns a value too large
                // for size_t.
                const size_t nLength =
                    static_cast<size_t>( VSIFTellL( fp ) + 1 );
                if( VSIFSeekL( fp, SEEK_SET, 0 ) == 0 )
                {
                    pszResultLicence = static_cast<char *>(
                        VSICalloc(1, nLength) );
                    if (pszResultLicence)
                        CPL_IGNORE_RET_VAL(
                            VSIFReadL( pszResultLicence, 1, nLength - 1, fp ) );
                }
            }

            CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
        }

        if (!pszResultLicence)
        {
            pszResultLicence = CPLStrdup(
                     "GDAL/OGR is released under the MIT/X license.\n"
                     "The LICENSE.TXT distributed with GDAL/OGR should\n"
                     "contain additional details.\n" );
        }

        CPLSetTLS( CTLS_VERSIONINFO_LICENCE, pszResultLicence, TRUE );
        return pszResultLicence;
    }

/* -------------------------------------------------------------------- */
/*      All other strings are fairly small.                             */
/* -------------------------------------------------------------------- */
    CPLString osVersionInfo;

    if( pszRequest == NULL || EQUAL(pszRequest,"VERSION_NUM") )
        osVersionInfo.Printf( "%d", GDAL_VERSION_NUM );
    else if( EQUAL(pszRequest,"RELEASE_DATE") )
        osVersionInfo.Printf( "%d", GDAL_RELEASE_DATE );
    else if( EQUAL(pszRequest,"RELEASE_NAME") )
        osVersionInfo.Printf( GDAL_RELEASE_NAME );
    else // --version
        osVersionInfo.Printf( "GDAL %s, released %d/%02d/%02d",
                              GDAL_RELEASE_NAME,
                              GDAL_RELEASE_DATE / 10000,
                              (GDAL_RELEASE_DATE % 10000) / 100,
                              GDAL_RELEASE_DATE % 100 );

    CPLFree(CPLGetTLS(CTLS_VERSIONINFO)); // clear old value.
    CPLSetTLS(CTLS_VERSIONINFO, CPLStrdup(osVersionInfo), TRUE );
    return static_cast<char *>( CPLGetTLS(CTLS_VERSIONINFO) );
}

/************************************************************************/
/*                         GDALCheckVersion()                           */
/************************************************************************/

/** Return TRUE if GDAL library version at runtime matches nVersionMajor.nVersionMinor.

    The purpose of this method is to ensure that calling code will run
    with the GDAL version it is compiled for. It is primarily intended
    for external plugins.

    @param nVersionMajor Major version to be tested against
    @param nVersionMinor Minor version to be tested against
    @param pszCallingComponentName If not NULL, in case of version mismatch, the method
                                   will issue a failure mentioning the name of
                                   the calling component.

    @return TRUE if GDAL library version at runtime matches
    nVersionMajor.nVersionMinor, FALSE otherwise.
  */
int CPL_STDCALL GDALCheckVersion( int nVersionMajor, int nVersionMinor,
                                  const char* pszCallingComponentName)
{
    if (nVersionMajor == GDAL_VERSION_MAJOR &&
        nVersionMinor == GDAL_VERSION_MINOR)
        return TRUE;

    if (pszCallingComponentName)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s was compiled against GDAL %d.%d, but "
                  "the current library version is %d.%d",
                  pszCallingComponentName, nVersionMajor, nVersionMinor,
                  GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR);
    }
    return FALSE;
}

/************************************************************************/
/*                            GDALDecToDMS()                            */
/************************************************************************/

/** Translate a decimal degrees value to a DMS string with hemisphere.
 */
const char * CPL_STDCALL GDALDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    return CPLDecToDMS( dfAngle, pszAxis, nPrecision );
}

/************************************************************************/
/*                         GDALPackedDMSToDec()                         */
/************************************************************************/

/**
 * \brief Convert a packed DMS value (DDDMMMSSS.SS) into decimal degrees.
 *
 * See CPLPackedDMSToDec().
 */

double CPL_STDCALL GDALPackedDMSToDec( double dfPacked )

{
    return CPLPackedDMSToDec( dfPacked );
}

/************************************************************************/
/*                         GDALDecToPackedDMS()                         */
/************************************************************************/

/**
 * \brief Convert decimal degrees into packed DMS value (DDDMMMSSS.SS).
 *
 * See CPLDecToPackedDMS().
 */

double CPL_STDCALL GDALDecToPackedDMS( double dfDec )

{
    return CPLDecToPackedDMS( dfDec );
}

/************************************************************************/
/*                       GDALGCPsToGeoTransform()                       */
/************************************************************************/

/**
 * \brief Generate Geotransform from GCPs.
 *
 * Given a set of GCPs perform first order fit as a geotransform.
 *
 * Due to imprecision in the calculations the fit algorithm will often
 * return non-zero rotational coefficients even if given perfectly non-rotated
 * inputs.  A special case has been implemented for corner corner coordinates
 * given in TL, TR, BR, BL order.  So when using this to get a geotransform
 * from 4 corner coordinates, pass them in this order.
 * 
 * Starting with GDAL 2.2.2, if bApproxOK = FALSE, the
 * GDAL_GCPS_TO_GEOTRANSFORM_APPROX_OK configuration option will be read. If
 * set to YES, then bApproxOK will be overriden with TRUE.
 * Starting with GDAL 2.2.2, when exact fit is asked, the
 * GDAL_GCPS_TO_GEOTRANSFORM_APPROX_THRESHOLD configuration option can be set to
 * give the maximum error threshold in pixel. The default is 0.25.
 *
 * @param nGCPCount the number of GCPs being passed in.
 * @param pasGCPs the list of GCP structures.
 * @param padfGeoTransform the six double array in which the affine
 * geotransformation will be returned.
 * @param bApproxOK If FALSE the function will fail if the geotransform is not
 * essentially an exact fit (within 0.25 pixel) for all GCPs.
 *
 * @return TRUE on success or FALSE if there aren't enough points to prepare a
 * geotransform, the pointers are ill-determined or if bApproxOK is FALSE
 * and the fit is poor.
 */

// TODO(schwehr): Add consts to args.
int CPL_STDCALL
GDALGCPsToGeoTransform( int nGCPCount, const GDAL_GCP *pasGCPs,
                        double *padfGeoTransform, int bApproxOK )

{
    double dfPixelThreshold = 0.25;
    if( !bApproxOK )
    {
        bApproxOK =
            CPLTestBool(CPLGetConfigOption(
                            "GDAL_GCPS_TO_GEOTRANSFORM_APPROX_OK", "NO"));
        if( !bApproxOK )
        {
            dfPixelThreshold =
                CPLAtof(CPLGetConfigOption(
                    "GDAL_GCPS_TO_GEOTRANSFORM_APPROX_THRESHOLD", "0.25"));
        }
    }

/* -------------------------------------------------------------------- */
/*      Recognise a few special cases.                                  */
/* -------------------------------------------------------------------- */
    if( nGCPCount < 2 )
        return FALSE;

    if( nGCPCount == 2 )
    {
        if( pasGCPs[1].dfGCPPixel == pasGCPs[0].dfGCPPixel
            || pasGCPs[1].dfGCPLine == pasGCPs[0].dfGCPLine )
            return FALSE;

        padfGeoTransform[1] = (pasGCPs[1].dfGCPX - pasGCPs[0].dfGCPX)
            / (pasGCPs[1].dfGCPPixel - pasGCPs[0].dfGCPPixel);
        padfGeoTransform[2] = 0.0;

        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = (pasGCPs[1].dfGCPY - pasGCPs[0].dfGCPY)
            / (pasGCPs[1].dfGCPLine - pasGCPs[0].dfGCPLine);

        padfGeoTransform[0] = pasGCPs[0].dfGCPX
            - pasGCPs[0].dfGCPPixel * padfGeoTransform[1]
            - pasGCPs[0].dfGCPLine * padfGeoTransform[2];

        padfGeoTransform[3] = pasGCPs[0].dfGCPY
            - pasGCPs[0].dfGCPPixel * padfGeoTransform[4]
            - pasGCPs[0].dfGCPLine * padfGeoTransform[5];

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Special case of 4 corner coordinates of a non-rotated           */
/*      image.  The points must be in TL-TR-BR-BL order for now.        */
/*      This case helps avoid some imprecision in the general           */
/*      calculations.                                                   */
/* -------------------------------------------------------------------- */
    if( nGCPCount == 4
        && pasGCPs[0].dfGCPLine == pasGCPs[1].dfGCPLine
        && pasGCPs[2].dfGCPLine == pasGCPs[3].dfGCPLine
        && pasGCPs[0].dfGCPPixel == pasGCPs[3].dfGCPPixel
        && pasGCPs[1].dfGCPPixel == pasGCPs[2].dfGCPPixel
        && pasGCPs[0].dfGCPLine != pasGCPs[2].dfGCPLine
        && pasGCPs[0].dfGCPPixel != pasGCPs[1].dfGCPPixel
        && pasGCPs[0].dfGCPY == pasGCPs[1].dfGCPY
        && pasGCPs[2].dfGCPY == pasGCPs[3].dfGCPY
        && pasGCPs[0].dfGCPX == pasGCPs[3].dfGCPX
        && pasGCPs[1].dfGCPX == pasGCPs[2].dfGCPX
        && pasGCPs[0].dfGCPY != pasGCPs[2].dfGCPY
        && pasGCPs[0].dfGCPX != pasGCPs[1].dfGCPX )
    {
        padfGeoTransform[1] = (pasGCPs[1].dfGCPX - pasGCPs[0].dfGCPX)
            / (pasGCPs[1].dfGCPPixel - pasGCPs[0].dfGCPPixel);
        padfGeoTransform[2] = 0.0;
        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = (pasGCPs[2].dfGCPY - pasGCPs[1].dfGCPY)
            / (pasGCPs[2].dfGCPLine - pasGCPs[1].dfGCPLine);

        padfGeoTransform[0] =
            pasGCPs[0].dfGCPX - pasGCPs[0].dfGCPPixel * padfGeoTransform[1];
        padfGeoTransform[3] =
            pasGCPs[0].dfGCPY - pasGCPs[0].dfGCPLine * padfGeoTransform[5];
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Compute source and destination ranges so we can normalize       */
/*      the values to make the least squares computation more stable.   */
/* -------------------------------------------------------------------- */
    double min_pixel = pasGCPs[0].dfGCPPixel;
    double max_pixel = pasGCPs[0].dfGCPPixel;
    double min_line = pasGCPs[0].dfGCPLine;
    double max_line = pasGCPs[0].dfGCPLine;
    double min_geox = pasGCPs[0].dfGCPX;
    double max_geox = pasGCPs[0].dfGCPX;
    double min_geoy = pasGCPs[0].dfGCPY;
    double max_geoy = pasGCPs[0].dfGCPY;

    for( int i = 1; i < nGCPCount; ++i ) {
        min_pixel = std::min(min_pixel, pasGCPs[i].dfGCPPixel);
        max_pixel = std::max(max_pixel, pasGCPs[i].dfGCPPixel);
        min_line = std::min(min_line, pasGCPs[i].dfGCPLine);
        max_line = std::max(max_line, pasGCPs[i].dfGCPLine);
        min_geox = std::min(min_geox, pasGCPs[i].dfGCPX);
        max_geox = std::max(max_geox, pasGCPs[i].dfGCPX);
        min_geoy = std::min(min_geoy, pasGCPs[i].dfGCPY);
        max_geoy = std::max(max_geoy, pasGCPs[i].dfGCPY);
    }

    double EPS = 1.0e-12;

    if( std::abs(max_pixel - min_pixel) < EPS
        || std::abs(max_line - min_line) < EPS
        || std::abs(max_geox - min_geox) < EPS
        || std::abs(max_geoy - min_geoy) < EPS)
    {
        return FALSE;  // degenerate in at least one dimension.
    }

    double pl_normalize[6], geo_normalize[6];

    pl_normalize[0] = -min_pixel / (max_pixel - min_pixel);
    pl_normalize[1] = 1.0 / (max_pixel - min_pixel);
    pl_normalize[2] = 0.0;
    pl_normalize[3] = -min_line / (max_line - min_line);
    pl_normalize[4] = 0.0;
    pl_normalize[5] = 1.0 / (max_line - min_line);

    geo_normalize[0] = -min_geox / (max_geox - min_geox);
    geo_normalize[1] = 1.0 / (max_geox - min_geox);
    geo_normalize[2] = 0.0;
    geo_normalize[3] = -min_geoy / (max_geoy - min_geoy);
    geo_normalize[4] = 0.0;
    geo_normalize[5] = 1.0 / (max_geoy - min_geoy);

/* -------------------------------------------------------------------- */
/* In the general case, do a least squares error approximation by       */
/* solving the equation Sum[(A - B*x + C*y - Lon)^2] = minimum          */
/* -------------------------------------------------------------------- */

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_xx = 0.0;
    double sum_yy = 0.0;
    double sum_Lon = 0.0;
    double sum_Lonx = 0.0;
    double sum_Lony = 0.0;
    double sum_Lat = 0.0;
    double sum_Latx = 0.0;
    double sum_Laty = 0.0;

    for ( int i = 0; i < nGCPCount; ++i ) {
        double pixel, line, geox, geoy;

        GDALApplyGeoTransform(pl_normalize,
                              pasGCPs[i].dfGCPPixel,
                              pasGCPs[i].dfGCPLine,
                              &pixel, &line);
        GDALApplyGeoTransform(geo_normalize,
                              pasGCPs[i].dfGCPX,
                              pasGCPs[i].dfGCPY,
                              &geox, &geoy);

        sum_x += pixel;
        sum_y += line;
        sum_xy += pixel * line;
        sum_xx += pixel * pixel;
        sum_yy += line * line;
        sum_Lon += geox;
        sum_Lonx += geox * pixel;
        sum_Lony += geox * line;
        sum_Lat += geoy;
        sum_Latx += geoy * pixel;
        sum_Laty += geoy * line;
    }

    const double divisor =
        nGCPCount * (sum_xx * sum_yy - sum_xy * sum_xy)
        + 2 * sum_x * sum_y * sum_xy - sum_y * sum_y * sum_xx
        - sum_x * sum_x * sum_yy;

/* -------------------------------------------------------------------- */
/*      If the divisor is zero, there is no valid solution.             */
/* -------------------------------------------------------------------- */
    if (divisor == 0.0)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compute top/left origin.                                        */
/* -------------------------------------------------------------------- */
  double gt_normalized[6] = { 0.0 };
    gt_normalized[0] = (sum_Lon * (sum_xx * sum_yy - sum_xy * sum_xy)
                  + sum_Lonx * (sum_y * sum_xy - sum_x *  sum_yy)
                  + sum_Lony * (sum_x * sum_xy - sum_y * sum_xx))
        / divisor;

    gt_normalized[3] = (sum_Lat * (sum_xx * sum_yy - sum_xy * sum_xy)
                  + sum_Latx * (sum_y * sum_xy - sum_x *  sum_yy)
                  + sum_Laty * (sum_x * sum_xy - sum_y * sum_xx))
        / divisor;

/* -------------------------------------------------------------------- */
/*      Compute X related coefficients.                                 */
/* -------------------------------------------------------------------- */
    gt_normalized[1] = (sum_Lon * (sum_y * sum_xy - sum_x * sum_yy)
                  + sum_Lonx * (nGCPCount * sum_yy - sum_y * sum_y)
                  + sum_Lony * (sum_x * sum_y - sum_xy * nGCPCount))
        / divisor;

    gt_normalized[2] = (sum_Lon * (sum_x * sum_xy - sum_y * sum_xx)
                  + sum_Lonx * (sum_x * sum_y - nGCPCount * sum_xy)
                  + sum_Lony * (nGCPCount * sum_xx - sum_x * sum_x))
        / divisor;

/* -------------------------------------------------------------------- */
/*      Compute Y related coefficients.                                 */
/* -------------------------------------------------------------------- */
    gt_normalized[4] = (sum_Lat * (sum_y * sum_xy - sum_x * sum_yy)
                  + sum_Latx * (nGCPCount * sum_yy - sum_y * sum_y)
                  + sum_Laty * (sum_x * sum_y - sum_xy * nGCPCount))
        / divisor;

    gt_normalized[5] = (sum_Lat * (sum_x * sum_xy - sum_y * sum_xx)
                  + sum_Latx * (sum_x * sum_y - nGCPCount * sum_xy)
                  + sum_Laty * (nGCPCount * sum_xx - sum_x * sum_x))
        / divisor;

/* -------------------------------------------------------------------- */
/*      Compose the resulting transformation with the normalization     */
/*      geotransformations.                                             */
/* -------------------------------------------------------------------- */
    double gt1p2[6] = { 0.0 };
    double inv_geo_normalize[6] = { 0.0 };
    if( !GDALInvGeoTransform(geo_normalize, inv_geo_normalize))
        return FALSE;

    GDALComposeGeoTransforms(pl_normalize, gt_normalized, gt1p2);
    GDALComposeGeoTransforms(gt1p2, inv_geo_normalize, padfGeoTransform);

/* -------------------------------------------------------------------- */
/*      Now check if any of the input points fit this poorly.           */
/* -------------------------------------------------------------------- */
    if( !bApproxOK )
    {
        // FIXME? Not sure if it is the more accurate way of computing
        // pixel size
        double dfPixelSize =
            0.5 * (std::abs(padfGeoTransform[1])
            + std::abs(padfGeoTransform[2])
            + std::abs(padfGeoTransform[4])
            + std::abs(padfGeoTransform[5]));
        if( dfPixelSize == 0.0 )
        {
            CPLDebug("GDAL", "dfPixelSize = 0");
            return FALSE;
        }

        for( int i = 0; i < nGCPCount; i++ )
        {
            const double dfErrorX =
                (pasGCPs[i].dfGCPPixel * padfGeoTransform[1]
                 + pasGCPs[i].dfGCPLine * padfGeoTransform[2]
                 + padfGeoTransform[0])
                - pasGCPs[i].dfGCPX;
            const double dfErrorY =
                (pasGCPs[i].dfGCPPixel * padfGeoTransform[4]
                 + pasGCPs[i].dfGCPLine * padfGeoTransform[5]
                 + padfGeoTransform[3])
                - pasGCPs[i].dfGCPY;

            if( std::abs(dfErrorX) > dfPixelThreshold * dfPixelSize
                || std::abs(dfErrorY) > dfPixelThreshold * dfPixelSize )
            {
                CPLDebug("GDAL", "dfErrorX/dfPixelSize = %.2f, "
                         "dfErrorY/dfPixelSize = %.2f",
                         std::abs(dfErrorX)/dfPixelSize,
                         std::abs(dfErrorY)/dfPixelSize);
                return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                      GDALComposeGeoTransforms()                      */
/************************************************************************/

/**
 * \brief Compose two geotransforms.
 *
 * The resulting geotransform is the equivalent to padfGT1 and then padfGT2
 * being applied to a point.
 *
 * @param padfGT1 the first geotransform, six values.
 * @param padfGT2 the second geotransform, six values.
 * @param padfGTOut the output geotransform, six values, may safely be the same
 * array as padfGT1 or padfGT2.
 */

void GDALComposeGeoTransforms(const double *padfGT1, const double *padfGT2,
                              double *padfGTOut)

{
    double gtwrk[6] = { 0.0 };
    // We need to think of the geotransform in a more normal form to do
    // the matrix multiple:
    //
    //  __                     __
    //  | gt[1]   gt[2]   gt[0] |
    //  | gt[4]   gt[5]   gt[3] |
    //  |  0.0     0.0     1.0  |
    //  --                     --
    //
    // Then we can use normal matrix multiplication to produce the
    // composed transformation.  I don't actually reform the matrix
    // explicitly which is why the following may seem kind of spagettish.

    gtwrk[1] =
        padfGT2[1] * padfGT1[1]
        + padfGT2[2] * padfGT1[4];
    gtwrk[2] =
        padfGT2[1] * padfGT1[2]
        + padfGT2[2] * padfGT1[5];
    gtwrk[0] =
        padfGT2[1] * padfGT1[0]
        + padfGT2[2] * padfGT1[3]
        + padfGT2[0] * 1.0;

    gtwrk[4] =
        padfGT2[4] * padfGT1[1]
        + padfGT2[5] * padfGT1[4];
    gtwrk[5] =
        padfGT2[4] * padfGT1[2]
        + padfGT2[5] * padfGT1[5];
    gtwrk[3] =
        padfGT2[4] * padfGT1[0]
        + padfGT2[5] * padfGT1[3]
        + padfGT2[3] * 1.0;
    memcpy(padfGTOut, gtwrk, sizeof(gtwrk));
}

/************************************************************************/
/*                    GDALGeneralCmdLineProcessor()                     */
/************************************************************************/

/**
 * \brief General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline
 * options for all GDAL commandline utilities.  It takes care of the following
 * commandline options:
 *
 *  --version: report version of GDAL in use.
 *  --build: report build info about GDAL in use.
 *  --license: report GDAL license info.
 *  --formats: report all format drivers configured.
 *  --format [format]: report details of one format driver.
 *  --optfile filename: expand an option file into the argument list.
 *  --config key value: set system configuration option.
 *  --debug [on/off/value]: set debug level.
 *  --mempreload dir: preload directory contents into /vsimem
 *  --pause: Pause for user input (allows time to attach debugger)
 *  --locale [locale]: Install a locale using setlocale() (debugging)
 *  --help-general: report detailed help on general options.
 *
 * The argument array is replaced "in place" and should be freed with
 * CSLDestroy() when no longer needed.  The typical usage looks something
 * like the following.  Note that the formats should be registered so that
 * the --formats and --format options will work properly.
 *
 *  int main( int argc, char ** argv )
 *  {
 *    GDALAllRegister();
 *
 *    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
 *    if( argc < 1 )
 *        exit( -argc );
 *
 * @param nArgc number of values in the argument list.
 * @param ppapszArgv pointer to the argument list array (will be updated in place).
 * @param nOptions a or-able combination of GDAL_OF_RASTER and GDAL_OF_VECTOR
 *                 to determine which drivers should be displayed by --formats.
 *                 If set to 0, GDAL_OF_RASTER is assumed.
 *
 * @return updated nArgc argument count.  Return of 0 requests terminate
 * without error, return of -1 requests exit with error code.
 */

int CPL_STDCALL
GDALGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions )

{
    char **papszReturn = NULL;
    int  iArg;
    char **papszArgv = *ppapszArgv;

/* -------------------------------------------------------------------- */
/*      Preserve the program name.                                      */
/* -------------------------------------------------------------------- */
    papszReturn = CSLAddString( papszReturn, papszArgv[0] );

/* ==================================================================== */
/*      Loop over all arguments.                                        */
/* ==================================================================== */
    for( iArg = 1; iArg < nArgc; iArg++ )
    {
/* -------------------------------------------------------------------- */
/*      --version                                                       */
/* -------------------------------------------------------------------- */
        if( EQUAL(papszArgv[iArg],"--version") )
        {
            printf( "%s\n", GDALVersionInfo( "--version" ) );/*ok*/
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --build                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--build") )
        {
            printf( "%s", GDALVersionInfo( "BUILD_INFO" ) );/*ok*/
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --license                                                       */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--license") )
        {
            printf( "%s\n", GDALVersionInfo( "LICENSE" ) );/*ok*/
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --config                                                        */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--config") )
        {
            if( iArg + 2 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--config option given without a key and value argument." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( papszArgv[iArg+1], papszArgv[iArg+2] );

            iArg += 2;
        }

/* -------------------------------------------------------------------- */
/*      --mempreload                                                    */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--mempreload") )
        {
            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--mempreload option given without directory path.");
                CSLDestroy( papszReturn );
                return -1;
            }

            char **papszFiles = VSIReadDir( papszArgv[iArg+1] );
            if( CSLCount(papszFiles) == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--mempreload given invalid or empty directory.");
                CSLDestroy( papszReturn );
                return -1;
            }

            for( int i = 0; papszFiles[i] != NULL; i++ )
            {
                if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
                    continue;

                CPLString osOldPath, osNewPath;
                osOldPath = CPLFormFilename( papszArgv[iArg+1],
                                             papszFiles[i], NULL );
                osNewPath.Printf( "/vsimem/%s", papszFiles[i] );

                VSIStatBufL sStatBuf;
                if( VSIStatL( osOldPath, &sStatBuf ) != 0
                    || VSI_ISDIR( sStatBuf.st_mode ) )
                {
                    CPLDebug( "VSI", "Skipping preload of %s.",
                              osOldPath.c_str() );
                    continue;
                }

                CPLDebug( "VSI", "Preloading %s to %s.",
                          osOldPath.c_str(), osNewPath.c_str() );

                if( CPLCopyFile( osNewPath, osOldPath ) != 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to copy %s to /vsimem",
                              osOldPath.c_str() );
                    CSLDestroy( papszReturn );
                    return -1;
                }
            }

            CSLDestroy( papszFiles );
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --debug                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--debug") )
        {
            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--debug option given without debug level." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( "CPL_DEBUG", papszArgv[iArg+1] );
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --optfile                                                       */
/*                                                                      */
/*      Annoyingly the options inserted by --optfile will *not* be      */
/*      processed properly if they are general options.                 */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--optfile") )
        {
            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--optfile option given without filename." );
                CSLDestroy( papszReturn );
                return -1;
            }

            VSILFILE *fpOptFile = VSIFOpenL( papszArgv[iArg+1], "rb" );

            if( fpOptFile == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to open optfile '%s'.\n%s",
                          papszArgv[iArg+1], VSIStrerror( errno ) );
                CSLDestroy( papszReturn );
                return -1;
            }

            const char *pszLine;
            while( (pszLine = CPLReadLineL( fpOptFile )) != NULL )
            {
                if( pszLine[0] == '#' || strlen(pszLine) == 0 )
                    continue;

                char **papszTokens = CSLTokenizeString( pszLine );
                for( int i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++)
                    papszReturn = CSLAddString( papszReturn, papszTokens[i] );
                CSLDestroy( papszTokens );
            }

            VSIFCloseL( fpOptFile );

            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --formats                                                       */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg], "--formats") )
        {
            if( nOptions == 0 )
                nOptions = GDAL_OF_RASTER;

            printf( "Supported Formats:\n" );/*ok*/
            for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                GDALDriverH hDriver = GDALGetDriver(iDr);

                const char *pszRFlag = "", *pszWFlag, *pszVirtualIO, *pszSubdatasets, *pszKind;
                char** papszMD = GDALGetMetadata( hDriver, NULL );

                if( nOptions == GDAL_OF_RASTER &&
                    !CPLFetchBool( papszMD, GDAL_DCAP_RASTER, false ) )
                    continue;
                if( nOptions == GDAL_OF_VECTOR &&
                    !CPLFetchBool( papszMD, GDAL_DCAP_VECTOR, false ) )
                    continue;
                if( nOptions == GDAL_OF_GNM &&
                    !CPLFetchBool( papszMD, GDAL_DCAP_GNM, false ) )
                    continue;

                if( CPLFetchBool( papszMD, GDAL_DCAP_OPEN, false ) )
                    pszRFlag = "r";

                if( CPLFetchBool( papszMD, GDAL_DCAP_CREATE, false ) )
                    pszWFlag = "w+";
                else if( CPLFetchBool( papszMD, GDAL_DCAP_CREATECOPY, false ) )
                    pszWFlag = "w";
                else
                    pszWFlag = "o";

                if( CPLFetchBool( papszMD, GDAL_DCAP_VIRTUALIO, false ) )
                    pszVirtualIO = "v";
                else
                    pszVirtualIO = "";

                if( CPLFetchBool( papszMD, GDAL_DMD_SUBDATASETS, false ) )
                    pszSubdatasets = "s";
                else
                    pszSubdatasets = "";

                if( CPLFetchBool( papszMD, GDAL_DCAP_RASTER, false ) &&
                    CPLFetchBool( papszMD, GDAL_DCAP_VECTOR, false ))
                    pszKind = "raster,vector";
                else if( CPLFetchBool( papszMD, GDAL_DCAP_RASTER, false ) )
                    pszKind = "raster";
                else if( CPLFetchBool( papszMD, GDAL_DCAP_VECTOR, false ) )
                    pszKind = "vector";
                else if( CPLFetchBool( papszMD, GDAL_DCAP_GNM, false ) )
                    pszKind = "geography network";
                else
                    pszKind = "unknown kind";

                printf( "  %s -%s- (%s%s%s%s): %s\n",/*ok*/
                        GDALGetDriverShortName( hDriver ),
                        pszKind,
                        pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                        GDALGetDriverLongName( hDriver ) );
            }

            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --format                                                        */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg], "--format") )
        {
            GDALDriverH hDriver;
            char **papszMD;

            CSLDestroy( papszReturn );

            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--format option given without a format code." );
                return -1;
            }

            hDriver = GDALGetDriverByName( papszArgv[iArg+1] );
            if( hDriver == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "--format option given with format '%s', but that "
                          "format not\nrecognised.  Use the --formats option "
                          "to get a list of available formats,\n"
                          "and use the short code (i.e. GTiff or HFA) as the "
                          "format identifier.\n",
                          papszArgv[iArg+1] );
                return -1;
            }

            printf( "Format Details:\n" );/*ok*/
            printf( "  Short Name: %s\n", GDALGetDriverShortName( hDriver ) );/*ok*/
            printf( "  Long Name: %s\n", GDALGetDriverLongName( hDriver ) );/*ok*/

            papszMD = GDALGetMetadata( hDriver, NULL );
            if( CPLFetchBool( papszMD, GDAL_DCAP_RASTER, false ) )
                printf( "  Supports: Raster\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_VECTOR, false ) )
                printf( "  Supports: Vector\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_GNM, false ) )
                printf( "  Supports: Geography Network\n" );/*ok*/

            const char* pszExt = CSLFetchNameValue( papszMD, GDAL_DMD_EXTENSIONS );
            if( pszExt != NULL )
                printf( "  Extension%s: %s\n", (strchr(pszExt, ' ') ? "s" : ""),/*ok*/
                        pszExt );

            if( CSLFetchNameValue( papszMD, GDAL_DMD_MIMETYPE ) )
                printf( "  Mime Type: %s\n",/*ok*/
                        CSLFetchNameValue( papszMD, GDAL_DMD_MIMETYPE ) );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_HELPTOPIC ) )
                printf( "  Help Topic: %s\n",/*ok*/
                        CSLFetchNameValue( papszMD, GDAL_DMD_HELPTOPIC ) );

            if( CPLFetchBool( papszMD, GDAL_DMD_SUBDATASETS, false ) )
                printf( "  Supports: Subdatasets\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_OPEN, false ) )
                printf( "  Supports: Open() - Open existing dataset.\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_CREATE, false ) )
                printf( "  Supports: Create() - Create writable dataset.\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_CREATECOPY, false ) )
                printf( "  Supports: CreateCopy() - Create dataset by copying another.\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_VIRTUALIO, false ) )
                printf( "  Supports: Virtual IO - eg. /vsimem/\n" );/*ok*/
            if( CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONDATATYPES ) )
                printf( "  Creation Datatypes: %s\n",/*ok*/
                        CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONDATATYPES ) );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONFIELDDATATYPES ) )
                printf( "  Creation Field Datatypes: %s\n",/*ok*/
                        CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONFIELDDATATYPES ) );
            if( CPLFetchBool( papszMD, GDAL_DCAP_NOTNULL_FIELDS, false ) )
                printf( "  Supports: Creating fields with NOT NULL constraint.\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_DEFAULT_FIELDS, false ) )
                printf( "  Supports: Creating fields with DEFAULT values.\n" );/*ok*/
            if( CPLFetchBool( papszMD, GDAL_DCAP_NOTNULL_GEOMFIELDS, false ) )
                printf( "  Supports: Creating geometry fields with NOT NULL constraint.\n" );/*ok*/
            if( CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONOPTIONLIST ) )
            {
                CPLXMLNode *psCOL =
                    CPLParseXMLString(
                        CSLFetchNameValue( papszMD,
                                           GDAL_DMD_CREATIONOPTIONLIST ) );
                char *pszFormattedXML =
                    CPLSerializeXMLTree( psCOL );

                CPLDestroyXMLNode( psCOL );

                printf( "\n%s\n", pszFormattedXML );/*ok*/
                CPLFree( pszFormattedXML );
            }
            if( CSLFetchNameValue( papszMD, GDAL_DS_LAYER_CREATIONOPTIONLIST ) )
            {
                CPLXMLNode *psCOL =
                    CPLParseXMLString(
                        CSLFetchNameValue( papszMD,
                                           GDAL_DS_LAYER_CREATIONOPTIONLIST ) );
                char *pszFormattedXML =
                    CPLSerializeXMLTree( psCOL );

                CPLDestroyXMLNode( psCOL );

                printf( "\n%s\n", pszFormattedXML );/*ok*/
                CPLFree( pszFormattedXML );
            }

            if( CSLFetchNameValue( papszMD, GDAL_DMD_CONNECTION_PREFIX ) )
                printf( "  Connection prefix: %s\n",/*ok*/
                        CSLFetchNameValue( papszMD, GDAL_DMD_CONNECTION_PREFIX ) );

            if( CSLFetchNameValue( papszMD, GDAL_DMD_OPENOPTIONLIST ) )
            {
                CPLXMLNode *psCOL =
                    CPLParseXMLString(
                        CSLFetchNameValue( papszMD,
                                           GDAL_DMD_OPENOPTIONLIST ) );
                char *pszFormattedXML =
                    CPLSerializeXMLTree( psCOL );

                CPLDestroyXMLNode( psCOL );

                printf( "%s\n", pszFormattedXML );/*ok*/
                CPLFree( pszFormattedXML );
            }

            bool bFirstOtherOption = true;
            for( char** papszIter = papszMD;
                 papszIter && *papszIter; ++papszIter )
            {
                if( !STARTS_WITH(*papszIter, "DCAP_") &&
                    !STARTS_WITH(*papszIter, "DMD_") &&
                    !STARTS_WITH(*papszIter, "DS_") )
                {
                    if( bFirstOtherOption )
                        printf("  Other metadata items:\n"); /*ok*/
                    bFirstOtherOption = false;
                    printf("    %s\n", *papszIter); /*ok*/
                }
            }

            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --help-general                                                  */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--help-general") )
        {
            printf( "Generic GDAL utility command options:\n" );/*ok*/
            printf( "  --version: report version of GDAL in use.\n" );/*ok*/
            printf( "  --license: report GDAL license info.\n" );/*ok*/
            printf( "  --formats: report all configured format drivers.\n" );/*ok*/
            printf( "  --format [format]: details of one format.\n" );/*ok*/
            printf( "  --optfile filename: expand an option file into the argument list.\n" );/*ok*/
            printf( "  --config key value: set system configuration option.\n" );/*ok*/
            printf( "  --debug [on/off/value]: set debug level.\n" );/*ok*/
            printf( "  --pause: wait for user input, time to attach debugger\n" );/*ok*/
            printf( "  --locale [locale]: install locale for debugging "/*ok*/
                    "(i.e. en_US.UTF-8)\n" );
            printf( "  --help-general: report detailed help on general options.\n" );/*ok*/
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --locale                                                        */
/* -------------------------------------------------------------------- */
        else if( iArg < nArgc-1 && EQUAL(papszArgv[iArg],"--locale") )
        {
            CPLsetlocale( LC_ALL, papszArgv[++iArg] );
        }

/* -------------------------------------------------------------------- */
/*      --pause                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--pause") )
        {
            std::cout << "Hit <ENTER> to Continue." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

/* -------------------------------------------------------------------- */
/*      Carry through unrecognized options.                             */
/* -------------------------------------------------------------------- */
        else
        {
            papszReturn = CSLAddString( papszReturn, papszArgv[iArg] );
        }
    }

    *ppapszArgv = papszReturn;

    return CSLCount( *ppapszArgv );
}

/************************************************************************/
/*                          _FetchDblFromMD()                           */
/************************************************************************/

static bool _FetchDblFromMD( char **papszMD, const char *pszKey,
                             double *padfTarget, int nCount, double dfDefault )

{
    char szFullKey[200];

    snprintf( szFullKey, sizeof(szFullKey), "%s", pszKey );

    const char *pszValue = CSLFetchNameValue( papszMD, szFullKey );

    for( int i = 0; i < nCount; i++ )
        padfTarget[i] = dfDefault;

    if( pszValue == NULL )
        return false;

    if( nCount == 1 )
    {
        *padfTarget = CPLAtofM( pszValue );
        return true;
    }

    char **papszTokens = CSLTokenizeStringComplex( pszValue, " ,",
                                                   FALSE, FALSE );

    if( CSLCount( papszTokens ) != nCount )
    {
        CSLDestroy( papszTokens );
        return false;
    }

    for( int i = 0; i < nCount; i++ )
        padfTarget[i] = CPLAtofM(papszTokens[i]);

    CSLDestroy( papszTokens );

    return true;
}

/************************************************************************/
/*                         GDALExtractRPCInfo()                         */
/************************************************************************/

/** Extract RPC info from metadata, and apply to an RPCInfo structure.
 *
 * The inverse of this function is RPCInfoToMD() in alg/gdal_rpc.cpp
 *
 * @param papszMD Dictionary of metadata representing RPC
 * @param psRPC (output) Pointer to structure to hold the RPC values.
 * @return TRUE in case of success. FALSE in case of failure.
 */
int CPL_STDCALL GDALExtractRPCInfo( char **papszMD, GDALRPCInfo *psRPC )

{
    if( CSLFetchNameValue( papszMD, RPC_LINE_NUM_COEFF ) == NULL )
        return FALSE;

    if( CSLFetchNameValue( papszMD, RPC_LINE_NUM_COEFF ) == NULL
        || CSLFetchNameValue( papszMD, RPC_LINE_DEN_COEFF ) == NULL
        || CSLFetchNameValue( papszMD, RPC_SAMP_NUM_COEFF ) == NULL
        || CSLFetchNameValue( papszMD, RPC_SAMP_DEN_COEFF ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Some required RPC metadata missing in GDALExtractRPCInfo()");
        return FALSE;
    }

    _FetchDblFromMD( papszMD, RPC_LINE_OFF, &(psRPC->dfLINE_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, RPC_LINE_SCALE, &(psRPC->dfLINE_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, RPC_SAMP_OFF, &(psRPC->dfSAMP_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, RPC_SAMP_SCALE, &(psRPC->dfSAMP_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, RPC_HEIGHT_OFF, &(psRPC->dfHEIGHT_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, RPC_HEIGHT_SCALE, &(psRPC->dfHEIGHT_SCALE),1, 1.0);
    _FetchDblFromMD( papszMD, RPC_LAT_OFF, &(psRPC->dfLAT_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, RPC_LAT_SCALE, &(psRPC->dfLAT_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, RPC_LONG_OFF, &(psRPC->dfLONG_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, RPC_LONG_SCALE, &(psRPC->dfLONG_SCALE), 1, 1.0 );

    _FetchDblFromMD( papszMD, RPC_LINE_NUM_COEFF, psRPC->adfLINE_NUM_COEFF,
                     20, 0.0 );
    _FetchDblFromMD( papszMD, RPC_LINE_DEN_COEFF, psRPC->adfLINE_DEN_COEFF,
                     20, 0.0 );
    _FetchDblFromMD( papszMD, RPC_SAMP_NUM_COEFF, psRPC->adfSAMP_NUM_COEFF,
                     20, 0.0 );
    _FetchDblFromMD( papszMD, RPC_SAMP_DEN_COEFF, psRPC->adfSAMP_DEN_COEFF,
                     20, 0.0 );

    _FetchDblFromMD( papszMD, "MIN_LONG", &(psRPC->dfMIN_LONG), 1, -180.0 );
    _FetchDblFromMD( papszMD, "MIN_LAT", &(psRPC->dfMIN_LAT), 1, -90.0 );
    _FetchDblFromMD( papszMD, "MAX_LONG", &(psRPC->dfMAX_LONG), 1, 180.0 );
    _FetchDblFromMD( papszMD, "MAX_LAT", &(psRPC->dfMAX_LAT), 1, 90.0 );

    return TRUE;
}

/************************************************************************/
/*                     GDALFindAssociatedAuxFile()                      */
/************************************************************************/

GDALDataset *GDALFindAssociatedAuxFile( const char *pszBasename,
                                        GDALAccess eAccess,
                                        GDALDataset *poDependentDS )

{
    const char *pszAuxSuffixLC = "aux";
    const char *pszAuxSuffixUC = "AUX";

    if( EQUAL(CPLGetExtension(pszBasename), pszAuxSuffixLC) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Don't even try to look for an .aux file if we don't have a      */
/*      path of any kind.                                               */
/* -------------------------------------------------------------------- */
    if( strlen(pszBasename) == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We didn't find that, so try and find a corresponding aux        */
/*      file.  Check that we are the dependent file of the aux          */
/*      file, or if we aren't verify that the dependent file does       */
/*      not exist, likely mean it is us but some sort of renaming       */
/*      has occurred.                                                   */
/* -------------------------------------------------------------------- */
    CPLString osJustFile = CPLGetFilename(pszBasename); // without dir
    CPLString osAuxFilename = CPLResetExtension(pszBasename, pszAuxSuffixLC);
    GDALDataset *poODS = NULL;
    GByte abyHeader[32];

    VSILFILE *fp = VSIFOpenL( osAuxFilename, "rb" );

    if ( fp == NULL && VSIIsCaseSensitiveFS(osAuxFilename))
    {
        // Can't found file with lower case suffix. Try the upper case one.
        osAuxFilename = CPLResetExtension(pszBasename, pszAuxSuffixUC);
        fp = VSIFOpenL( osAuxFilename, "rb" );
    }

    if( fp != NULL )
    {
        if( VSIFReadL( abyHeader, 1, 32, fp ) == 32 &&
            STARTS_WITH_CI((char *) abyHeader, "EHFA_HEADER_TAG") )
        {
            /* Avoid causing failure in opening of main file from SWIG bindings */
            /* when auxiliary file cannot be opened (#3269) */
            CPLTurnFailureIntoWarning(TRUE);
            if( poDependentDS != NULL && poDependentDS->GetShared() )
                poODS = (GDALDataset *) GDALOpenShared( osAuxFilename, eAccess );
            else
                poODS = (GDALDataset *) GDALOpen( osAuxFilename, eAccess );
            CPLTurnFailureIntoWarning(FALSE);
        }
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
    }

/* -------------------------------------------------------------------- */
/*      Try replacing extension with .aux                               */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        const char *pszDep
            = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
        if( pszDep == NULL  )
        {
            CPLDebug( "AUX",
                      "Found %s but it has no dependent file, ignoring.",
                      osAuxFilename.c_str() );
            GDALClose( poODS );
            poODS = NULL;
        }
        else if( !EQUAL(pszDep,osJustFile) )
        {
            VSIStatBufL sStatBuf;

            if( VSIStatExL( pszDep, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
            {
                CPLDebug( "AUX", "%s is for file %s, not %s, ignoring.",
                          osAuxFilename.c_str(),
                          pszDep, osJustFile.c_str() );
                GDALClose( poODS );
                poODS = NULL;
            }
            else
            {
                CPLDebug( "AUX", "%s is for file %s, not %s, but since\n"
                          "%s does not exist, we will use .aux file as our own.",
                          osAuxFilename.c_str(),
                          pszDep, osJustFile.c_str(),
                          pszDep );
            }
        }

/* -------------------------------------------------------------------- */
/*      Confirm that the aux file matches the configuration of the      */
/*      dependent dataset.                                              */
/* -------------------------------------------------------------------- */
        if( poODS != NULL && poDependentDS != NULL
            && (poODS->GetRasterCount() != poDependentDS->GetRasterCount()
                || poODS->GetRasterXSize() != poDependentDS->GetRasterXSize()
                || poODS->GetRasterYSize() != poDependentDS->GetRasterYSize()) )
        {
            CPLDebug( "AUX",
                      "Ignoring aux file %s as its raster configuration\n"
                      "(%dP x %dL x %dB) does not match master file (%dP x %dL x %dB)",
                      osAuxFilename.c_str(),
                      poODS->GetRasterXSize(),
                      poODS->GetRasterYSize(),
                      poODS->GetRasterCount(),
                      poDependentDS->GetRasterXSize(),
                      poDependentDS->GetRasterYSize(),
                      poDependentDS->GetRasterCount() );

            GDALClose( poODS );
            poODS = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try appending .aux to the end of the filename.                  */
/* -------------------------------------------------------------------- */
    if( poODS == NULL )
    {
        osAuxFilename = pszBasename;
        osAuxFilename += ".";
        osAuxFilename += pszAuxSuffixLC;
        fp = VSIFOpenL( osAuxFilename, "rb" );
        if ( fp == NULL && VSIIsCaseSensitiveFS(osAuxFilename) )
        {
            // Can't found file with lower case suffix. Try the upper case one.
            osAuxFilename = pszBasename;
            osAuxFilename += ".";
            osAuxFilename += pszAuxSuffixUC;
            fp = VSIFOpenL( osAuxFilename, "rb" );
        }

        if( fp != NULL )
        {
            if( VSIFReadL( abyHeader, 1, 32, fp ) == 32 &&
                STARTS_WITH_CI((char *) abyHeader, "EHFA_HEADER_TAG") )
            {
                /* Avoid causing failure in opening of main file from SWIG bindings */
                /* when auxiliary file cannot be opened (#3269) */
                CPLTurnFailureIntoWarning(TRUE);
                if( poDependentDS != NULL && poDependentDS->GetShared() )
                    poODS = (GDALDataset *) GDALOpenShared( osAuxFilename, eAccess );
                else
                    poODS = (GDALDataset *) GDALOpen( osAuxFilename, eAccess );
                CPLTurnFailureIntoWarning(FALSE);
            }
            CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
        }

        if( poODS != NULL )
        {
            const char *pszDep
                = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
            if( pszDep == NULL  )
            {
                CPLDebug( "AUX",
                          "Found %s but it has no dependent file, ignoring.",
                          osAuxFilename.c_str() );
                GDALClose( poODS );
                poODS = NULL;
            }
            else if( !EQUAL(pszDep,osJustFile) )
            {
                VSIStatBufL sStatBuf;

                if( VSIStatExL( pszDep, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
                {
                    CPLDebug( "AUX", "%s is for file %s, not %s, ignoring.",
                              osAuxFilename.c_str(),
                              pszDep, osJustFile.c_str() );
                    GDALClose( poODS );
                    poODS = NULL;
                }
                else
                {
                    CPLDebug( "AUX", "%s is for file %s, not %s, but since\n"
                              "%s does not exist, we will use .aux file as our own.",
                              osAuxFilename.c_str(),
                              pszDep, osJustFile.c_str(),
                              pszDep );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Confirm that the aux file matches the configuration of the      */
/*      dependent dataset.                                              */
/* -------------------------------------------------------------------- */
    if( poODS != NULL && poDependentDS != NULL
        && (poODS->GetRasterCount() != poDependentDS->GetRasterCount()
            || poODS->GetRasterXSize() != poDependentDS->GetRasterXSize()
            || poODS->GetRasterYSize() != poDependentDS->GetRasterYSize()) )
    {
        CPLDebug( "AUX",
                  "Ignoring aux file %s as its raster configuration\n"
                  "(%dP x %dL x %dB) does not match master file (%dP x %dL x %dB)",
                  osAuxFilename.c_str(),
                  poODS->GetRasterXSize(),
                  poODS->GetRasterYSize(),
                  poODS->GetRasterCount(),
                  poDependentDS->GetRasterXSize(),
                  poDependentDS->GetRasterYSize(),
                  poDependentDS->GetRasterCount() );

        GDALClose( poODS );
        poODS = NULL;
    }

    return poODS;
}

/************************************************************************/
/* Infrastructure to check that dataset characteristics are valid       */
/************************************************************************/

CPL_C_START

/**
  * \brief Return TRUE if the dataset dimensions are valid.
  *
  * @param nXSize raster width
  * @param nYSize raster height
  *
  * @since GDAL 1.7.0
  */
int GDALCheckDatasetDimensions( int nXSize, int nYSize )
{
    if (nXSize <= 0 || nYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid dataset dimensions : %d x %d", nXSize, nYSize);
        return FALSE;
    }
    return TRUE;
}

/**
  * \brief Return TRUE if the band count is valid.
  *
  * If the configuration option GDAL_MAX_BAND_COUNT is defined,
  * the band count will be compared to the maximum number of band allowed.
  * If not defined, the maximum number allowed is 65536.
  *
  * @param nBands the band count
  * @param bIsZeroAllowed TRUE if band count == 0 is allowed
  *
  * @since GDAL 1.7.0
  */

int GDALCheckBandCount( int nBands, int bIsZeroAllowed )
{
    if (nBands < 0 || (!bIsZeroAllowed && nBands == 0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid band count : %d", nBands);
        return FALSE;
    }
    const char* pszMaxBandCount = CPLGetConfigOption("GDAL_MAX_BAND_COUNT", "65536");
    /* coverity[tainted_data] */
    int nMaxBands = atoi(pszMaxBandCount);
    if ( nBands > nMaxBands )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid band count : %d. Maximum allowed currently is %d. "
                 "Define GDAL_MAX_BAND_COUNT to a higher level if it is a legitimate number.",
                 nBands, nMaxBands);
        return FALSE;
    }
    return TRUE;
}

CPL_C_END

/************************************************************************/
/*                     GDALSerializeGCPListToXML()                      */
/************************************************************************/

void GDALSerializeGCPListToXML( CPLXMLNode* psParentNode,
                                GDAL_GCP* pasGCPList,
                                int nGCPCount,
                                const char* pszGCPProjection )
{
    CPLString oFmt;

    CPLXMLNode *psPamGCPList = CPLCreateXMLNode( psParentNode, CXT_Element,
                                                 "GCPList" );

    CPLXMLNode* psLastChild = NULL;

    if( pszGCPProjection != NULL
        && strlen(pszGCPProjection) > 0 )
    {
        CPLSetXMLValue( psPamGCPList, "#Projection",
                        pszGCPProjection );
        psLastChild = psPamGCPList->psChild;
    }

    for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        GDAL_GCP *psGCP = pasGCPList + iGCP;

        CPLXMLNode *psXMLGCP = CPLCreateXMLNode( NULL, CXT_Element, "GCP" );

        if( psLastChild == NULL )
            psPamGCPList->psChild = psXMLGCP;
        else
            psLastChild->psNext = psXMLGCP;
        psLastChild = psXMLGCP;

        CPLSetXMLValue( psXMLGCP, "#Id", psGCP->pszId );

        if( psGCP->pszInfo != NULL && strlen(psGCP->pszInfo) > 0 )
            CPLSetXMLValue( psXMLGCP, "Info", psGCP->pszInfo );

        CPLSetXMLValue( psXMLGCP, "#Pixel",
                        oFmt.Printf( "%.4f", psGCP->dfGCPPixel ) );

        CPLSetXMLValue( psXMLGCP, "#Line",
                        oFmt.Printf( "%.4f", psGCP->dfGCPLine ) );

        CPLSetXMLValue( psXMLGCP, "#X",
                        oFmt.Printf( "%.12E", psGCP->dfGCPX ) );

        CPLSetXMLValue( psXMLGCP, "#Y",
                        oFmt.Printf( "%.12E", psGCP->dfGCPY ) );

        /* Note: GDAL 1.10.1 and older generated #GCPZ, but could not read it back */
        if( psGCP->dfGCPZ != 0.0 )
            CPLSetXMLValue( psXMLGCP, "#Z",
                            oFmt.Printf( "%.12E", psGCP->dfGCPZ ) );
    }
}

/************************************************************************/
/*                     GDALDeserializeGCPListFromXML()                  */
/************************************************************************/

void GDALDeserializeGCPListFromXML( CPLXMLNode* psGCPList,
                                    GDAL_GCP** ppasGCPList,
                                    int* pnGCPCount,
                                    char** ppszGCPProjection )
{
    if( ppszGCPProjection )
    {
        const char *pszRawProj = CPLGetXMLValue(psGCPList, "Projection", "");

        OGRSpatialReference oSRS;
        if( strlen(pszRawProj) > 0
            && oSRS.SetFromUserInput( pszRawProj ) == OGRERR_NONE )
            oSRS.exportToWkt( ppszGCPProjection );
        else
            *ppszGCPProjection = CPLStrdup("");
    }

    // Count GCPs.
    int nGCPMax = 0;

    for( CPLXMLNode *psXMLGCP = psGCPList->psChild;
         psXMLGCP != NULL;
         psXMLGCP = psXMLGCP->psNext )
    {

        if( !EQUAL(psXMLGCP->pszValue,"GCP") ||
            psXMLGCP->eType != CXT_Element )
            continue;

        nGCPMax++;
    }

    *ppasGCPList = static_cast<GDAL_GCP *>(
        nGCPMax ? CPLCalloc(sizeof(GDAL_GCP), nGCPMax) : NULL );
    *pnGCPCount = 0;

    if( nGCPMax == 0 )
        return;

    for( CPLXMLNode *psXMLGCP = psGCPList->psChild;
         *ppasGCPList != NULL && psXMLGCP != NULL;
         psXMLGCP = psXMLGCP->psNext )
    {
        GDAL_GCP *psGCP = *ppasGCPList + *pnGCPCount;

        if( !EQUAL(psXMLGCP->pszValue,"GCP") ||
            psXMLGCP->eType != CXT_Element )
            continue;

        GDALInitGCPs( 1, psGCP );

        CPLFree( psGCP->pszId );
        psGCP->pszId = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Id",""));

        CPLFree( psGCP->pszInfo );
        psGCP->pszInfo = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Info",""));

        psGCP->dfGCPPixel = CPLAtof(CPLGetXMLValue(psXMLGCP,"Pixel","0.0"));
        psGCP->dfGCPLine = CPLAtof(CPLGetXMLValue(psXMLGCP,"Line","0.0"));

        psGCP->dfGCPX = CPLAtof(CPLGetXMLValue(psXMLGCP,"X","0.0"));
        psGCP->dfGCPY = CPLAtof(CPLGetXMLValue(psXMLGCP,"Y","0.0"));
        const char* pszZ = CPLGetXMLValue(psXMLGCP,"Z",NULL);
        if( pszZ == NULL )
        {
            // Note: GDAL 1.10.1 and older generated #GCPZ,
            // but could not read it back.
            pszZ = CPLGetXMLValue(psXMLGCP,"GCPZ","0.0");
        }
        psGCP->dfGCPZ = CPLAtof(pszZ);

        (*pnGCPCount)++;
    }
}

/************************************************************************/
/*                   GDALSerializeOpenOptionsToXML()                    */
/************************************************************************/

void GDALSerializeOpenOptionsToXML( CPLXMLNode* psParentNode, char** papszOpenOptions)
{
    if( papszOpenOptions != NULL )
    {
        CPLXMLNode* psOpenOptions = CPLCreateXMLNode( psParentNode, CXT_Element, "OpenOptions" );
        CPLXMLNode* psLastChild = NULL;

        for(char** papszIter = papszOpenOptions; *papszIter != NULL; papszIter ++ )
        {
            const char *pszRawValue;
            char *pszKey = NULL;
            CPLXMLNode *psOOI;

            pszRawValue = CPLParseNameValue( *papszIter, &pszKey );

            psOOI = CPLCreateXMLNode( NULL, CXT_Element, "OOI" );
            if( psLastChild == NULL )
                psOpenOptions->psChild = psOOI;
            else
                psLastChild->psNext = psOOI;
            psLastChild = psOOI;

            CPLSetXMLValue( psOOI, "#key", pszKey );
            CPLCreateXMLNode( psOOI, CXT_Text, pszRawValue );

            CPLFree( pszKey );
        }
    }
}

/************************************************************************/
/*                  GDALDeserializeOpenOptionsFromXML()                 */
/************************************************************************/

char** GDALDeserializeOpenOptionsFromXML( CPLXMLNode* psParentNode )
{
    char** papszOpenOptions = NULL;
    CPLXMLNode* psOpenOptions = CPLGetXMLNode(psParentNode, "OpenOptions");
    if( psOpenOptions != NULL )
    {
        CPLXMLNode* psOOI;
        for( psOOI = psOpenOptions->psChild; psOOI != NULL;
                psOOI = psOOI->psNext )
        {
            if( !EQUAL(psOOI->pszValue,"OOI")
                || psOOI->eType != CXT_Element
                || psOOI->psChild == NULL
                || psOOI->psChild->psNext == NULL
                || psOOI->psChild->eType != CXT_Attribute
                || psOOI->psChild->psChild == NULL )
                continue;

            char* pszName = psOOI->psChild->psChild->pszValue;
            char* pszValue = psOOI->psChild->psNext->pszValue;
            if( pszName != NULL && pszValue != NULL )
                papszOpenOptions = CSLSetNameValue( papszOpenOptions, pszName, pszValue );
        }
    }
    return papszOpenOptions;
}

/************************************************************************/
/*                    GDALRasterIOGetResampleAlg()                      */
/************************************************************************/

GDALRIOResampleAlg GDALRasterIOGetResampleAlg(const char* pszResampling)
{
    GDALRIOResampleAlg eResampleAlg = GRIORA_NearestNeighbour;
    if( STARTS_WITH_CI(pszResampling, "NEAR") )
        eResampleAlg = GRIORA_NearestNeighbour;
    else if( EQUAL(pszResampling, "BILINEAR") )
        eResampleAlg = GRIORA_Bilinear;
    else if( EQUAL(pszResampling, "CUBIC") )
        eResampleAlg = GRIORA_Cubic;
    else if( EQUAL(pszResampling, "CUBICSPLINE") )
        eResampleAlg = GRIORA_CubicSpline;
    else if( EQUAL(pszResampling, "LANCZOS") )
        eResampleAlg = GRIORA_Lanczos;
    else if( EQUAL(pszResampling, "AVERAGE") )
        eResampleAlg = GRIORA_Average;
    else if( EQUAL(pszResampling, "MODE") )
        eResampleAlg = GRIORA_Mode;
    else if( EQUAL(pszResampling, "GAUSS") )
        eResampleAlg = GRIORA_Gauss;
    else
        CPLError(CE_Warning, CPLE_NotSupported,
                "GDAL_RASTERIO_RESAMPLING = %s not supported", pszResampling);
    return eResampleAlg;
}

/************************************************************************/
/*                    GDALRasterIOGetResampleAlgStr()                   */
/************************************************************************/

const char* GDALRasterIOGetResampleAlg(GDALRIOResampleAlg eResampleAlg)
{
    switch(eResampleAlg)
    {
        case GRIORA_NearestNeighbour:
            return "NearestNeighbour";
        case GRIORA_Bilinear:
            return "Bilinear";
        case GRIORA_Cubic:
            return "Cubic";
        case GRIORA_CubicSpline:
            return "CubicSpline";
        case GRIORA_Lanczos:
            return "Lanczos";
        case GRIORA_Average:
            return "Average";
        case GRIORA_Mode:
            return "Mode";
        case GRIORA_Gauss:
            return "Gauss";
        default:
            CPLAssert(false);
            return "Unknown";
    }
}

/************************************************************************/
/*                   GDALRasterIOExtraArgSetResampleAlg()               */
/************************************************************************/

void GDALRasterIOExtraArgSetResampleAlg(GDALRasterIOExtraArg* psExtraArg,
                                        int nXSize, int nYSize,
                                        int nBufXSize, int nBufYSize)
{
    if( (nBufXSize != nXSize || nBufYSize != nYSize) &&
        psExtraArg->eResampleAlg == GRIORA_NearestNeighbour  )
    {
        const char* pszResampling = CPLGetConfigOption("GDAL_RASTERIO_RESAMPLING", NULL);
        if( pszResampling != NULL )
        {
            psExtraArg->eResampleAlg = GDALRasterIOGetResampleAlg(pszResampling);
        }
    }
}

/************************************************************************/
/*                     GDALCanFileAcceptSidecarFile()                   */
/************************************************************************/

int GDALCanFileAcceptSidecarFile(const char* pszFilename)
{
    if( strstr(pszFilename, "/vsicurl/") && strchr(pszFilename, '?') )
        return FALSE;
    // Do no attempt reading side-car files on /vsisubfile/ (#6241)
    if( strncmp(pszFilename, "/vsisubfile/", strlen("/vsisubfile/")) == 0 )
        return FALSE;
    return TRUE;
}
