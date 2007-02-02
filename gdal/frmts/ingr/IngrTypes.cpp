/*****************************************************************************
 * $Id: $
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Types support function
 * Author:   Ivan Lucena, ivan@ilucena.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************
 *
 * $Log: $
 *
 */

#include "IngrTypes.h"
#include "aigrid.h"

// ------------------------------------------------------------------------
//                                                   GetDataTypeForFormat()
// ------------------------------------------------------------------------

/**
 * \brief Get the Data appropieted for a given Raster Data Type Code.
 *
 * Data Type Code ( DTC )
 * Intergraph Raster File Formats, pg. 26
 *
 * @param eCode Data Type Code.
 * @return the correspondent GDALDataType.
 */
const GDALDataType CPL_STDCALL GetDataTypeForFormat( uint16 eCode )
{
    unsigned int i;

    for( i = 0; i < FORMAT_TYPE_COUNT; i++ )
    {
		if( eCode == ahIngrFormatTypeTab[i].eFormatCode )
        {
            return ahIngrFormatTypeTab[i].eDataType;
        }
    }

    return GDT_Byte;
}

// ------------------------------------------------------------------------
//                                                  GetFormatNameFromCode()
// ------------------------------------------------------------------------

/**
 * \brief Get Intergraph Raster Data Type Name from a given code
 *
 * Data Type Code ( DTC )
 * Intergraph Raster File Formats, pg. 26
 *
 * @param eCode Data Type Code.
 * @return string with the name of a data type.
 */
const char * CPL_STDCALL GetFormatNameFromCode( uint16 eCode )
{
    unsigned int i;

    for( i = 0; i < FORMAT_TYPE_COUNT; i++ )
    {
        if( eCode == ahIngrFormatTypeTab[i].eFormatCode )
        {
            return CPLStrdup( ahIngrFormatTypeTab[i].pszName );
        }
    }

    return "Not Identified";
}


// ------------------------------------------------------------------------
//                                                  GetFormatCodeFromName()
// ------------------------------------------------------------------------

/**
 * \brief Get Intergraph Raster Data Type Code from a given name
 *
 * Data Type Code ( DTC )
 * Intergraph Raster File Formats, pg. 26
 *
 * @param eCode Data Type Code.
 * @return string with the name of a data type.
 */
const IngrFormatType GetFormatCodeFromName( GDALDataType eType, const char *pszCompression )
{
    if( EQUAL( pszCompression, "None" ) ||
        EQUAL( pszCompression, "" ) )
    {
        switch ( eType )
        {
        case GDT_Byte:      return ByteInteger;
        case GDT_Int16:     return WordIntegers;
        case GDT_UInt16:    return WordIntegers;
        case GDT_Int32:     return Integers32Bit;
        case GDT_UInt32:    return Integers32Bit;
        case GDT_Float32:   return FloatingPoint32Bit;
        case GDT_Float64:   return FloatingPoint64Bit;
        default:            return ByteInteger;
        }
    }

    unsigned int i;

    for( i = 0; i < FORMAT_TYPE_COUNT; i++ )
    {
        if( EQUAL( pszCompression, ahIngrFormatTypeTab[i].pszName ) )
        {
            return ahIngrFormatTypeTab[i].eFormatCode;
        }
    }

    return ByteInteger;
}

// ------------------------------------------------------------------------
//                                                      GetTransforMatrix()
// ------------------------------------------------------------------------

/**
 * \brief Get the Homogeneous Transformation Matrix ( TRN )
 *
 * Homogeneous Transformation Matrix ( TRN )
 * Intergraph Raster File Formats, pg. 29
 *
 * @param pHeaderOne pointer to Raster Header One.
 * @param padfGeoTransform an output GDAL GeoTransformation pointer.
 */
void CPL_STDCALL GetTransforMatrix( real64 *padfMatrix, double *padfGeoTransform )
{
    padfGeoTransform[0] = padfMatrix[3];
    padfGeoTransform[1] = padfMatrix[0];
    padfGeoTransform[2] = padfMatrix[1];
    padfGeoTransform[3] = padfMatrix[7];
    padfGeoTransform[4] = padfMatrix[4];
    padfGeoTransform[5] = padfMatrix[5];

    padfGeoTransform[0] -= ( padfGeoTransform[1] / 2 );
    padfGeoTransform[3] += ( padfGeoTransform[5] / 2 );
    padfGeoTransform[5] *= -1;
}

/**
 * \brief Set the Homogeneous Transformation Matrix ( TRN )
 *
 * Homogeneous Transformation Matrix ( TRN )
 * Intergraph Raster File Formats, pg. 29
 *
 * @param pHeaderOne pointer to Raster Header One.
 * @param padfGeoTransform an output GDAL GeoTransformation pointer.
 */
void CPL_STDCALL SetTransforMatrix( real64 *padfMatrix, double *padfGeoTransform )
{
    for( int i = 0; i < 15; i++ )
    {
        padfMatrix[i] = 0.0;
    }
    padfMatrix[15] = 1.0;

    padfMatrix[3] = padfGeoTransform[0];
    padfMatrix[0] = padfGeoTransform[1];
    padfMatrix[1] = padfGeoTransform[2];
    padfMatrix[7] = padfGeoTransform[3];
    padfMatrix[4] = padfGeoTransform[4];
    padfMatrix[5] = padfGeoTransform[5];

    padfMatrix[5] *= -1;
    padfMatrix[3] -= ( padfMatrix[5] / 2 );
    padfMatrix[7] -= ( padfMatrix[5] / 2 );
}

/**
 * \brief Set IGDS Color Table
 *
 * IGDS Color Table
 * Intergraph Raster File Formats, pg. 38
 * 
 * @param poColorTable the GDAL Color Table.
 * @param pColorTableIGDS the IGDS Color Table.
 */
uint32 CPL_STDCALL SetIGDSColorTable( GDALColorTable *poColorTable,
                                   IngrColorTable *pColorTableIGDS )
{
    GDALColorEntry oEntry;
    int i;

    for( i = 0; i < poColorTable->GetColorEntryCount(); i++ )
    {
        poColorTable->GetColorEntryAsRGB( i, &oEntry );
        pColorTableIGDS->Entry->v_red    = ( uint8 ) oEntry.c1;
        pColorTableIGDS->Entry->v_green  = ( uint8 ) oEntry.c2;
        pColorTableIGDS->Entry->v_blue   = ( uint8 ) oEntry.c3;
    }

    return i;
}

// ------------------------------------------------------------------------
//                                                      GetIGDSColorTable()
// ------------------------------------------------------------------------

/**
 * \brief Get IGDS Color Table
 *
 * IGDS Color Table
 * Intergraph Raster File Formats, pg. 38
 * 
 * @param poColorTable the GDAL Color Table.
 * @param pColorTableIGDS the IGDS Color Table.
 */
void CPL_STDCALL GetIGDSColorTable( GDALColorTable *poColorTable,
                                   IngrColorTable *pColorTableIGDS )
{
    GDALColorEntry oEntry;
    int i;

    for( i = 0; i < 256; i++ )
    {
        oEntry.c1 = ( GByte ) pColorTableIGDS->Entry->v_red;
        oEntry.c2 = ( GByte ) pColorTableIGDS->Entry->v_green;
        oEntry.c2 = ( GByte ) pColorTableIGDS->Entry->v_blue;
        poColorTable->SetColorEntry( i, &oEntry );
    }
}

// ------------------------------------------------------------------------
//                                                      SetIGDSColorTable()
// ------------------------------------------------------------------------

/**
 * \brief Set Environ-V Color Tables
 *
 * Environ-V Color Tables
 * Intergraph Raster File Formats, pg. 38
 * 
 * @param poColorTable the GDAL Color Table.
 * @param pHeaderThree the continuation of the color table
 * @return the number os color index.
 */
uint32 CPL_STDCALL SetEnvironColorTable( GDALColorTable *poColorTable,
                                      IngrEnvVTable *pEnvironTable )
{
    GDALColorEntry oEntry;
    real32 fNormFactor = 0xfff / 255;
    int i;

    for( i = 0; i < poColorTable->GetColorEntryCount(); i++ )
    {
        poColorTable->GetColorEntryAsRGB( i, &oEntry );
        pEnvironTable->Entry->v_slot   = ( uint16 ) i;
        pEnvironTable->Entry->v_red    = ( uint16 ) ( oEntry.c1 * fNormFactor );
        pEnvironTable->Entry->v_green  = ( uint16 ) ( oEntry.c2 * fNormFactor );
        pEnvironTable->Entry->v_blue   = ( uint16 ) ( oEntry.c3 * fNormFactor );
    }

    return i;
}

// ------------------------------------------------------------------------
//                                                   GetEnvironColorTable()
// ------------------------------------------------------------------------

/**
 * \brief Get Environ-V Color Tables
 *
 * Environ-V Color Tables
 * Intergraph Raster File Formats, pg. 38
 * 
 * @param nColorsCount the number of color index.
 * @param poColorTable the GDAL Color Table.
 * @param pHeaderThree the continuation of the color table
 */
void CPL_STDCALL GetEnvironColorTable( GDALColorTable *poColorTable,
                                      IngrEnvVTable *pEnvironTable,
                                      uint32 nColorsCount )
{
    // -------------------------------------------------------------------- 
    // Sort records by crescent order of "slot" 
    // -------------------------------------------------------------------- 

    vlt_slot hSwapSlot;
    bool bContinue = true; /* inprove bubble sort performance */

    uint32 i;
    uint32 j;

    for( j = 1; j < nColorsCount && bContinue; j++ )
    {
        bContinue = false;
        for( i = 0; i < nColorsCount - j; i++ )
        {
            if( pEnvironTable->Entry[i].v_slot > 
                pEnvironTable->Entry[i + 1].v_slot )
            {
                hSwapSlot = pEnvironTable->Entry[i];
                pEnvironTable->Entry[i]   = pEnvironTable->Entry[i+1];
                pEnvironTable->Entry[i+1] = hSwapSlot;
                bContinue = true;
            }
        }
    }

    // -------------------------------------------------------------------- 
    // Get Maximum Intensity and Index Values
    // -------------------------------------------------------------------- 

    uint32 nMaxIndex    = 0;
    real32 fMaxRed      = 0.0;
    real32 fMaxGreen    = 0.0;
    real32 fMaxBlues    = 0.0;

    for( i = 0; i < nColorsCount; i++ )
    {
        if( pEnvironTable->Entry[i].v_slot > nMaxIndex )
            nMaxIndex = pEnvironTable->Entry[i].v_slot;
        if( pEnvironTable->Entry[i].v_red > fMaxRed )
            fMaxRed = pEnvironTable->Entry[i].v_red;
        if( pEnvironTable->Entry[i].v_green > fMaxGreen )
            fMaxGreen = pEnvironTable->Entry[i].v_green;
        if( pEnvironTable->Entry[i].v_blue > fMaxBlues )
            fMaxBlues = pEnvironTable->Entry[i].v_blue;
    }

    // -------------------------------------------------------------------- 
    // Calculate Normalization Factor
    // -------------------------------------------------------------------- 

    real32 fNormFactor  = 0.0;

    fNormFactor  = ( fMaxRed > fMaxGreen ? fMaxRed : fMaxGreen );
    fNormFactor  = ( fNormFactor > fMaxBlues ? fNormFactor : fMaxBlues );
    fNormFactor  = 256 / fNormFactor;

    // -------------------------------------------------------------------- 
    // Loads GDAL Color Table ( filling the wholes )
    // -------------------------------------------------------------------- 

    GDALColorEntry oEntry;
    GDALColorEntry oNullEntry;

    oNullEntry.c1 = ( GByte ) 0;
    oNullEntry.c2 = ( GByte ) 0;
    oNullEntry.c3 = ( GByte ) 0;
    oNullEntry.c4 = ( GByte ) 255;

    for( i = 0, j = 0; i <= nMaxIndex; i++ )
    {
        if( pEnvironTable->Entry[j].v_slot == i )
        {
            oEntry.c1 = ( GByte ) ( pEnvironTable->Entry[j].v_red   * fNormFactor );
            oEntry.c2 = ( GByte ) ( pEnvironTable->Entry[j].v_green * fNormFactor );
            oEntry.c3 = ( GByte ) ( pEnvironTable->Entry[j].v_blue  * fNormFactor );
            oEntry.c4 = ( GByte ) 255;
            poColorTable->SetColorEntry( i, &oEntry );
            j++;
        }
        else
        {
            poColorTable->SetColorEntry( i, &oNullEntry );
        }
    }
}

// ------------------------------------------------------------------------
//                                                              SetMiniMax()
// ------------------------------------------------------------------------

/**
 * \brief Set Minimum or Maximum
 *
 * Minimum Value ( MN1, MN2, MN4, MNR, MN8 )
 * Minimum Value ( MX1, MX2, MX4, MXR, MX8 )
 * Intergraph Raster File Formats, pg. 36
 * 
 * @param eType Gdal Data type
 * @param dValue new value
 * @return IngrMinMax the minimum or maximum value union
 */
IngrMinMax CPL_STDCALL SetMinMax( GDALDataType eType, double dValue )
{
    IngrMinMax uResult;

    switch ( eType )
    {
    case GDT_Byte:
        uResult.AsUint8   = ( uint8 ) dValue;
        break;
    case GDT_Int16:
        uResult.AsUint16  = ( int16 ) dValue;
        break;
    case GDT_UInt16:
        uResult.AsUint16  = ( uint16 ) dValue;
        break;
    case GDT_Int32:
        uResult.AsUint32  = ( int32 ) dValue;
        break;
    case GDT_UInt32:
        uResult.AsUint32  = ( uint32 ) dValue;
        break;
    case GDT_Float32:
        uResult.AsReal32  = ( real32 ) dValue;
        break;
    case GDT_Float64:
        uResult.AsReal64  = ( real64 ) dValue;
    }

    return uResult;
}

// ------------------------------------------------------------------------
//                                                              GetMinMax()
// ------------------------------------------------------------------------

/**
 * \brief Get Minimum or Maximum
 *
 * Minimum Value ( MN1, MN2, MN4, MNR, MN8 )
 * Minimum Value ( MX1, MX2, MX4, MXR, MX8 )
 * Intergraph Raster File Formats, pg. 36
 * 
 * @param eType Gdal Data type
 * @param dValue IngrMinMin new value
 * @return double minimum or maximum
 */
double CPL_STDCALL GetMinMax( GDALDataType eType, IngrMinMax hValue )
{
    switch ( eType )
    {
    case GDT_Byte:    return ( double ) hValue.AsUint8;
    case GDT_Int16:   return ( double ) hValue.AsUint16;
    case GDT_UInt16:  return ( double ) hValue.AsUint16;
    case GDT_Int32:   return ( double ) hValue.AsUint32;
    case GDT_UInt32:  return ( double ) hValue.AsUint32;
    case GDT_Float32: return ( double ) hValue.AsReal32;
    case GDT_Float64: return ( double ) hValue.AsReal64;
    default:          return ( double ) 0.0;
    }
}

// ------------------------------------------------------------------------
//                                                     GetOrientationName()
// ------------------------------------------------------------------------

/**
 * \brief Get Origin Orientation
 *
 * Origin Orientation ( SLO )
 * Intergraph Raster File Formats, pg. 32
 * 
 * @param eCode Origin Orientation type
 * @return string name of Orientation
 */
const char *GetOrientationName( uint8 eType )
{
    switch ( eType )
    {
    case UpperLeftHorizontal:       return CPLStrdup( "Upper Left Horizontal" );
    case UpperLeftVertical:         return CPLStrdup( "Upper Left Vertical" );
    case UpperRightHorizontal:      return CPLStrdup( "Upper Right Horizontal" );
    case UpperRightVertical:        return CPLStrdup( "Upper Right Vertical" );
    case LowerLeftHorizontal:       return CPLStrdup( "Lower Left Horizontal" );
    case LowerLeftVertical:         return CPLStrdup( "Lower Left Vertical" );
    case LowerRightHorizontal:      return CPLStrdup( "Lower Right Horizontal" );
    case LowerRightVertical:        return CPLStrdup( "Lower Right Vertical" );
    default:                        return CPLStrdup( "Not Identified" );
    }
}

// ------------------------------------------------------------------------
//                                                 GetApplicationTypeName()
// ------------------------------------------------------------------------

/**
 * \brief Get Application Type Name
 *
 * Application Type Code ( UTC )
 * Intergraph Raster File Formats, pg. 27
 * 
 * @param eCode Origin Orientation type
 * @return string name of Application Type
 */
const char *GetApplicationTypeName( uint8 eType )
{
    switch ( eType )
    {
    case 0: return CPLStrdup( "\"Generic\" Raster Image File" );
    case 1: return CPLStrdup( "Digital Terrain Modeling" );
    case 2: return CPLStrdup( "Grid Data Utilities" );
    case 3: return CPLStrdup( "Drawing Scanning" );
    case 4: return CPLStrdup( "Image Processing" );
    case 5: return CPLStrdup( "Hidden Surfaces" );
    case 6: return CPLStrdup( "Imagitex Scanner Product" );
    case 7: return CPLStrdup( "Screen Copy Plotting" );
    case 8: return CPLStrdup( "I/IMAGE and MicroStation Imager" );
    case 9: return CPLStrdup( "ModelView" );
    }
}

// ------------------------------------------------------------------------
//                                                     DecodePackedBinary()
// ------------------------------------------------------------------------

/**
 * \brief Decode Packed Binary Block
 *
 * 
 * @param GByte* pabyCur - pointer to raw buffer
 * @param int nDataSize - size of the block
 * @param int nBlockXSize - size of the block in X
 * @param int nBlockYSize - size of the block in Y
 * @param GInt32* panData - data pointer
 * @return CPLErr error code
 */
CPLErr DecodePackedBinary( GByte *pabyDst, 
                           int nDataSize, 
                           int nBlockXSize, 
                           int nBlockYSize,
                           GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodePackedBinary not implemented yet");
    return CE_Failure;
}

CPLErr DecodeRunLengthEncoded( GByte *pabyDst, 
                               int nDataSize, 
                               int nBlockXSize, 
                               int nBlockYSize,
                               GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeRunLengthEncoded not implemented yet");
    return CE_Failure;
}
CPLErr DecodeCCITTGroup4( GByte *pabyDst, 
                          int nDataSize, 
                          int nBlockXSize, 
                          int nBlockYSize,
                          GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeCCITTGroup4 not implemented yet");
    return CE_Failure;
}
CPLErr DecodeAdaptiveRGB( GByte *pabyDst, 
                          int nDataSize, 
                          int nBlockXSize, 
                          int nBlockYSize,
                          GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveRGB not implemented yet");
    return CE_Failure;
}
CPLErr DecodeAdaptiveGrayScale( GByte *pabyDst, 
                                int nDataSize, 
                                int nBlockXSize, 
                                int nBlockYSize,
                                GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeAdaptiveGrayScale not implemented yet");
    return CE_Failure;
}
CPLErr DecodeContinuousTone( GByte *pabyDst, 
                             int nDataSize, 
                             int nBlockXSize, 
                             int nBlockYSize,
                             GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeContinuousTone not implemented yet");
    return CE_Failure;
}
CPLErr DecodeJPEG( GByte *pabyDst, 
                   int nDataSize, 
                   int nBlockXSize, 
                   int nBlockYSize,
                   GByte *pabySrc )
{
    memcpy( pabyDst, 0, nDataSize );
    CPLError( CE_Failure, CPLE_FileIO, "DecodeJPEG not implemented yet");
    return CE_Failure;
}

