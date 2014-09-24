/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
 * Author:   Even Rouault, <even dot rouault at mines-dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "filegdbtable_priv.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "ogrpgeogeometry.h" /* SHPT_ constants and OGRCreateFromMultiPatchPart() */

CPL_CVSID("$Id");

#define TEST_BIT(ar, bit)                       (ar[(bit) / 8] & (1 << ((bit) % 8)))
#define BIT_ARRAY_SIZE_IN_BYTES(bitsize)        (((bitsize)+7)/8)

#define UUID_SIZE_IN_BYTES              16

#define IS_VALID_LAYER_GEOM_TYPE(byVal)         ((byVal) <= FGTGT_POLYGON || (byVal) == FGTGT_MULTIPATCH)

/* Reserve one extra byte in case the last field is a string */
/*       or 2 for 2 ReadVarIntAndAddNoCheck() in a row */
/*       or 4 for SkipVarUInt() with nIter = 4 */
/*       or for 4 ReadVarUInt64NoCheck */
#define ZEROES_AFTER_END_OF_BUFFER      4

namespace OpenFileGDB
{

/************************************************************************/
/*                      FileGDBTablePrintError()                        */
/************************************************************************/

void FileGDBTablePrintError(const char* pszFile, int nLineNumber)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Error occured in %s at line %d",
             pszFile, nLineNumber);
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
    fpTable = NULL;
    fpTableX = NULL;
    memset(&sCurField, 0, sizeof(sCurField));
    bError = FALSE;
    nCurRow = -1;
    nLastCol = -1;
    pabyIterVals = NULL;
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
    pabyBuffer = NULL;
    nFilterXMin = 0;
    nFilterXMax = 0;
    nFilterYMin = 0;
    nFilterYMax = 0;
    osObjectIdColName = "";
    nChSaved = -1;
    pabyTablXBlockMap = NULL;
    bHasReadGDBIndexes = FALSE;
    nOffsetFieldDesc = 0;
    nFieldDescLength = 0;
    nTablxOffsetSize = 0;
    anFeatureOffsets.resize(0);
    nOffsetHeaderEnd = 0;
}

/************************************************************************/
/*                                Close()                               */
/************************************************************************/

void FileGDBTable::Close()
{
    if( fpTable )
        VSIFCloseL(fpTable);
    fpTable = NULL;

    if( fpTableX )
        VSIFCloseL(fpTableX);
    fpTableX = NULL;

    CPLFree(pabyBuffer);
    pabyBuffer = NULL;

    for(size_t i=0;i<apoFields.size();i++)
        delete apoFields[i];
    apoFields.resize(0);

    CPLFree(pabyTablXBlockMap);
    pabyTablXBlockMap = NULL;

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

template < class OutType, class ControleType >
static int ReadVarUInt(GByte*& pabyIter, GByte* pabyEnd, OutType& nOutVal)
{
    const int errorRetValue = FALSE;
    if( !(ControleType::check_bounds) )
    {
        /* nothing */
    }
    else if( ControleType::verbose_error )
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
    while(TRUE)
    {
        if( !(ControleType::check_bounds) )
        {
            /* nothing */
        }
        else if( ControleType::verbose_error )
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
    }
}

struct ControleTypeVerboseErrorTrue
{
    static const bool check_bounds = true;
    static const bool verbose_error = true;
};

struct ControleTypeVerboseErrorFalse
{
    static const bool check_bounds = true;
    static const bool verbose_error = false;
};

struct ControleTypeNone
{
    static const bool check_bounds = false;
    static const bool verbose_error = false;
};

static int ReadVarUInt32(GByte*& pabyIter, GByte* pabyEnd, GUInt32& nOutVal)
{
    return ReadVarUInt<GUInt32, ControleTypeVerboseErrorTrue>(pabyIter, pabyEnd, nOutVal);
}

static void ReadVarUInt32NoCheck(GByte*& pabyIter, GUInt32& nOutVal)
{
    GByte* pabyEnd = NULL;
    ReadVarUInt<GUInt32, ControleTypeNone>(pabyIter, pabyEnd, nOutVal);
}

static int ReadVarUInt32Silent(GByte*& pabyIter, GByte* pabyEnd, GUInt32& nOutVal)
{
    return ReadVarUInt<GUInt32, ControleTypeVerboseErrorFalse>(pabyIter, pabyEnd, nOutVal);
}

static void ReadVarUInt64NoCheck(GByte*& pabyIter, GUIntBig& nOutVal)
{
    GByte* pabyEnd = NULL;
    ReadVarUInt<GUIntBig, ControleTypeNone>(pabyIter, pabyEnd, nOutVal);
}

/************************************************************************/
/*                      IsLikelyFeatureAtOffset()                       */
/************************************************************************/

int FileGDBTable::IsLikelyFeatureAtOffset(vsi_l_offset nFileSize,
                                          vsi_l_offset nOffset,
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
        GByte* pabyNewBuffer = (GByte*) VSIRealloc( pabyBuffer,
                                nRowBlobLength + ZEROES_AFTER_END_OF_BUFFER );
        if( pabyNewBuffer == NULL )
            return FALSE;

        pabyBuffer = pabyNewBuffer;
        nBufferMaxSize = nRowBlobLength;
    }
    if( pabyBuffer == NULL ) return FALSE; /* to please Coverity. Not needed */
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
                CPLAssert(FALSE);
                break;
        }
    }
    if( !bExactSizeKnown )
    {
        if( nRowBlobLength < nRequiredLength )
            return FALSE;
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
                    nRequiredLength = pabyIter - pabyBuffer;
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
                    nRequiredLength = pabyIter - pabyBuffer;
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
                    CPLAssert(FALSE);
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

int FileGDBTable::GuessFeatureLocations()
{
    vsi_l_offset nFileSize;
    VSIFSeekL(fpTable, 0, SEEK_END);
    nFileSize = VSIFTellL(fpTable);

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
        if( nSize < 0 && -nSize < 1024 * 1024 &&
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
        if( !IsLikelyFeatureAtOffset(nFileSize, nOffset, &nSize, &bDeletedRecord) )
        {
            nOffset ++;
        }
        else
        {
            /*CPLDebug("OpenFileGDB", "Feature found at offset %d (size = %d)",
                     nOffset, nSize);*/
            if( bDeletedRecord )
            {
                nInvalidRecords ++;
                anFeatureOffsets.push_back(0);
            }
            else
                anFeatureOffsets.push_back(nOffset);
            nOffset += nSize;
        }
    }
    nTotalRecordCount = (int) anFeatureOffsets.size();
    if( nTotalRecordCount - nInvalidRecords > nValidRecordCount )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "More features found (%d) than declared number of valid features (%d). "
                 "So deleted features will likely be reported.",
                 nTotalRecordCount - nInvalidRecords, nValidRecordCount);
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
            pabyTablXBlockMap = (GByte*) VSIMalloc( nSizeInBytes );
            returnErrorIf(pabyTablXBlockMap == NULL );
            returnErrorIf(VSIFReadL( pabyTablXBlockMap, nSizeInBytes, 1, fpTableX ) != 1 );
            /* returnErrorIf(nMagic2 == 0 ); */

            // Check that the map is consistant with n1024Blocks
            GUInt32 nCountBlocks = 0;
            for(GUInt32 i=0;i<nBitsForBlockMap;i++)
                nCountBlocks += TEST_BIT(pabyTablXBlockMap, i) != 0;
            returnErrorIf(nCountBlocks != n1024Blocks );
        }
    }
    return TRUE;
}

/************************************************************************/
/*                                 Open()                               */
/************************************************************************/

int FileGDBTable::Open(const char* pszFilename)
{
    const int errorRetValue = FALSE;
    CPLAssert(fpTable == NULL);

    osFilename = pszFilename;

    fpTable = VSIFOpenL( pszFilename, "rb" );
    returnErrorIf(fpTable == NULL );

    // Read .gdtable file header
    GByte abyHeader[40];
    returnErrorIf(VSIFReadL( abyHeader, 40, 1, fpTable ) != 1 );
    nValidRecordCount = GetInt32(abyHeader + 4, 0);
    returnErrorIf(nValidRecordCount < 0 );

    if( nValidRecordCount > 0 &&
        !CSLTestBoolean(CPLGetConfigOption("OPENFILEGDB_IGNORE_GDBTABLX", "FALSE")) )
    {
        const char* pszTableXName = CPLFormFilename(CPLGetPath(pszFilename),
                                        CPLGetBasename(pszFilename), "gdbtablx");
        fpTableX = VSIFOpenL( pszTableXName, "rb" );
        if( fpTableX == NULL )
        {
            const char* pszIgnoreGDBTablXAbsence =
                CPLGetConfigOption("OPENFILEGDB_IGNORE_GDBTABLX_ABSENCE", NULL);
            if( pszIgnoreGDBTablXAbsence == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "%s could not be found. "
                        "Trying to guess feature locations, but this might fail or "
                        "return incorrect results", pszTableXName);
            }
            else if( !CSLTestBoolean(pszIgnoreGDBTablXAbsence) )
            {
                returnErrorIf(fpTableX == NULL );
            }
        }
        else if( !ReadTableXHeader() )
            return FALSE;
    }

    if( fpTableX != NULL )
    {
        returnErrorIf(nValidRecordCount > nTotalRecordCount );

#ifdef DEBUG_VERBOSE
        if( nTotalRecordCount != nValidRecordCount )
        {
            CPLDebug("OpenFileGDB", "%s: nTotalRecordCount=%d nValidRecordCount=%d",
                    pszFilename,
                    nTotalRecordCount, nValidRecordCount);
        }
#endif
    }

    nOffsetFieldDesc = GetUInt32(abyHeader + 32, 0);

#ifdef DEBUG_VERBOSE
    if( nOffsetFieldDesc != 40 )
    {
        CPLDebug("OpenFileGDB", "%s: nOffsetFieldDesc=%d",
                 pszFilename, nOffsetFieldDesc);
    }
#endif

    // Skip to field description section
    VSIFSeekL( fpTable, nOffsetFieldDesc, SEEK_SET );
    returnErrorIf(VSIFReadL( abyHeader, 14, 1, fpTable ) != 1 );
    nFieldDescLength = GetUInt32(abyHeader, 0);

    nOffsetHeaderEnd = nOffsetFieldDesc + nFieldDescLength;

    returnErrorIf(nFieldDescLength > 10 * 1024 * 1024 || nFieldDescLength < 10 );
    GByte byTableGeomType = abyHeader[8];
    if( IS_VALID_LAYER_GEOM_TYPE(byTableGeomType) )
        eTableGeomType = (FileGDBTableGeometryType) byTableGeomType;
    else
        CPLDebug("OpenFileGDB", "Unknown table geometry type: %d", byTableGeomType);
    GUInt16 iField, nFields;
    nFields = GetUInt16(abyHeader + 12, 0);

    /* No interest in guessing a trivial file */
    returnErrorIf( fpTableX == NULL && nFields == 0) ;

    GUInt32 nRemaining = nFieldDescLength - 10;
    nBufferMaxSize = nRemaining;
    pabyBuffer = (GByte*)VSIMalloc(nBufferMaxSize + ZEROES_AFTER_END_OF_BUFFER);
    returnErrorIf(pabyBuffer == NULL );
    returnErrorIf(VSIFReadL(pabyBuffer, nRemaining, 1, fpTable) != 1 );

    GByte* pabyIter = pabyBuffer;
    for(iField = 0; iField < nFields; iField ++)
    {
        returnErrorIf(nRemaining < 1 );
        GByte nCarCount = pabyIter[0];

        pabyIter ++;
        nRemaining --;
        returnErrorIf(nRemaining < (GUInt32)(2 * nCarCount + 1) );
        std::string osName;
        for(int j=0;j<nCarCount;j++) //FIXME? UTF16
            osName += pabyIter[2 * j];
        pabyIter += 2 * nCarCount;
        nRemaining -= 2 * nCarCount;

        returnErrorIf(nRemaining < 1 );
        nCarCount = pabyIter[0];
        pabyIter ++;
        nRemaining --;
        returnErrorIf(nRemaining < (GUInt32)(2 * nCarCount + 1) );
        std::string osAlias;
        for(int j=0;j<nCarCount;j++) //FIXME? UTF16
            osAlias += pabyIter[2 * j];
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
                    nRemaining -= (pabyIter - pabyIterBefore);
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

            if( (flags & 4) != 0 )
            {
                /* Default value */
                /* Found on PreNIS.gdb/a0000000d.gdbtable */
                returnErrorIf(nRemaining < defaultValueLength );
                pabyIter += defaultValueLength;
                nRemaining -= defaultValueLength;
            }

            if( eType == FGFT_OBJECTID )
            {
                returnErrorIf(osObjectIdColName.size() > 0 );
                osObjectIdColName = osName;
                continue;
            }

            FileGDBField* poField = new FileGDBField(this);
            poField->osName = osName;
            poField->osAlias = osAlias;
            poField->eType = eType;
            poField->bNullable = (flags & 1);
            poField->nMaxWidth = nMaxWidth;
            apoFields.push_back(poField);
        }
        else
        {

            FileGDBRasterField* poRasterField = NULL;
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
                std::string osRasterColumn;
                for(int j=0;j<nCarCount;j++) //FIXME? UTF16
                    osRasterColumn += pabyIter[2 * j];
                pabyIter += 2 * nCarCount;
                nRemaining -= 2 * nCarCount;
                poRasterField->osRasterColumnName = osRasterColumn;
            }

            returnErrorIf(nRemaining < 2 );
            GUInt16 nLengthWKT = GetUInt16(pabyIter, 0);
            pabyIter += sizeof(nLengthWKT);
            nRemaining -= sizeof(nLengthWKT);

            returnErrorIf(nRemaining < (GUInt32)(1 + nLengthWKT) );
            for(int j=0;j<nLengthWKT/2;j++) //FIXME? UTF16
                poField->osWKT += pabyIter[2 * j];
            pabyIter += nLengthWKT;
            nRemaining -= nLengthWKT;

            GByte abyGeomFlags = pabyIter[0];
            pabyIter ++;
            nRemaining --;
            poField->bHasM = (abyGeomFlags & 2) != 0;
            poField->bHasZ = (abyGeomFlags & 4) != 0;
            returnErrorIf(
                    nRemaining < (GUInt32)(sizeof(double) * ( 8 + (poField->bHasM + poField->bHasZ) * 3 )) );

#define READ_DOUBLE(field) do { \
    field = GetFloat64(pabyIter, 0); \
    pabyIter += sizeof(double); \
    nRemaining -= sizeof(double); } while(0)

            READ_DOUBLE(poField->dfXOrigin);
            READ_DOUBLE(poField->dfYOrigin);
            READ_DOUBLE(poField->dfXYScale);

            if( poField->bHasM )
            {
                READ_DOUBLE(poField->dfMOrigin);
                READ_DOUBLE(poField->dfMScale);
            }

            if( poField->bHasZ )
            {
                READ_DOUBLE(poField->dfZOrigin);
                READ_DOUBLE(poField->dfZScale);
            }

            READ_DOUBLE(poField->dfXYTolerance);

            if( poField->bHasM )
            {
                READ_DOUBLE(poField->dfMTolerance);
            }

            if( poField->bHasZ )
            {
                READ_DOUBLE(poField->dfZTolerance);
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
                READ_DOUBLE(poField->dfXMin);
                READ_DOUBLE(poField->dfYMin);
                READ_DOUBLE(poField->dfXMax);
                READ_DOUBLE(poField->dfYMax);

                /* Purely empiric logic ! */
                while( TRUE )
                {
                    returnErrorIf(nRemaining < 5 );

                    if( pabyIter[0] == 0x00 && pabyIter[1] >= 1 && pabyIter[1] <= 3 &&
                        pabyIter[2] == 0x00 && pabyIter[3] == 0x00 && pabyIter[4] == 0x00 )
                    {
                        GByte nToSkip = pabyIter[1];
                        pabyIter += 5;
                        nRemaining -= 5;
                        returnErrorIf(nRemaining < (GUInt32)(nToSkip * 8) );
                        pabyIter += nToSkip * 8;
                        nRemaining -= nToSkip * 8;
                        break;
                    }
                    else
                    {
                        returnErrorIf(nRemaining < 8 );
                        pabyIter += 8;
                        nRemaining -= 8;
                    }
                }
            }
        }

        nCountNullableFields += apoFields[apoFields.size()-1]->bNullable;
    }
    nNullableFieldsSizeInBytes = BIT_ARRAY_SIZE_IN_BYTES(nCountNullableFields);

#ifdef DEBUG_VERBOSE
    if( nRemaining > 0 )
    {
        CPLDebug("OpenFileGDB", "%u remaining (ignored) bytes in field header section",
                 nRemaining);
    }
#endif

    if( nValidRecordCount > 0 && fpTableX == NULL )
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
        while(TRUE)
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

static void ReadVarIntAndAddNoCheck(GByte*& pabyIter, GIntBig& nOutVal)
{
    GUInt32 b;

    b = *pabyIter;
    GUIntBig nVal = (b & 0x3F);
    int nSign = 1;
    if( (b & 0x40) != 0 )
        nSign = -1;
    if( (b & 0x80) == 0 )
    {
        pabyIter ++;
        nOutVal += nVal * nSign;
        return;
    }

    GByte* pabyLocalIter = pabyIter + 1;
    int nShift = 6;
    while(TRUE)
    {
        GUIntBig b = *pabyLocalIter;
        pabyLocalIter ++;
        nVal |= ( b & 0x7F ) << nShift;
        if( (b & 0x80) == 0 )
        {
            pabyIter = pabyLocalIter;
            nOutVal += nVal * nSign;
            return;
        }
        nShift += 7;
    }
}

/************************************************************************/
/*                       GetOffsetInTableForRow()                       */
/************************************************************************/

vsi_l_offset FileGDBTable::GetOffsetInTableForRow(int iRow)
{
    const int errorRetValue = 0;
    returnErrorIf(iRow < 0 || iRow >= nTotalRecordCount );

    if( fpTableX == NULL )
        return anFeatureOffsets[iRow];

    if( pabyTablXBlockMap != NULL )
    {
        GUInt32 nCountBlocksBefore = 0;
        int iBlock = iRow / 1024;

        // Check if the block is not empty
        if( TEST_BIT(pabyTablXBlockMap, iBlock) == 0 )
            return 0;

        for(int i=0;i<iBlock;i++)
            nCountBlocksBefore += TEST_BIT(pabyTablXBlockMap, i) != 0;
        int iCorrectedRow = nCountBlocksBefore * 1024 + (iRow % 1024);
        VSIFSeekL(fpTableX, 16 + nTablxOffsetSize * iCorrectedRow, SEEK_SET);
    }
    else
    {
        VSIFSeekL(fpTableX, 16 + nTablxOffsetSize * iRow, SEEK_SET);
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
        CPLDebug("OpenFileGDB", "%s: first feature offset = " CPL_FRMT_GUIB ". Expected %d",
                 osFilename.c_str(), nOffset, nOffsetHeaderEnd);
#endif

    return nOffset;
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
        if( !(apoFields.size() == 0 && nRowBlobLength == 0) )
        {
            /* CPLDebug("OpenFileGDB", "nRowBlobLength = %u", nRowBlobLength); */
            returnErrorAndCleanupIf(
                    nRowBlobLength < (GUInt32)nNullableFieldsSizeInBytes ||
                    nRowBlobLength > INT_MAX - ZEROES_AFTER_END_OF_BUFFER, nCurRow = -1 );

            if( nRowBlobLength > nBufferMaxSize )
            {
                GByte* pabyNewBuffer = (GByte*) VSIRealloc( pabyBuffer,
                                nRowBlobLength + ZEROES_AFTER_END_OF_BUFFER );
                returnErrorAndCleanupIf(pabyNewBuffer == NULL, nCurRow = -1 );

                pabyBuffer = pabyNewBuffer;
                nBufferMaxSize = nRowBlobLength;
            }
            returnErrorAndCleanupIf(
                VSIFReadL(pabyBuffer, nRowBlobLength, 1, fpTable) != 1, nCurRow = -1 );
            /* Protection for 4 ReadVarUInt64NoCheck */
            CPLAssert(ZEROES_AFTER_END_OF_BUFFER == 4);
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
    struct tm brokendowntime;

    /* 25569 = Number of days between 1899/12/30 00:00:00 and 1970/01/01 00:00:00 */
    CPLUnixTimeToYMDHMS((GIntBig)((dfVal - 25569) * 3600 * 24), &brokendowntime);

    psField->Date.Year = (GInt16)(brokendowntime.tm_year + 1900);
    psField->Date.Month = (GByte)brokendowntime.tm_mon + 1;
    psField->Date.Day = (GByte)brokendowntime.tm_mday;
    psField->Date.Hour = (GByte)brokendowntime.tm_hour;
    psField->Date.Minute = (GByte)brokendowntime.tm_min;
    psField->Date.Second = (GByte)brokendowntime.tm_sec;
    psField->Date.TZFlag = 0;

    return TRUE;
}

/************************************************************************/
/*                          GetFieldValue()                             */
/************************************************************************/

const OGRField* FileGDBTable::GetFieldValue(int iCol)
{
    const OGRField* errorRetValue = NULL;

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

        GUInt32 nLength;
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
                nLength = 0;
                CPLAssert(FALSE);
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
            return NULL;
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
            double dfVal = GetFloat64(pabyIterVals, 0);

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
            sCurField.Set.nMarker1 = OGRUnsetMarker;
            sCurField.Set.nMarker2 = OGRUnsetMarker;

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
            sprintf(achGUIDBuffer,
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
            CPLAssert(FALSE);
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
    if( fpIndexes == NULL )
    {
        if ( VSIStatExL( pszIndexesName, &sStat, VSI_STAT_EXISTS_FLAG) == 0 )
            returnError();
        else
            return 0;
    }

    VSIFSeekL(fpIndexes, 0, SEEK_END);
    vsi_l_offset nFileSize = VSIFTellL(fpIndexes);
    returnErrorAndCleanupIf(nFileSize > 1024 * 1024, VSIFCloseL(fpIndexes) );

    GByte* pabyIdx = (GByte*)VSIMalloc((size_t)nFileSize);
    returnErrorAndCleanupIf(pabyIdx == NULL, VSIFCloseL(fpIndexes) );

    VSIFSeekL(fpIndexes, 0, SEEK_SET);
    int nRead = (int)VSIFReadL( pabyIdx, (size_t)nFileSize, 1, fpIndexes );
    VSIFCloseL(fpIndexes);
    returnErrorAndCleanupIf(nRead != 1, VSIFree(pabyIdx) );

    GByte* pabyCur = pabyIdx;
    GByte* pabyEnd = pabyIdx + nFileSize;
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
        std::string osIndexName;
        GUInt32 j;
        for(j=0;j<nIdxNameCarCount;j++) //FIXME? UTF16
            osIndexName += pabyCur[2 * j];
        pabyCur += 2 * nIdxNameCarCount;

        // Skip magic fields
        pabyCur += 2 + 4 + 2 + 4;

        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < sizeof(GUInt32), VSIFree(pabyIdx) );
        GUInt32 nColNameCarCount = GetUInt32(pabyCur, 0);
        pabyCur += sizeof(GUInt32);
        returnErrorAndCleanupIf(nColNameCarCount > 1024, VSIFree(pabyIdx) );
        returnErrorAndCleanupIf((GUInt32)(pabyEnd - pabyCur) < 2 * nColNameCarCount, VSIFree(pabyIdx) );
        std::string osFieldName;
        for(j=0;j<nColNameCarCount;j++) //FIXME? UTF16
            osFieldName += pabyCur[2 * j];
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
                if( apoFields[nFieldIdx]->poIndex != NULL )
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
/*                       InstallFilterEnvelope()                        */
/************************************************************************/

#define MAX_GUINTBIG    (~((GUIntBig)0))

void FileGDBTable::InstallFilterEnvelope(const OGREnvelope* psFilterEnvelope)
{
    if( psFilterEnvelope != NULL )
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
                                        MAX_GUINTBIG / poGeomField->dfXYScale )
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
                                        MAX_GUINTBIG / poGeomField->dfXYScale )
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
        {
            GUIntBig x, y;
            ReadVarUInt64NoCheck(pabyCur, x);
            x --;
            ReadVarUInt64NoCheck(pabyCur, y);
            y --;
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
            nToSkip = 1 + ((nGeomType & 0x20000000) ? 1 : 0);
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
    psOutFeatureEnvelope->MaxX = (vxmin + vdx) / poGeomField->dfXYScale + poGeomField->dfXOrigin;
    psOutFeatureEnvelope->MaxY = (vymin + vdy) / poGeomField->dfXYScale + poGeomField->dfYOrigin;

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
        {
            GUIntBig x, y;
            ReadVarUInt64NoCheck(pabyCur, x);
            x --;
            if( x < nFilterXMin || x > nFilterXMax )
                return FALSE;
            ReadVarUInt64NoCheck(pabyCur, y);
            y --;
            return( y >= nFilterYMin && y <= nFilterYMax );
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
            nToSkip = 1 + ((nGeomType & 0x20000000) ? 1 : 0);
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
    if( vxmin + vdx < nFilterXMin )
        return FALSE;
    ReadVarUInt64NoCheck(pabyCur, vdy);
    return vymin + vdy >= nFilterYMin;
}

/************************************************************************/
/*                           FileGDBField()                             */
/************************************************************************/

FileGDBField::FileGDBField(FileGDBTable* poParent) :
    poParent(poParent), eType(FGFT_UNDEFINED), bNullable(FALSE),
    nMaxWidth(0), poIndex(NULL)
{
}

/************************************************************************/
/*                          ~FileGDBField()                             */
/************************************************************************/

FileGDBField::~FileGDBField()
{
}


/************************************************************************/
/*                            HasIndex()                                */
/************************************************************************/

int FileGDBField::HasIndex()
{
     poParent->GetIndexCount();
     return poIndex != NULL;
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

FileGDBGeomField::FileGDBGeomField(FileGDBTable* poParent) :
    FileGDBField(poParent), bHasZ(FALSE), bHasM(FALSE),
    dfXOrigin(0.0), dfYOrigin(0.0), dfXYScale(0.0), dfMOrigin(0.0),
    dfMScale(0.0), dfZOrigin(0.0), dfZScale(0.0), dfXYTolerance(0.0),
    dfMTolerance(0.0), dfZTolerance(0.0), dfXMin(0.0), dfYMin(0.0),
    dfXMax(0.0), dfYMax(0.0)
{
}

/************************************************************************/
/*                      FileGDBOGRGeometryConverterImpl                 */
/************************************************************************/

class FileGDBOGRGeometryConverterImpl : public FileGDBOGRGeometryConverter
{
        const FileGDBGeomField      *poGeomField;
        GUInt32                     *panPointCount;
        GUInt32                      nPointCountMax;
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
        int                          bUseOrganize;
#endif

        int                         ReadPartDefs( GByte*& pabyCur,
                                                  GByte* pabyEnd,
                                                  GUInt32& nPoints,
                                                  GUInt32& nParts,
                                                  int bHasCurveDesc,
                                                  int bIsMultiPatch );
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

    public:
                                        FileGDBOGRGeometryConverterImpl(
                                            const FileGDBGeomField* poGeomField);
       virtual                         ~FileGDBOGRGeometryConverterImpl();

       virtual OGRGeometry*             GetAsGeometry(const OGRField* psField);
};

/************************************************************************/
/*                  FileGDBOGRGeometryConverterImpl()                   */
/************************************************************************/

FileGDBOGRGeometryConverterImpl::FileGDBOGRGeometryConverterImpl(
                                    const FileGDBGeomField* poGeomField) :
                                                poGeomField(poGeomField)
{
    panPointCount = NULL;
    nPointCountMax = 0;
#ifdef ASSUME_INNER_RINGS_IMMEDIATELY_AFTER_OUTER_RING
    bUseOrganize = CPLGetConfigOption("OGR_ORGANIZE_POLYGONS", NULL) != NULL;
#endif
}

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

int FileGDBOGRGeometryConverterImpl::ReadPartDefs( GByte*& pabyCur,
                                GByte* pabyEnd,
                                GUInt32& nPoints,
                                GUInt32& nParts,
                                int bHasCurveDesc,
                                int bIsMultiPatch )
{
    const int errorRetValue = FALSE;
    returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nPoints));
    if( nPoints == 0 )
    {
        nParts = 0;
        return TRUE;
    }
    returnErrorIf(nPoints > (GUInt32)(pabyEnd - pabyCur) );
    if( bIsMultiPatch )
        returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd) );
    returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nParts));
    returnErrorIf(nParts > (GUInt32)(pabyEnd - pabyCur));
    if( bHasCurveDesc )
        returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd) );
    if( nParts == 0 )
        return TRUE;
    GUInt32 i;
    returnErrorIf(!SkipVarUInt(pabyCur, pabyEnd, 4) );
    if( nParts > nPointCountMax )
    {
        GUInt32* panPointCountNew =
            (GUInt32*) VSIRealloc( panPointCount, nParts * sizeof(GUInt32) );
        returnErrorIf(panPointCountNew == NULL );
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

    return TRUE;
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
        XYLineStringSetter(OGRRawPoint* paoPoints) : paoPoints(paoPoints) {}

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
        XYMultiPointSetter(OGRMultiPoint* poMPoint) : poMPoint(poMPoint) {}

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
        XYArraySetter(double* padfX, double* padfY) : padfX(padfX), padfY(padfY) {}

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
        ZLineStringSetter(OGRLineString* poLS) : poLS(poLS) {}

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
        ZMultiPointSetter(OGRMultiPoint* poMPoint) : poMPoint(poMPoint) {}

        void set(int i, double dfZ)
        {
            ((OGRPoint*)poMPoint->getGeometryRef(i))->setZ(dfZ);
        }
};

/************************************************************************/
/*                             ZArraySetter                            */
/************************************************************************/

class ZArraySetter
{
        double* padfZ;
    public:
        ZArraySetter(double* padfZ) : padfZ(padfZ) {}

        void set(int i, double dfZ)
        {
            padfZ[i] = dfZ;
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
    for(GUInt32 i = 0; i < nPoints; i++ )
    {
        returnErrorIf(pabyCur >= pabyEnd);
        ReadVarIntAndAddNoCheck(pabyCur, dz);

        double dfZ = dz / poGeomField->GetZScale() + poGeomField->GetZOrigin();
        setter.set(i, dfZ);
    }
    return TRUE;
}

/************************************************************************/
/*                          GetAsGeometry()                             */
/************************************************************************/

OGRGeometry* FileGDBOGRGeometryConverterImpl::GetAsGeometry(const OGRField* psField)
{
    OGRGeometry* errorRetValue = NULL;
    GByte* pabyCur = psField->Binary.paData;
    GByte* pabyEnd = pabyCur + psField->Binary.nCount;
    GUInt32 nGeomType, i, nPoints, nParts;
    GUIntBig x, y, z;
    GIntBig dx, dy, dz;

    ReadVarUInt32NoCheck(pabyCur, nGeomType);

    int bHasZ = (nGeomType & 0x80000000) != 0;
    switch( (nGeomType & 0xff) )
    {
        case SHPT_NULL:
            return NULL;

        case SHPT_POINTZ:
        case SHPT_POINTZM:
            bHasZ = TRUE; /* go on */
        case SHPT_POINT:
        case SHPT_POINTM:
        {
            double dfX, dfY, dfZ;
            ReadVarUInt64NoCheck(pabyCur, x);
            ReadVarUInt64NoCheck(pabyCur, y);

            dfX = (x - 1) / poGeomField->GetXYScale() + poGeomField->GetXOrigin();
            dfY = (y - 1) / poGeomField->GetXYScale() + poGeomField->GetYOrigin();
            if( bHasZ )
            {
                ReadVarUInt64NoCheck(pabyCur, z);
                dfZ = (z - 1) / poGeomField->GetZScale() + poGeomField->GetZOrigin();
                return new OGRPoint(dfX, dfY, dfZ);
            }
            else
            {
                return new OGRPoint(dfX, dfY);
            }
            break;
        }

        case SHPT_MULTIPOINTZM:
        case SHPT_MULTIPOINTZ:
            bHasZ = TRUE; /* go on */
        case SHPT_MULTIPOINT:
        case SHPT_MULTIPOINTM:
        {
            returnErrorIf(!ReadVarUInt32(pabyCur, pabyEnd, nPoints) );
            if( nPoints == 0 )
            {
                OGRMultiPoint* poMP = new OGRMultiPoint();
                if( bHasZ )
                    poMP->setCoordinateDimension(3);
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

            return poMP;
            break;
        }

        case SHPT_ARCZ:
        case SHPT_ARCZM:
            bHasZ = TRUE; /* go on */
        case SHPT_ARC:
        case SHPT_ARCM:
        case SHPT_GENERALPOLYLINE:
        {
            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts,
                              (nGeomType & 0x20000000) != 0,
                              FALSE) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRLineString* poLS = new OGRLineString();
                if( bHasZ )
                    poLS->setCoordinateDimension(3);
                return poLS;
            }

            OGRMultiLineString* poMLS = NULL;
            FileGDBOGRLineString* poLS = NULL;
            if( nParts > 1 )
                poMLS = new OGRMultiLineString();

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

            if( poMLS )
                return poMLS;
            else
                return poLS;

            break;
        }

        case SHPT_POLYGONZ:
        case SHPT_POLYGONZM:
            bHasZ = TRUE; /* go on */
        case SHPT_POLYGON:
        case SHPT_POLYGONM:
        case SHPT_GENERALPOLYGON:
        {
            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts,
                              (nGeomType & 0x20000000) != 0,
                              FALSE) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                if( bHasZ )
                    poPoly->setCoordinateDimension(3);
                return poPoly;
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
                    while( (int)i >= 0 )
                        delete papoRings[i--];
                    delete[] papoRings;
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
                        while( (int)i >= 0 )
                            delete papoRings[i--];
                        delete[] papoRings;
                        returnError();
                    }
                }
            }

            OGRGeometry* poRet;
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
                papoRings = NULL;
                const char* papszOptions[] = { "METHOD=ONLY_CCW", NULL };
                poRet = OGRGeometryFactory::organizePolygons(
                    (OGRGeometry**) papoPolygons, nParts, NULL, papszOptions );
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

            break;
        }

        case SHPT_MULTIPATCHM:
        case SHPT_MULTIPATCH:
            bHasZ = TRUE; /* go on */
        case SHPT_GENERALMULTIPATCH:
        {
            returnErrorIf(!ReadPartDefs(pabyCur, pabyEnd, nPoints, nParts, FALSE, TRUE ) );

            if( nPoints == 0 || nParts == 0 )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                if( bHasZ )
                    poPoly->setCoordinateDimension(3);
                return poPoly;
            }
            int* panPartType = (int*) VSIMalloc(sizeof(int) * nParts);
            double* padfXYZ =  (double*) VSIMalloc(3 * sizeof(double) * nPoints);
            double* padfX = padfXYZ;
            double* padfY = padfXYZ + nPoints;
            double* padfZ = padfXYZ + 2 * nPoints;
            if( panPartType == NULL || padfXYZ == NULL  )
            {
                VSIFree(panPartType);
                VSIFree(padfXYZ);
                returnError();
            }
            for(i=0;i<nParts;i++)
            {
                GUInt32 nPartType;
                if( !ReadVarUInt32(pabyCur, pabyEnd, nPartType) )
                {
                    VSIFree(panPartType);
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
                VSIFree(padfXYZ);
                returnError();
            }

            if( bHasZ )
            {
                ZArraySetter arrayzSetter(padfZ);
                if( !ReadZArray<ZArraySetter>(arrayzSetter,
                                pabyCur, pabyEnd, nPoints, dz) )
                {
                    VSIFree(panPartType);
                    VSIFree(padfXYZ);
                    returnError();
                }
            }
            else
            {
                memset(padfZ, 0, nPoints * sizeof(double));
            }

            OGRMultiPolygon* poMP = new OGRMultiPolygon();
            OGRPolygon* poLastPoly = NULL;
            int iAccPoints = 0;
            for(i=0;i<nParts;i++)
            {
                OGRCreateFromMultiPatchPart(poMP, poLastPoly,
                                            panPartType[i],
                                            (int) panPointCount[i],
                                            padfX + iAccPoints,
                                            padfY + iAccPoints,
                                            padfZ + iAccPoints);
                iAccPoints += (int) panPointCount[i];
            }

            if( poLastPoly != NULL )
            {
                poMP->addGeometryDirectly( poLastPoly );
                poLastPoly = NULL;
            }

            VSIFree(panPartType);
            VSIFree(padfXYZ);

            return poMP;

            break;
        }

        default:
            CPLDebug("OpenFileGDB", "Unhandled geometry type = %d", (int)nGeomType);
            break;
/*
#define SHPT_GENERALPOINT       52
#define SHPT_GENERALMULTIPOINT  53
*/
    }
    return NULL;
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
    { "esriGeometryMultiPatch", wkbMultiPolygon }
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

}; /* namespace OpenFileGDB */
