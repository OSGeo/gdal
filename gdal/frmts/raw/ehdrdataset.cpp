/******************************************************************************
 *
 * Project:  ESRI .hdr Driver
 * Purpose:  Implementation of EHdrDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ehdrdataset.h"
#include "rawdataset.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

constexpr int HAS_MIN_FLAG = 0x1;
constexpr int HAS_MAX_FLAG = 0x2;
constexpr int HAS_MEAN_FLAG = 0x4;
constexpr int HAS_STDDEV_FLAG = 0x8;
constexpr int HAS_ALL_FLAGS =
    HAS_MIN_FLAG | HAS_MAX_FLAG | HAS_MEAN_FLAG | HAS_STDDEV_FLAG;

/************************************************************************/
/*                           EHdrRasterBand()                           */
/************************************************************************/

EHdrRasterBand::EHdrRasterBand( GDALDataset *poDSIn,
                                int nBandIn, VSILFILE *fpRawIn,
                                vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                int nLineOffsetIn,
                                GDALDataType eDataTypeIn, int bNativeOrderIn,
                                int nBitsIn) :
  RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                nLineOffsetIn, eDataTypeIn, bNativeOrderIn, RawRasterBand::OwnFP::NO),
  nBits(nBitsIn),
  nStartBit(0),
  nPixelOffsetBits(0),
  nLineOffsetBits(0),
  bNoDataSet(FALSE),
  dfNoData(0.0),
  dfMin(0.0),
  dfMax(0.0),
  dfMean(0.0),
  dfStdDev(0.0),
  minmaxmeanstddev(0)
{
    EHdrDataset *poEDS = reinterpret_cast<EHdrDataset *>(poDS);

    if (nBits < 8)
    {
        int nSkipBytes = atoi(poEDS->GetKeyValue("SKIPBYTES"));
        if( nSkipBytes < 0 ||
            nSkipBytes > std::numeric_limits<int>::max() / 8 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid SKIPBYTES: %d", nSkipBytes);
            nStartBit = 0;
        }
        else
        {
            nStartBit = nSkipBytes * 8;
        }
        if (nBand >= 2)
        {
            GIntBig nBandRowBytes =
                CPLAtoGIntBig(poEDS->GetKeyValue("BANDROWBYTES"));
            if( nBandRowBytes < 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid BANDROWBYTES: " CPL_FRMT_GIB,
                         nBandRowBytes);
                nBandRowBytes = 0;
            }
            vsi_l_offset nRowBytes = 0;
            if (nBandRowBytes == 0)
                nRowBytes =
                    (static_cast<vsi_l_offset>(nBits) *
                     poDS->GetRasterXSize() + 7) / 8;
            else
                nRowBytes = static_cast<vsi_l_offset>(nBandRowBytes);

            nStartBit += nRowBytes * (nBand - 1) * 8;
        }

        nPixelOffsetBits = nBits;
        GIntBig nTotalRowBytes =
            CPLAtoGIntBig(poEDS->GetKeyValue("TOTALROWBYTES"));
        if( nTotalRowBytes < 0 || nTotalRowBytes >
                                    GINTBIG_MAX / 8 / poDS->GetRasterYSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid TOTALROWBYTES: " CPL_FRMT_GIB, nTotalRowBytes);
            nTotalRowBytes = 0;
        }
        if( nTotalRowBytes > 0 )
            nLineOffsetBits = static_cast<vsi_l_offset>(nTotalRowBytes * 8);
        else
            nLineOffsetBits = static_cast<vsi_l_offset>(nPixelOffsetBits) *
                              poDS->GetRasterXSize();

        nBlockXSize = poDS->GetRasterXSize();
        nBlockYSize = 1;

        SetMetadataItem("NBITS", CPLString().Printf("%d", nBits),
                        "IMAGE_STRUCTURE");
    }

    if( eDataType == GDT_Byte &&
        EQUAL(poEDS->GetKeyValue("PIXELTYPE", ""), "SIGNEDINT") )
        SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
}


/************************************************************************/
/*                          ~EHdrRasterBand()                           */
/************************************************************************/

EHdrRasterBand::~EHdrRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr EHdrRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    if (nBits >= 8)
        return RawRasterBand::IReadBlock(nBlockXOff, nBlockYOff, pImage);

    // Establish desired position.
    const vsi_l_offset nLineStart =
        (nStartBit + nLineOffsetBits * nBlockYOff) / 8;
    int iBitOffset =
        static_cast<int>((nStartBit + nLineOffsetBits * nBlockYOff) % 8);
    const vsi_l_offset nLineEnd =
        (nStartBit + nLineOffsetBits * nBlockYOff +
            static_cast<vsi_l_offset>(nPixelOffsetBits) * nBlockXSize - 1) / 8;
    const vsi_l_offset nLineBytesBig = nLineEnd - nLineStart + 1;
    if( nLineBytesBig >
        static_cast<vsi_l_offset>(std::numeric_limits<int>::max()) )
        return CE_Failure;
    const unsigned int nLineBytes = static_cast<unsigned int>(nLineBytesBig);

    // Read data into buffer.
    GByte *pabyBuffer = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nLineBytes));
    if( pabyBuffer == nullptr )
        return CE_Failure;

    if( VSIFSeekL(GetFPL(), nLineStart, SEEK_SET) != 0 ||
        VSIFReadL(pabyBuffer, 1, nLineBytes, GetFPL()) != nLineBytes )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to read %u bytes at offset %lu.\n%s",
                 nLineBytes, static_cast<unsigned long>(nLineStart),
                 VSIStrerror(errno));
        CPLFree(pabyBuffer);
        return CE_Failure;
    }

    // Copy data, promoting to 8bit.
    for( int iX = 0, iPixel = 0; iX < nBlockXSize; iX++ )
    {
        int nOutWord = 0;

        for( int iBit = 0; iBit < nBits; iBit++ )
        {
            if( pabyBuffer[iBitOffset >> 3] & (0x80 >>(iBitOffset & 7)) )
                nOutWord |= (1 << (nBits - 1 - iBit));
            iBitOffset++;
        }

        iBitOffset = iBitOffset + nPixelOffsetBits - nBits;

        reinterpret_cast<GByte *>(pImage)[iPixel++] =
            static_cast<GByte>(nOutWord);
    }

    CPLFree(pabyBuffer);

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr EHdrRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    if (nBits >= 8)
        return RawRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);

    // Establish desired position.
    const vsi_l_offset nLineStart =
        (nStartBit + nLineOffsetBits * nBlockYOff) / 8;
    int iBitOffset =
        static_cast<int>((nStartBit + nLineOffsetBits * nBlockYOff) % 8);
    const vsi_l_offset nLineEnd =
        (nStartBit + nLineOffsetBits * nBlockYOff +
            static_cast<vsi_l_offset>(nPixelOffsetBits) * nBlockXSize - 1) / 8;
    const vsi_l_offset nLineBytesBig = nLineEnd - nLineStart + 1;
    if( nLineBytesBig >
        static_cast<vsi_l_offset>(std::numeric_limits<int>::max()) )
        return CE_Failure;
    const unsigned int nLineBytes = static_cast<unsigned int>(nLineBytesBig);

    // Read data into buffer.
    GByte *pabyBuffer =
        static_cast<GByte *>(VSI_CALLOC_VERBOSE(nLineBytes, 1));
    if( pabyBuffer == nullptr )
        return CE_Failure;

    if( VSIFSeekL(GetFPL(), nLineStart, SEEK_SET) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to read %u bytes at offset %lu.\n%s",
                 nLineBytes, static_cast<unsigned long>(nLineStart),
                 VSIStrerror(errno));
        CPLFree(pabyBuffer);
        return CE_Failure;
    }

    CPL_IGNORE_RET_VAL(VSIFReadL(pabyBuffer, nLineBytes, 1, GetFPL()));

    // Copy data, promoting to 8bit.
    for( int iX = 0, iPixel = 0; iX < nBlockXSize; iX++ )
    {
        const int nOutWord = reinterpret_cast<GByte *>(pImage)[iPixel++];

        for( int iBit = 0; iBit < nBits; iBit++ )
        {
            if( nOutWord & (1 << (nBits - 1 - iBit)) )
                pabyBuffer[iBitOffset >> 3] |= (0x80 >> (iBitOffset & 7));
            else
                pabyBuffer[iBitOffset >> 3] &= ~((0x80 >> (iBitOffset & 7)));

            iBitOffset++;
        }

        iBitOffset = iBitOffset + nPixelOffsetBits - nBits;
    }

    // Write the data back out.
    if( VSIFSeekL(GetFPL(), nLineStart, SEEK_SET) != 0 ||
        VSIFWriteL(pabyBuffer, 1, nLineBytes, GetFPL()) != nLineBytes )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to write %u bytes at offset %lu.\n%s",
                 nLineBytes, static_cast<unsigned long>(nLineStart),
                 VSIStrerror(errno));
        return CE_Failure;
    }

    CPLFree(pabyBuffer);

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr EHdrRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace,
                                  GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )

{
    // Defer to RawRasterBand
    if (nBits >= 8)
        return RawRasterBand::IRasterIO(eRWFlag,
                                        nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize,
                                        eBufType, nPixelSpace, nLineSpace,
                                        psExtraArg);

    // Force use of IReadBlock() and IWriteBlock()
    return GDALRasterBand::IRasterIO(eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType, nPixelSpace, nLineSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static const char*OSR_GDS( char* pszResult, int nResultLen,
                           char **papszNV, const char * pszField,
                           const char *pszDefaultValue )

{
    if( papszNV == nullptr || papszNV[0] == nullptr )
        return pszDefaultValue;

    int iLine = 0;  // Used after for.
    for( ;
         papszNV[iLine] != nullptr &&
             !EQUALN(papszNV[iLine], pszField, strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == nullptr )
        return pszDefaultValue;

    char **papszTokens = CSLTokenizeString(papszNV[iLine]);

    if( CSLCount(papszTokens) > 1 )
        strncpy(pszResult, papszTokens[1], nResultLen-1);
    else
        strncpy(pszResult, pszDefaultValue, nResultLen-1);
    pszResult[nResultLen - 1] = '\0';

    CSLDestroy(papszTokens);
    return pszResult;
}

/************************************************************************/
/* ==================================================================== */
/*                            EHdrDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            EHdrDataset()                             */
/************************************************************************/

EHdrDataset::EHdrDataset() :
    fpImage(nullptr),
    osHeaderExt("hdr"),
    bGotTransform(false),
    pszProjection(CPLStrdup("")),
    bHDRDirty(false),
    papszHDR(nullptr),
    bCLRDirty(false)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~EHdrDataset()                            */
/************************************************************************/

EHdrDataset::~EHdrDataset()

{
    FlushCache();

    if( nBands > 0 && GetAccess() == GA_Update )
    {
        int bNoDataSet;
        RawRasterBand *poBand =
            reinterpret_cast<RawRasterBand *>(GetRasterBand(1));

        const double dfNoData = poBand->GetNoDataValue(&bNoDataSet);
        if( bNoDataSet )
        {
            ResetKeyValue("NODATA", CPLString().Printf("%.8g", dfNoData));
        }

        if( bCLRDirty )
            RewriteCLR(poBand);

        if( bHDRDirty )
            RewriteHDR();
    }

    if( fpImage != nullptr )
    {
        if( VSIFCloseL(fpImage) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }

    CPLFree(pszProjection);
    CSLDestroy(papszHDR);
}

/************************************************************************/
/*                            GetKeyValue()                             */
/************************************************************************/

const char *EHdrDataset::GetKeyValue( const char *pszKey,
                                      const char *pszDefault )

{
    for( int i = 0; papszHDR[i] != nullptr; i++ )
    {
        if( EQUALN(pszKey,papszHDR[i],strlen(pszKey)) &&
            isspace(static_cast<unsigned char>(papszHDR[i][strlen(pszKey)])) )
        {
            const char *pszValue = papszHDR[i] + strlen(pszKey);
            while( isspace(static_cast<unsigned char>(*pszValue)) )
                pszValue++;

            return pszValue;
        }
    }

    return pszDefault;
}

/************************************************************************/
/*                           ResetKeyValue()                            */
/*                                                                      */
/*      Replace or add the keyword with the indicated value in the      */
/*      papszHDR list.                                                  */
/************************************************************************/

void EHdrDataset::ResetKeyValue( const char *pszKey, const char *pszValue )

{
    if( strlen(pszValue) > 65 )
    {
        CPLAssert(strlen(pszValue) <= 65);
        return;
    }

    char szNewLine[82] = { '\0' };
    snprintf(szNewLine, sizeof(szNewLine), "%-15s%s", pszKey, pszValue);

    for( int i = CSLCount(papszHDR)-1; i >= 0; i-- )
    {
        if( EQUALN(papszHDR[i], szNewLine, strlen(pszKey) + 1) )
        {
            if( strcmp(papszHDR[i],szNewLine) != 0 )
            {
                CPLFree(papszHDR[i]);
                papszHDR[i] = CPLStrdup(szNewLine);
                bHDRDirty = true;
            }
            return;
        }
    }

    bHDRDirty = true;
    papszHDR = CSLAddString(papszHDR, szNewLine);
}

/************************************************************************/
/*                           RewriteCLR()                               */
/************************************************************************/

void EHdrDataset::RewriteCLR( GDALRasterBand* poBand ) const

{
    CPLString osCLRFilename = CPLResetExtension(GetDescription(), "clr");
    GDALColorTable* poTable = poBand->GetColorTable();
    GDALRasterAttributeTable* poRAT = poBand->GetDefaultRAT();
    if( poTable || poRAT )
    {
        VSILFILE *fp = VSIFOpenL(osCLRFilename, "wt");
        if( fp != nullptr )
        {
            // Write RAT in priority if both are defined
            if( poRAT )
            {
                for( int iEntry = 0;
                    iEntry < poRAT->GetRowCount();
                    iEntry++ )
                {
                    CPLString oLine;
                    oLine.Printf("%3d %3d %3d %3d\n",
                                 poRAT->GetValueAsInt(iEntry, 0),
                                 poRAT->GetValueAsInt(iEntry, 1),
                                 poRAT->GetValueAsInt(iEntry, 2),
                                 poRAT->GetValueAsInt(iEntry, 3));
                    if( VSIFWriteL(
                        reinterpret_cast<void *>(
                            const_cast<char *>( oLine.c_str() ) ),
                        strlen(oLine), 1, fp ) != 1 )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                "Error while write color table");
                        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                        return;
                    }
                }
            }
            else
            {
                for( int iColor = 0;
                    iColor < poTable->GetColorEntryCount();
                    iColor++ )
                {
                    GDALColorEntry sEntry;
                    poTable->GetColorEntryAsRGB(iColor, &sEntry);

                    // I wish we had a way to mark transparency.
                    CPLString oLine;
                    oLine.Printf("%3d %3d %3d %3d\n",
                                iColor, sEntry.c1, sEntry.c2, sEntry.c3);
                    if( VSIFWriteL(
                        reinterpret_cast<void *>(
                            const_cast<char *>( oLine.c_str() ) ),
                        strlen(oLine), 1, fp ) != 1 )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                "Error while write color table");
                        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                        return;
                    }
                }
            }
            if( VSIFCloseL(fp) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Error while write color table");
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to create color file %s.",
                     osCLRFilename.c_str());
        }
    }
    else
    {
        VSIUnlink(osCLRFilename);
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *EHdrDataset::_GetProjectionRef()

{
    if (pszProjection && strlen(pszProjection) > 0)
        return pszProjection;

    return GDALPamDataset::_GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr EHdrDataset::_SetProjection( const char *pszSRS )

{
    // Reset coordinate system on the dataset.
    CPLFree(pszProjection);
    pszProjection = CPLStrdup(pszSRS);

    if( strlen(pszSRS) == 0 )
        return CE_None;

    // Convert to ESRI WKT.
    OGRSpatialReference oSRS(pszSRS);
    oSRS.morphToESRI();

    char *pszESRI_SRS = nullptr;
    oSRS.exportToWkt(&pszESRI_SRS);

    // Write to .prj file.
    CPLString osPrjFilename = CPLResetExtension(GetDescription(), "prj");
    VSILFILE *fp = VSIFOpenL(osPrjFilename.c_str(), "wt");
    if( fp != nullptr )
    {
        size_t nCount = VSIFWriteL(pszESRI_SRS, strlen(pszESRI_SRS), 1, fp);
        nCount += VSIFWriteL("\n", 1, 1, fp);
        if( VSIFCloseL(fp) != 0 || nCount != 2 )
        {
            CPLFree(pszESRI_SRS);
            return CE_Failure;
        }
    }

    CPLFree(pszESRI_SRS);

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr EHdrDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr EHdrDataset::SetGeoTransform( double *padfGeoTransform )

{
    // We only support non-rotated images with info in the .HDR file.
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0 )
    {
        return GDALPamDataset::SetGeoTransform(padfGeoTransform);
    }

    // Record new geotransform.
    bGotTransform = true;
    memcpy(adfGeoTransform, padfGeoTransform, sizeof(double) * 6);

    // Strip out all old geotransform keywords from HDR records.
    for( int i = CSLCount(papszHDR) - 1; i >= 0; i-- )
    {
        if( STARTS_WITH_CI(papszHDR[i], "ul") ||
            STARTS_WITH_CI(papszHDR[i] + 1, "ll") ||
            STARTS_WITH_CI(papszHDR[i], "cell") ||
            STARTS_WITH_CI(papszHDR[i] + 1, "dim") )
        {
            papszHDR = CSLRemoveStrings(papszHDR, i, 1, nullptr);
        }
    }

    // Set the transformation information.
    CPLString  oValue;

    oValue.Printf("%.15g", adfGeoTransform[0] + adfGeoTransform[1] * 0.5);
    ResetKeyValue("ULXMAP", oValue);

    oValue.Printf("%.15g", adfGeoTransform[3] + adfGeoTransform[5] * 0.5);
    ResetKeyValue("ULYMAP", oValue);

    oValue.Printf("%.15g", adfGeoTransform[1]);
    ResetKeyValue("XDIM", oValue);

    oValue.Printf("%.15g", fabs(adfGeoTransform[5]));
    ResetKeyValue("YDIM", oValue);

    return CE_None;
}

/************************************************************************/
/*                             RewriteHDR()                             */
/************************************************************************/

CPLErr EHdrDataset::RewriteHDR()

{
    const CPLString osPath = CPLGetPath(GetDescription());
    const CPLString osName = CPLGetBasename(GetDescription());
    const CPLString osHDRFilename =
        CPLFormCIFilename(osPath, osName, osHeaderExt);

    // Write .hdr file.
    VSILFILE *fp = VSIFOpenL(osHDRFilename, "wt");

    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to rewrite .hdr file %s.",
                 osHDRFilename.c_str());
        return CE_Failure;
    }

    for( int i = 0; papszHDR[i] != nullptr; i++ )
    {
        size_t nCount = VSIFWriteL(papszHDR[i], strlen(papszHDR[i]), 1, fp);
        nCount += VSIFWriteL("\n", 1, 1, fp);
        if( nCount != 2 )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
            return CE_Failure;
        }
    }

    bHDRDirty = false;

    if( VSIFCloseL(fp) != 0 )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                             RewriteSTX()                             */
/************************************************************************/

CPLErr EHdrDataset::RewriteSTX() const
{
    const CPLString osPath = CPLGetPath(GetDescription());
    const CPLString osName = CPLGetBasename(GetDescription());
    const CPLString osSTXFilename = CPLFormCIFilename(osPath, osName, "stx");

    VSILFILE *fp = VSIFOpenL(osSTXFilename, "wt");
    if( fp == nullptr )
    {
        CPLDebug("EHDR", "Failed to rewrite .stx file %s.",
                 osSTXFilename.c_str());
        return CE_Failure;
    }

    bool bOK = true;
    for ( int i = 0; bOK && i < nBands; ++i )
    {
        EHdrRasterBand* poBand =
            reinterpret_cast<EHdrRasterBand *>(papoBands[i]);
        bOK &= VSIFPrintfL(fp, "%d %.10f %.10f ", i + 1,
                           poBand->dfMin, poBand->dfMax) >= 0;
        if ( poBand->minmaxmeanstddev & HAS_MEAN_FLAG )
            bOK &= VSIFPrintfL(fp, "%.10f ", poBand->dfMean) >= 0;
        else
            bOK &= VSIFPrintfL(fp, "# ") >= 0;

        if ( poBand->minmaxmeanstddev & HAS_STDDEV_FLAG )
            bOK &= VSIFPrintfL(fp, "%.10f\n", poBand->dfStdDev) >= 0;
        else
            bOK &= VSIFPrintfL(fp, "#\n") >= 0;
    }

    if( VSIFCloseL(fp) != 0 )
        bOK = false;

    return bOK ? CE_None : CE_Failure;
}

/************************************************************************/
/*                              ReadSTX()                               */
/************************************************************************/

CPLErr EHdrDataset::ReadSTX() const
{
    const CPLString osPath = CPLGetPath(GetDescription());
    const CPLString osName = CPLGetBasename(GetDescription());
    const CPLString osSTXFilename = CPLFormCIFilename(osPath, osName, "stx");

    VSILFILE *fp = VSIFOpenL(osSTXFilename, "rt");
    if (fp == nullptr)
        return CE_None;

    const char *pszLine = nullptr;
    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        char **papszTokens =
            CSLTokenizeStringComplex(pszLine, " \t", TRUE, FALSE);
        const int nTokens = CSLCount(papszTokens);
        if( nTokens >= 5 )
        {
            const int i = atoi(papszTokens[0]);
            if (i > 0 && i <= nBands)
            {
                EHdrRasterBand *poBand =
                    reinterpret_cast<EHdrRasterBand *>(papoBands[i - 1]);
                poBand->dfMin = CPLAtof(papszTokens[1]);
                poBand->dfMax = CPLAtof(papszTokens[2]);

                int bNoDataSet = FALSE;
                const double dfNoData = poBand->GetNoDataValue(&bNoDataSet);
                if (bNoDataSet && dfNoData == poBand->dfMin)
                {
                    // Triggered by
                    // /vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/HongKong/srtm/n22e113.zip/n22e113.bil
                    CPLDebug(
                        "EHDr",
                        "Ignoring .stx file where min == nodata. "
                        "The nodata value should not be taken into account "
                        "in minimum value computation.");
                    CSLDestroy(papszTokens);
                    papszTokens = nullptr;
                    break;
                }

                poBand->minmaxmeanstddev = HAS_MIN_FLAG | HAS_MAX_FLAG;
                // Reads optional mean and stddev.
                if (!EQUAL(papszTokens[3], "#") )
                {
                    poBand->dfMean = CPLAtof(papszTokens[3]);
                    poBand->minmaxmeanstddev |= HAS_MEAN_FLAG;
                }
                if ( !EQUAL(papszTokens[4], "#") )
                {
                    poBand->dfStdDev = CPLAtof(papszTokens[4]);
                    poBand->minmaxmeanstddev |= HAS_STDDEV_FLAG;
                }

                if( nTokens >= 6 && !EQUAL(papszTokens[5], "#") )
                    poBand->SetMetadataItem("STRETCHMIN", papszTokens[5],
                                            "RENDERING_HINTS");

                if( nTokens >= 7 && !EQUAL(papszTokens[6], "#") )
                    poBand->SetMetadataItem("STRETCHMAX", papszTokens[6],
                                            "RENDERING_HINTS");
            }
        }

        CSLDestroy(papszTokens);
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

    return CE_None;
}

/************************************************************************/
/*                      GetImageRepFilename()                           */
/************************************************************************/

// Check for IMAGE.REP (Spatiocarte Defense 1.0) or name_of_image.rep
// if it is a GIS-GeoSPOT image.
// For the specification of SPDF (in French), see
//   http://eden.ign.fr/download/pub/doc/emabgi/spdf10.pdf/download

CPLString EHdrDataset::GetImageRepFilename(const char *pszFilename)
{

    const CPLString osPath = CPLGetPath(pszFilename);
    const CPLString osName = CPLGetBasename(pszFilename);
    const CPLString osREPFilename = CPLFormCIFilename(osPath, osName, "rep");

    VSIStatBufL sStatBuf;
    if( VSIStatExL(
            osREPFilename.c_str(), &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
        return osREPFilename;

    if (EQUAL(CPLGetFilename(pszFilename), "imspatio.bil") ||
        EQUAL(CPLGetFilename(pszFilename), "haspatio.bil"))
    {
        CPLString osImageRepFilename(CPLFormCIFilename(osPath, "image", "rep"));
        if( VSIStatExL(
                osImageRepFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
            return osImageRepFilename;

        // Try in the upper directories if not found in the BIL image directory.
        CPLString dirName(CPLGetDirname(osPath));
        if (CPLIsFilenameRelative(osPath.c_str()))
        {
            char *cwd = CPLGetCurrentDir();
            if (cwd)
            {
                dirName = CPLFormFilename(cwd, dirName.c_str(), nullptr);
                CPLFree(cwd);
            }
        }
        while( dirName[0] != 0 && EQUAL(dirName, ".") == FALSE &&
               EQUAL(dirName, "/") == FALSE )
        {
            osImageRepFilename =
                CPLFormCIFilename(dirName.c_str(), "image", "rep");
            if( VSIStatExL(
                    osImageRepFilename.c_str(), &sStatBuf,
                    VSI_STAT_EXISTS_FLAG) == 0 )
                return osImageRepFilename;

            // Don't try to recurse above the 'image' subdirectory.
            if( EQUAL(dirName, "image") )
            {
                break;
            }
            dirName = CPLString(CPLGetDirname(dirName));
        }
    }
    return CPLString();
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **EHdrDataset::GetFileList()

{
    const CPLString osPath = CPLGetPath(GetDescription());
    const CPLString osName = CPLGetBasename(GetDescription());

    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    // Header file.
    CPLString osFilename = CPLFormCIFilename(osPath, osName, osHeaderExt);
    papszFileList = CSLAddString(papszFileList, osFilename);

    // Statistics file
    osFilename = CPLFormCIFilename(osPath, osName, "stx");
    VSIStatBufL sStatBuf;
    if( VSIStatExL(osFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
        papszFileList = CSLAddString(papszFileList, osFilename);

    // color table file.
    osFilename = CPLFormCIFilename(osPath, osName, "clr");
    if( VSIStatExL(osFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
        papszFileList = CSLAddString(papszFileList, osFilename);

    // projections file.
    osFilename = CPLFormCIFilename(osPath, osName, "prj");
    if( VSIStatExL(osFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
        papszFileList = CSLAddString(papszFileList, osFilename);

    const CPLString imageRepFilename = GetImageRepFilename(GetDescription());
    if (!imageRepFilename.empty())
        papszFileList = CSLAddString(papszFileList, imageRepFilename.c_str());

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *EHdrDataset::Open( GDALOpenInfo * poOpenInfo )

{
    return Open(poOpenInfo, true);
}

GDALDataset *EHdrDataset::Open( GDALOpenInfo * poOpenInfo, bool bFileSizeCheck )

{
    // Assume the caller is pointing to the binary (i.e. .bil) file.
    if( poOpenInfo->nHeaderBytes < 2 || poOpenInfo->fpL == nullptr )
        return nullptr;

    // Tear apart the filename to form a .HDR filename.
    const CPLString osPath = CPLGetPath(poOpenInfo->pszFilename);
    const CPLString osName = CPLGetBasename(poOpenInfo->pszFilename);

    const char *pszHeaderExt = "hdr";
    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "SRC") &&
        osName.size() == 7 &&
        (osName[0] == 'e' || osName[0] == 'E' ||
         osName[0] == 'w' || osName[0] == 'W') &&
        (osName[4] == 'n' || osName[4] == 'N' ||
         osName[4] == 's' || osName[4] == 'S') )
    {
        // It is a GTOPO30 or SRTM30 source file, whose header extension is .sch
        // see http://dds.cr.usgs.gov/srtm/version1/SRTM30/GTOPO30_Documentation
        pszHeaderExt = "sch";
    }

    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    CPLString osHDRFilename;
    if( papszSiblingFiles )
    {
        const int iFile = CSLFindString(
            papszSiblingFiles, CPLFormFilename(nullptr, osName, pszHeaderExt));
        if( iFile < 0 )  // Return if there is no corresponding .hdr file.
            return nullptr;

        osHDRFilename = CPLFormFilename(osPath, papszSiblingFiles[iFile], nullptr);
    }
    else
    {
        osHDRFilename = CPLFormCIFilename(osPath, osName, pszHeaderExt);
    }

    const bool bSelectedHDR = EQUAL(osHDRFilename, poOpenInfo->pszFilename);

    // Do we have a .hdr file?
    VSILFILE *fp = VSIFOpenL(osHDRFilename, "r");
    if( fp == nullptr )
    {
        return nullptr;
    }

    // Is this file an ESRI header file?  Read a few lines of text
    // searching for something starting with nrows or ncols.
    int nRows = -1;
    int nCols = -1;
    int nBands = 1;
    int nSkipBytes = 0;
    double dfULXMap = 0.5;
    double dfULYMap = 0.5;
    double dfYLLCorner = -123.456;
    int bCenter = TRUE;
    double dfXDim = 1.0;
    double dfYDim = 1.0;
    double dfNoData = 0.0;
    int nLineCount = 0;
    int bNoDataSet = FALSE;
    GDALDataType eDataType = GDT_Byte;
    int nBits = -1;
    char chByteOrder = 'M';
    char chPixelType = 'N';  // Not defined.
    char szLayout[10] = "BIL";
    char **papszHDR = nullptr;
    int bHasInternalProjection = FALSE;
    int bHasMin = FALSE;
    int bHasMax = FALSE;
    double dfMin = 0;
    double dfMax = 0;

    const char *pszLine = nullptr;
    while( (pszLine = CPLReadLineL(fp)) != nullptr )
    {
        nLineCount++;

        if( nLineCount > 50 || strlen(pszLine) > 1000 )
            break;

        papszHDR = CSLAddString(papszHDR, pszLine);

        char **papszTokens =
            CSLTokenizeStringComplex(pszLine, " \t", TRUE, FALSE);
        if( CSLCount(papszTokens) < 2 )
        {
            CSLDestroy(papszTokens);
            continue;
        }

        if( EQUAL(papszTokens[0], "ncols") )
        {
            nCols = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "nrows") )
        {
            nRows = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "skipbytes") )
        {
            nSkipBytes = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "ulxmap") ||
                 EQUAL(papszTokens[0], "xllcorner") ||
                 EQUAL(papszTokens[0], "xllcenter") )
        {
            dfULXMap = CPLAtofM(papszTokens[1]);
            if( EQUAL(papszTokens[0], "xllcorner") )
                bCenter = FALSE;
        }
        else if( EQUAL(papszTokens[0], "ulymap") )
        {
            dfULYMap = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "yllcorner") ||
                 EQUAL(papszTokens[0], "yllcenter") )
        {
            dfYLLCorner = CPLAtofM(papszTokens[1]);
            if( EQUAL(papszTokens[0], "yllcorner") )
                bCenter = FALSE;
        }
        else if( EQUAL(papszTokens[0], "xdim") )
        {
            dfXDim = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "ydim") )
        {
            dfYDim = CPLAtofM(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "cellsize") )
        {
            dfXDim = CPLAtofM(papszTokens[1]);
            dfYDim = dfXDim;
        }
        else if( EQUAL(papszTokens[0], "nbands") )
        {
            nBands = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "layout") )
        {
            snprintf( szLayout, sizeof(szLayout), "%s", papszTokens[1] );
        }
        else if( EQUAL(papszTokens[0], "NODATA_value") ||
                 EQUAL(papszTokens[0], "NODATA") )
        {
            dfNoData = CPLAtofM(papszTokens[1]);
            bNoDataSet = TRUE;
        }
        else if( EQUAL(papszTokens[0], "NBITS") )
        {
            nBits = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "PIXELTYPE") )
        {
            chPixelType = static_cast<char>(toupper(papszTokens[1][0]));
        }
        else if( EQUAL(papszTokens[0], "byteorder") )
        {
            chByteOrder = static_cast<char>(toupper(papszTokens[1][0]));
        }

        // http://www.worldclim.org/futdown.htm have the projection extensions
        else if( EQUAL(papszTokens[0], "Projection") )
        {
            bHasInternalProjection = TRUE;
        }
        else if( EQUAL(papszTokens[0], "MinValue") ||
                 EQUAL(papszTokens[0], "MIN_VALUE") )
        {
            dfMin = CPLAtofM(papszTokens[1]);
            bHasMin = TRUE;
        }
        else if( EQUAL(papszTokens[0], "MaxValue") ||
                 EQUAL(papszTokens[0], "MAX_VALUE") )
        {
            dfMax = CPLAtofM(papszTokens[1]);
            bHasMax = TRUE;
        }

        CSLDestroy(papszTokens);
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

    // Did we get the required keywords?  If not, return with this never having
    // been considered to be a match. This isn't an error!
    if( nRows == -1 || nCols == -1 )
    {
        CSLDestroy(papszHDR);
        return nullptr;
    }

    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        CSLDestroy(papszHDR);
        return nullptr;
    }

    // Has the caller selected the .hdr file to open?
    if( bSelectedHDR )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The selected file is an ESRI BIL header file, but to "
                 "open ESRI BIL datasets, the data file should be selected "
                 "instead of the .hdr file.  Please try again selecting "
                 "the data file (often with the extension .bil) corresponding "
                 "to the header file: %s",
                 poOpenInfo->pszFilename);
        CSLDestroy(papszHDR);
        return nullptr;
    }

    // If we aren't sure of the file type, check the data file size.  If it is 4
    // bytes or more per pixel then we assume it is floating point data.
    if( nBits == -1 && chPixelType == 'N' )
    {
        VSIStatBufL sStatBuf;
        if( VSIStatL(poOpenInfo->pszFilename, &sStatBuf) == 0 )
        {
            const size_t nBytes =
                static_cast<size_t>(sStatBuf.st_size/nCols/nRows/nBands);
            if( nBytes > 0 && nBytes != 3 )
                nBits = static_cast<int>(nBytes * 8);

            if( nBytes == 4 )
                chPixelType = 'F';
        }
    }

    // If the extension is FLT it is likely a floating point file.
    if( chPixelType == 'N' )
    {
        if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "FLT") )
            chPixelType = 'F';
    }

    // If we have a negative nodata value, assume that the
    // pixel type is signed. This is necessary for datasets from
    // http://www.worldclim.org/futdown.htm

    if( bNoDataSet && dfNoData < 0 && chPixelType == 'N' )
    {
        chPixelType = 'S';
    }

    EHdrDataset *poDS = new EHdrDataset();

    poDS->osHeaderExt = pszHeaderExt;

    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->papszHDR = papszHDR;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->eAccess = poOpenInfo->eAccess;

    // Figure out the data type.
    if( nBits == 16 )
    {
        if ( chPixelType == 'S' )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;  // Default
    }
    else if( nBits == 32 )
    {
        if( chPixelType == 'S' )
            eDataType = GDT_Int32;
        else if( chPixelType == 'F' )
            eDataType = GDT_Float32;
        else
            eDataType = GDT_UInt32;  // Default
    }
    else if( nBits >= 1 && nBits <= 8 )
    {
        eDataType = GDT_Byte;
        nBits = 8;
    }
    else if( nBits == -1 )
    {
        if( chPixelType == 'F' )
        {
            eDataType = GDT_Float32;
            nBits = 32;
        }
        else
        {
            eDataType = GDT_Byte;
            nBits = 8;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "EHdr driver does not support %d NBITS value.",
                 nBits);
        delete poDS;
        return nullptr;
    }

    // Compute the line offset.
    const int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
    CPLAssert(nItemSize != 0);
    CPLAssert(nBands != 0);

    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;

    if( EQUAL(szLayout, "BIP") )
    {
        if (nCols > std::numeric_limits<int>::max() / (nItemSize * nBands))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            delete poDS;
            return nullptr;
        }
        nPixelOffset = nItemSize * nBands;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize);
    }
    else if( EQUAL(szLayout, "BSQ") )
    {
        if (nCols > std::numeric_limits<int>::max() / nItemSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            delete poDS;
            return nullptr;
        }
        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else
    {
        // Assume BIL.
        if (nCols > std::numeric_limits<int>::max() / (nItemSize * nBands))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
            delete poDS;
            return nullptr;
        }
        nPixelOffset = nItemSize;
        nLineOffset = nItemSize * nBands * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize) * nCols;
    }

    if( nBits >= 8 && bFileSizeCheck &&
        !RAWDatasetCheckMemoryUsage(
                        poDS->nRasterXSize, poDS->nRasterYSize, nBands,
                        nItemSize,
                        nPixelOffset, nLineOffset, nSkipBytes, nBandOffset,
                        poDS->fpImage) )
    {
        delete poDS;
        return nullptr;
    }

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->PamInitialize();

    // Create band information objects.
    poDS->nBands = nBands;
    CPLErrorReset();
    for( int i = 0; i < poDS->nBands; i++ )
    {
        EHdrRasterBand *poBand = new EHdrRasterBand(
            poDS, i + 1, poDS->fpImage, nSkipBytes + nBandOffset * i,
            nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB
            chByteOrder == 'I' || chByteOrder == 'L',
#else
            chByteOrder == 'M',
#endif
            nBits);

        poBand->bNoDataSet = bNoDataSet;
        poBand->dfNoData = dfNoData;

        if( bHasMin && bHasMax )
        {
            poBand->dfMin = dfMin;
            poBand->dfMax = dfMax;
            poBand->minmaxmeanstddev = HAS_MIN_FLAG | HAS_MAX_FLAG;
        }

        poDS->SetBand(i + 1, poBand);
        if( CPLGetLastErrorType() != CE_None )
        {
            poDS->nBands = i + 1;
            delete poDS;
            return nullptr;
        }
    }

    // If we didn't get bounds in the .hdr, look for a worldfile.
    if( dfYLLCorner != -123.456 )
    {
        if( bCenter )
            dfULYMap = dfYLLCorner + (nRows-1) * dfYDim;
        else
            dfULYMap = dfYLLCorner + nRows * dfYDim;
    }

    if( dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->bGotTransform = true;

        if( bCenter )
        {
            poDS->adfGeoTransform[0] = dfULXMap - dfXDim * 0.5;
            poDS->adfGeoTransform[1] = dfXDim;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = dfULYMap + dfYDim * 0.5;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = - dfYDim;
        }
        else
        {
            poDS->adfGeoTransform[0] = dfULXMap;
            poDS->adfGeoTransform[1] = dfXDim;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = dfULYMap;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = - dfYDim;
        }
    }

    if( !poDS->bGotTransform )
        poDS->bGotTransform = CPL_TO_BOOL(
            GDALReadWorldFile(poOpenInfo->pszFilename, nullptr,
                              poDS->adfGeoTransform));

    if( !poDS->bGotTransform )
        poDS->bGotTransform = CPL_TO_BOOL(
            GDALReadWorldFile(poOpenInfo->pszFilename, "wld",
                              poDS->adfGeoTransform));

    // Check for a .prj file.
    const char *pszPrjFilename = CPLFormCIFilename(osPath, osName, "prj");

    fp = VSIFOpenL(pszPrjFilename, "r");

    // .hdr files from http://www.worldclim.org/futdown.htm have the projection
    // info in the .hdr file itself.
    if (fp == nullptr && bHasInternalProjection)
    {
        pszPrjFilename = osHDRFilename;
        fp = VSIFOpenL(pszPrjFilename, "r");
    }

    if( fp != nullptr )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        fp = nullptr;

        char **papszLines = CSLLoad(pszPrjFilename);

        OGRSpatialReference oSRS;
        if( oSRS.importFromESRI(papszLines) == OGRERR_NONE )
        {
            // If geographic values are in seconds, we must transform.
            // Is there a code for minutes too?
            char szResult[80] = { '\0' };
            if( oSRS.IsGeographic()
                && EQUAL(OSR_GDS(szResult, sizeof(szResult),
                                 papszLines, "Units", ""), "DS") )
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

        CSLDestroy(papszLines);
    }
    else
    {
        // Check for IMAGE.REP (Spatiocarte Defense 1.0) or name_of_image.rep
        // if it is a GIS-GeoSPOT image
        // For the specification of SPDF (in French), see
        //   http://eden.ign.fr/download/pub/doc/emabgi/spdf10.pdf/download
        const CPLString szImageRepFilename =
            GetImageRepFilename(poOpenInfo->pszFilename);
        if (!szImageRepFilename.empty())
        {
            fp = VSIFOpenL(szImageRepFilename.c_str(), "r");
        }
        if (fp != nullptr)
        {
            bool bUTM = false;
            bool bWGS84 = false;
            int bNorth = FALSE;
            bool bSouth = false;
            int utmZone = 0;

            while( (pszLine = CPLReadLineL(fp)) != nullptr )
            {
                if (STARTS_WITH(pszLine, "PROJ_ID") &&
                    strstr(pszLine, "UTM"))
                {
                    bUTM = true;
                }
                else if (STARTS_WITH(pszLine, "PROJ_ZONE"))
                {
                    const char *c = strchr(pszLine, '"');
                    if (c)
                    {
                        c++;
                        if (*c >= '0' && *c <= '9')
                        {
                            utmZone = atoi(c);
                            if (utmZone >= 1 && utmZone <= 60)
                            {
                                if( strstr(pszLine, "Nord") ||
                                    strstr(pszLine, "NORD") )
                                {
                                    bNorth = TRUE;
                                }
                                else if( strstr(pszLine, "Sud") ||
                                         strstr(pszLine, "SUD") )
                                {
                                    bSouth = true;
                                }
                            }
                        }
                    }
                }
                else if (STARTS_WITH(pszLine, "PROJ_CODE") &&
                         strstr(pszLine, "FR-MINDEF"))
                {
                    const char *c = strchr(pszLine, 'A');
                    if (c)
                    {
                        c++;
                        if (*c >= '0' && *c <= '9')
                        {
                            utmZone = atoi(c);
                            if (utmZone >= 1 && utmZone <= 60)
                            {
                                if (c[1] == 'N' ||
                                    (c[1] != '\0' && c[2] == 'N'))
                                {
                                    bNorth = TRUE;
                                }
                                else if (c[1] == 'S' ||
                                         (c[1] != '\0' && c[2] == 'S'))
                                {
                                    bSouth = true;
                                }
                            }
                        }
                    }
                }
                else if( STARTS_WITH(pszLine, "HORIZ_DATUM") &&
                         (strstr(pszLine, "WGS 84") ||
                          strstr(pszLine, "WGS84")) )
                {
                    bWGS84 = true;
                }
                else if(STARTS_WITH(pszLine, "MAP_NUMBER") )
                {
                    const char *c = strchr(pszLine, '"');
                    if (c)
                    {
                        char *pszMapNumber = CPLStrdup(c + 1);
                        char *c2 = strchr(pszMapNumber, '"');
                        if (c2)
                            *c2 = 0;
                        poDS->SetMetadataItem("SPDF_MAP_NUMBER", pszMapNumber);
                        CPLFree(pszMapNumber);
                    }
                }
                else if (STARTS_WITH(pszLine, "PRODUCTION_DATE"))
                {
                    const char *c = pszLine + strlen("PRODUCTION_DATE");
                    while(*c == ' ')
                        c++;
                    if (*c)
                    {
                        poDS->SetMetadataItem("SPDF_PRODUCTION_DATE", c);
                    }
                }
            }

            CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

            if (utmZone >= 1 && utmZone <= 60 &&
                bUTM && bWGS84 && (bNorth || bSouth))
            {
                char projCSStr[64] = { '\0' };
                snprintf(projCSStr, sizeof(projCSStr), "WGS 84 / UTM zone %d%c",
                         utmZone, (bNorth) ? 'N' : 'S');

                OGRSpatialReference oSRS;
                oSRS.SetProjCS(projCSStr);
                oSRS.SetWellKnownGeogCS("WGS84");
                oSRS.SetUTM(utmZone, bNorth);
                oSRS.SetAuthority("PROJCS", "EPSG",
                                  (bNorth ? 32600 : 32700) + utmZone);
                oSRS.AutoIdentifyEPSG();

                CPLFree(poDS->pszProjection);
                oSRS.exportToWkt(&(poDS->pszProjection));
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Cannot retrieve projection from IMAGE.REP");
            }
        }
    }

    // Check for a color table.
    const char *pszCLRFilename = CPLFormCIFilename(osPath, osName, "clr");

    // Only read the .clr for byte, int16 or uint16 bands.
    if (nItemSize <= 2)
        fp = VSIFOpenL(pszCLRFilename, "r");
    else
        fp = nullptr;

    if( fp != nullptr )
    {
        std::shared_ptr<GDALRasterAttributeTable> poRat(
            new GDALDefaultRasterAttributeTable());
        poRat->CreateColumn("Value", GFT_Integer, GFU_Generic);
        poRat->CreateColumn("Red", GFT_Integer, GFU_Red);
        poRat->CreateColumn("Green", GFT_Integer, GFU_Green);
        poRat->CreateColumn("Blue", GFT_Integer, GFU_Blue);

        poDS->m_poColorTable.reset(new GDALColorTable());

        bool bHasFoundNonCTValues = false;
        int nRatRow = 0;

        while( true )
        {
            pszLine = CPLReadLineL(fp);
            if ( !pszLine )
                break;

            if( *pszLine == '#' || *pszLine == '!' )
                continue;

            char **papszValues =
                CSLTokenizeString2(pszLine, "\t ", CSLT_HONOURSTRINGS);

            if ( CSLCount(papszValues) >= 4 )
            {
                const int nIndex = atoi(papszValues[0]);
                poRat->SetValue(nRatRow, 0, nIndex);
                poRat->SetValue(nRatRow, 1, atoi(papszValues[1]));
                poRat->SetValue(nRatRow, 2, atoi(papszValues[2]));
                poRat->SetValue(nRatRow, 3, atoi(papszValues[3]));
                nRatRow ++;

                if (nIndex >= 0 && nIndex < 65536)
                {
                    const GDALColorEntry oEntry =
                    {
                        static_cast<short>(atoi(papszValues[1])),  // Red
                        static_cast<short>(atoi(papszValues[2])),  // Green
                        static_cast<short>(atoi(papszValues[3])),  // Blue
                        255
                    };

                    poDS->m_poColorTable->SetColorEntry(nIndex, &oEntry);
                }
                else
                {
                    // Negative values are valid. At least we can find use of
                    // them here:
                    //   http://www.ngdc.noaa.gov/mgg/topo/elev/esri/clr/
                    // But, there's no way of representing them with GDAL color
                    // table model.
                    if (!bHasFoundNonCTValues)
                        CPLDebug("EHdr", "Ignoring color index : %d", nIndex);
                    bHasFoundNonCTValues = true;
                }
            }

            CSLDestroy(papszValues);
        }

        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

        if( bHasFoundNonCTValues )
        {
            poDS->m_poRAT.swap(poRat);
        }

        for( int i = 1; i <= poDS->nBands; i++ )
        {
            EHdrRasterBand *poBand =
                    dynamic_cast<EHdrRasterBand*>(poDS->GetRasterBand(i));
            poBand->m_poColorTable = poDS->m_poColorTable;
            poBand->m_poRAT = poDS->m_poRAT;
            poBand->SetColorInterpretation(GCI_PaletteIndex);
        }

        poDS->bCLRDirty = false;
    }

    // Read statistics (.STX).
    poDS->ReadSTX();

    // Initialize any PAM information.
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *EHdrDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszParamList )

{
    // Verify input options.
    if (nBands <= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "EHdr driver does not support %d bands.", nBands);
        return nullptr;
    }

    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16 &&
        eType != GDT_Int16 && eType != GDT_Int32 && eType != GDT_UInt32 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create ESRI .hdr labelled dataset with an illegal"
                 "data type (%s).",
                 GDALGetDataTypeName(eType));

        return nullptr;
    }

    // Try to create the file.
    VSILFILE *fp = VSIFOpenL(pszFilename, "wb");

    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.",
                 pszFilename);
        return nullptr;
    }

    // Just write out a couple of bytes to establish the binary
    // file, and then close it.
    bool bOK = VSIFWriteL(reinterpret_cast<void *>(const_cast<char *>("\0\0")),
                          2, 1, fp) == 1;
    if( VSIFCloseL(fp) != 0 )
        bOK = false;
    fp = nullptr;
    if( !bOK )
        return nullptr;

    // Create the hdr filename.
    char *const pszHdrFilename =
        CPLStrdup(CPLResetExtension(pszFilename, "hdr"));

    // Open the file.
    fp = VSIFOpenL(pszHdrFilename, "wt");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file `%s' failed.",
                 pszHdrFilename);
        CPLFree(pszHdrFilename);
        return nullptr;
    }

    // Decide how many bits the file should have.
    int nBits = GDALGetDataTypeSize(eType);

    if( CSLFetchNameValue(papszParamList, "NBITS") != nullptr )
        nBits = atoi(CSLFetchNameValue(papszParamList, "NBITS"));

    const int nRowBytes = (nBits * nXSize + 7) / 8;

    // Check for signed byte.
    const char *pszPixelType = CSLFetchNameValue(papszParamList, "PIXELTYPE");
    if( pszPixelType == nullptr )
        pszPixelType = "";

    // Write out the raw definition for the dataset as a whole.
    bOK &= VSIFPrintfL(fp, "BYTEORDER      I\n") >= 0;
    bOK &= VSIFPrintfL(fp, "LAYOUT         BIL\n") >= 0;
    bOK &= VSIFPrintfL(fp, "NROWS          %d\n", nYSize) >= 0;
    bOK &= VSIFPrintfL(fp, "NCOLS          %d\n", nXSize) >= 0;
    bOK &= VSIFPrintfL(fp, "NBANDS         %d\n", nBands) >= 0;
    bOK &= VSIFPrintfL(fp, "NBITS          %d\n", nBits) >= 0;
    bOK &= VSIFPrintfL(fp, "BANDROWBYTES   %d\n", nRowBytes) >= 0;
    bOK &= VSIFPrintfL(fp, "TOTALROWBYTES  %d\n", nRowBytes * nBands) >= 0;

    if( eType == GDT_Float32 )
        bOK &= VSIFPrintfL(fp, "PIXELTYPE      FLOAT\n") >= 0;
    else if( eType == GDT_Int16 || eType == GDT_Int32 )
        bOK &= VSIFPrintfL(fp, "PIXELTYPE      SIGNEDINT\n") >= 0;
    else if( eType == GDT_Byte && EQUAL(pszPixelType, "SIGNEDBYTE") )
        bOK &= VSIFPrintfL(fp, "PIXELTYPE      SIGNEDINT\n") >= 0;
    else
        bOK &= VSIFPrintfL(fp, "PIXELTYPE      UNSIGNEDINT\n") >= 0;

    if( VSIFCloseL(fp) != 0 )
        bOK = false;

    CPLFree(pszHdrFilename);

    if( !bOK )
        return nullptr;

    GDALOpenInfo oOpenInfo( pszFilename, GA_Update );
    return Open(&oOpenInfo, false);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *EHdrDataset::CreateCopy( const char * pszFilename,
                                      GDALDataset * poSrcDS,
                                      int bStrict, char ** papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void * pProgressData )

{
    const int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "EHdr driver does not support source dataset without any "
                 "bands.");
        return nullptr;
    }

    char **papszAdjustedOptions = CSLDuplicate(papszOptions);

    // Ensure we pass on NBITS and PIXELTYPE structure information.
    if( poSrcDS->GetRasterBand(1)->GetMetadataItem("NBITS",
                                                   "IMAGE_STRUCTURE") != nullptr
        && CSLFetchNameValue(papszOptions, "NBITS") == nullptr )
    {
        papszAdjustedOptions =
            CSLSetNameValue(papszAdjustedOptions, "NBITS",
                            poSrcDS->GetRasterBand(1)->GetMetadataItem(
                                "NBITS", "IMAGE_STRUCTURE"));
    }

    if( poSrcDS->GetRasterBand(1)->GetMetadataItem("PIXELTYPE",
                                                   "IMAGE_STRUCTURE") != nullptr
        && CSLFetchNameValue(papszOptions, "PIXELTYPE") == nullptr )
    {
        papszAdjustedOptions =
            CSLSetNameValue(papszAdjustedOptions, "PIXELTYPE",
                            poSrcDS->GetRasterBand(1)->GetMetadataItem(
                                "PIXELTYPE", "IMAGE_STRUCTURE"));
    }

    // Proceed with normal copying using the default createcopy  operators.
    GDALDriver *poDriver =
        reinterpret_cast<GDALDriver *>(GDALGetDriverByName("EHdr"));

    GDALDataset *poOutDS = poDriver->DefaultCreateCopy(
        pszFilename, poSrcDS, bStrict, papszAdjustedOptions, pfnProgress,
        pProgressData);
    CSLDestroy(papszAdjustedOptions);

    if( poOutDS != nullptr )
        poOutDS->FlushCache();

    return poOutDS;
}

/************************************************************************/
/*                        GetNoDataValue()                              */
/************************************************************************/

double EHdrRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    if( bNoDataSet )
        return dfNoData;

    return RawRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                           GetMinimum()                               */
/************************************************************************/

double EHdrRasterBand::GetMinimum( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = (minmaxmeanstddev & HAS_MIN_FLAG) != 0;

    if( minmaxmeanstddev & HAS_MIN_FLAG )
        return dfMin;

    return RawRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                           GetMaximum()                               */
/************************************************************************/

double EHdrRasterBand::GetMaximum( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = (minmaxmeanstddev & HAS_MAX_FLAG) != 0;

    if( minmaxmeanstddev & HAS_MAX_FLAG )
      return dfMax;

    return RawRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr EHdrRasterBand::GetStatistics(
    int bApproxOK, int bForce,
    double *pdfMin, double *pdfMax,
    double *pdfMean, double *pdfStdDev )
{
    if( !(GetMetadataItem("STATISTICS_APPROXIMATE") && !bApproxOK) )
    {
        if( (minmaxmeanstddev & HAS_ALL_FLAGS) == HAS_ALL_FLAGS)
        {
            if( pdfMin ) *pdfMin = dfMin;
            if( pdfMax ) *pdfMax = dfMax;
            if( pdfMean ) *pdfMean = dfMean;
            if( pdfStdDev ) *pdfStdDev = dfStdDev;
            return CE_None;
        }
    }

    const CPLErr eErr = RawRasterBand::GetStatistics(bApproxOK, bForce,
                                                     &dfMin, &dfMax,
                                                     &dfMean, &dfStdDev);
    if( eErr != CE_None )
        return eErr;

    EHdrDataset *poEDS = reinterpret_cast<EHdrDataset *>(poDS);

    minmaxmeanstddev = HAS_ALL_FLAGS;

    if( !bApproxOK && poEDS->RewriteSTX() != CE_None )
        RawRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);

    if( pdfMin )
        *pdfMin = dfMin;
    if( pdfMax )
        *pdfMax = dfMax;
    if( pdfMean )
        *pdfMean = dfMean;
    if( pdfStdDev )
        *pdfStdDev = dfStdDev;

    return CE_None;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr EHdrRasterBand::SetStatistics( double dfMinIn, double dfMaxIn,
                                      double dfMeanIn, double dfStdDevIn )
{
    // Avoid churn if nothing is changing.
    if( dfMin == dfMinIn
        && dfMax == dfMaxIn
        && dfMean == dfMeanIn
        && dfStdDev == dfStdDevIn )
        return CE_None;

    dfMin = dfMinIn;
    dfMax = dfMaxIn;
    dfMean = dfMeanIn;
    dfStdDev = dfStdDevIn;

    // marks stats valid
    minmaxmeanstddev = HAS_ALL_FLAGS;

    EHdrDataset *poEDS = reinterpret_cast<EHdrDataset *>(poDS);

    if( GetMetadataItem( "STATISTICS_APPROXIMATE" ) == nullptr )
    {
        if( GetMetadataItem("STATISTICS_MINIMUM") )
        {
            SetMetadataItem( "STATISTICS_MINIMUM", nullptr );
            SetMetadataItem( "STATISTICS_MAXIMUM", nullptr );
            SetMetadataItem( "STATISTICS_MEAN", nullptr );
            SetMetadataItem( "STATISTICS_STDDEV", nullptr );
        }
        return poEDS->RewriteSTX();
    }

    return RawRasterBand::SetStatistics(
        dfMinIn, dfMaxIn, dfMeanIn, dfStdDevIn);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable* EHdrRasterBand::GetColorTable()
{
    return m_poColorTable.get();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr EHdrRasterBand::SetColorTable( GDALColorTable *poNewCT )
{
    if( poNewCT == nullptr )
        m_poColorTable.reset();
    else
        m_poColorTable.reset(poNewCT->Clone());

    reinterpret_cast<EHdrDataset *>(poDS)->bCLRDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable* EHdrRasterBand::GetDefaultRAT()
{
    return m_poRAT.get();
}

/************************************************************************/
/*                            SetDefaultRAT()                           */
/************************************************************************/

CPLErr EHdrRasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )
{
    if( poRAT )
    {
        if( !(poRAT->GetColumnCount() == 4 &&
              poRAT->GetTypeOfCol(0) == GFT_Integer &&
              poRAT->GetTypeOfCol(1) == GFT_Integer &&
              poRAT->GetTypeOfCol(2) == GFT_Integer &&
              poRAT->GetTypeOfCol(3) == GFT_Integer &&
              poRAT->GetUsageOfCol(0) == GFU_Generic &&
              poRAT->GetUsageOfCol(1) == GFU_Red &&
              poRAT->GetUsageOfCol(2) == GFU_Green &&
              poRAT->GetUsageOfCol(3) == GFU_Blue) )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported type of RAT: "
                     "only value,R,G,B ones are supported");
            return CE_Failure;
        }
    }

    if( poRAT == nullptr )
        m_poRAT.reset();
    else
        m_poRAT.reset(poRAT->Clone());

    reinterpret_cast<EHdrDataset *>(poDS)->bCLRDirty = true;

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_EHdr()                          */
/************************************************************************/

void GDALRegister_EHdr()

{
    if( GDALGetDriverByName("EHdr") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("EHdr");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRI .hdr Labelled");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/ehdr.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "bil");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Float32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='NBITS' type='int' description='Special pixel bits (1-7)'/>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"</CreationOptionList>" );

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->pfnOpen = EHdrDataset::Open;
    poDriver->pfnCreate = EHdrDataset::Create;
    poDriver->pfnCreateCopy = EHdrDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
