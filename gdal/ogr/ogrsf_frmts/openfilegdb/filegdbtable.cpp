/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "filegdbtable.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <limits>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "filegdbtable_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogrpgeogeometry.h"

CPL_CVSID("$Id$")

#define TEST_BIT(ar, bit)                       (ar[(bit) / 8] & (1 << ((bit) % 8)))
#define BIT_ARRAY_SIZE_IN_BYTES(bitsize)        (((bitsize)+7)/8)

#define UUID_SIZE_IN_BYTES              16

#define IS_VALID_LAYER_GEOM_TYPE(byVal)         ((byVal) <= FGTGT_POLYGON || (byVal) == FGTGT_MULTIPATCH)

/* Reserve one extra byte in case the last field is a string */
/*       or 2 for 2 ReadVarIntAndAddNoCheck() in a row */
/*       or 4 for SkipVarUInt() with nIter = 4 */
/*       or for 4 ReadVarUInt64NoCheck */
#define ZEROES_AFTER_END_OF_BUFFER      4

constexpr GUInt32 EXT_SHAPE_Z_FLAG     = 0x80000000U;
constexpr GUInt32 EXT_SHAPE_M_FLAG     = 0x40000000U;
constexpr GUInt32 EXT_SHAPE_CURVE_FLAG = 0x20000000U;

constexpr GUInt32 EXT_SHAPE_SEGMENT_ARC = 1;
constexpr GUInt32 EXT_SHAPE_SEGMENT_BEZIER = 4;
constexpr GUInt32 EXT_SHAPE_SEGMENT_ELLIPSE = 5;

namespace OpenFileGDB
{

/************************************************************************/
/*                           SanitizeScale()                            */
/************************************************************************/

static double SanitizeScale(double dfVal)
{
    if( dfVal == 0.0 )
        return std::numeric_limits<double>::min(); // to prevent divide by zero
    return dfVal;
}

/************************************************************************/
/*                      FileGDBTablePrintError()                        */
/************************************************************************/

void FileGDBTablePrintError(const char* pszFile, int nLineNumber)
{
    CPLError( CE_Failure, CPLE_AppDefined, "Error occurred in %s at line %d",
              pszFile, nLineNumber );
}

/************************************************************************/
/*                            FileGDBTable()                            */
/************************************************************************/

FileGDBTable::FileGDBTable()
{
    Init();
}

/************************************************************************/
/*                           ~FileGDBTable()                            */
/************************************************************************/

FileGDBTable::~FileGDBTable()
{
    Close();
}

/************************************************************************/
/*                                Init()                                */
/************************************************************************/

void FileGDBTable::Init()
{
    osFilename = "";
    fpTable = nullptr;
    fpTableX = nullptr;
    nFileSize = 0;
    memset(&sCurField, 0, sizeof(sCurField));
    bError = FALSE;
    nCurRow = -1;
    nLastCol = -1;
    pabyIterVals = nullptr;
    iAccNullable = 0;
    nRowBlobLength = 0;
    /* eCurFieldType = OFTInteger; */
    eTableGeomType = FGTGT_NONE;
    nValidRecordCount = 0;
    nTotalRecordCount = 0;
    iGeomField = -1;
    nCountNullableFields = 0;
    nNullableFieldsSizeInBytes = 0;
    nBufferMaxSize = 0;
    pabyBuffer = nullptr;
    nFilterXMin = 0;
    nFilterXMax = 0;
    nFilterYMin = 0;
    nFilterYMax = 0;
    osObjectIdColName = "";
    achGUIDBuffer[0] = 0;
    nChSaved = -1;
    pabyTablXBlockMap = nullptr;
    nCountBlocksBeforeIBlockIdx = 0;
    nCountBlocksBeforeIBlockValue = 0;
    bHasReadGDBIndexes = FALSE;
    nOffsetFieldDesc = 0;
    nFieldDescLength = 0;
    nTablxOffsetSize = 0;
    anFeatureOffsets.resize(0);
    nOffsetHeaderEnd = 0;
    bHasDeletedFeaturesListed = FALSE;
    bIsDeleted = FALSE;
}

/************************************************************************/
/*                                Close()                               */
/************************************************************************/

void FileGDBTable::Close()
{
    if( fpTable )
        VSIFCloseL(fpTable);
    fpTable = nullptr;

    if( fpTableX )
        VSIFCloseL(fpTableX);
    fpTableX = nullptr;

    CPLFree(pabyBuffer);
    pabyBuffer = nullptr;

    for(size_t i=0;i<apoFields.size();i++)
        delete apoFields[i];
    apoFields.resize(0);

    CPLFree(pabyTablXBlockMap);
    pabyTablXBlockMap = nullptr;

    for(size_t i=0;i<apoIndexes.size();i++)
        delete apoIndexes[i];
    apoIndexes.resize(0);

    Init();
}

/************************************************************************/
/*                              GetFieldIdx()                           */
/************************************************************************/

int FileGDBTable::GetFieldIdx(const std::string& osName) const
{
    for(size_t i=0;i<apoFields.size();i++)
    {
        if( apoFields[i]->GetName() == osName )
            return (int)i;
    }
    return -1;
}

/************************************************************************/
/*                          ReadVarUInt()                               */
/************************************************************************/

template < class OutType, class ControlType >
static int ReadVarUInt(GByte*& pabyIter, GByte* pabyEnd, OutType& nOutVal)
{
    const int errorRetValue = FALSE;
    if( !(ControlType::check_bounds) )
    {
        /* nothing */
    }
    else if( ControlType::verbose_error )
    {
        returnErrorIf(pabyIter >= pabyEnd);
    }
    else
    {
        if( pabyIter >= pabyEnd )
            return FALSE;
    }
    OutType b = *pabyIter;
    if( (b & 0x80) == 0 )
    {
        pabyIter ++;
        nOutVal = b;
        return TRUE;
    }
    GByte* pabyLocalIter = pabyIter + 1;
    int nShift = 7;
    OutType nVal = ( b & 0x7F );
    while( true )
    {
        if( !(ControlType::check_bounds) )
        {
            /* nothing */
        }
        else if( ControlType::verbose_error )
        {
            returnErrorIf(pabyLocalIter >= pabyEnd);
        }
        else
        {
            if( pabyLocalIter >= pabyEnd )
                return FALSE;
        }
        b = *pabyLocalIter;
        pabyLocalIter ++;
        nVal |= ( b & 0x7F ) << nShift;
        if( (b & 0x80) == 0 )
        {
            pabyIter = pabyLocalIter;
            nOutVal = nVal;
            return TRUE;
        }
        nShift += 7;
        // To avoid undefined behavior later when doing << nShift
        if( nShift >= static_cast<int>(sizeof(OutType)) * 8 )
        {
            pabyIter = pabyLocalIter;
            nOutVal = nVal;
            returnError();
        }
    }
}

struct ControlTypeVerboseErrorTrue
{
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL check_bounds = true;
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL verbose_error = true;
};

struct ControlTypeVerboseErrorFalse
{
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL check_bounds = true;
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL verbose_error = false;
};

struct ControlTypeNone
{
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL check_bounds = false;
    // cppcheck-suppress unusedStructMember
    static const EMULATED_BOOL verbose_error = false;
};

static int ReadVarUInt32(GByte*& pabyIter, GByte* pabyEnd, GUInt32& nOutVal)
{
    return ReadVarUInt<GUInt32, ControlTypeVerboseErrorTrue>(pabyIter, pabyEnd, nOutVal);
}

static void ReadVarUInt32NoCheck(GByte*& pabyIter, GUInt32& nOutVal)
{
    GByte* pabyEnd = nullptr;
    ReadVarUInt<GUInt32, ControlTypeNone>(pabyIter, pabyEnd, nOutVal);
}

static int ReadVarUInt32Silent(GByte*& pabyIter, GByte* pabyEnd, GUInt32& nOutVal)
{
    return ReadVarUInt<GUInt32, ControlTypeVerboseErrorFalse>(pabyIter, pabyEnd, nOutVal);
}

static void ReadVarUInt64NoCheck(GByte*& pabyIter, GUIntBig& nOutVal)
{
    GByte* pabyEnd = nullptr;
    ReadVarUInt<GUIntBig, ControlTypeNone>(pabyIter, pabyEnd, nOutVal);
}

/************************************************************************/
/*                      IsLikelyFeatureAtOffset()                       */
/************************************************************************/

int FileGDBTable::IsLikelyFeatureAtOffset(vsi_l_offset nOffset,
                                          GUInt32* pnSize,
                                          int* pbDeletedRecord)
{
    VSIFSeekL(fpTable, nOffset, SEEK_SET);
    GByte abyBuffer[4];
    if( VSIFReadL(abyBuffer, 4, 1, fpTable) != 1 )
        return FALSE;

    nRowBlobLength = GetUInt32(abyBuffer, 0);
    if( nRowBlobLength < (GUInt32)nNullableFieldsSizeInBytes ||
        nRowBlobLength > nFileSize - nOffset ||
        nRowBlobLength > INT_MAX - ZEROES_AFTER_END_OF_BUFFER ||
        nRowBlobLength > 10 * (nFileSize / nValidRecordCount) )
    {
        /* Is it a deleted record ? */
        if( (int)nRowBlobLength < 0 && nRowBlobLength != 0x80000000U )
        {
            nRowBlobLength = (GUInt32) (-(int)nRowBlobLength);
            if( nRowBlobLength < (GUInt32)nNullableFieldsSizeInBytes ||
                nRowBlobLength > nFileSize - nOffset ||
                nRowBlobLength > INT_MAX - ZEROES_AFTER_END_OF_BUFFER ||
                nRowBlobLength > 10 * (nFileSize / nValidRecordCount) )
                return FALSE;
            else
                *pbDeletedRecord = TRUE;
        }
        else
            return FALSE;
    }
    else
        *pbDeletedRecord = FALSE;

    if( nRowBlobLength > nBufferMaxSize )
    {
        GByte* pabyNewBuffer = (GByte*) VSI_REALLOC_VERBOSE( pabyBuffer,
                                nRowBlobLength + ZEROES_AFTER_END_OF_BUFFER );
        if( pabyNewBuffer == nullptr )
            return FALSE;

        pabyBuffer = pabyNewBuffer;
        nBufferMaxSize = nRowBlobLength;
    }
    if( pabyBuffer == nullptr ) return FALSE; /* to please Coverity. Not needed */
    if( nCountNullableFields > 0 )
    {
        if( VSIFReadL(pabyBuffer, nNullableFieldsSizeInBytes, 1, fpTable) != 1 )
            return FALSE;
    }
    size_t i;
    iAccNullable = 0;
    int bExactSizeKnown = TRUE;
    GUInt32 nRequiredLength = nNullableFieldsSizeInBytes;
    for(i=0;i<apoFields.size();i++)
    {
        if( apoFields[i]->bNullable )
        {
            int bIsNull = TEST_BIT(pabyBuffer, iAccNullable);
            iAccNullable ++;
            if( bIsNull )
                continue;
        }

        switch( apoFields[i]->eType )
        {
            case FGFT_STRING:
            case FGFT_XML:
            case FGFT_GEOMETRY:
            case FGFT_BINARY:
            {
                nRequiredLength += 1; /* varuint32 so at least one byte */
                bExactSizeKnown = FALSE;
                break;
            }

            /* Only 4 bytes ? */
            case FGFT_RASTER: nRequiredLength += sizeof(GInt32); break;

            case FGFT_INT16: nRequiredLength += sizeof(GInt16); break;
            case FGFT_INT32: nRequiredLength += sizeof(GInt32); break;
            case FGFT_FLOAT32: nRequiredLength += sizeof(float); break;
            case FGFT_FLOAT64: nRequiredLength += sizeof(double); break;
            case FGFT_DATETIME: nRequiredLength += sizeof(double); break;
            case FGFT_UUID_1:
            case FGFT_UUID_2: nRequiredLength += UUID_SIZE_IN_BYTES; break;

            default:
                CPLAssert(false);
                break;
        }
        if( nRowBlobLength < nRequiredLength )
            return FALSE;
    }
    if( !bExactSizeKnown )
    {
        if( VSIFReadL(pabyBuffer + nNullableFieldsSizeInBytes,
                nRowBlobLength - nNullableFieldsSizeInBytes, 1, fpTable) != 1 )
            return FALSE;

        iAccNullable = 0;
        nRequiredLength = nNullableFieldsSizeInBytes;
        for(i=0;i<apoFields.size();i++)
        {
            if( apoFields[i]->bNullable )
            {
                int bIsNull = TEST_BIT(pabyBuffer, iAccNullable);
                iAccNullable ++;
                if( bIsNull )
                    continue;
            }

            switch( apoFields[i]->eType )
            {
                case FGFT_STRING:
                case FGFT_XML:
                {
                    GByte* pabyIter = pabyBuffer + nRequiredLength;
                    GUInt32 nLength;
                    if( !ReadVarUInt32Silent(pabyIter, pabyBuffer + nRowBlobLength, nLength) ||
                        pabyIter - (pabyBuffer + nRequiredLength) > 5 )
                        return FALSE;
                    nRequiredLength = static_cast<GUInt32>(pabyIter - pabyBuffer);
                    if( nLength > nRowBlobLength - nRequiredLength )
                        return FALSE;
                    for( GUInt32 j=0;j<nLength;j++ )
                    {
                        if( pabyIter[j] == 0 )
                            return FALSE;
                    }
                    if( !CPLIsUTF8((const char*)pabyIter, nLength) )
                        return FALSE;
                    nRequiredLength += nLength;
                    break;
                }

                case FGFT_GEOMETRY:
                case FGFT_BINARY:
                {
                    GByte* pabyIter = pabyBuffer + nRequiredLength;
                    GUInt32 nLength;
                    if( !ReadVarUInt32Silent(pabyIter, pabyBuffer + nRowBlobLength, nLength) ||
                        pabyIter - (pabyBuffer + nRequiredLength) > 5 )
                        return FALSE;
                    nRequiredLength = static_cast<GUInt32>(pabyIter - pabyBuffer);
                    if( nLength > nRowBlobLength - nRequiredLength )
                        return FALSE;
                    nRequiredLength += nLength;
                    break;
                }

                /* Only 4 bytes ? */
                case FGFT_RASTER: nRequiredLength += sizeof(GInt32); break;

                case FGFT_INT16: nRequiredLength += sizeof(GInt16); break;
                case FGFT_INT32: nRequiredLength += sizeof(GInt32); break;
                case FGFT_FLOAT32: nRequiredLength += sizeof(float); break;
                case FGFT_FLOAT64: nRequiredLength += sizeof(double); break;
                case FGFT_DATETIME: nRequiredLength += sizeof(double); break;
                case FGFT_UUID_1:
                case FGFT_UUID_2: nRequiredLength += UUID_SIZE_IN_BYTES; break;

                default:
                    CPLAssert(false);
                    break;
            }
            if( nRequiredLength > nRowBlobLength )
                return FALSE;
        }
    }

    *pnSize = 4 + nRequiredLength;
    return nRequiredLength == nRowBlobLength;
}

/************************************************************************/
/*                      GuessFeatureLocations()                         */
/************************************************************************/

#define MARK_DELETED(x)  ((x) | (((GUIntBig)1) << 63))
#define IS_DELETED(x)    (((x) & (((GUIntBig)1) << 63)) != 0)
#define GET_OFFSET(x)    ((x) & ~(((GUIntBig)1) << 63))

int FileGDBTable::GuessFeatureLocations()
{
    VSIFSeekL(fpTable, 0, SEEK_END);
    nFileSize = VSIFTellL(fpTable);

    int bReportDeletedFeatures =
        CPLTestBool(CPLGetConfigOption("OPENFILEGDB_REPORT_DELETED_FEATURES", "NO"));

    vsi_l_offset nOffset = 40 + nFieldDescLength;

    if( nOffsetFieldDesc != 40 )
    {
        /* Check if there is a deleted field description at offset 40 */
        GByte abyBuffer[14];
        VSIFSeekL(fpTable, 40, SEEK_SET);
        if( VSIFReadL(abyBuffer, 14, 1, fpTable) != 1 )
            return FALSE;
        int nSize = GetInt32(abyBuffer, 0);
        int nVersion = GetInt32(abyBuffer + 4, 0);
        if( nSize < 0 && nSize > -1024 * 1024 &&
            (nVersion == 3 || nVersion == 4) &&
            IS_VALID_LAYER_GEOM_TYPE(abyBuffer[8]) &&
            abyBuffer[9] == 3 && abyBuffer[10] == 0 && abyBuffer[11] == 0 )
        {
            nOffset = 40 + (-nSize);
        }
        else
        {
            nOffset = 40;
        }
    }

    int nInvalidRecords = 0;
    while(nOffset < nFileSize)
    {
        GUInt32 nSize;
        int bDeletedRecord;
        if( !IsLikelyFeatureAtOffset(nOffset, &nSize, &bDeletedRecord) )
        {
            nOffset ++;
        }
        else
        {
            /*CPLDebug("OpenFileGDB", "Feature found at offset %d (size = %d)",
                     nOffset, nSize);*/
            if( bDeletedRecord )
            {
                if( bReportDeletedFeatures )
                {
                    bHasDeletedFeaturesListed = TRUE;
                    anFeatureOffsets.push_back(MARK_DELETED(nOffset));
                }
                else
                {
                    nInvalidRecords ++;
                    anFeatureOffsets.push_back(0);
                }
            }
            else
                anFeatureOffsets.push_back(nOffset);
            nOffset += nSize;
        }
    }
    nTotalRecordCount = (int) anFeatureOffsets.size();
    if( nTotalRecordCount - nInvalidRecords > nValidRecordCount )
    {
        if( !bHasDeletedFeaturesListed )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "More features found (%d) than declared number of valid features (%d). "
                    "So deleted features will likely be reported.",
                    nTotalRecordCount - nInvalidRecords, nValidRecordCount);
        }
        nValidRecordCount = nTotalRecordCount - nInvalidRecords;
    }

    return nTotalRecordCount > 0;
}

/************************************************************************/
/*                            ReadTableXHeader()                        */
/************************************************************************/

int FileGDBTable::ReadTableXHeader()
{
    const int errorRetValue = FALSE;
    GByte abyHeader[16];

    // Read .gdbtablx file header
    returnErrorIf(VSIFReadL( abyHeader, 16, 1, fpTableX ) != 1 );
    GUInt32 n1024Blocks = GetUInt32(abyHeader + 4, 0);

    nTotalRecordCount = GetInt32(abyHeader + 8, 0);
    if( n1024Blocks == 0 )
        returnErrorIf(nTotalRecordCount != 0 );
    else
        returnErrorIf(nTotalRecordCount < 0 );

    nTablxOffsetSize = GetUInt32(abyHeader + 12, 0);
    returnErrorIf(nTablxOffsetSize < 4 || nTablxOffsetSize > 6);

    if( n1024Blocks != 0 )
    {
        GByte abyTrailer[16];

        VSIFSeekL( fpTableX, nTablxOffsetSize * 1024 * (vsi_l_offset)n1024Blocks + 16, SEEK_SET );
        returnErrorIf(VSIFReadL( abyTrailer, 16, 1, fpTableX ) != 1 );

        GUInt32 nMagic = GetUInt32(abyTrailer, 0);

        GUInt32 nBitsForBlockMap = GetUInt32(abyTrailer + 4, 0);
        returnErrorIf(nBitsForBlockMap > INT_MAX / 1024);

        GUInt32 n1024BlocksBis = GetUInt32(abyTrailer + 8, 0);
        returnErrorIf(n1024BlocksBis != n1024Blocks );

        /* GUInt32 nMagic2 = GetUInt32(abyTrailer + 12, 0); */

        if( nMagic == 0 )
        {
            returnErrorIf(nBitsForBlockMap != n1024Blocks );
            /* returnErrorIf(nMagic2 != 0 ); */
        }
        else
        {
            returnErrorIf((GUInt32)nTotalRecordCount > nBitsForBlockMap * 1024 );
#ifdef DEBUG_VERBOSE
            CPLDebug("OpenFileGDB", "%s .gdbtablx has block map array",
                     osFilename.c_str());
#endif

            // Allocate a bit mask array for blocks of 1024 features.
            int nSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(nBitsForBlockMap);
            pabyTablXBlockMap = (GByte*) VSI_MALLOC_VERBOSE( nSizeInBytes );
            returnErrorIf(pabyTablXBlockMap == nullptr );
            returnErrorIf(VSIFReadL( pabyTablXBlockMap, nSizeInBytes, 1, fpTableX ) != 1 );
            /* returnErrorIf(nMagic2 == 0 ); */

            // Check that the map is consistent with n1024Blocks
            GUInt32 nCountBlocks = 0;
            for(GUInt32 i=0;i<nBitsForBlockMap;i++)
                nCountBlocks += TEST_BIT(pabyTablXBlockMap, i) != 0;
            returnErrorIf(nCountBlocks != n1024Blocks );
        }
    }
    return TRUE;
}

/************************************************************************/
/*                            ReadUTF16String()                         */
/************************************************************************/

static std::string ReadUTF16String(const GByte* pabyIter, int nCarCount)
{
    std::wstring osWideStr;
    for(int j=0;j<nCarCount;j++)
        osWideStr += pabyIter[2 * j] | (pabyIter[2 * j + 1] << 8);
    char* pszStr = CPLRecodeFromWChar(osWideStr.c_str(), CPL_ENC_UCS2, CPL_ENC_UTF8);
    std::string osRet(pszStr);
    CPLFree(pszStr);
    return osRet;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

int FileGDBTable::Open(const char* pszFilename,
                       const char* pszLayerName)
{
    const int errorRetValue = FALSE;
    CPLAssert(fpTable == nullptr);

    osFilename = pszFilename;
    CPLString osFilenameWithLayerName(osFilename);
    if( pszLayerName )
        osFilenameWithLayerName += CPLSPrintf(" (layer %s)", pszLayerName);

    fpTable = VSIFOpenL( pszFilename, "rb" );
    if( fpTable == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Cannot open %s: %s", osFilenameWithLayerName.c_str(),
                 VSIStrerror(errno));
        return FALSE;
    }

    // Read .gdtable file header
    GByte abyHeader[40];
    returnErrorIf(VSIFReadL( abyHeader, 40, 1, fpTable ) != 1 );
    nValidRecordCount = GetInt32(abyHeader + 4, 0);
    returnErrorIf(nValidRecordCount < 0 );

    CPLString osTableXName;
    if( nValidRecordCount > 0 &&
        !CPLTestBool(CPLGetConfigOption("OPENFILEGDB_IGNORE_GDBTABLX", "FALSE")) )
    {
        osTableXName = CPLFormFilename(CPLGetPath(pszFilename),
                                        CPLGetBasename(pszFilename), "gdbtablx");
        fpTableX = VSIFOpenL( osTableXName, "rb" );
        if( fpTableX == nullptr )
        {
            const char* pszIgnoreGDBTablXAbsence =
                CPLGetConfigOption("OPENFILEGDB_IGNORE_GDBTABLX_ABSENCE", nullptr);
            if( pszIgnoreGDBTablXAbsence == nullptr )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "%s could not be found. "
                        "Trying to guess feature locations, but this might fail or "
                        "return incorrect results", osTableXName.c_str());
            }
            else if( !CPLTestBool(pszIgnoreGDBTablXAbsence) )
            {
                returnErrorIf(fpTableX == nullptr );
            }
        }
        else if( !ReadTableXHeader() )
            return FALSE;
    }

    if( fpTableX != nullptr )
    {
        if(nValidRecordCount > nTotalRecordCount )
        {
            if( CPLTestBool(CPLGetConfigOption("OPENFILEGDB_USE_GDBTABLE_RECORD_COUNT", "FALSE")) )
            {
                /* Potentially unsafe. See #5842 */
                CPLDebug("OpenFileGDB", "%s: nTotalRecordCount (was %d) forced to nValidRecordCount=%d",
                        osFilenameWithLayerName.c_str(),
                        nTotalRecordCount, nValidRecordCount);
                nTotalRecordCount = nValidRecordCount;
            }
            else
            {
                /* By default err on the safe side */
                CPLError(CE_Warning, CPLE_AppDefined,
                        "File %s declares %d valid records, but %s declares "
                        "only %d total records. Using that later value for safety "
                        "(this possibly ignoring features). "
                        "You can also try setting OPENFILEGDB_IGNORE_GDBTABLX=YES to "
                        "completely ignore the .gdbtablx file (but possibly retrieving "
                        "deleted features), or set OPENFILEGDB_USE_GDBTABLE_RECORD_COUNT=YES "
                        "(but that setting can potentially cause crashes)",
                        osFilenameWithLayerName.c_str(), nValidRecordCount,
                        osTableXName.c_str(), nTotalRecordCount);
                nValidRecordCount = nTotalRecordCount;
            }
        }

#ifdef DEBUG_VERBOSE
        else if( nTotalRecordCount != nValidRecordCount )
        {
            CPLDebug("OpenFileGDB", "%s: nTotalRecordCount=%d nValidRecordCount=%d",
                    pszFilename,
                    nTotalRecordCount, nValidRecordCount);
        }
#endif
    }

    nOffsetFieldDesc = GetUInt32(abyHeader + 32, 0) |
            (static_cast<GUIntBig>(GetUInt32(abyHeader + 36, 0)) << 32);

#ifdef DEBUG_VERBOSE
    if( nOffsetFieldDesc != 40 )
    {
        CPLDebug("OpenFileGDB", "%s: nOffsetFieldDesc=" CPL_FRMT_GUIB,
                 pszFilename, nOffsetFieldDesc);
    }
#endif

    // Skip to field description section
    VSIFSeekL( fpTable, nOffsetFieldDesc, SEEK_SET );
    returnErrorIf(VSIFReadL( abyHeader, 14, 1, fpTable ) != 1 );
    nFieldDescLength = GetUInt32(abyHeader, 0);

    returnErrorIf(nOffsetFieldDesc >
                    std::numeric_limits<GUIntBig>::max() - nFieldDescLength);
    nOffsetHeaderEnd = nOffsetFieldDesc + nFieldDescLength;

    returnErrorIf(nFieldDescLength > 10 * 1024 * 1024 || nFieldDescLength < 10 );
    GByte byTableGeomType = abyHeader[8];
    if( IS_VALID_LAYER_GEOM_TYPE(byTableGeomType) )
        eTableGeomType = (FileGDBTableGeometryType) byTableGeomType;
    else
        CPLDebug("OpenFileGDB", "Unknown table geometry type: %d", byTableGeomType);
    const GByte byTableGeomTypeFlags = abyHeader[11];
    m_bGeomTypeHasM = (byTableGeomTypeFlags & (1 << 6)) != 0;
    m_bGeomTypeHasZ = (byTableGeomTypeFlags & (1 << 7)) != 0;

    GUInt16 iField, nFields;
    nFields = GetUInt16(abyHeader + 12, 0);

    /* No interest in guessing a trivial file */
    returnErrorIf( fpTableX == nullptr && nFields == 0) ;

    GUInt32 nRemaining = nFieldDescLength - 10;
    nBufferMaxSize = nRemaining;
    pabyBuffer = (GByte*)VSI_MALLOC_VERBOSE(nBufferMaxSize + ZEROES_AFTER_END_OF_BUFFER);
    returnErrorIf(pabyBuffer == nullptr );
    returnErrorIf(VSIFReadL(pabyBuffer, nRemaining, 1, fpTable) != 1 );

    GByte* pabyIter = pabyBuffer;
    for(iField = 0; iField < nFields; iField ++)
    {
        returnErrorIf(nRemaining < 1 );
        GByte nCarCount = pabyIter[0];

        pabyIter ++;
        nRemaining --;
        returnErrorIf(nRemaining < (GUInt32)(2 * nCarCount + 1) );
        // coverity[tainted_data,tainted_data_argument]
        std::string osName(ReadUTF16String(pabyIter, nCarCount));
        pabyIter += 2 * nCarCount;
        nRemaining -= 2 * nCarCount;

        returnErrorIf(nRemaining < 1 );
        nCarCount = pabyIter[0];
        pabyIter ++;
        nRemaining --;
        returnErrorIf(nRemaining < (GUInt32)(2 * nCarCount + 1) );
        // coverity[tainted_data,tainted_data_argument]
        std::string osAlias(ReadUTF16String(pabyIter, nCarCount));
        pabyIter += 2 * nCarCount;
        nRemaining -= 2 * nCarCount;

        returnErrorIf(nRemaining < 1 );
        GByte byFieldType = pabyIter[0];
        pabyIter ++;
        nRemaining --;

        if( byFieldType > FGFT_XML)
        {
            CPLDebug("OpenFileGDB", "Unhandled field type : %d", byFieldType);
            returnError();
        }

        FileGDBFieldType eType = (FileGDBFieldType) byFieldType;
        if( eType != FGFT_GEOMETRY && eType != FGFT_RASTER )
        {
            GByte flags = 0;
            int nMaxWidth = 0;
            GUInt32 defaultValueLength = 0;

            switch( eType )
            {
                case FGFT_STRING:
                {
                    returnErrorIf(nRemaining < 6 );
                    nMaxWidth = GetInt32(pabyIter, 0);
                    returnErrorIf(nMaxWidth < 0);
                    flags = pabyIter[4];
                    pabyIter += 5;
                    nRemaining -= 5;
                    GByte* pabyIterBefore = pabyIter;
                    returnErrorIf(!ReadVarUInt32(pabyIter, pabyIter + nRemaining, defaultValueLength));
                    nRemaining -= static_cast<GUInt32>(pabyIter - pabyIterBefore);
                    break;
                }

                case FGFT_OBJECTID:
                case FGFT_BINARY:
                case FGFT_UUID_1:
                case FGFT_UUID_2:
                case FGFT_XML:
                    returnErrorIf(nRemaining < 2 );
                    flags = pabyIter[1];
                    pabyIter += 2;
                    nRemaining -= 2;
                    break;

                default:
                    returnErrorIf(nRemaining < 3 );
                    flags = pabyIter[1];
                    defaultValueLength = pabyIter[2];
                    pabyIter += 3;
                    nRemaining -= 3;
                    break;
            }

            OGRField sDefault;
            OGR_RawField_SetUnset(&sDefault);
            if( (flags & 4) != 0 )
            {
                /* Default value */
                /* Found on PreNIS.gdb/a0000000d.gdbtable */
                returnErrorIf(nRemaining < defaultValueLength );
                if( defaultValueLength )
                {
                    if( eType == FGFT_STRING )
                    {
                        sDefault.String = (char*)CPLMalloc(defaultValueLength+1);
                        memcpy(sDefault.String, pabyIter, defaultValueLength);
                        sDefault.String[defaultValueLength] = 0;
                    }
                    else if( eType == FGFT_INT16 && defaultValueLength == 2 )
                    {
                        sDefault.Integer = GetInt16(pabyIter, 0);
                        sDefault.Set.nMarker2 = 0;
                        sDefault.Set.nMarker3 = 0;
                    }
                    else if( eType == FGFT_INT32 && defaultValueLength == 4 )
                    {
                        sDefault.Integer = GetInt32(pabyIter, 0);
                        sDefault.Set.nMarker2 = 0;
                        sDefault.Set.nMarker3 = 0;
                    }
                    else if( eType == FGFT_FLOAT32 && defaultValueLength == 4 )
                    {
                        sDefault.Real = GetFloat32(pabyIter, 0);
                    }
                    else if( eType == FGFT_FLOAT64 && defaultValueLength == 8 )
                    {
                        sDefault.Real = GetFloat64(pabyIter, 0);
                    }
                    else if( eType == FGFT_DATETIME && defaultValueLength == 8 )
                    {
                        const double dfVal = GetFloat64(pabyIter, 0);
                        FileGDBDoubleDateToOGRDate(dfVal, &sDefault);
                    }
                }

                pabyIter += defaultValueLength;
                nRemaining -= defaultValueLength;
            }

            if( eType == FGFT_OBJECTID )
            {
                returnErrorIf(!osObjectIdColName.empty() );
                osObjectIdColName = osName;
                continue;
            }

            FileGDBField* poField = new FileGDBField(this);
            poField->osName = osName;
            poField->osAlias = osAlias;
            poField->eType = eType;
            poField->bNullable = (flags & 1);
            poField->nMaxWidth = nMaxWidth;
            poField->sDefault = sDefault;
            apoFields.push_back(poField);
        }
        else
        {

            FileGDBRasterField* poRasterField = nullptr;
            FileGDBGeomField* poField;
            if( eType == FGFT_GEOMETRY )
            {
                returnErrorIf(iGeomField >= 0 );
                poField = new FileGDBGeomField(this);
            }
            else
            {
                poRasterField = new FileGDBRasterField(this);
                poField = poRasterField;
            }

            poField->osName = osName;
            poField->osAlias = osAlias;
            poField->eType = eType;
            if( eType == FGFT_GEOMETRY )
                iGeomField = (int)apoFields.size();
            apoFields.push_back(poField);

            returnErrorIf(nRemaining < 2 );
            GByte flags = pabyIter[1];
            poField->bNullable = (flags & 1);
            pabyIter += 2;
            nRemaining -= 2;

            if( eType == FGFT_RASTER )
            {
                returnErrorIf(nRemaining < 1 );
                nCarCount = pabyIter[0];
                pabyIter ++;
                nRemaining --;
                returnErrorIf(nRemaining < (GUInt32)(2 * nCarCount + 1) );
                std::string osRasterColumn(ReadUTF16String(pabyIter, nCarCount));
                pabyIter += 2 * nCarCount;
                nRemaining -= 2 * nCarCount;
                poRasterField->osRasterColumnName = osRasterColumn;
            }

            returnErrorIf(nRemaining < 2 );
            GUInt16 nLengthWKT = GetUInt16(pabyIter, 0);
            pabyIter += sizeof(nLengthWKT);
            nRemaining -= sizeof(nLengthWKT);

            returnErrorIf(nRemaining < (GUInt32)(1 + nLengthWKT) );
            poField->osWKT = ReadUTF16String(pabyIter, nLengthWKT/2);
            pabyIter += nLengthWKT;
            nRemaining -= nLengthWKT;

            GByte abyGeomFlags = pabyIter[0];
            pabyIter ++;
            nRemaining --;
            poField->bHasMOriginScaleTolerance = (abyGeomFlags & 2) != 0;
            poField->bHasZOriginScaleTolerance = (abyGeomFlags & 4) != 0;

            if( eType == FGFT_GEOMETRY || abyGeomFlags > 0 )
            {
                returnErrorIf(
                        nRemaining < (GUInt32)(sizeof(double) * ( 4 + (( eType == FGFT_GEOMETRY ) ? 4 : 0) + (poField->bHasMOriginScaleTolerance + poField->bHasZOriginScaleTolerance) * 3 )) );

    #define READ_DOUBLE(field) do { \
        field = GetFloat64(pabyIter, 0); \
        pabyIter += sizeof(double); \
        nRemaining -= sizeof(double); } while( false )

                READ_DOUBLE(poField->dfXOrigin);
                READ_DOUBLE(poField->dfYOrigin);
                READ_DOUBLE(poField->dfXYScale);
                returnErrorIf( poField->dfXYScale == 0 );

                if( poField->bHasMOriginScaleTolerance )
                {
                    READ_DOUBLE(poField->dfMOrigin);
                    READ_DOUBLE(poField->dfMScale);
                }

                if( poField->bHasZOriginScaleTolerance )
                {
                    READ_DOUBLE(poField->dfZOrigin);
                    READ_DOUBLE(poField->dfZScale);
                }

                READ_DOUBLE(poField->dfXYTolerance);

                if( poField->bHasMOriginScaleTolerance )
                {
                    READ_DOUBLE(poField->dfMTolerance);
#ifdef DEBUG_VERBOSE
                    CPLDebug("OpenFileGDB", "MOrigin = %g, MScale = %g, MTolerance = %g",
                             poField->dfMOrigin, poField->dfMScale, poField->dfMTolerance);
#endif
                }

                if( poField->bHasZOriginScaleTolerance )
                {
                    READ_DOUBLE(poField->dfZTolerance);
                }
            }

            if( eType == FGFT_RASTER )
            {
                /* Always one byte at end ? */
                returnErrorIf(nRemaining < 1 );
                pabyIter += 1;
                nRemaining -= 1;
            }
            else
            {
                returnErrorIf(nRemaining < 4 * sizeof(double) );
                READ_DOUBLE(poField->dfXMin);
                READ_DOUBLE(poField->dfYMin);
                READ_DOUBLE(poField->dfXMax);
                READ_DOUBLE(poField->dfYMax);

                if( m_bGeomTypeHasZ )
                {
                    returnErrorIf(nRemaining < 2 * sizeof(double) );
                    READ_DOUBLE(poField->dfZMin);
                    READ_DOUBLE(poField->dfZMax);
                }

                if( m_bGeomTypeHasM )
                {
                    returnErrorIf(nRemaining < 2 * sizeof(double) );
                    READ_DOUBLE(poField->dfMMin);
                    READ_DOUBLE(poField->dfMMax);
                }

                returnErrorIf(nRemaining < 5 );
                // Skip byte at zero
                pabyIter += 1;
                nRemaining -= 1;

                GUInt32 nGridSizeCount = GetUInt32(pabyIter, 0);
                pabyIter += sizeof(nGridSizeCount);
                nRemaining -= sizeof(nGridSizeCount);
                returnErrorIf(nGridSizeCount == 0 || nGridSizeCount > 3);
                returnErrorIf(nRemaining < nGridSizeCount * sizeof(double) );
                for( GUInt32 i = 0; i < nGridSizeCount; i++ )
                {
                    double dfGridResolution;
                    READ_DOUBLE(dfGridResolution);
                    m_adfSpatialIndexGridResolution.push_back(dfGridResolution);
                }
            }
        }

        nCountNullableFields += apoFields.back()->bNullable;
    }
    nNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(nCountNullableFields);

#ifdef DEBUG_VERBOSE
    if( nRemaining > 0 )
    {
        CPLDebug("OpenFileGDB", "%u remaining (ignored) bytes in field header section",
                 nRemaining);
    }
#endif

    if( nValidRecordCount > 0 && fpTableX == nullptr )
        return GuessFeatureLocations();

    return TRUE;
}

/************************************************************************/
/*                          SkipVarUInt()                               */
/************************************************************************/

/* Bound check only valid if nIter <= 4 */
static int SkipVarUInt(GByte*& pabyIter, GByte* pabyEnd, int nIter = 1)
{
    const int errorRetValue = FALSE;
    GByte* pabyLocalIter = pabyIter;
    returnErrorIf(pabyLocalIter /*+ nIter - 1*/ >= pabyEnd);
    while( nIter -- > 0 )
    {
        while( true )
        {
            GByte b = *pabyLocalIter;
            pabyLocalIter ++;
            if( (b & 0x80) == 0 )
                break;
        }
    }
    pabyIter = pabyLocalIter;
    return TRUE;
}

/************************************************************************/
/*                      ReadVarIntAndAddNoCheck()                       */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static void ReadVarIntAndAddNoCheck(GByte*& pabyIter, GIntBig& nOutVal)
{
    GUInt32 b;

    b = *pabyIter;
    GUIntBig nVal = (b & 0x3F);
    bool bNegative = (b & 0x40) != 0;
    if( (b & 0x80) == 0 )
    {
        pabyIter ++;
        if( bNegative )
            nOutVal -= nVal;
        else
            nOutVal += nVal;
        return;
    }

    GByte* pabyLocalIter = pabyIter + 1;
    int nShift = 6;
    while( true )
    {
        GUIntBig b64 = *pabyLocalIter;
        pabyLocalIter ++;
        nVal |= ( b64 & 0x7F ) << nShift;
        if( (b64 & 0x80) == 0 )
        {
            pabyIter = pabyLocalIter;
            if( bNegative )
                nOutVal -= nVal;
            else
                nOutVal += nVal;
            return;
        }
        nShift += 7;
        // To avoid undefined behavior later when doing << nShift
        if( nShift >= static_cast<int>(sizeof(GIntBig)) * 8 )
        {
            pabyIter = pabyLocalIter;
            nOutVal = nVal;
            return;
        }
    }
}

/************************************************************************/
/*                       GetOffsetInTableForRow()                       */
/************************************************************************/

vsi_l_offset FileGDBTable::GetOffsetInTableForRow(int iRow)
{
    const int errorRetValue = 0;
    returnErrorIf(iRow < 0 || iRow >= nTotalRecordCount );

    bIsDeleted = FALSE;
    if( fpTableX == nullptr )
    {
        bIsDeleted = IS_DELETED(anFeatureOffsets[iRow]);
        return GET_OFFSET(anFeatureOffsets[iRow]);
    }

    if( pabyTablXBlockMap != nullptr )
    {
        GUInt32 nCountBlocksBefore = 0;
        int iBlock = iRow / 1024;

        // Check if the block is not empty
        if( TEST_BIT(pabyTablXBlockMap, iBlock) == 0 )
            return 0;

        // In case of sequential reading, optimization to avoid recomputing
        // the number of blocks since the beginning of the map
        if( iBlock >= nCountBlocksBeforeIBlockIdx )
        {
            nCountBlocksBefore = nCountBlocksBeforeIBlockValue;
            for(int i=nCountBlocksBeforeIBlockIdx;i<iBlock;i++)
                nCountBlocksBefore += TEST_BIT(pabyTablXBlockMap, i) != 0;
        }
        else
        {
            nCountBlocksBefore = 0;
            for(int i=0;i<iBlock;i++)
                nCountBlocksBefore += TEST_BIT(pabyTablXBlockMap, i) != 0;
        }
        nCountBlocksBeforeIBlockIdx = iBlock;
        nCountBlocksBeforeIBlockValue = nCountBlocksBefore;
        int iCorrectedRow = nCountBlocksBefore * 1024 + (iRow % 1024);
        VSIFSeekL(fpTableX, 16 + static_cast<vsi_l_offset>(nTablxOffsetSize) * iCorrectedRow, SEEK_SET);
    }
    else
    {
        VSIFSeekL(fpTableX, 16 + static_cast<vsi_l_offset>(nTablxOffsetSize) * iRow, SEEK_SET);
    }

    GByte abyBuffer[6];
    bError = VSIFReadL(abyBuffer, nTablxOffsetSize, 1, fpTableX) != 1;
    returnErrorIf(bError );
    vsi_l_offset nOffset;

    if( nTablxOffsetSize == 4 )
        nOffset = GetUInt32(abyBuffer, 0);
    else if( nTablxOffsetSize == 5 )
        nOffset = GetUInt32(abyBuffer, 0) | (((vsi_l_offset)abyBuffer[4]) << 32);
    else
        nOffset = GetUInt32(abyBuffer, 0) | (((vsi_l_offset)abyBuffer[4]) << 32) | (((vsi_l_offset)abyBuffer[5]) << 40);

#ifdef DEBUG_VERBOSE
    if( iRow == 0 && nOffset != 0 &&
        nOffset != nOffsetHeaderEnd && nOffset != nOffsetHeaderEnd + 4 )
        CPLDebug("OpenFileGDB", "%s: first feature offset = " CPL_FRMT_GUIB ". Expected " CPL_FRMT_GUIB,
                 osFilename.c_str(), nOffset, nOffsetHeaderEnd);
#endif

    return nOffset;
}

/************************************************************************/
/*                      GetAndSelectNextNonEmptyRow()                   */
/************************************************************************/

int FileGDBTable::GetAndSelectNextNonEmptyRow(int iRow)
{
    const int errorRetValue = -1;
    returnErrorAndCleanupIf(iRow < 0 || iRow >= nTotalRecordCount, nCurRow = -1 );

    while( iRow < nTotalRecordCount )
    {
        if( pabyTablXBlockMap != nullptr && (iRow % 1024) == 0 )
        {
            int iBlock = iRow / 1024;
            if( TEST_BIT(pabyTablXBlockMap, iBlock) == 0 )
            {
                int nBlocks = (nTotalRecordCount+1023)/1024;
                do
                {
                    iBlock ++;
                }
                while( iBlock < nBlocks &&
                    TEST_BIT(pabyTablXBlockMap, iBlock) == 0 );

                iRow = iBlock * 1024;
                if( iRow >= nTotalRecordCount )
                    return -1;
            }
        }

        if( SelectRow(iRow) )
            return iRow;
        if( HasGotError() )
            return -1;
        iRow ++;
    }

    return -1;
}

/************************************************************************/
/*                            SelectRow()                               */
/************************************************************************/

int FileGDBTable::SelectRow(int iRow)
{
    const int errorRetValue = FALSE;
    returnErrorAndCleanupIf(iRow < 0 || iRow >= nTotalRecordCount, nCurRow = -1 );

    if( nCurRow != iRow )
    {
        vsi_l_offset nOffsetTable = GetOffsetInTableForRow(iRow);
        if( nOffsetTable == 0 )
        {
            nCurRow = -1;
            return FALSE;
        }

        VSIFSeekL(fpTable, nOffsetTable, SEEK_SET);
        GByte abyBuffer[4];
        returnErrorAndCleanupIf(
                VSIFReadL(abyBuffer, 4, 1, fpTable) != 1, nCurRow = -1 );

        nRowBlobLength = GetUInt32(abyBuffer, 0);
        if( bIsDeleted )
        {
            nRowBlobLength = (GUInt32)(-(int)nRowBlobLength);
        }

        if( !(apoFields.empty() && nRowBlobLength == 0) )
        {
            /* CPLDebug("OpenFileGDB", "nRowBlobLength = %u", nRowBlobLength); */
            returnErrorAndCleanupIf(
                    nRowBlobLength < (GUInt32)nNullableFieldsSizeInBytes ||
                    nRowBlobLength > INT_MAX - ZEROES_AFTER_END_OF_BUFFER, nCurRow = -1 );

            if( nRowBlobLength > nBufferMaxSize )
            {
                /* For suspicious row blob length, check if we don't go beyond file size */
                if( nRowBlobLength > 100 * 1024 * 1024 )
                {
                    if( nFileSize == 0 )
                    {
                        VSIFSeekL(fpTable, 0, SEEK_END);
                        nFileSize = VSIFTellL(fpTable);
                        VSIFSeekL(fpTable, nOffsetTable + 4, SEEK_SET);
                    }
                    returnErrorAndCleanupIf( nOffsetTable + 4 + nRowBlobLength > nFileSize, nCurRow = -1 );
                }

                GByte* pabyNewBuffer = (GByte*) VSI_REALLOC_VERBOSE( pabyBuffer,
                                nRowBlobLength + ZEROES_AFTER_END_OF_BUFFER );
                returnErrorAndCleanupIf(pabyNewBuffer == nullptr, nCurRow = -1 );

                pabyBuffer = pabyNewBuffer;
                nBufferMaxSize = nRowBlobLength;
            }
            returnErrorAndCleanupIf(
                VSIFReadL(pabyBuffer, nRowBlobLength, 1, fpTable) != 1, nCurRow = -1 );
            /* Protection for 4 ReadVarUInt64NoCheck */
            CPL_STATIC_ASSERT(ZEROES_AFTER_END_OF_BUFFER == 4);
            pabyBuffer[nRowBlobLength] = 0;
            pabyBuffer[nRowBlobLength+1] = 0;
            pabyBuffer[nRowBlobLength+2] = 0;
            pabyBuffer[nRowBlobLength+3] = 0;
        }

        nCurRow = iRow;
        nLastCol = -1;
        pabyIterVals = pabyBuffer + nNullableFieldsSizeInBytes;
        iAccNullable = 0;
        bError = FALSE;
        nChSaved = -1;
    }

    return TRUE;
}

/************************************************************************/
/*                      FileGDBDoubleDateToOGRDate()                    */
/************************************************************************/

int FileGDBDoubleDateToOGRDate(double dfVal, OGRField* psField)
{
    // 25569: Number of days between 1899/12/30 00:00:00 and 1970/01/01 00:00:00
    double dfSeconds = (dfVal - 25569.0) * 3600.0 * 24.0;
    if( CPLIsNan(dfSeconds) ||
        dfSeconds < static_cast<double>(std::numeric_limits<GIntBig>::min())+1000 ||
        dfSeconds > static_cast<double>(std::numeric_limits<GIntBig>::max())-1000 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "FileGDBDoubleDateToOGRDate: Invalid days: %lf", dfVal);
        dfSeconds = 0.0;
    }

    struct tm brokendowntime;
    CPLUnixTimeToYMDHMS(static_cast<GIntBig>(dfSeconds), &brokendowntime);

    psField->Date.Year = (GInt16)(brokendowntime.tm_year + 1900);
    psField->Date.Month = (GByte)brokendowntime.tm_mon + 1;
    psField->Date.Day = (GByte)brokendowntime.tm_mday;
    psField->Date.Hour = (GByte)brokendowntime.tm_hour;
    psField->Date.Minute = (GByte)brokendowntime.tm_min;
    psField->Date.Second = (float)brokendowntime.tm_sec;
    psField->Date.TZFlag = 0;
    psField->Date.Reserved = 0;

    return TRUE;
}

/************************************************************************/
/*                          GetFieldValue()                             */
/************************************************************************/

OGRField* FileGDBTable::GetFieldValue(int iCol)
{
    OGRField* errorRetValue = nullptr;

    returnErrorIf(nCurRow < 0 );
    returnErrorIf((GUInt32)iCol >= apoFields.size() );
    returnErrorIf(bError );

    GByte* pabyEnd = pabyBuffer + nRowBlobLength;

    /* In case a string was previously read */
    if( nChSaved >= 0 )
    {
        *pabyIterVals = (GByte)nChSaved;
        nChSaved = -1;
    }

    if( iCol <= nLastCol )
    {
        nLastCol = -1;
        pabyIterVals = pabyBuffer + nNullableFieldsSizeInBytes;
        iAccNullable = 0;
    }

    // Skip previous fields
    for( int j = nLastCol + 1; j < iCol; j++ )
    {
        if( apoFields[j]->bNullable )
        {
            int bIsNull = TEST_BIT(pabyBuffer, iAccNullable);
            iAccNullable ++;
            if( bIsNull )
                continue;
        }

        GUInt32 nLength = 0;
        CPL_IGNORE_RET_VAL(nLength);
        switch( apoFields[j]->eType )
        {
            case FGFT_STRING:
            case FGFT_XML:
            case FGFT_GEOMETRY:
            case FGFT_BINARY:
            {
                if( !ReadVarUInt32(pabyIterVals, pabyEnd, nLength) )
                {
                    bError = TRUE;
                    returnError();
                }
                break;
            }

            /* Only 4 bytes ? */
            case FGFT_RASTER: nLength = sizeof(GInt32); break;

            case FGFT_INT16: nLength = sizeof(GInt16); break;
            case FGFT_INT32: nLength = sizeof(GInt32); break;
            case FGFT_FLOAT32: nLength = sizeof(float); break;
            case FGFT_FLOAT64: nLength = sizeof(double); break;
            case FGFT_DATETIME: nLength = sizeof(double); break;
            case FGFT_UUID_1:
            case FGFT_UUID_2: nLength = UUID_SIZE_IN_BYTES; break;

            default:
                CPLAssert(false);
                break;
        }

        if( nLength > (GUInt32)(pabyEnd - pabyIterVals) )
        {
            bError = TRUE;
            returnError();
        }
        pabyIterVals += nLength;
    }

    nLastCol = iCol;

    if( apoFields[iCol]->bNullable )
    {
        int bIsNull = TEST_BIT(pabyBuffer, iAccNullable);
        iAccNullable ++;
        if( bIsNull )
        {
            return nullptr;
        }
    }

    switch( apoFields[iCol]->eType )
    {
        case FGFT_STRING:
        case FGFT_XML:
        {
            GUInt32 nLength;
            if( !ReadVarUInt32(pabyIterVals, pabyEnd, nLength) )
            {
                bError = TRUE;
                returnError();
            }
            if( nLength > (GUInt32)(pabyEnd - pabyIterVals) )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTString; */
            sCurField.String = (char*) pabyIterVals;
            pabyIterVals += nLength;

            /* This is a trick to avoid a alloc()+copy(). We null-terminate */
            /* after the string, and save the pointer and value to restore */
            nChSaved = *pabyIterVals;
            *pabyIterVals = '\0';

            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %s", iCol, nCurRow, sCurField.String); */

            break;
        }

        case FGFT_INT16:
        {
            if( pabyIterVals + sizeof(GInt16) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTInteger; */
            sCurField.Integer = GetInt16(pabyIterVals, 0);

            pabyIterVals += sizeof(GInt16);
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %d", iCol, nCurRow, sCurField.Integer); */

            break;
        }

        case FGFT_INT32:
        {
            if( pabyIterVals + sizeof(GInt32) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTInteger; */
            sCurField.Integer = GetInt32(pabyIterVals, 0);

            pabyIterVals += sizeof(GInt32);
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %d", iCol, nCurRow, sCurField.Integer); */

            break;
        }

        case FGFT_FLOAT32:
        {
            if( pabyIterVals + sizeof(float) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTReal; */
            sCurField.Real = GetFloat32(pabyIterVals, 0);

            pabyIterVals += sizeof(float);
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %f", iCol, nCurRow, sCurField.Real); */

            break;
        }

        case FGFT_FLOAT64:
        {
            if( pabyIterVals + sizeof(double) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTReal; */
            sCurField.Real = GetFloat64(pabyIterVals, 0);

            pabyIterVals += sizeof(double);
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %f", iCol, nCurRow, sCurField.Real); */

            break;
        }

        case FGFT_DATETIME:
        {
            if( pabyIterVals + sizeof(double) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* Number of days since 1899/12/30 00:00:00 */
            const double dfVal = GetFloat64(pabyIterVals, 0);

            FileGDBDoubleDateToOGRDate(dfVal, &sCurField);
            /* eCurFieldType = OFTDateTime; */

            pabyIterVals += sizeof(double);

            break;
        }

        case FGFT_GEOMETRY:
        case FGFT_BINARY:
        {
            GUInt32 nLength;
            if( !ReadVarUInt32(pabyIterVals, pabyEnd, nLength) )
            {
                bError = TRUE;
                returnError();
            }
            if( nLength > (GUInt32)(pabyEnd - pabyIterVals) )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTBinary; */
            sCurField.Binary.nCount = nLength;
            sCurField.Binary.paData = (GByte*) pabyIterVals;

            pabyIterVals += nLength;

            /* Null terminate binary in case it is used as a string */
            nChSaved = *pabyIterVals;
            *pabyIterVals = '\0';

            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %d bytes", iCol, nCurRow, snLength); */

            break;
        }

        /* Only 4 bytes ? */
        case FGFT_RASTER:
        {
            if( pabyIterVals + sizeof(GInt32) > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* GInt32 nVal = GetInt32(pabyIterVals, 0); */

            /* eCurFieldType = OFTBinary; */
            OGR_RawField_SetUnset(&sCurField);

            pabyIterVals += sizeof(GInt32);
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %d", iCol, nCurRow, sCurField.Integer); */
            break;
        }

        case FGFT_UUID_1:
        case FGFT_UUID_2:
        {
            if( pabyIterVals + UUID_SIZE_IN_BYTES > pabyEnd )
            {
                bError = TRUE;
                returnError();
            }

            /* eCurFieldType = OFTString; */
            sCurField.String = achGUIDBuffer;
            /*78563412BC9AF0DE1234567890ABCDEF --> {12345678-9ABC-DEF0-1234-567890ABCDEF} */
            snprintf(achGUIDBuffer, sizeof(achGUIDBuffer),
                    "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                    pabyIterVals[3], pabyIterVals[2], pabyIterVals[1], pabyIterVals[0],
                    pabyIterVals[5], pabyIterVals[4],
                    pabyIterVals[7], pabyIterVals[6],
                    pabyIterVals[8], pabyIterVals[9],
                    pabyIterVals[10], pabyIterVals[11], pabyIterVals[12],
                    pabyIterVals[13], pabyIterVals[14], pabyIterVals[15]);

            pabyIterVals += UUID_SIZE_IN_BYTES;
            /* CPLDebug("OpenFileGDB", "Field %d, row %d: %s", iCol, nCurRow, sCurField.String); */

            break;
        }

        default:
            CPLAssert(false);
            break;
    }

    if( iCol == (int)apoFields.size() - 1 && pabyIterVals < pabyEnd )
    {
        CPLDebug("OpenFileGDB", "%d bytes remaining at end of record %d",
                 (int)(pabyEnd - pabyIterVals), nCurRow);
    }

    return &sCurField;
}

/************************************************************************/
/*                           GetIndexCount()                            */
/************************************************************************/

int FileGDBTable::GetIndexCount()
{
    const int errorRetValue = 0;
    if( bHasReadGDBIndexes )
        return (int) apoIndexes.size();

    bHasReadGDBIndexes = TRUE;

    const char* pszIndexesName = CPLFormFilename(CPLGetPath(osFilename.c_str()),
                                    CPLGetBasename(osFilename.c_str()), "gdbindexes");
    VSILFILE* fpIndexes = VSIFOpenL( pszIndexesName, "rb" );
    VSIStatBufL sStat;
    if( fpIndexes == nullptr )
    {
        if ( VSIStatExL( pszIndexesName, &sStat, VSI_STAT_EXISTS_FLAG) == 0 )
            returnError();
        else
            return 0;
    }

    VSIFSeekL(fpIndexes, 0, SEEK_END);
    vsi_l_offset l_nFileSize = VSIFTellL(fpIndexes);
    returnErrorAndCleanupIf(l_nFileSize > 1024 * 1024, VSIFCloseL(fpIndexes) );

    GByte* pabyIdx = (GByte*)VSI_MALLOC_VERBOSE((size_t)l_nFileSize);
    returnErrorAndCleanupIf(pabyIdx == nullptr, VSIFCloseL(fpIndexes) );

    VSIFSeekL(fpIndexes, 0, SEEK_SET);
    int nRead = (int)VSIFReadL( pabyIdx, (size_t)l_nFileSize, 1, fpIndexes );
    VSIFCloseL(fpIndexes);
    returnErrorAndCleanupIf(nRead != 1, VSIFree(pabyIdx) );

    GByte* pabyCur = pabyIdx;
    GByte* pabyEnd = pabyIdx + l_nFileSize;
    returnErrorAndCleanupIf(pabyEnd - pabyCur < 4, VSIFree(pabyIdx) );
    GUInt32 nIndexCount = GetUInt32(pabyCur, 0);
    pabyCur += 4;

    // FileGDB v9 indexes structure not handled yet. Start with 13 98 85 03
    if( nIndexCount == 0x03859813 )
    {
        CPLDebug("OpenFileGDB", ".gdbindexes v9 not handled yet");
        VSIFree(pabyIdx);
        return 0;
    }
    returnErrorAndCleanupIf(nIndexCount >= (size_t)(GetFieldCount() + 1) * 10, VSIFree(pabyIdx) );

    GUInt32 i;
    for(i=0;i<nIndexCount;i++)
    {
        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < sizeof(GUInt32), VSIFree(pabyIdx) );
        GUInt32 nIdxNameCarCount = GetUInt32(pabyCur, 0);
        pabyCur += sizeof(GUInt32);
        returnErrorAndCleanupIf(nIdxNameCarCount > 1024, VSIFree(pabyIdx) );
        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < 2 * nIdxNameCarCount, VSIFree(pabyIdx) );
        std::string osIndexName(ReadUTF16String(pabyCur, nIdxNameCarCount));
        pabyCur += 2 * nIdxNameCarCount;

        // Skip magic fields
        pabyCur += 2 + 4 + 2 + 4;

        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < sizeof(GUInt32), VSIFree(pabyIdx) );
        GUInt32 nColNameCarCount = GetUInt32(pabyCur, 0);
        pabyCur += sizeof(GUInt32);
        returnErrorAndCleanupIf(nColNameCarCount > 1024, VSIFree(pabyIdx) );
        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < 2 * nColNameCarCount, VSIFree(pabyIdx) );
        std::string osFieldName(ReadUTF16String(pabyCur, nColNameCarCount));
        pabyCur += 2 * nColNameCarCount;

        // Skip magic field
        pabyCur += 2;

        FileGDBIndex* poIndex = new FileGDBIndex();
        poIndex->osIndexName = osIndexName;
        poIndex->osFieldName = osFieldName;
        apoIndexes.push_back(poIndex);

        if( osFieldName != osObjectIdColName )
        {
            int nFieldIdx = GetFieldIdx(osFieldName);
            if( nFieldIdx < 0 )
            {
                CPLDebug("OpenFileGDB",
                         "Index defined for field %s that does not exist",
                         osFieldName.c_str());
            }
            else
            {
                if( apoFields[nFieldIdx]->poIndex != nullptr )
                {
                    CPLDebug("OpenFileGDB",
                             "There is already one index defined for field %s",
                              osFieldName.c_str());
                }
                else
                {
                    apoFields[nFieldIdx]->poIndex = poIndex;
                }
            }
        }
    }

    VSIFree(pabyIdx);

    return (int) apoIndexes.size();
}

/************************************************************************/
/*                           HasSpatialIndex()                          */
/************************************************************************/

bool FileGDBTable::HasSpatialIndex()
{
    if( m_nHasSpatialIndex < 0 )
    {
        const char* pszSpxName = CPLFormFilename(
            CPLGetPath(osFilename.c_str()),
            CPLGetBasename(osFilename.c_str()), "spx");
        VSIStatBufL sStat;
        m_nHasSpatialIndex =
            ( VSIStatExL( pszSpxName, &sStat, VSI_STAT_EXISTS_FLAG) == 0 );
    }
    return m_nHasSpatialIndex != FALSE;
}

/************************************************************************/
/*                       InstallFilterEnvelope()                        */
/************************************************************************/

#define MAX_GUINTBIG    (~((GUIntBig)0))

void FileGDBTable::InstallFilterEnvelope(const OGREnvelope* psFilterEnvelope)
{
    if( psFilterEnvelope != nullptr )
    {
        CPLAssert( iGeomField >= 0 );
        FileGDBGeomField* poGeomField = (FileGDBGeomField*) GetField(iGeomField);

        /* We store the bounding box as unscaled coordinates, so that BBOX */
        /* intersection is done with integer comparisons */
        if( psFilterEnvelope->MinX >= poGeomField->dfXOrigin )
            nFilterXMin = (GUIntBig)(0.5 + (psFilterEnvelope->MinX -
                                poGeomField->dfXOrigin) * poGeomField->dfXYScale);
        else
            nFilterXMin = 0;
        if( psFilterEnvelope->MaxX - poGeomField->dfXOrigin <
                                        static_cast<double>(MAX_GUINTBIG) / poGeomField->dfXYScale )
            nFilterXMax = (GUIntBig)(0.5 + (psFilterEnvelope->MaxX -
                            poGeomField->dfXOrigin) * poGeomField->dfXYScale);
        else
            nFilterXMax = MAX_GUINTBIG;
        if( psFilterEnvelope->MinY >= poGeomField->dfYOrigin )
            nFilterYMin = (GUIntBig)(0.5 + (psFilterEnvelope->MinY -
                                poGeomField->dfYOrigin) * poGeomField->dfXYScale);
        else
            nFilterYMin = 0;
        if( psFilterEnvelope->MaxY - poGeomField->dfYOrigin <
                                        static_cast<double>(MAX_GUINTBIG) / poGeomField->dfXYScale )
            nFilterYMax = (GUIntBig)(0.5 + (psFilterEnvelope->MaxY -
                                poGeomField->dfYOrigin) * poGeomField->dfXYScale);
        else
            nFilterYMax = MAX_GUINTBIG;
    }
    else
    {
        nFilterXMin = 0;
        nFilterXMax = 0;
        nFilterYMin = 0;
        nFilterYMax = 0;
    }
}

/************************************************************************/
/*                         GetFeatureExtent()                           */
/************************************************************************/

int FileGDBTable::GetFeatureExtent(const OGRField* psField,
                                   OGREnvelope* psOutFeatureEnvelope)
{
    const int errorRetValue = FALSE;
    GByte* pabyCur = psField->Binary.paData;
    GByte* pabyEnd = pabyCur + psField->Binary.nCount;
    GUInt32 nGeomType;
    int nToSkip = 0;

    CPLAssert( iGeomField >= 0 );
    FileGDBGeomField* poGeomField = (FileGDBGeomField*) GetField(iGeomField);

    ReadVarUInt32NoCheck(pabyCur, nGeomType);

    switch( (nGeomType & 0xff) )
    {
        case SHPT_NULL:
            return FALSE;

        case SHPT_POINTZ:
        case SHPT_POINTZM:
        case SHPT_POINT:
        case SHPT_POINTM:
        case SHPT_GENERALPOINT:
        {
            GUIntBig x, y;
            ReadVarUInt64NoCheck(pabyCur, x);
            x = CPLUnsanitizedAdd<GUIntBig>(x, -1);
            ReadVarUInt64NoCheck(pabyCur, y);
            y = CPLUnsanitizedAdd<GUIntBig>(y, -1);
            psOutFeatureEnvelope->MinX = x / poGeomField->dfXYScale + poGeomField->dfXOrigin;
            psOutFeatureEnvelope->MinY = y / poGeomField->dfXYScale + poGeomField->dfYOrigin;
            psOutFeatureEnvelope->MaxX = psOutFeatureEnvelope->MinX;
            psOutFeatureEnvelope->MaxY = psOutFeatureEnvelope->MinY;
            return TRUE;
        }

        case SHPT_MULTIPOINTZM:
        case SHPT_MULTIPOINTZ:
        case SHPT_MULTIPOINT:
        case SHPT_MULTIPOINTM:
        {
            break;
        }

        case SHPT_ARC:
        case SHPT_ARCZ:
        case SHPT_ARCZM:
        case SHPT_ARCM:
        case SHPT_POLYGON:
        case SHPT_POLYGONZ:
        case SHPT_POLYGONZM:
        case SHPT_POLYGONM:
        {
            nToSkip = 1;
            break;
        }
        case SHPT_GENERALPOLYLINE:
        case SHPT_GENERALPOLYGON:
        {
            nToSkip = 1 + ((nGeomType & EXT_SHAPE_CURVE_FLAG) ? 1 : 0);
            break;
        }

        case SHPT_GENERALMULTIPATCH:
        case SHPT_MULTIPATCHM:
        case SHPT_MULTIPATCH:
        {
            nToSkip = 2;
            break;
        }

        default:
            return FALSE;
    }

    GUInt32 nPoints;
    ReadVarUInt32NoCheck(pabyCur, nPoints);
    if( nPoints == 0 )
        return TRUE;
    returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd, nToSkip) );

    GUIntBig vxmin, vymin, vdx, vdy;

    returnErrorIf(pabyCur >= pabyEnd);
    ReadVarUInt64NoCheck(pabyCur, vxmin);
    ReadVarUInt64NoCheck(pabyCur, vymin);
    ReadVarUInt64NoCheck(pabyCur, vdx);
    ReadVarUInt64NoCheck(pabyCur, vdy);

    psOutFeatureEnvelope->MinX = vxmin / poGeomField->dfXYScale + poGeomField->dfXOrigin;
    psOutFeatureEnvelope->MinY = vymin / poGeomField->dfXYScale + poGeomField->dfYOrigin;
    psOutFeatureEnvelope->MaxX = CPLUnsanitizedAdd<GUIntBig>(vxmin, vdx) / poGeomField->dfXYScale + poGeomField->dfXOrigin;
    psOutFeatureEnvelope->MaxY = CPLUnsanitizedAdd<GUIntBig>(vymin, vdy) / poGeomField->dfXYScale + poGeomField->dfYOrigin;

    return TRUE;
}

/************************************************************************/
/*                 DoesGeometryIntersectsFilterEnvelope()               */
/************************************************************************/

int FileGDBTable::DoesGeometryIntersectsFilterEnvelope(const OGRField* psField)
{
    const int errorRetValue = TRUE;
    GByte* pabyCur = psField->Binary.paData;
    GByte* pabyEnd = pabyCur + psField->Binary.nCount;
    GUInt32 nGeomType;
    int nToSkip = 0;

    ReadVarUInt32NoCheck(pabyCur, nGeomType);

    switch( (nGeomType & 0xff) )
    {
        case SHPT_NULL:
            return TRUE;

        case SHPT_POINTZ:
        case SHPT_POINTZM:
        case SHPT_POINT:
        case SHPT_POINTM:
        case SHPT_GENERALPOINT:
        {
            GUIntBig x, y;
            ReadVarUInt64NoCheck(pabyCur, x);
            x --;
            if( x < nFilterXMin || x > nFilterXMax )
                return FALSE;
            ReadVarUInt64NoCheck(pabyCur, y);
            y --;
            return y >= nFilterYMin && y <= nFilterYMax;
        }

        case SHPT_MULTIPOINTZM:
        case SHPT_MULTIPOINTZ:
        case SHPT_MULTIPOINT:
        case SHPT_MULTIPOINTM:
        {
            break;
        }

        case SHPT_ARC:
        case SHPT_ARCZ:
        case SHPT_ARCZM:
        case SHPT_ARCM:
        case SHPT_POLYGON:
        case SHPT_POLYGONZ:
        case SHPT_POLYGONZM:
        case SHPT_POLYGONM:
        {
            nToSkip = 1;
            break;
        }

        case SHPT_GENERALPOLYLINE:
        case SHPT_GENERALPOLYGON:
        {
            nToSkip = 1 + ((nGeomType & EXT_SHAPE_CURVE_FLAG) ? 1 : 0);
            break;
        }

        case SHPT_GENERALMULTIPATCH:
        case SHPT_MULTIPATCHM:
        case SHPT_MULTIPATCH:
        {
            nToSkip = 2;
            break;
        }

        default:
            return TRUE;
    }

    GUInt32 nPoints;
    ReadVarUInt32NoCheck(pabyCur, nPoints);
    if( nPoints == 0 )
        return TRUE;
    returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd, nToSkip) );

    GUIntBig vxmin, vymin, vdx, vdy;

    returnErrorIf(pabyCur >= pabyEnd);
    ReadVarUInt64NoCheck(pabyCur, vxmin);
    if( vxmin > nFilterXMax )
        return FALSE;
    ReadVarUInt64NoCheck(pabyCur, vymin);
    if( vymin > nFilterYMax )
        return FALSE;
    ReadVarUInt64NoCheck(pabyCur, vdx);
    if( CPLUnsanitizedAdd<GUIntBig>(vxmin, vdx) < nFilterXMin )
        return FALSE;
    ReadVarUInt64NoCheck(pabyCur, vdy);
    return CPLUnsanitizedAdd<GUIntBig>(vymin, vdy) >= nFilterYMin;
}

/************************************************************************/
/*                           FileGDBField()                             */
/************************************************************************/

FileGDBField::FileGDBField( FileGDBTable* poParentIn ) :
    poParent(poParentIn),
    eType(FGFT_UNDEFINED),
    bNullable(FALSE),
    nMaxWidth(0),
    poIndex(nullptr)
{
    OGR_RawField_SetUnset(&sDefault);
}

/************************************************************************/
/*                          ~FileGDBField()                             */
/************************************************************************/

FileGDBField::~FileGDBField()
{
    if( eType == FGFT_STRING &&
        !OGR_RawField_IsUnset(&sDefault) &&
        !OGR_RawField_IsNull(&sDefault) )
        CPLFree(sDefault.String);
}

/************************************************************************/
/*                            HasIndex()                                */
/************************************************************************/

int FileGDBField::HasIndex()
{
     poParent->GetIndexCount();
     return poIndex != nullptr;
}

/************************************************************************/
/*                            GetIndex()                                */
/************************************************************************/

FileGDBIndex *FileGDBField::GetIndex()
{
     poParent->GetIndexCount();
     return poIndex;
}

/************************************************************************/
/*                           FileGDBGeomField()                         */
/************************************************************************/

FileGDBGeomField::FileGDBGeomField( FileGDBTable* poParentIn ) :
    FileGDBField(poParentIn)
{}

/************************************************************************/
/*                      FileGDBOGRGeometryConverterImpl                 */
/************************************************************************/

class FileGDBOGRGeometryConverterImpl final : public FileGDBOGRGeometryConverter
{
        const FileGDBGeomField      *poGeomField;
        GUInt32                     *panPointCount;
        GUInt32                      nPointCountMax;
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
        int                          bUseOrganize;
#endif

        bool                        ReadPartDefs( GByte*& pabyCur,
                                                  GByte* pabyEnd,
                                                  GUInt32& nPoints,
                                                  GUInt32& nParts,
                                                  GUInt32& nCurves,
                                                  bool bHasCurveDesc,
                                                  bool bIsMultiPatch );
        template <class XYSetter> int ReadXYArray(XYSetter& setter,
                                                  GByte*& pabyCur,
                                                  GByte* pabyEnd,
                                                  GUInt32 nPoints,
                                                  GIntBig& dx,
                                                  GIntBig& dy);
        template <class ZSetter> int ReadZArray(ZSetter& setter,
                                                GByte*& pabyCur,
                                                GByte* pabyEnd,
                                                GUInt32 nPoints,
                                                GIntBig& dz);
        template <class MSetter> int ReadMArray(MSetter& setter,
                                                GByte*& pabyCur,
                                                GByte* pabyEnd,
                                                GUInt32 nPoints,
                                                GIntBig& dm);

        OGRGeometry* CreateCurveGeometry(
                  GUInt32 nBaseShapeType,
                  GUInt32 nParts, GUInt32 nPoints, GUInt32 nCurves,
                  bool bHasZ, bool bHasM,
                  GByte*& pabyCur, GByte* pabyEnd );

    public:
       explicit                         FileGDBOGRGeometryConverterImpl(
                                            const FileGDBGeomField* poGeomField);
       virtual                         ~FileGDBOGRGeometryConverterImpl();

       virtual OGRGeometry*             GetAsGeometry(const OGRField* psField) override;
};

/************************************************************************/
/*                  FileGDBOGRGeometryConverterImpl()                   */
/************************************************************************/

FileGDBOGRGeometryConverterImpl::FileGDBOGRGeometryConverterImpl(
    const FileGDBGeomField* poGeomFieldIn) :
    poGeomField(poGeomFieldIn),
    panPointCount(nullptr),
    nPointCountMax(0)
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
    ,
    bUseOrganize(CPLGetConfigOption("OGR_ORGANIZE_POLYGONS", NULL) != NULL)
#endif
{}

/************************************************************************/
/*                 ~FileGDBOGRGeometryConverter()                       */
/************************************************************************/

FileGDBOGRGeometryConverterImpl::~FileGDBOGRGeometryConverterImpl()
{
    CPLFree(panPointCount);
}

/************************************************************************/
/*                          ReadPartDefs()                              */
/************************************************************************/

bool FileGDBOGRGeometryConverterImpl::ReadPartDefs( GByte*& pabyCur,
                                GByte* pabyEnd,
                                GUInt32& nPoints,
                                GUInt32& nParts,
                                GUInt32& nCurves,
                                bool bHasCurveDesc,
                                bool bIsMultiPatch )
{
    const bool errorRetValue = false;
    returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nPoints));
    if( nPoints == 0 )
    {
        nParts = 0;
        nCurves = 0;
        return true;
    }
    returnErrorIf(nPoints > (GUInt32)(pabyEnd - pabyCur) );
    if( bIsMultiPatch )
        returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd) );
    returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nParts));
    returnErrorIf(nParts > (GUInt32)(pabyEnd - pabyCur));
    returnErrorIf(nParts > static_cast<GUInt32>(INT_MAX) / sizeof(GUInt32));
    if( bHasCurveDesc )
    {
        returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nCurves) );
        returnErrorIf(nCurves > (GUInt32)(pabyEnd - pabyCur));
    }
    else
        nCurves = 0;
    if( nParts == 0 )
        return true;
    GUInt32 i;
    returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd, 4) );
    if( nParts > nPointCountMax )
    {
        GUInt32* panPointCountNew =
            (GUInt32*) VSI_REALLOC_VERBOSE( panPointCount, nParts * sizeof(GUInt32) );
        returnErrorIf(panPointCountNew == nullptr );
        panPointCount = panPointCountNew;
        nPointCountMax = nParts;
    }
    GUIntBig nSumNPartsM1 = 0;
    for(i=0;i<nParts-1;i++)
    {
        GUInt32 nTmp;
        returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nTmp));
        returnErrorIf(nTmp > (GUInt32)(pabyEnd - pabyCur) );
        panPointCount[i] = nTmp;
        nSumNPartsM1 += nTmp;
    }
    returnErrorIf(nSumNPartsM1 > nPoints );
    panPointCount[nParts-1] = (GUInt32)(nPoints - nSumNPartsM1);

    return true;
}

/************************************************************************/
/*                         XYLineStringSetter                           */
/************************************************************************/

class FileGDBOGRLineString: public OGRLineString
{
    public:
        FileGDBOGRLineString() {}

        OGRRawPoint * GetPoints() const { return paoPoints; }
};

class FileGDBOGRLinearRing: public OGRLinearRing
{
    public:
        FileGDBOGRLinearRing() {}

        OGRRawPoint * GetPoints() const { return paoPoints; }
};

class XYLineStringSetter
{
        OGRRawPoint* paoPoints;
    public:
        explicit XYLineStringSetter(OGRRawPoint* paoPointsIn) :
                                            paoPoints(paoPointsIn) {}

        void set(int i, double dfX, double dfY)
        {
            paoPoints[i].x = dfX;
            paoPoints[i].y = dfY;
        }
};

/************************************************************************/
/*                         XYMultiPointSetter                           */
/************************************************************************/

class XYMultiPointSetter
{
        OGRMultiPoint* poMPoint;
    public:
        explicit XYMultiPointSetter(OGRMultiPoint* poMPointIn) :
                                                poMPoint(poMPointIn) {}

        void set(int i, double dfX, double dfY)
        {
            (void)i;
            poMPoint->addGeometryDirectly(new OGRPoint(dfX, dfY));
        }
};

/************************************************************************/
/*                             XYArraySetter                            */
/************************************************************************/

class XYArraySetter
{
        double* padfX;
        double* padfY;
    public:
        XYArraySetter(double* padfXIn, double* padfYIn) : padfX(padfXIn), padfY(padfYIn) {}

        void set(int i, double dfX, double dfY)
        {
            padfX[i] = dfX;
            padfY[i] = dfY;
        }
};

/************************************************************************/
/*                          ReadXYArray()                               */
/************************************************************************/

template <class XYSetter> int FileGDBOGRGeometryConverterImpl::ReadXYArray(XYSetter& setter,
                                                        GByte*& pabyCur,
                                                        GByte* pabyEnd,
                                                        GUInt32 nPoints,
                                                        GIntBig& dx,
                                                        GIntBig& dy)
{
    const int errorRetValue = FALSE;
    GIntBig dxLocal = dx;
    GIntBig dyLocal = dy;

    for(GUInt32 i = 0; i < nPoints; i++ )
    {
        returnErrorIf(pabyCur /*+ 1*/ >= pabyEnd);

        ReadVarIntAndAddNoCheck(pabyCur, dxLocal);
        ReadVarIntAndAddNoCheck(pabyCur, dyLocal);

        double dfX = dxLocal / poGeomField->GetXYScale() + poGeomField->GetXOrigin();
        double dfY = dyLocal / poGeomField->GetXYScale() + poGeomField->GetYOrigin();
        setter.set(i, dfX, dfY);
    }

    dx = dxLocal;
    dy = dyLocal;
    return TRUE;
}

/************************************************************************/
/*                          ZLineStringSetter                           */
/************************************************************************/

class ZLineStringSetter
{
        OGRLineString* poLS;
    public:
        explicit ZLineStringSetter(OGRLineString* poLSIn) : poLS(poLSIn) {}

        void set(int i, double dfZ)
        {
            poLS->setZ(i, dfZ);
        }
};

/************************************************************************/
/*                         ZMultiPointSetter                           */
/************************************************************************/

class ZMultiPointSetter
{
        OGRMultiPoint* poMPoint;
    public:
        explicit ZMultiPointSetter(OGRMultiPoint* poMPointIn) :
                                                    poMPoint(poMPointIn) {}

        void set(int i, double dfZ)
        {
            poMPoint->getGeometryRef(i)->setZ(dfZ);
        }
};

/************************************************************************/
/*                             FileGDBArraySetter                            */
/************************************************************************/

class FileGDBArraySetter
{
        double* padfValues;
    public:
        explicit FileGDBArraySetter(double* padfValuesIn) :
                                                padfValues(padfValuesIn) {}

        void set(int i, double dfValue)
        {
            padfValues[i] = dfValue;
        }
};

/************************************************************************/
/*                          ReadZArray()                                */
/************************************************************************/

template <class ZSetter> int FileGDBOGRGeometryConverterImpl::ReadZArray(ZSetter& setter,
                                                      GByte*& pabyCur,
                                                      GByte* pabyEnd,
                                                      GUInt32 nPoints,
                                                      GIntBig& dz)
{
    const int errorRetValue = FALSE;
    const double dfZScale = SanitizeScale(poGeomField->GetZScale());
    for(GUInt32 i = 0; i < nPoints; i++ )
    {
        returnErrorIf(pabyCur >= pabyEnd);
        ReadVarIntAndAddNoCheck(pabyCur, dz);

        double dfZ = dz / dfZScale + poGeomField->GetZOrigin();
        setter.set(i, dfZ);
    }
    return TRUE;
}

/************************************************************************/
/*                          MLineStringSetter                           */
/************************************************************************/

class MLineStringSetter
{
        OGRLineString* poLS;
    public:
        explicit MLineStringSetter(OGRLineString* poLSIn) : poLS(poLSIn) {}

        void set(int i, double dfM)
        {
            poLS->setM(i, dfM);
        }
};

/************************************************************************/
/*                         MMultiPointSetter                           */
/************************************************************************/

class MMultiPointSetter
{
        OGRMultiPoint* poMPoint;
    public:
        explicit MMultiPointSetter(OGRMultiPoint* poMPointIn) :
                                                    poMPoint(poMPointIn) {}

        void set(int i, double dfM)
        {
            poMPoint->getGeometryRef(i)->setM(dfM);
        }
};

/************************************************************************/
/*                          ReadMArray()                                */
/************************************************************************/

template <class MSetter> int FileGDBOGRGeometryConverterImpl::ReadMArray(MSetter& setter,
                                                      GByte*& pabyCur,
                                                      GByte* pabyEnd,
                                                      GUInt32 nPoints,
                                                      GIntBig& dm)
{
    const int errorRetValue = FALSE;
    const double dfMScale = SanitizeScale(poGeomField->GetMScale());
    for(GUInt32 i = 0; i < nPoints; i++ )
    {
        returnErrorIf(pabyCur >= pabyEnd);
        ReadVarIntAndAddNoCheck(pabyCur, dm);

        double dfM = dm / dfMScale + poGeomField->GetMOrigin();
        setter.set(i, dfM);
    }
    return TRUE;
}

/************************************************************************/
/*                          CreateCurveGeometry()                       */
/************************************************************************/

class XYBufferSetter
{
        GByte* pabyBuffer;
    public:
        explicit XYBufferSetter(GByte* pabyBufferIn) :
                                                    pabyBuffer(pabyBufferIn) {}

        void set(int i, double dfX, double dfY)
        {
            CPL_LSBPTR64(&dfX);
            memcpy( pabyBuffer + 16 * i, &dfX, 8 );
            CPL_LSBPTR64(&dfY);
            memcpy( pabyBuffer + 16 * i + 8, &dfY, 8 );
        }
};

class ZOrMBufferSetter
{
        GByte* pabyBuffer;
    public:
        explicit ZOrMBufferSetter(GByte* pabyBufferIn) :
                                                    pabyBuffer(pabyBufferIn) {}

        void set(int i, double dfValue)
        {
            CPL_LSBPTR64(&dfValue);
            memcpy( pabyBuffer + 8 * i, &dfValue, 8 );
        }
};

/* We first create an extended shape buffer from the compressed stream */
/* and finally use OGRCreateFromShapeBin() to make a geometry from it */

OGRGeometry* FileGDBOGRGeometryConverterImpl::CreateCurveGeometry(
                  GUInt32 nBaseShapeType,
                  GUInt32 nParts, GUInt32 nPoints, GUInt32 nCurves,
                  bool bHasZ, bool bHasM,
                  GByte*& pabyCur, GByte* pabyEnd )
{
    OGRGeometry* errorRetValue = nullptr;
    GUInt32 i;
    const int nDims = 2 + (bHasZ ? 1 : 0) + (bHasM ? 1 : 0);
    GIntBig nMaxSize64 = 44 + 4 * static_cast<GUIntBig>(nParts) +
                         8 * nDims * static_cast<GUIntBig>(nPoints);
    nMaxSize64 += 4; // nCurves
    nMaxSize64 += static_cast<GUIntBig>(nCurves) * (4 + /* start index */
                            4 + /* curve type */
                            44 /* size of ellipse struct */ );
    nMaxSize64 += ((bHasZ ? 1 : 0) + (bHasM ? 1 : 0)) * 16; // space for bounding boxes
    if( nMaxSize64 >= INT_MAX )
    {
        returnError();
    }
    const int nMaxSize = static_cast<int>(nMaxSize64);
    GByte* pabyExtShapeBuffer = (GByte*) VSI_MALLOC_VERBOSE(nMaxSize);
    if( pabyExtShapeBuffer == nullptr )
    {
        VSIFree(pabyExtShapeBuffer);
        returnError();
    }
    GUInt32 nShapeType = nBaseShapeType | EXT_SHAPE_CURVE_FLAG;
    if( bHasZ ) nShapeType |= EXT_SHAPE_Z_FLAG;
    if( bHasM ) nShapeType |= EXT_SHAPE_M_FLAG;
    GUInt32 nTmp;
    nTmp = CPL_LSBWORD32(nShapeType);
    GByte* pabyShapeTypePtr = pabyExtShapeBuffer;
    memcpy( pabyExtShapeBuffer, &nTmp, 4 );
    memset( pabyExtShapeBuffer + 4, 0, 32 ); /* bbox: unused */
    nTmp = CPL_LSBWORD32(nParts);
    memcpy( pabyExtShapeBuffer + 36, &nTmp, 4 );
    nTmp = CPL_LSBWORD32(nPoints);
    memcpy( pabyExtShapeBuffer + 40, &nTmp, 4 );
    GUInt32 nIdx = 0;
    for( i=0; i<nParts; i++ )
    {
        nTmp = CPL_LSBWORD32(nIdx);
        nIdx += panPointCount[i];
        memcpy( pabyExtShapeBuffer + 44 + 4 * i, &nTmp, 4 );
    }
    int nOffset = 44 + 4 * nParts;
    GIntBig dx = 0;
    GIntBig dy = 0;
    XYBufferSetter arraySetter(pabyExtShapeBuffer + nOffset);
    if( !ReadXYArray<XYBufferSetter>(arraySetter,
                    pabyCur, pabyEnd, nPoints, dx, dy) )
    {
        VSIFree(pabyExtShapeBuffer);
        returnError();
    }
    nOffset += 16 * nPoints;

    if( bHasZ )
    {
        memset( pabyExtShapeBuffer + nOffset, 0, 16 ); /* bbox: unused */
        nOffset += 16;
        GIntBig dz = 0;
        ZOrMBufferSetter arrayzSetter(pabyExtShapeBuffer + nOffset);
        if( !ReadZArray<ZOrMBufferSetter>(arrayzSetter,
                        pabyCur, pabyEnd, nPoints, dz) )
        {
            VSIFree(pabyExtShapeBuffer);
            returnError();
        }
        nOffset += 8 * nPoints;
    }

    if( bHasM )
    {
        // It seems that absence of M is marked with a single byte
        // with value 66.
        if( *pabyCur == 66 )
        {
            pabyCur ++;
#if 1
            // In other code paths of this file, we drop the M component when
            // it is at null. The disabled code path would fill it with NaN
            // instead.
            nShapeType &= ~EXT_SHAPE_M_FLAG;
            nTmp = CPL_LSBWORD32(nShapeType);
            memcpy( pabyShapeTypePtr, &nTmp, 4 );
#else
            memset( pabyExtShapeBuffer + nOffset, 0, 16 ); /* bbox: unused */
            nOffset += 16;
            const double myNan = std::numeric_limits<double>::quiet_NaN();
            for( i = 0; i < nPoints; i++ )
            {
                memcpy(pabyExtShapeBuffer + nOffset + 8 * i, &myNan, 8);
                CPL_LSBPTR64(pabyExtShapeBuffer + nOffset + 8 * i);
            }
            nOffset += 8 * nPoints;
#endif
        }
        else
        {
            memset( pabyExtShapeBuffer + nOffset, 0, 16 ); /* bbox: unused */
            nOffset += 16;
            ZOrMBufferSetter arraymSetter(pabyExtShapeBuffer + nOffset);
            GIntBig dm = 0;
            if( !ReadMArray<ZOrMBufferSetter>(arraymSetter,
                            pabyCur, pabyEnd, nPoints, dm) )
            {
                VSIFree(pabyExtShapeBuffer);
                returnError();
            }
            nOffset += 8 * nPoints;
        }
    }

    nTmp = CPL_LSBWORD32(nCurves);
    memcpy( pabyExtShapeBuffer + nOffset, &nTmp, 4 );
    nOffset += 4;
    for( i=0; i<nCurves; i++ )
    {
        // start index
        returnErrorAndCleanupIf( !ReadVarUInt32(pabyCur, pabyEnd, nTmp),
                                 VSIFree(pabyExtShapeBuffer) );
        CPL_LSBPTR32(&nTmp);
        memcpy( pabyExtShapeBuffer + nOffset, &nTmp, 4 );
        nOffset += 4;

        GUInt32 nCurveType;
        returnErrorAndCleanupIf( !ReadVarUInt32(pabyCur, pabyEnd, nCurveType),
                                 VSIFree(pabyExtShapeBuffer) );
        nTmp = CPL_LSBWORD32(nCurveType);
        memcpy( pabyExtShapeBuffer + nOffset, &nTmp, 4 );
        nOffset += 4;

        int nStructureSize = 0;
        if( nCurveType == EXT_SHAPE_SEGMENT_ARC )
            nStructureSize = 2 * 8 + 4;
        else if( nCurveType == EXT_SHAPE_SEGMENT_BEZIER )
            nStructureSize = 4 * 8;
        else if( nCurveType == EXT_SHAPE_SEGMENT_ELLIPSE )
            nStructureSize = 5 * 8 + 4;
        if( nStructureSize == 0 || pabyCur + nStructureSize > pabyEnd )
        {
            VSIFree(pabyExtShapeBuffer);
            returnError();
        }
        memcpy( pabyExtShapeBuffer + nOffset, pabyCur, nStructureSize );
        pabyCur += nStructureSize;
        nOffset += nStructureSize;
    }
    CPLAssert( nOffset <= nMaxSize );

    OGRGeometry* poRet = nullptr;
    OGRCreateFromShapeBin(pabyExtShapeBuffer, &poRet, nOffset);
    VSIFree(pabyExtShapeBuffer);
    return poRet;
}

/************************************************************************/
/*                          GetAsGeometry()                             */
/************************************************************************/

OGRGeometry* FileGDBOGRGeometryConverterImpl::GetAsGeometry(const OGRField* psField)
{
    OGRGeometry* errorRetValue = nullptr;
    GByte* pabyCur = psField->Binary.paData;
    GByte* pabyEnd = pabyCur + psField->Binary.nCount;
    GUInt32 nGeomType, i, nPoints, nParts, nCurves;
    GUIntBig x, y, z;
    GIntBig dx, dy, dz;

    ReadVarUInt32NoCheck(pabyCur, nGeomType);

    bool bHasZ = (nGeomType & EXT_SHAPE_Z_FLAG) != 0;
    bool bHasM = (nGeomType & EXT_SHAPE_M_FLAG) != 0;
    switch( (nGeomType & 0xff) )
    {
        case SHPT_NULL:
            return nullptr;

        case SHPT_POINTZ:
        case SHPT_POINTZM:
            bHasZ = true; /* go on */
            CPL_FALLTHROUGH
        case SHPT_POINT:
        case SHPT_POINTM:
        case SHPT_GENERALPOINT:
        {
            if( nGeomType == SHPT_POINTM || nGeomType == SHPT_POINTZM )
                bHasM = true;

            ReadVarUInt64NoCheck(pabyCur, x);
            ReadVarUInt64NoCheck(pabyCur, y);

            const double dfX =
                CPLUnsanitizedAdd<GUIntBig>(x, -1) / poGeomField->GetXYScale() + poGeomField->GetXOrigin();
            const double dfY =
                CPLUnsanitizedAdd<GUIntBig>(y, -1) / poGeomField->GetXYScale() + poGeomField->GetYOrigin();
            if( bHasZ )
            {
                ReadVarUInt64NoCheck(pabyCur, z);
                const double dfZScale = SanitizeScale(poGeomField->GetZScale());
                const double dfZ = CPLUnsanitizedAdd<GUIntBig>(z, -1) / dfZScale + poGeomField->GetZOrigin();
                if( bHasM )
                {
                    GUIntBig m = 0;
                    ReadVarUInt64NoCheck(pabyCur, m);
                    const double dfMScale =
                        SanitizeScale(poGeomField->GetMScale());
                    const double dfM =
                        CPLUnsanitizedAdd<GUIntBig>(m, -1) / dfMScale + poGeomField->GetMOrigin();
                    return new OGRPoint(dfX, dfY, dfZ, dfM);
                }
                return new OGRPoint(dfX, dfY, dfZ);
            }
            else if( bHasM )
            {
                OGRPoint* poPoint = new OGRPoint(dfX, dfY);
                GUIntBig m = 0;
                ReadVarUInt64NoCheck(pabyCur, m);
                const double dfMScale = SanitizeScale(poGeomField->GetMScale());
                const double dfM =
                    CPLUnsanitizedAdd<GUIntBig>(m, -1) / dfMScale + poGeomField->GetMOrigin();
                poPoint->setM(dfM);
                return poPoint;
            }
            else
            {
                return new OGRPoint(dfX, dfY);
            }
            break;
        }

        case SHPT_MULTIPOINTZM:
        case SHPT_MULTIPOINTZ:
            bHasZ = true; /* go on */
            CPL_FALLTHROUGH
        case SHPT_MULTIPOINT:
        case SHPT_MULTIPOINTM:
        {
            if( nGeomType == SHPT_MULTIPOINTM || nGeomType == SHPT_MULTIPOINTZM )
                bHasM = true;

            returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nPoints) );
            if( nPoints == 0 )
            {
                OGRMultiPoint* poMP = new OGRMultiPoint();
                if( bHasZ )
                    poMP->set3D(TRUE);
                if( bHasM )
                    poMP->setMeasured(TRUE);
                return poMP;
            }

            returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd, 4) );

            dx = dy = dz = 0;

            OGRMultiPoint* poMP = new OGRMultiPoint();
            XYMultiPointSetter mpSetter(poMP);
            if( !ReadXYArray<XYMultiPointSetter>(mpSetter,
                             pabyCur, pabyEnd, nPoints, dx, dy) )
            {
                delete poMP;
                returnError();
            }

            if( bHasZ )
            {
                poMP->setCoordinateDimension(3);
                ZMultiPointSetter mpzSetter(poMP);
                if( !ReadZArray<ZMultiPointSetter>(mpzSetter,
                                pabyCur, pabyEnd, nPoints, dz) )
                {
                    delete poMP;
                    returnError();
                }
            }

            // It seems that absence of M is marked with a single byte
            // with value 66. Be more tolerant and only try to parse the M
            // array is there are at least as many remaining bytes as
            // expected points
            if( bHasM && pabyCur + nPoints <= pabyEnd )
            {
                poMP->setMeasured(TRUE);
                GIntBig dm = 0;
                MMultiPointSetter mpmSetter(poMP);
                if( !ReadMArray<MMultiPointSetter>(mpmSetter,
                                pabyCur, pabyEnd, nPoints, dm) )
                {
                    delete poMP;
                    returnError();
                }
            }

            return poMP;
            // cppcheck-suppress duplicateBreak
            break;
        }

        case SHPT_ARCZ:
        case SHPT_ARCZM:
            bHasZ = true; /* go on */
            CPL_FALLTHROUGH
        case SHPT_ARC:
        case SHPT_ARCM:
        case SHPT_GENERALPOLYLINE:
        {
            if( nGeomType == SHPT_ARCM || nGeomType == SHPT_ARCZM )
                bHasM = true;

            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts, nCurves,
                              (nGeomType & EXT_SHAPE_CURVE_FLAG) != 0,
                              false) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRLineString* poLS = new OGRLineString();
                if( bHasZ )
                    poLS->set3D(TRUE);
                if( bHasM )
                    poLS->setMeasured(TRUE);
                return poLS;
            }

            if( nCurves )
            {
                GByte* pabyCurBackup = pabyCur;
                OGRGeometry* poRet = CreateCurveGeometry(
                    SHPT_GENERALPOLYLINE,
                    nParts, nPoints, nCurves,
                    bHasZ, bHasM,
                    pabyCur, pabyEnd );
                if( poRet )
                    return poRet;
                // In case something went wrong, go on without curves
                pabyCur = pabyCurBackup;
            }

            OGRMultiLineString* poMLS = nullptr;
            FileGDBOGRLineString* poLS = nullptr;
            if( nParts > 1 )
            {
                poMLS = new OGRMultiLineString();
                if( bHasZ )
                    poMLS->setCoordinateDimension(3);
            }

            dx = dy = dz = 0;
            for(i=0;i<nParts;i++)
            {
                poLS = new FileGDBOGRLineString();
                poLS->setNumPoints(panPointCount[i], FALSE);
                if( nParts > 1 )
                    poMLS->addGeometryDirectly(poLS);

                XYLineStringSetter lsSetter(poLS->GetPoints());
                if( !ReadXYArray<XYLineStringSetter>(lsSetter,
                                 pabyCur, pabyEnd,
                                 panPointCount[i],
                                 dx, dy) )
                {
                    if( nParts > 1 )
                        delete poMLS;
                    else
                        delete poLS;
                    returnError();
                }
            }

            if( bHasZ )
            {
                for(i=0;i<nParts;i++)
                {
                    if( nParts > 1 )
                        poLS = (FileGDBOGRLineString*) poMLS->getGeometryRef(i);

                    ZLineStringSetter lszSetter(poLS);
                    if( !ReadZArray<ZLineStringSetter>(lszSetter,
                                    pabyCur, pabyEnd,
                                    panPointCount[i], dz) )
                    {
                        if( nParts > 1 )
                            delete poMLS;
                        else
                            delete poLS;
                        returnError();
                    }
                }
            }

            if( bHasM )
            {
                GIntBig dm = 0;
                for(i=0;i<nParts;i++)
                {
                    if( nParts > 1 )
                        poLS = (FileGDBOGRLineString*) poMLS->getGeometryRef(i);

                    // It seems that absence of M is marked with a single byte
                    // with value 66. Be more tolerant and only try to parse the M
                    // array is there are at least as many remaining bytes as
                    // expected points
                    if( pabyCur + panPointCount[i] > pabyEnd )
                    {
                        if( nParts > 1 )
                            poMLS->setMeasured(FALSE);
                        break;
                    }

                    MLineStringSetter lsmSetter(poLS);
                    if( !ReadMArray<MLineStringSetter>(lsmSetter,
                                    pabyCur, pabyEnd,
                                    panPointCount[i], dm) )
                    {
                        if( nParts > 1 )
                            delete poMLS;
                        else
                            delete poLS;
                        returnError();
                    }
                }
            }

            if( poMLS )
                return poMLS;
            else
                return poLS;

            break;
        }

        case SHPT_POLYGONZ:
        case SHPT_POLYGONZM:
            bHasZ = true; /* go on */
            CPL_FALLTHROUGH
        case SHPT_POLYGON:
        case SHPT_POLYGONM:
        case SHPT_GENERALPOLYGON:
        {
            if( nGeomType == SHPT_POLYGONM || nGeomType == SHPT_POLYGONZM )
                bHasM = true;

            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts, nCurves,
                              (nGeomType & EXT_SHAPE_CURVE_FLAG) != 0,
                              false) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                if( bHasZ )
                    poPoly->set3D(TRUE);
                if( bHasM )
                    poPoly->setMeasured(TRUE);
                return poPoly;
            }

            if( nCurves )
            {
                GByte* pabyCurBackup = pabyCur;
                OGRGeometry* poRet = CreateCurveGeometry(
                    SHPT_GENERALPOLYGON,
                    nParts, nPoints, nCurves,
                    bHasZ, bHasM,
                    pabyCur, pabyEnd );
                if( poRet )
                    return poRet;
                // In case something went wrong, go on without curves
                pabyCur = pabyCurBackup;
            }

            OGRLinearRing** papoRings = new OGRLinearRing*[nParts];

            dx = dy = dz = 0;
            for(i=0;i<nParts;i++)
            {
                FileGDBOGRLinearRing* poRing = new FileGDBOGRLinearRing();
                papoRings[i] = poRing;
                poRing->setNumPoints(panPointCount[i], FALSE);

                XYLineStringSetter lsSetter(poRing->GetPoints());
                if( !ReadXYArray<XYLineStringSetter>(lsSetter,
                                 pabyCur, pabyEnd,
                                 panPointCount[i],
                                 dx, dy) )
                {
                    while( true )
                    {
                        delete papoRings[i];
                        if( i == 0 )
                            break;
                        i--;
                    }
                    delete[] papoRings;
                    // For some reason things that papoRings is leaking
                    // cppcheck-suppress memleak
                    returnError();
                }
            }

            if( bHasZ )
            {
                for(i=0;i<nParts;i++)
                {
                    papoRings[i]->setCoordinateDimension(3);

                    ZLineStringSetter lszSetter(papoRings[i]);
                    if( !ReadZArray<ZLineStringSetter>(lszSetter,
                                    pabyCur, pabyEnd,
                                    panPointCount[i], dz) )
                    {
                        for(i=0;i<nParts;i++)
                            delete papoRings[i];
                        delete[] papoRings;
                        returnError();
                    }
                }
            }

            if( bHasM )
            {
                GIntBig dm = 0;
                for(i=0;i<nParts;i++)
                {
                    // It seems that absence of M is marked with a single byte
                    // with value 66. Be more tolerant and only try to parse the M
                    // array is there are at least as many remaining bytes as
                    // expected points
                    if( pabyCur + panPointCount[i] > pabyEnd )
                    {
                        while( i != 0 )
                        {
                            --i;
                            papoRings[i]->setMeasured(FALSE);
                        }
                        break;
                    }

                    papoRings[i]->setMeasured(TRUE);

                    MLineStringSetter lsmSetter(papoRings[i]);
                    if( !ReadMArray<MLineStringSetter>(lsmSetter,
                                    pabyCur, pabyEnd,
                                    panPointCount[i], dm) )
                    {
                        for(i=0;i<nParts;i++)
                            delete papoRings[i];
                        delete[] papoRings;
                        returnError();
                    }
                }
            }

            OGRGeometry* poRet = nullptr;
            if( nParts == 1 )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                poRet = poPoly;
                poPoly->addRingDirectly(papoRings[0]);
            }
            else
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
                if( bUseOrganize || !(papoRings[0]->isClockwise()) )
#endif
            {
                /* Slow method : not used by default */
                OGRPolygon** papoPolygons = new OGRPolygon*[nParts];
                for(i=0;i<nParts;i++)
                {
                    papoPolygons[i] = new OGRPolygon();
                    papoPolygons[i]->addRingDirectly(papoRings[i]);
                }
                delete[] papoRings;
                papoRings = nullptr;
                poRet = OGRGeometryFactory::organizePolygons(
                    (OGRGeometry**) papoPolygons, nParts, nullptr, nullptr );
                delete[] papoPolygons;
            }
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
            else
            {
                /* Inner rings are CCW oriented and follow immediately the outer */
                /* ring (that is CW oriented) in which they are included */
                OGRMultiPolygon* poMulti = NULL;
                OGRPolygon* poPoly = new OGRPolygon();
                OGRPolygon* poCur = poPoly;
                poRet = poCur;
                /* We have already checked that the first ring is CW */
                poPoly->addRingDirectly(papoRings[0]);
                OGREnvelope sEnvelope;
                papoRings[0]->getEnvelope(&sEnvelope);
                for(i=1;i<nParts;i++)
                {
                    int bIsCW = papoRings[i]->isClockwise();
                    if( bIsCW )
                    {
                        if( poMulti == NULL )
                        {
                            poMulti = new OGRMultiPolygon();
                            poRet = poMulti;
                            poMulti->addGeometryDirectly(poCur);
                        }
                        OGRPolygon* poPoly = new OGRPolygon();
                        poCur = poPoly;
                        poMulti->addGeometryDirectly(poCur);
                        poPoly->addRingDirectly(papoRings[i]);
                        papoRings[i]->getEnvelope(&sEnvelope);
                    }
                    else
                    {
                        poCur->addRingDirectly(papoRings[i]);
                        OGRPoint oPoint;
                        papoRings[i]->getPoint(0, &oPoint);
                        CPLAssert(oPoint.getX() >= sEnvelope.MinX &&
                                  oPoint.getX() <= sEnvelope.MaxX &&
                                  oPoint.getY() >= sEnvelope.MinY &&
                                  oPoint.getY() <= sEnvelope.MaxY);
                    }
                }
            }
#endif

            delete[] papoRings;
            return poRet;
            // cppcheck-suppress duplicateBreak
            break;
        }

        case SHPT_MULTIPATCHM:
        case SHPT_MULTIPATCH:
            bHasZ = true; /* go on */
            CPL_FALLTHROUGH
        case SHPT_GENERALMULTIPATCH:
        {
            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts, nCurves, false, true ) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                if( bHasZ )
                    poPoly->setCoordinateDimension(3);
                return poPoly;
            }
            int* panPartType = (int*) VSI_MALLOC_VERBOSE(sizeof(int) * nParts);
            int* panPartStart = (int*) VSI_MALLOC_VERBOSE(sizeof(int) * nParts);
            double* padfXYZ =  (double*) VSI_MALLOC_VERBOSE(3 * sizeof(double) * nPoints);
            double* padfX = padfXYZ;
            double* padfY = padfXYZ ? padfXYZ + nPoints : nullptr;
            double* padfZ = padfXYZ ? padfXYZ + 2 * nPoints : nullptr;
            if( panPartType == nullptr || panPartStart == nullptr || padfXYZ == nullptr  )
            {
                VSIFree(panPartType);
                VSIFree(panPartStart);
                VSIFree(padfXYZ);
                returnError();
            }
            for(i=0;i<nParts;i++)
            {
                GUInt32 nPartType;
                if( !ReadVarUInt32(pabyCur, pabyEnd, nPartType) )
                {
                    VSIFree(panPartType);
                    VSIFree(panPartStart);
                    VSIFree(padfXYZ);
                    returnError();
                }
                panPartType[i] = (int)nPartType;
            }
            dx = dy = dz = 0;

            XYArraySetter arraySetter(padfX, padfY);
            if( !ReadXYArray<XYArraySetter>(arraySetter,
                             pabyCur, pabyEnd, nPoints, dx, dy) )
            {
                VSIFree(panPartType);
                VSIFree(panPartStart);
                VSIFree(padfXYZ);
                returnError();
            }

            if( bHasZ )
            {
                FileGDBArraySetter arrayzSetter(padfZ);
                if( !ReadZArray<FileGDBArraySetter>(arrayzSetter,
                                pabyCur, pabyEnd, nPoints, dz) )
                {
                    VSIFree(panPartType);
                    VSIFree(panPartStart);
                    VSIFree(padfXYZ);
                    returnError();
                }
            }
            else
            {
                memset(padfZ, 0, nPoints * sizeof(double));
            }

            panPartStart[0] = 0;
            for( i = 1; i < nParts; ++i )
                panPartStart[i] = panPartStart[i-1] + panPointCount[i-1];
            // (CID 1404102)
            // coverity[overrun-buffer-arg]
            OGRGeometry* poRet = OGRCreateFromMultiPatch(
                                            static_cast<int>(nParts),
                                            panPartStart,
                                            panPartType,
                                            static_cast<int>(nPoints),
                                            padfX, padfY, padfZ );

            VSIFree(panPartType);
            VSIFree(panPartStart);
            VSIFree(padfXYZ);

            return poRet;
            // cppcheck-suppress duplicateBreak
            break;
        }

        default:
            CPLDebug("OpenFileGDB", "Unhandled geometry type = %d", (int)nGeomType);
            break;
/*
#define SHPT_GENERALMULTIPOINT  53
*/
    }
    return nullptr;
}

/************************************************************************/
/*                           BuildConverter()                           */
/************************************************************************/

FileGDBOGRGeometryConverter* FileGDBOGRGeometryConverter::BuildConverter(
                                        const FileGDBGeomField* poGeomField)
{
    return new FileGDBOGRGeometryConverterImpl(poGeomField);
}

/************************************************************************/
/*                      GetGeometryTypeFromESRI()                       */
/************************************************************************/

static const struct
{
    const char          *pszStr;
    OGRwkbGeometryType   eType;
} AssocESRIGeomTypeToOGRGeomType[] =
{
    { "esriGeometryPoint", wkbPoint },
    { "esriGeometryMultipoint", wkbMultiPoint },
    { "esriGeometryLine", wkbMultiLineString },
    { "esriGeometryPolyline", wkbMultiLineString },
    { "esriGeometryPolygon", wkbMultiPolygon },
    { "esriGeometryMultiPatch", wkbUnknown }
};

OGRwkbGeometryType FileGDBOGRGeometryConverter::GetGeometryTypeFromESRI(
                                                    const char* pszESRIType)
{
    for(size_t i=0;i<sizeof(AssocESRIGeomTypeToOGRGeomType)/
                     sizeof(AssocESRIGeomTypeToOGRGeomType[0]);i++)
    {
        if( strcmp(pszESRIType, AssocESRIGeomTypeToOGRGeomType[i].pszStr) == 0 )
            return AssocESRIGeomTypeToOGRGeomType[i].eType;
    }
    CPLDebug("OpenFileGDB", "Unhandled geometry type : %s", pszESRIType);
    return wkbUnknown;
}

} /* namespace OpenFileGDB */
