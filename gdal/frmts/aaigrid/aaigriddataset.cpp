/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Arc/Info ASCII Grid Format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2014, Kyle Shannon <kyle at pobox dot com>
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

// We need cpl_port as first include to avoid VSIStatBufL being not
// defined on i586-mingw32msvc.
#include "cpl_port.h"
#include "aaigriddataset.h"
#include "gdal_frmts.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

namespace {

float DoubleToFloatClamp(double dfValue) {
#if HAVE_CXX11
    if( dfValue <= std::numeric_limits<float>::lowest() )
        return std::numeric_limits<float>::lowest();
#else
    if( dfValue <= -std::numeric_limits<float>::max() )
        return -std::numeric_limits<float>::max();
#endif
    if( dfValue >= std::numeric_limits<float>::max() )
        return std::numeric_limits<float>::max();
    return static_cast<float>(dfValue);
}

}  // namespace

static CPLString OSR_GDS( char **papszNV, const char *pszField,
                          const char *pszDefaultValue );

/************************************************************************/
/*                           AAIGRasterBand()                           */
/************************************************************************/

AAIGRasterBand::AAIGRasterBand( AAIGDataset *poDSIn, int nDataStart ) :
    panLineOffset(NULL)
{
    poDS = poDSIn;

    nBand = 1;
    eDataType = poDSIn->eDataType;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;

    panLineOffset = static_cast<GUIntBig *>(
        VSI_CALLOC_VERBOSE(poDSIn->nRasterYSize, sizeof(GUIntBig)));
    if (panLineOffset == NULL)
    {
        return;
    }
    panLineOffset[0] = nDataStart;
}

/************************************************************************/
/*                          ~AAIGRasterBand()                           */
/************************************************************************/

AAIGRasterBand::~AAIGRasterBand() { CPLFree(panLineOffset); }

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AAIGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void *pImage )

{
    AAIGDataset *poODS = static_cast<AAIGDataset *>(poDS);

    if( nBlockYOff < 0 || nBlockYOff > poODS->nRasterYSize - 1 ||
        nBlockXOff != 0 || panLineOffset == NULL || poODS->fp == NULL )
        return CE_Failure;

    if( panLineOffset[nBlockYOff] == 0 )
    {
        for( int iPrevLine = 1; iPrevLine <= nBlockYOff; iPrevLine++ )
            if( panLineOffset[iPrevLine] == 0 )
                IReadBlock(nBlockXOff, iPrevLine - 1, NULL);
    }

    if( panLineOffset[nBlockYOff] == 0 )
        return CE_Failure;

    if( poODS->Seek(panLineOffset[nBlockYOff]) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Can't seek to offset %lu in input file to read data.",
                 static_cast<long unsigned int>(panLineOffset[nBlockYOff]));
        return CE_Failure;
    }

    for( int iPixel = 0; iPixel < poODS->nRasterXSize; )
    {
        // Suck up any pre-white space.
        char chNext = '\0';
        do {
            chNext = poODS->Getc();
        } while( isspace(static_cast<unsigned char>(chNext)) );

        char szToken[500] = { '\0' };
        int iTokenChar = 0;
        while( chNext != '\0' && !isspace((unsigned char)chNext) )
        {
            if( iTokenChar == sizeof(szToken) - 2 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Token too long at scanline %d.", nBlockYOff);
                return CE_Failure;
            }

            szToken[iTokenChar++] = chNext;
            chNext = poODS->Getc();
        }

        if( chNext == '\0' &&
            (iPixel != poODS->nRasterXSize - 1 ||
            nBlockYOff != poODS->nRasterYSize - 1) )
        {
            CPLError(CE_Failure, CPLE_FileIO, "File short, can't read line %d.",
                     nBlockYOff);
            return CE_Failure;
        }

        szToken[iTokenChar] = '\0';

        if( pImage != NULL )
        {
            if( eDataType == GDT_Float64 )
                reinterpret_cast<double *>(pImage)[iPixel] = CPLAtofM(szToken);
            else if( eDataType == GDT_Float32 )
                reinterpret_cast<float *>(pImage)[iPixel] =
                    DoubleToFloatClamp(CPLAtofM(szToken));
            else
                reinterpret_cast<GInt32 *>(pImage)[iPixel] =
                    static_cast<GInt32>(atoi(szToken));
        }

        iPixel++;
    }

    if( nBlockYOff < poODS->nRasterYSize - 1 )
        panLineOffset[nBlockYOff + 1] = poODS->Tell();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double AAIGRasterBand::GetNoDataValue( int *pbSuccess )

{
    AAIGDataset *poODS = static_cast<AAIGDataset *>(poDS);

    if( pbSuccess )
        *pbSuccess = poODS->bNoDataSet;

    return poODS->dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr AAIGRasterBand::SetNoDataValue( double dfNoData )

{
    AAIGDataset *poODS = static_cast<AAIGDataset *>(poDS);

    poODS->bNoDataSet = true;
    poODS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            AAIGDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            AAIGDataset()                            */
/************************************************************************/

AAIGDataset::AAIGDataset() :
    fp(NULL),
    papszPrj(NULL),
    pszProjection(CPLStrdup("")),
    nBufferOffset(0),
    nOffsetInBuffer(256),
    eDataType(GDT_Int32),
    bNoDataSet(false),
    dfNoDataValue(-9999.0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    memset(achReadBuf, 0, sizeof(achReadBuf));
}

/************************************************************************/
/*                           ~AAIGDataset()                            */
/************************************************************************/

AAIGDataset::~AAIGDataset()

{
    FlushCache();

    if( fp != NULL )
    {
        if( VSIFCloseL(fp) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }

    CPLFree(pszProjection);
    CSLDestroy(papszPrj);
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

GUIntBig AAIGDataset::Tell() { return nBufferOffset + nOffsetInBuffer; }

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int AAIGDataset::Seek( GUIntBig nNewOffset )

{
    nOffsetInBuffer = sizeof(achReadBuf);
    return VSIFSeekL(fp, nNewOffset, SEEK_SET);
}

/************************************************************************/
/*                                Getc()                                */
/*                                                                      */
/*      Read a single character from the input file (efficiently we     */
/*      hope).                                                          */
/************************************************************************/

char AAIGDataset::Getc()

{
    if( nOffsetInBuffer < static_cast<int>(sizeof(achReadBuf)) )
        return achReadBuf[nOffsetInBuffer++];

    nBufferOffset = VSIFTellL(fp);
    const int nRead =
        static_cast<int>(VSIFReadL(achReadBuf, 1, sizeof(achReadBuf), fp));
    for( unsigned int i = nRead; i < sizeof(achReadBuf); i++ )
        achReadBuf[i] = '\0';

    nOffsetInBuffer = 0;

    return achReadBuf[nOffsetInBuffer++];
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **AAIGDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( papszPrj != NULL )
        papszFileList = CSLAddString(papszFileList, osPrjFilename);

    return papszFileList;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int AAIGDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    // Does this look like an AI grid file?
    if( poOpenInfo->nHeaderBytes < 40 ||
        !(STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "ncols") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "nrows") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "xllcorner") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "yllcorner") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "xllcenter") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "yllcenter") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "dx") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "dy") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "cellsize")) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GRASSASCIIDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    // Does this look like a GRASS ASCII grid file?
    if( poOpenInfo->nHeaderBytes < 40 ||
        !(STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "north:") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "south:") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "east:") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "west:") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "rows:") ||
          STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "cols:")) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AAIGDataset::Open( GDALOpenInfo * poOpenInfo )
{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if (!Identify(poOpenInfo))
        return NULL;
#endif

    return CommonOpen(poOpenInfo, FORMAT_AAIG);
}

/************************************************************************/
/*                          ParseHeader()                               */
/************************************************************************/

int AAIGDataset::ParseHeader(const char *pszHeader, const char *pszDataType)
{
    char **papszTokens = CSLTokenizeString2(pszHeader, " \n\r\t", 0);
    const int nTokens = CSLCount(papszTokens);

    int i = 0;
    if ( (i = CSLFindString(papszTokens, "ncols")) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }
    nRasterXSize = atoi(papszTokens[i + 1]);
    if ( (i = CSLFindString(papszTokens, "nrows")) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }
    nRasterYSize = atoi(papszTokens[i + 1]);

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    // TODO(schwehr): Would be good to also factor the file size into the max.
    // TODO(schwehr): Allow the user to disable this check.
    // The driver allocates a panLineOffset array based on nRasterYSize
    const int kMaxDimSize =  10000000;  // 1e7 cells.
    if (nRasterXSize > kMaxDimSize || nRasterYSize > kMaxDimSize)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    double dfCellDX = 0.0;
    double dfCellDY = 0.0;
    if ( (i = CSLFindString(papszTokens, "cellsize")) < 0 )
    {
        int iDX, iDY;
        if( (iDX = CSLFindString(papszTokens, "dx")) < 0 ||
            (iDY = CSLFindString(papszTokens, "dy")) < 0 ||
            iDX + 1 >= nTokens ||
            iDY + 1 >= nTokens )
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }

        dfCellDX = CPLAtofM(papszTokens[iDX + 1]);
        dfCellDY = CPLAtofM(papszTokens[iDY + 1]);
    }
    else
    {
        if (i + 1 >= nTokens)
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }
        dfCellDY = CPLAtofM(papszTokens[i + 1]);
        dfCellDX = dfCellDY;
    }

    int j = 0;
    if ((i = CSLFindString(papszTokens, "xllcorner")) >= 0 &&
        (j = CSLFindString(papszTokens, "yllcorner")) >= 0 &&
        i + 1 < nTokens && j + 1 < nTokens)
    {
        adfGeoTransform[0] = CPLAtofM(papszTokens[i + 1]);

        // Small hack to compensate from insufficient precision in cellsize
        // parameter in datasets of
        // http://ccafs-climate.org/data/A2a_2020s/hccpr_hadcm3
        if ((nRasterXSize % 360) == 0 &&
            fabs(adfGeoTransform[0] - (-180.0)) < 1e-12 &&
            dfCellDX == dfCellDY &&
            fabs(dfCellDX - (360.0 / nRasterXSize)) < 1e-9)
        {
            dfCellDY = 360.0 / nRasterXSize;
            dfCellDX = dfCellDY;
        }

        adfGeoTransform[1] = dfCellDX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] =
            CPLAtofM(papszTokens[j + 1]) + nRasterYSize * dfCellDY;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -dfCellDY;
    }
    else if ((i = CSLFindString(papszTokens, "xllcenter")) >= 0 &&
             (j = CSLFindString(papszTokens, "yllcenter")) >= 0 &&
             i + 1 < nTokens && j + 1 < nTokens)
    {
        SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

        adfGeoTransform[0] = CPLAtofM(papszTokens[i + 1]) - 0.5 * dfCellDX;
        adfGeoTransform[1] = dfCellDX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = CPLAtofM(papszTokens[j + 1]) - 0.5 * dfCellDY +
                             nRasterYSize * dfCellDY;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -dfCellDY;
    }
    else
    {
        adfGeoTransform[0] = 0.0;
        adfGeoTransform[1] = dfCellDX;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = 0.0;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -dfCellDY;
    }

    if( (i = CSLFindString( papszTokens, "NODATA_value" )) >= 0 &&
        i + 1 < nTokens)
    {
        const char *pszNoData = papszTokens[i + 1];

        bNoDataSet = true;
        dfNoDataValue = CPLAtofM(pszNoData);
        if( pszDataType == NULL &&
            (strchr(pszNoData, '.') != NULL ||
             strchr(pszNoData, ',') != NULL ||
             INT_MIN > dfNoDataValue || dfNoDataValue > INT_MAX) )
        {
            eDataType = GDT_Float32;
            if( !CPLIsInf(dfNoDataValue) &&
                (fabs(dfNoDataValue) < std::numeric_limits<float>::min() ||
                 fabs(dfNoDataValue) > std::numeric_limits<float>::max()) )
            {
                eDataType = GDT_Float64;
            }
        }
        if( eDataType == GDT_Float32 )
        {
            dfNoDataValue =
                static_cast<double>(static_cast<float>(dfNoDataValue));
        }
    }

    CSLDestroy(papszTokens);

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GRASSASCIIDataset::Open( GDALOpenInfo *poOpenInfo )
{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if (!Identify(poOpenInfo))
        return NULL;
#endif

    return CommonOpen(poOpenInfo, FORMAT_GRASSASCII);
}

/************************************************************************/
/*                          ParseHeader()                               */
/************************************************************************/

int GRASSASCIIDataset::ParseHeader(const char *pszHeader,
                                   const char *pszDataType)
{
    char **papszTokens = CSLTokenizeString2(pszHeader, " \n\r\t:", 0);
    const int nTokens = CSLCount(papszTokens);
    int i = 0;
    if ( (i = CSLFindString(papszTokens, "cols")) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }
    nRasterXSize = atoi(papszTokens[i + 1]);
    if ( (i = CSLFindString( papszTokens, "rows" )) < 0 ||
         i + 1 >= nTokens)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }
    nRasterYSize = atoi(papszTokens[i + 1]);

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    const int iNorth = CSLFindString(papszTokens, "north");
    const int iSouth = CSLFindString(papszTokens, "south");
    const int iEast = CSLFindString(papszTokens, "east");
    const int iWest = CSLFindString(papszTokens, "west");

    if (iNorth == -1 || iSouth == -1 || iEast == -1 || iWest == -1 ||
        std::max(std::max(iNorth, iSouth),
                 std::max(iEast, iWest)) + 1 >= nTokens)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    const double dfNorth = CPLAtofM(papszTokens[iNorth + 1]);
    const double dfSouth = CPLAtofM(papszTokens[iSouth + 1]);
    const double dfEast = CPLAtofM(papszTokens[iEast + 1]);
    const double dfWest = CPLAtofM(papszTokens[iWest + 1]);
    const double dfPixelXSize = (dfEast - dfWest) / nRasterXSize;
    const double dfPixelYSize = (dfNorth - dfSouth) / nRasterYSize;

    adfGeoTransform[0] = dfWest;
    adfGeoTransform[1] = dfPixelXSize;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = dfNorth;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfPixelYSize;

    if( (i = CSLFindString(papszTokens, "null")) >= 0 &&
        i + 1 < nTokens)
    {
        const char *pszNoData = papszTokens[i + 1];

        bNoDataSet = true;
        dfNoDataValue = CPLAtofM(pszNoData);
        if( pszDataType == NULL &&
            (strchr(pszNoData, '.') != NULL ||
             strchr(pszNoData, ',') != NULL ||
             INT_MIN > dfNoDataValue || dfNoDataValue > INT_MAX) )
        {
            eDataType = GDT_Float32;
        }
        if( eDataType == GDT_Float32 )
        {
            // TODO(schwehr): Is this really what we want?
            dfNoDataValue =
                static_cast<double>(static_cast<float>(dfNoDataValue));
        }
    }

    if( (i = CSLFindString(papszTokens, "type")) >= 0 &&
        i + 1 < nTokens)
    {
        const char *pszType = papszTokens[i + 1];
        if (EQUAL(pszType, "int"))
            eDataType = GDT_Int32;
        else if (EQUAL(pszType, "float"))
            eDataType = GDT_Float32;
        else if (EQUAL(pszType, "double"))
            eDataType = GDT_Float64;
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid value for type parameter : %s", pszType);
        }
    }

    CSLDestroy(papszTokens);

    return TRUE;
}

/************************************************************************/
/*                           CommonOpen()                               */
/************************************************************************/

GDALDataset *AAIGDataset::CommonOpen( GDALOpenInfo *poOpenInfo,
                                      GridFormat eFormat )
{
    // Create a corresponding GDALDataset.
    AAIGDataset *poDS = NULL;

    if (eFormat == FORMAT_AAIG)
        poDS = new AAIGDataset();
    else
        poDS = new GRASSASCIIDataset();

    const char *pszDataTypeOption =
        eFormat == FORMAT_AAIG ?
        "AAIGRID_DATATYPE" : "GRASSASCIIGRID_DATATYPE";

    const char *pszDataType = CPLGetConfigOption(pszDataTypeOption, NULL);
    if( pszDataType == NULL )
        pszDataType =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "DATATYPE");
    if (pszDataType != NULL)
    {
        poDS->eDataType = GDALGetDataTypeByName(pszDataType);
        if (!(poDS->eDataType == GDT_Int32 || poDS->eDataType == GDT_Float32 ||
              poDS->eDataType == GDT_Float64))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for %s : %s",
                     pszDataTypeOption, pszDataType);
            poDS->eDataType = GDT_Int32;
            pszDataType = NULL;
        }
    }

    // Parse the header.
    if (!poDS->ParseHeader((const char *)poOpenInfo->pabyHeader, pszDataType))
    {
        delete poDS;
        return NULL;
    }

    // Open file with large file API.
    poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "r");
    if( poDS->fp == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "VSIFOpenL(%s) failed unexpectedly.",
                 poOpenInfo->pszFilename);
        delete poDS;
        return NULL;
    }

    // Find the start of real data.
    int nStartOfData = 0;

    for( int i = 2; true; i++ )
    {
        if( poOpenInfo->pabyHeader[i] == '\0' )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Couldn't find data values in ASCII Grid file.");
            delete poDS;
            return NULL;
        }

        if( poOpenInfo->pabyHeader[i - 1] == '\n' ||
            poOpenInfo->pabyHeader[i - 2] == '\n' ||
            poOpenInfo->pabyHeader[i - 1] == '\r' ||
            poOpenInfo->pabyHeader[i - 2] == '\r' )
        {
            if( !isalpha(poOpenInfo->pabyHeader[i]) &&
                poOpenInfo->pabyHeader[i] != '\n' &&
                poOpenInfo->pabyHeader[i] != '\r')
            {
                nStartOfData = i;

                // Beginning of real data found.
                break;
            }
        }
    }

    // Recognize the type of data.
    CPLAssert(NULL != poDS->fp);

    if( pszDataType == NULL &&
        poDS->eDataType != GDT_Float32 && poDS->eDataType != GDT_Float64)
    {
        // Allocate 100K chunk + 1 extra byte for NULL character.
        const size_t nChunkSize = 1024 * 100;
        GByte *pabyChunk = static_cast<GByte *>(
            VSI_CALLOC_VERBOSE(nChunkSize + 1, sizeof(GByte)));
        if (pabyChunk == NULL)
        {
            delete poDS;
            return NULL;
        }
        pabyChunk[nChunkSize] = '\0';

        if( VSIFSeekL(poDS->fp, nStartOfData, SEEK_SET) < 0 )
        {
            delete poDS;
            VSIFree(pabyChunk);
            return NULL;
        }

        // Scan for dot in subsequent chunks of data.
        while( !VSIFEofL(poDS->fp) )
        {
            CPL_IGNORE_RET_VAL(VSIFReadL(pabyChunk, nChunkSize, 1, poDS->fp));

            for( int i = 0; i < static_cast<int>(nChunkSize); i++)
            {
                GByte ch = pabyChunk[i];
                if (ch == '.' || ch == ',' || ch == 'e' || ch == 'E')
                {
                    poDS->eDataType = GDT_Float32;
                    break;
                }
            }
        }

        // Deallocate chunk.
        VSIFree(pabyChunk);
    }

    // Create band information objects.
    AAIGRasterBand *band = new AAIGRasterBand(poDS, nStartOfData);
    poDS->SetBand(1, band);
    if (band->panLineOffset == NULL)
    {
        delete poDS;
        return NULL;
    }

    // Try to read projection file.
    char *const pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    char *const pszBasename =
        CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

    poDS->osPrjFilename = CPLFormFilename(pszDirname, pszBasename, "prj");
    int nRet = 0;
    {
        VSIStatBufL sStatBuf;
        nRet = VSIStatL(poDS->osPrjFilename, &sStatBuf);
    }
    if( nRet != 0 && VSIIsCaseSensitiveFS(poDS->osPrjFilename) )
    {
        poDS->osPrjFilename = CPLFormFilename(pszDirname, pszBasename, "PRJ");

        VSIStatBufL sStatBuf;
        nRet = VSIStatL(poDS->osPrjFilename, &sStatBuf);
    }

    if( nRet == 0 )
    {
        poDS->papszPrj = CSLLoad(poDS->osPrjFilename);

        CPLDebug("AAIGrid", "Loaded SRS from %s", poDS->osPrjFilename.c_str());

        OGRSpatialReference oSRS;
        if( oSRS.importFromESRI(poDS->papszPrj) == OGRERR_NONE )
        {
            // If geographic values are in seconds, we must transform.
            // Is there a code for minutes too?
            if( oSRS.IsGeographic() &&
                EQUAL(OSR_GDS(poDS->papszPrj, "Units", ""), "DS") )
            {
                poDS->adfGeoTransform[0] /= 3600.0;
                poDS->adfGeoTransform[1] /= 3600.0;
                poDS->adfGeoTransform[2] /= 3600.0;
                poDS->adfGeoTransform[3] /= 3600.0;
                poDS->adfGeoTransform[4] /= 3600.0;
                poDS->adfGeoTransform[5] /= 3600.0;
            }

            CPLFree(poDS->pszProjection);
            oSRS.exportToWkt(&(poDS->pszProjection));
        }
    }

    CPLFree(pszDirname);
    CPLFree(pszBasename);

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for external overviews.
    poDS->oOvManager.Initialize(
        poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles());

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AAIGDataset::GetGeoTransform( double *padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *AAIGDataset::GetProjectionRef() { return pszProjection; }

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

GDALDataset * AAIGDataset::CreateCopy(
    const char *pszFilename, GDALDataset *poSrcDS,
    int /* bStrict */,
    char **papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData )
{
    const int nBands = poSrcDS->GetRasterCount();
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    // Some rudimentary checks.
    if( nBands != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AAIG driver doesn't support %d bands.  Must be 1 band.",
                 nBands);

        return NULL;
    }

    if( !pfnProgress(0.0, NULL, pProgressData) )
        return NULL;

    // Create the dataset.
    VSILFILE *fpImage = VSIFOpenL(pszFilename, "wt");
    if( fpImage == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create file %s.",
                 pszFilename);
        return NULL;
    }

    // Write ASCII Grid file header.
    double adfGeoTransform[6] = {};
    char szHeader[2000] = {};
    const char *pszForceCellsize =
        CSLFetchNameValue(papszOptions, "FORCE_CELLSIZE");

    poSrcDS->GetGeoTransform(adfGeoTransform);

    if( std::abs(adfGeoTransform[1] + adfGeoTransform[5]) < 0.0000001 ||
        std::abs(adfGeoTransform[1]-adfGeoTransform[5]) < 0.0000001 ||
        (pszForceCellsize && CPLTestBool(pszForceCellsize)) )
    {
        CPLsnprintf(
            szHeader, sizeof(szHeader),
            "ncols        %d\n"
            "nrows        %d\n"
            "xllcorner    %.12f\n"
            "yllcorner    %.12f\n"
            "cellsize     %.12f\n",
            nXSize, nYSize,
            adfGeoTransform[0],
            adfGeoTransform[3] - nYSize * adfGeoTransform[1],
            adfGeoTransform[1]);
    }
    else
    {
        if( pszForceCellsize == NULL )
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Producing a Golden Surfer style file with DX and DY "
                     "instead of CELLSIZE since the input pixels are "
                     "non-square.  Use the FORCE_CELLSIZE=TRUE creation "
                     "option to force use of DX for even though this will "
                     "be distorted.  Most ASCII Grid readers (ArcGIS "
                     "included) do not support the DX and DY parameters.");
        CPLsnprintf(
            szHeader, sizeof(szHeader),
            "ncols        %d\n"
            "nrows        %d\n"
            "xllcorner    %.12f\n"
            "yllcorner    %.12f\n"
            "dx           %.12f\n"
            "dy           %.12f\n",
            nXSize, nYSize,
            adfGeoTransform[0],
            adfGeoTransform[3] + nYSize * adfGeoTransform[5],
            adfGeoTransform[1],
            fabs(adfGeoTransform[5]));
    }

    // Builds the format string used for printing float values.
    char szFormatFloat[32] = { '\0' };
    strcpy(szFormatFloat, " %.20g");
    const char *pszDecimalPrecision =
        CSLFetchNameValue(papszOptions, "DECIMAL_PRECISION");
    const char *pszSignificantDigits =
        CSLFetchNameValue(papszOptions, "SIGNIFICANT_DIGITS");
    bool bIgnoreSigDigits = false;
    if( pszDecimalPrecision && pszSignificantDigits )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Conflicting precision arguments, using DECIMAL_PRECISION");
        bIgnoreSigDigits = true;
    }
    int nPrecision;
    if ( pszSignificantDigits && !bIgnoreSigDigits )
    {
        nPrecision = atoi(pszSignificantDigits);
        if (nPrecision >= 0)
            snprintf(szFormatFloat, sizeof(szFormatFloat), " %%.%dg",
                     nPrecision);
        CPLDebug("AAIGrid", "Setting precision format: %s", szFormatFloat);
    }
    else if( pszDecimalPrecision )
    {
        nPrecision = atoi(pszDecimalPrecision);
        if ( nPrecision >= 0 )
            snprintf(szFormatFloat, sizeof(szFormatFloat), " %%.%df",
                     nPrecision);
        CPLDebug("AAIGrid", "Setting precision format: %s", szFormatFloat);
    }

    // Handle nodata (optionally).
    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    const bool bReadAsInt =
        poBand->GetRasterDataType() == GDT_Byte ||
        poBand->GetRasterDataType() == GDT_Int16 ||
        poBand->GetRasterDataType() == GDT_UInt16 ||
        poBand->GetRasterDataType() == GDT_Int32;

    // Write `nodata' value to header if it is exists in source dataset
    int bSuccess = FALSE;
    const double dfNoData = poBand->GetNoDataValue(&bSuccess);
    if ( bSuccess )
    {
        snprintf(szHeader + strlen(szHeader),
                 sizeof(szHeader) - strlen(szHeader), "%s", "NODATA_value ");
        if( bReadAsInt )
            snprintf(szHeader + strlen(szHeader),
                     sizeof(szHeader) - strlen(szHeader), "%d",
                     static_cast<int>(dfNoData));
        else
            CPLsnprintf(szHeader + strlen(szHeader),
                        sizeof(szHeader) - strlen(szHeader),
                        szFormatFloat, dfNoData);
        snprintf(szHeader + strlen(szHeader),
                 sizeof(szHeader) - strlen(szHeader), "%s", "\n");
    }

    if( VSIFWriteL(szHeader, strlen(szHeader), 1, fpImage) != 1)
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpImage));
        return NULL;
    }

    // Loop over image, copying image data.

    // Write scanlines to output file
    int *panScanline = bReadAsInt
                           ? static_cast<int *>(CPLMalloc(
                                 nXSize * GDALGetDataTypeSizeBytes(GDT_Int32)))
                           : NULL;

    double *padfScanline =
        bReadAsInt ? NULL
                   : static_cast<double *>(CPLMalloc(
                         nXSize * GDALGetDataTypeSizeBytes(GDT_Float64)));

    CPLErr eErr = CE_None;

    bool bHasOutputDecimalDot = false;
    for( int iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
    {
        CPLString osBuf;
        eErr = poBand->RasterIO(
            GF_Read, 0, iLine, nXSize, 1,
            bReadAsInt ? reinterpret_cast<void *>(panScanline) :
            reinterpret_cast<void *>(padfScanline),
            nXSize, 1, bReadAsInt ? GDT_Int32 : GDT_Float64,
            0, 0, NULL);

        if( bReadAsInt )
        {
            for ( int iPixel = 0; iPixel < nXSize; iPixel++ )
            {
                snprintf(szHeader, sizeof(szHeader),
                         " %d", panScanline[iPixel]);
                osBuf += szHeader;
                if( (iPixel & 1023) == 0 || iPixel == nXSize - 1 )
                {
                    if ( VSIFWriteL(osBuf, static_cast<int>(osBuf.size()), 1,
                                    fpImage) != 1 )
                    {
                        eErr = CE_Failure;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Write failed, disk full?");
                        break;
                    }
                    osBuf = "";
                }
            }
        }
        else
        {
            for ( int iPixel = 0; iPixel < nXSize; iPixel++ )
            {
                CPLsnprintf(szHeader, sizeof(szHeader),
                            szFormatFloat, padfScanline[iPixel]);

                // Make sure that as least one value has a decimal point (#6060)
                if( !bHasOutputDecimalDot )
                {
                    if( strchr(szHeader, '.') || strchr(szHeader, 'e') ||
                        strchr(szHeader, 'E') )
                    {
                        bHasOutputDecimalDot = true;
                    }
                    else if( !CPLIsInf(padfScanline[iPixel]) &&
                             !CPLIsNan(padfScanline[iPixel]) )
                    {
                        strcat(szHeader, ".0");
                        bHasOutputDecimalDot = true;
                    }
                }

                osBuf += szHeader;
                if( (iPixel & 1023) == 0 || iPixel == nXSize - 1 )
                {
                  if ( VSIFWriteL(osBuf, static_cast<int>(osBuf.size()), 1,
                                  fpImage) != 1 )
                    {
                        eErr = CE_Failure;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Write failed, disk full?");
                        break;
                    }
                    osBuf = "";
                }
            }
        }
        if( VSIFWriteL("\n", 1, 1, fpImage) != 1 )
            eErr = CE_Failure;

        if( eErr == CE_None &&
            !pfnProgress((iLine + 1) / static_cast<double>(nYSize), NULL,
                         pProgressData) )
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
        }
    }

    CPLFree(panScanline);
    CPLFree(padfScanline);
    if( VSIFCloseL(fpImage) != 0 )
        eErr = CE_Failure;

    if( eErr != CE_None )
        return NULL;

    // Try to write projection file.
    const char *pszOriginalProjection = poSrcDS->GetProjectionRef();
    if( !EQUAL(pszOriginalProjection, "") )
    {
        char *pszDirname = CPLStrdup(CPLGetPath(pszFilename));
        char *pszBasename = CPLStrdup(CPLGetBasename(pszFilename));
        char *pszPrjFilename =
            CPLStrdup(CPLFormFilename(pszDirname, pszBasename, "prj"));
        VSILFILE *fp = VSIFOpenL(pszPrjFilename, "wt");
        if (fp != NULL)
        {
            OGRSpatialReference oSRS;
            // TODO(schwehr): importFromWkt should be const for the args.
            oSRS.importFromWkt(const_cast<char **>(&pszOriginalProjection));
            oSRS.morphToESRI();
            char *pszESRIProjection = NULL;
            oSRS.exportToWkt(&pszESRIProjection);
            CPL_IGNORE_RET_VAL(VSIFWriteL(pszESRIProjection, 1,
                                          strlen(pszESRIProjection), fp));

            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            CPLFree(pszESRIProjection);
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO, "Unable to create file %s.",
                     pszPrjFilename);
        }
        CPLFree(pszDirname);
        CPLFree(pszBasename);
        CPLFree(pszPrjFilename);
    }

    // Re-open dataset, and copy any auxiliary pam information.

    // If writing to stdout, we can't reopen it, so return
    // a fake dataset to make the caller happy.
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALPamDataset *poDS =
        reinterpret_cast<GDALPamDataset *>(GDALOpen(pszFilename, GA_ReadOnly));
    CPLPopErrorHandler();
    if (poDS)
    {
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
        return poDS;
    }

    CPLErrorReset();

    AAIGDataset *poAAIG_DS = new AAIGDataset();
    poAAIG_DS->nRasterXSize = nXSize;
    poAAIG_DS->nRasterYSize = nYSize;
    poAAIG_DS->nBands = 1;
    poAAIG_DS->SetBand(1, new AAIGRasterBand(poAAIG_DS, 1));
    return poAAIG_DS;
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static CPLString OSR_GDS( char **papszNV, const char *pszField,
                          const char *pszDefaultValue )

{
    if( papszNV == NULL || papszNV[0] == NULL )
        return pszDefaultValue;

    int iLine = 0;  // Used after for.
    for( ;
         papszNV[iLine] != NULL &&
             !EQUALN(papszNV[iLine],pszField,strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return pszDefaultValue;
    else
    {
        char **papszTokens = CSLTokenizeString(papszNV[iLine]);

        CPLString osResult;
        if( CSLCount(papszTokens) > 1 )
            osResult = papszTokens[1];
        else
            osResult = pszDefaultValue;

        CSLDestroy(papszTokens);
        return osResult;
    }
}

/************************************************************************/
/*                        GDALRegister_AAIGrid()                        */
/************************************************************************/

void GDALRegister_AAIGrid()

{
    if( GDALGetDriverByName("AAIGrid") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("AAIGrid");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Arc/Info ASCII Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#AAIGrid");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "asc");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 Int32 Float32");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='FORCE_CELLSIZE' type='boolean' description='Force use of CELLSIZE, default is FALSE.'/>\n"
"   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers(%f).'/>\n"
"   <Option name='SIGNIFICANT_DIGITS' type='int' description='Number of significant digits when writing floating-point numbers(%g).'/>\n"
"</CreationOptionList>\n");
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionLists>\n"
"   <Option name='DATATYPE' type='string-select' description='Data type to be used.'>\n"
"       <Value>Int32</Value>\n"
"       <Value>Float32</Value>\n"
"       <Value>Float64</Value>\n"
"   </Option>\n"
"</OpenOptionLists>\n");

    poDriver->pfnOpen = AAIGDataset::Open;
    poDriver->pfnIdentify = AAIGDataset::Identify;
    poDriver->pfnCreateCopy = AAIGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                   GDALRegister_GRASSASCIIGrid()                      */
/************************************************************************/

void GDALRegister_GRASSASCIIGrid()

{
    if( GDALGetDriverByName("GRASSASCIIGrid") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GRASSASCIIGrid");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GRASS ASCII Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "frmt_various.html#GRASSASCIIGrid");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = GRASSASCIIDataset::Open;
    poDriver->pfnIdentify = GRASSASCIIDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
