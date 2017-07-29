/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFABand, for accessing one Eimg_Layer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "hfa_p.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "hfa.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                              HFABand()                               */
/************************************************************************/

HFABand::HFABand( HFAInfo_t * psInfoIn, HFAEntry * poNodeIn ) :
    nBlocks(0),
    panBlockStart(NULL),
    panBlockSize(NULL),
    panBlockFlag(NULL),
    nBlockStart(0),
    nBlockSize(0),
    nLayerStackCount(0),
    nLayerStackIndex(0),
    nPCTColors(-1),
    padfPCTBins(NULL),
    psInfo(psInfoIn),
    fpExternal(NULL),
    eDataType(static_cast<EPTType>(poNodeIn->GetIntField("pixelType"))),
    poNode(poNodeIn),
    nBlockXSize(poNodeIn->GetIntField("blockWidth")),
    nBlockYSize(poNodeIn->GetIntField("blockHeight")),
    nWidth(poNodeIn->GetIntField("width")),
    nHeight(poNodeIn->GetIntField("height")),
    nBlocksPerRow(0),
    nBlocksPerColumn(0),
    bNoDataSet(false),
    dfNoData(0.0),
    bOverviewsPending(true),
    nOverviews(0),
    papoOverviews(NULL)
{
    const int nDataType = poNodeIn->GetIntField("pixelType");

    apadfPCT[0] = NULL;
    apadfPCT[1] = NULL;
    apadfPCT[2] = NULL;
    apadfPCT[3] = NULL;

    if( nWidth <= 0 || nHeight <= 0 || nBlockXSize <= 0 || nBlockYSize <= 0 )
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HFABand::HFABand : (nWidth <= 0 || nHeight <= 0 || "
                 "nBlockXSize <= 0 || nBlockYSize <= 0)");
        return;
    }
    if( nDataType < EPT_MIN || nDataType > EPT_MAX )
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HFABand::HFABand : nDataType=%d unhandled", nDataType);
        return;
    }

    // TODO(schwehr): Move to initializer list.
    nBlocksPerRow = DIV_ROUND_UP(nWidth, nBlockXSize);
    nBlocksPerColumn = DIV_ROUND_UP(nHeight, nBlockYSize);

    if( nBlocksPerRow > INT_MAX / nBlocksPerColumn )
    {
        nWidth = 0;
        nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HFABand::HFABand : too big dimensions / block size");
        return;
    }
    nBlocks = nBlocksPerRow * nBlocksPerColumn;

    // Check for nodata.  This is really an RDO (ESRI Raster Data Objects?),
    // not used by Imagine itself.
    HFAEntry *poNDNode = poNode->GetNamedChild("Eimg_NonInitializedValue");

    if( poNDNode != NULL )
    {
        bNoDataSet = true;
        dfNoData = poNDNode->GetDoubleField("valueBD");
    }
}

/************************************************************************/
/*                              ~HFABand()                              */
/************************************************************************/

HFABand::~HFABand()

{
    for( int iOverview = 0; iOverview < nOverviews; iOverview++ )
        delete papoOverviews[iOverview];

    if( nOverviews > 0 )
        CPLFree(papoOverviews);

    CPLFree(panBlockStart);
    CPLFree(panBlockSize);
    CPLFree(panBlockFlag);

    CPLFree(apadfPCT[0]);
    CPLFree(apadfPCT[1]);
    CPLFree(apadfPCT[2]);
    CPLFree(apadfPCT[3]);
    CPLFree(padfPCTBins);

    if( fpExternal != NULL )
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpExternal));
}

/************************************************************************/
/*                           LoadOverviews()                            */
/************************************************************************/

CPLErr HFABand::LoadOverviews()

{
    if( !bOverviewsPending )
        return CE_None;

    bOverviewsPending = false;

    // Does this band have overviews?  Try to find them.
    HFAEntry *poRRDNames = poNode->GetNamedChild("RRDNamesList");

    if( poRRDNames != NULL )
    {
        // Limit to 1000 to avoid infinite loop as in
        // https://oss-fuzz.com/v2/testcase-detail/6206784937132032
        for( int iName = 0; iName < 1000; iName++ )
        {
            char szField[128] = {};
            snprintf(szField, sizeof(szField), "nameList[%d].string", iName);

            CPLErr eErr = CE_None;
            const char *pszName = poRRDNames->GetStringField(szField, &eErr);
            if( pszName == NULL || eErr != CE_None )
                break;

            char *pszFilename = CPLStrdup(pszName);
            char *pszEnd = strstr(pszFilename, "(:");
            if( pszEnd == NULL )
            {
                CPLFree(pszFilename);
                continue;
            }

            pszEnd[0] = '\0';

            char *pszJustFilename = CPLStrdup(CPLGetFilename(pszFilename));
            HFAInfo_t *psHFA = HFAGetDependent(psInfo, pszJustFilename);
            CPLFree(pszJustFilename);

            // Try finding the dependent file as this file with the
            // extension .rrd.  This is intended to address problems
            // with users changing the names of their files.
            if( psHFA == NULL )
            {
                char *pszBasename =
                    CPLStrdup(CPLGetBasename(psInfo->pszFilename));

                pszJustFilename =
                    CPLStrdup(CPLFormFilename(NULL, pszBasename, "rrd"));
                CPLDebug("HFA", "Failed to find overview file with "
                         "expected name, try %s instead.",
                         pszJustFilename);
                psHFA = HFAGetDependent(psInfo, pszJustFilename);
                CPLFree(pszJustFilename);
                CPLFree(pszBasename);
            }

            if( psHFA == NULL )
            {
                CPLFree(pszFilename);
                continue;
            }

            char *pszPath = pszEnd + 2;
            if( pszPath[strlen(pszPath)-1] == ')' )
                pszPath[strlen(pszPath)-1] = '\0';

            for( int i = 0; pszPath[i] != '\0'; i++ )
            {
                if( pszPath[i] == ':' )
                    pszPath[i] = '.';
            }

            HFAEntry *poOvEntry = psHFA->poRoot->GetNamedChild(pszPath);
            CPLFree(pszFilename);

            if( poOvEntry == NULL )
                continue;

            // We have an overview node.  Instantiate a HFABand from it, and
            // add to the list.
            papoOverviews = static_cast<HFABand **>(
                CPLRealloc(papoOverviews, sizeof(void *) * ++nOverviews));
            papoOverviews[nOverviews - 1] = new HFABand(psHFA, poOvEntry);
            if( papoOverviews[nOverviews-1]->nWidth == 0 )
            {
                nWidth = 0;
                nHeight = 0;
                delete papoOverviews[nOverviews - 1];
                papoOverviews[nOverviews - 1] = NULL;
                return CE_None;
            }
        }
    }

    // If there are no overviews mentioned in this file, probe for
    // an .rrd file anyways.
    HFAEntry *poBandProxyNode = poNode;
    HFAInfo_t *psOvHFA = psInfo;

    if( nOverviews == 0 &&
        EQUAL(CPLGetExtension(psInfo->pszFilename), "aux") )
    {
        const CPLString osRRDFilename =
            CPLResetExtension(psInfo->pszFilename, "rrd");
        const CPLString osFullRRD =
            CPLFormFilename(psInfo->pszPath, osRRDFilename, NULL);
        VSIStatBufL sStatBuf;

        if( VSIStatL(osFullRRD, &sStatBuf) == 0 )
        {
            psOvHFA = HFAGetDependent(psInfo, osRRDFilename);
            if( psOvHFA )
                poBandProxyNode =
                    psOvHFA->poRoot->GetNamedChild(poNode->GetName());
            else
                psOvHFA = psInfo;
        }
    }

    // If there are no named overviews, try looking for unnamed
    // overviews within the same layer, as occurs in floodplain.img
    // for instance, or in the not-referenced rrd mentioned in #3463.
    if( nOverviews == 0 && poBandProxyNode != NULL )
    {
        for( HFAEntry *poChild = poBandProxyNode->GetChild();
             poChild != NULL;
             poChild = poChild->GetNext() )
        {
            if( EQUAL(poChild->GetType(), "Eimg_Layer_SubSample") )
            {
                papoOverviews = static_cast<HFABand **>(
                    CPLRealloc(papoOverviews, sizeof(void *) * ++nOverviews));
                papoOverviews[nOverviews - 1] = new HFABand(psOvHFA, poChild);
                if( papoOverviews[nOverviews - 1]->nWidth == 0 )
                {
                    nWidth = 0;
                    nHeight = 0;
                    delete papoOverviews[nOverviews - 1];
                    papoOverviews[nOverviews - 1] = NULL;
                    return CE_None;
                }
            }
        }

        // TODO(schwehr): Can this use std::sort?
        // Bubble sort into biggest to smallest order.
        for( int i1 = 0; i1 < nOverviews; i1++ )
        {
            for( int i2 = 0; i2 < nOverviews-1; i2++ )
            {
                if( papoOverviews[i2]->nWidth <
                    papoOverviews[i2 + 1]->nWidth )
                {
                    // TODO(schwehr): Use std::swap.
                    HFABand *poTemp = papoOverviews[i2 + 1];
                    papoOverviews[i2 + 1] = papoOverviews[i2];
                    papoOverviews[i2] = poTemp;
                }
            }
        }
    }
    return CE_None;
}

/************************************************************************/
/*                           LoadBlockInfo()                            */
/************************************************************************/

CPLErr HFABand::LoadBlockInfo()

{
    if( panBlockFlag != NULL )
        return CE_None;

    HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");
    if( poDMS == NULL )
    {
        if( poNode->GetNamedChild("ExternalRasterDMS") != NULL )
            return LoadExternalBlockInfo();

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Can't find RasterDMS field in Eimg_Layer with block list.");

        return CE_Failure;
    }

    if( sizeof(vsi_l_offset) + 2 * sizeof(int) >
                    (~(size_t)0) / static_cast<unsigned int>(nBlocks) )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Too many blocks");
        return CE_Failure;
    }
    const int MAX_INITIAL_BLOCKS = 1000 * 1000;
    const int nInitBlocks = std::min(nBlocks, MAX_INITIAL_BLOCKS);
    panBlockStart = static_cast<vsi_l_offset *>(
        VSI_MALLOC2_VERBOSE(sizeof(vsi_l_offset), nInitBlocks));
    panBlockSize =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nInitBlocks));
    panBlockFlag =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nInitBlocks));

    if( panBlockStart == NULL || panBlockSize == NULL || panBlockFlag == NULL )
    {
        CPLFree(panBlockStart);
        CPLFree(panBlockSize);
        CPLFree(panBlockFlag);
        panBlockStart = NULL;
        panBlockSize = NULL;
        panBlockFlag = NULL;
        return CE_Failure;
    }

    for( int iBlock = 0; iBlock < nBlocks; iBlock++ )
    {
        CPLErr eErr = CE_None;

        if( iBlock == MAX_INITIAL_BLOCKS )
        {
            vsi_l_offset* panBlockStartNew = static_cast<vsi_l_offset *>(
                VSI_REALLOC_VERBOSE(panBlockStart,
                                    sizeof(vsi_l_offset) * nBlocks));
            if( panBlockStartNew == NULL )
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = NULL;
                panBlockSize = NULL;
                panBlockFlag = NULL;
                return CE_Failure;
            }
            panBlockStart = panBlockStartNew;

            int* panBlockSizeNew = static_cast<int *>(
                VSI_REALLOC_VERBOSE(panBlockSize,
                                    sizeof(int) * nBlocks));
            if( panBlockSizeNew == NULL )
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = NULL;
                panBlockSize = NULL;
                panBlockFlag = NULL;
                return CE_Failure;
            }
            panBlockSize = panBlockSizeNew;

            int* panBlockFlagNew = static_cast<int *>(
                VSI_REALLOC_VERBOSE(panBlockFlag,
                                    sizeof(int) * nBlocks));
            if( panBlockFlagNew == NULL )
            {
                CPLFree(panBlockStart);
                CPLFree(panBlockSize);
                CPLFree(panBlockFlag);
                panBlockStart = NULL;
                panBlockSize = NULL;
                panBlockFlag = NULL;
                return CE_Failure;
            }
            panBlockFlag = panBlockFlagNew;
        }

        char szVarName[64] = {};
        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].offset", iBlock);
        panBlockStart[iBlock] =
            static_cast<GUInt32>(poDMS->GetIntField(szVarName, &eErr));
        if( eErr == CE_Failure )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].size", iBlock);
        panBlockSize[iBlock] = poDMS->GetIntField(szVarName, &eErr);
        if( eErr == CE_Failure )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }
        if( panBlockSize[iBlock] < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size");
            return CE_Failure;
        }

        snprintf(szVarName, sizeof(szVarName),
                 "blockinfo[%d].logvalid", iBlock);
        const int nLogvalid = poDMS->GetIntField(szVarName, &eErr);
        if( eErr == CE_Failure )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        snprintf(szVarName, sizeof(szVarName),
                 "blockinfo[%d].compressionType", iBlock);
        const int nCompressType = poDMS->GetIntField(szVarName, &eErr);
        if( eErr == CE_Failure )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s", szVarName);
            return eErr;
        }

        panBlockFlag[iBlock] = 0;
        if( nLogvalid )
            panBlockFlag[iBlock] |= BFLG_VALID;
        if( nCompressType != 0 )
            panBlockFlag[iBlock] |= BFLG_COMPRESSED;
    }

    return CE_None;
}

/************************************************************************/
/*                       LoadExternalBlockInfo()                        */
/************************************************************************/

CPLErr HFABand::LoadExternalBlockInfo()

{
    if( panBlockFlag != NULL )
        return CE_None;

    // Get the info structure.
    HFAEntry *poDMS = poNode->GetNamedChild("ExternalRasterDMS");
    CPLAssert(poDMS != NULL);

    nLayerStackCount = poDMS->GetIntField("layerStackCount");
    nLayerStackIndex = poDMS->GetIntField("layerStackIndex");

    // Open raw data file.
    const char *pszFullFilename = HFAGetIGEFilename(psInfo);
    if( pszFullFilename == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Cannot find external data file name");
        return CE_Failure;
    }

    if( psInfo->eAccess == HFA_ReadOnly )
        fpExternal = VSIFOpenL(pszFullFilename, "rb");
    else
        fpExternal = VSIFOpenL(pszFullFilename, "r+b");
    if( fpExternal == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to open external data file: %s", pszFullFilename);
        return CE_Failure;
    }

    // Verify header.
    char szHeader[49] = {};

    if( VSIFReadL(szHeader, sizeof(szHeader), 1, fpExternal) != 1 ||
        !STARTS_WITH(szHeader, "ERDAS_IMG_EXTERNAL_RASTER") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raw data file %s appears to be corrupt.",
                 pszFullFilename);
        return CE_Failure;
    }

    // Allocate blockmap.
    panBlockFlag =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nBlocks));
    if( panBlockFlag == NULL )
    {
        return CE_Failure;
    }

    // Load the validity bitmap.
    const int nBytesPerRow = (nBlocksPerRow + 7) / 8;
    unsigned char *pabyBlockMap = static_cast<unsigned char *>(
        VSI_MALLOC_VERBOSE(nBytesPerRow * nBlocksPerColumn + 20));
    if( pabyBlockMap == NULL )
    {
        return CE_Failure;
    }

    if( VSIFSeekL(fpExternal,
                  poDMS->GetBigIntField("layerStackValidFlagsOffset"),
                  SEEK_SET) < 0 ||
        VSIFReadL(pabyBlockMap, nBytesPerRow * nBlocksPerColumn + 20, 1,
                  fpExternal) != 1 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to read block validity map.");
        return CE_Failure;
    }

    // Establish block information.  Block position is computed
    // from data base address.  Blocks are never compressed.
    // Validity is determined from the validity bitmap.

    nBlockStart = poDMS->GetBigIntField("layerStackDataOffset");
    nBlockSize = (nBlockXSize * static_cast<vsi_l_offset>(nBlockYSize) *
                      HFAGetDataTypeBits(eDataType) +
                  7) /
                 8;

    for( int iBlock = 0; iBlock < nBlocks; iBlock++ )
    {
        const int nColumn = iBlock % nBlocksPerRow;
        const int nRow = iBlock / nBlocksPerRow;
        const int nBit = nRow * nBytesPerRow * 8 + nColumn + 20 * 8;

        if( (pabyBlockMap[nBit >> 3] >> (nBit & 7)) & 0x1 )
            panBlockFlag[iBlock] = BFLG_VALID;
        else
            panBlockFlag[iBlock] = 0;
    }

    CPLFree(pabyBlockMap);

    return CE_None;
}

/************************************************************************/
/*                          UncompressBlock()                           */
/*                                                                      */
/*      Uncompress ESRI Grid compression format block.                  */
/************************************************************************/

// TODO(schwehr): Get rid of this macro without a goto.
#define CHECK_ENOUGH_BYTES(n) \
    if( nSrcBytes < (n) ) \
    { \
        CPLError(CE_Failure, CPLE_AppDefined, \
                 "Not enough bytes in compressed block"); \
        return CE_Failure; \
    }

static CPLErr UncompressBlock( GByte *pabyCData, int nSrcBytes,
                               GByte *pabyDest, int nMaxPixels,
                               EPTType eDataType )

{
    CHECK_ENOUGH_BYTES(13);

    GUInt32 nTmp = 0;

    memcpy(&nTmp, pabyCData, 4);
    const GUInt32 nDataMin = CPL_LSBWORD32(nTmp);

    memcpy(&nTmp, pabyCData + 4, 4);
    const GInt32 nNumRuns = CPL_LSBWORD32(nTmp);

    memcpy(&nTmp, pabyCData + 8, 4);
    const GInt32 nDataOffset = CPL_LSBWORD32(nTmp);

    const int nNumBits = pabyCData[12];

    // If this is not run length encoded, but just reduced
    // precision, handle it now.

    int nPixelsOutput = 0;
    GByte *pabyValues = NULL;
    int nValueBitOffset = 0;

    if( nNumRuns == -1 )
    {
        pabyValues = pabyCData + 13;
        nValueBitOffset = 0;

        if( nNumBits > INT_MAX / nMaxPixels ||
            nNumBits * nMaxPixels > INT_MAX - 7 ||
            (nNumBits * nMaxPixels + 7) / 8 > INT_MAX - 13 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Integer overflow : nNumBits * nMaxPixels + 7");
            return CE_Failure;
        }
        CHECK_ENOUGH_BYTES(13 + (nNumBits * nMaxPixels + 7) / 8);

        // Loop over block pixels.
        for( nPixelsOutput = 0; nPixelsOutput < nMaxPixels; nPixelsOutput++ )
        {
            // Extract the data value in a way that depends on the number
            // of bits in it.

            int nRawValue = 0;

            if( nNumBits == 0 )
            {
                // nRawValue = 0;
            }
            else if( nNumBits == 1 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7))
                    & 0x1;
                nValueBitOffset++;
            }
            else if( nNumBits == 2 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7))
                    & 0x3;
                nValueBitOffset += 2;
            }
            else if( nNumBits == 4 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset >> 3] >> (nValueBitOffset & 7))
                    & 0xf;
                nValueBitOffset += 4;
            }
            else if( nNumBits == 8 )
            {
                nRawValue = *pabyValues;
                pabyValues++;
            }
            else if( nNumBits == 16 )
            {
                nRawValue = 256 * *(pabyValues++);
                nRawValue += *(pabyValues++);
            }
            else if( nNumBits == 32 )
            {
                nRawValue = 256 * 256 * 256 * *(pabyValues++);
                nRawValue += 256 * 256 * *(pabyValues++);
                nRawValue += 256 * *(pabyValues++);
                nRawValue += *(pabyValues++);
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported nNumBits value: %d", nNumBits);
                return CE_Failure;
            }

            // Offset by the minimum value.
            const int nDataValue = nRawValue + nDataMin;

            // Now apply to the output buffer in a type specific way.
            if( eDataType == EPT_u8 )
            {
                ((GByte *)pabyDest)[nPixelsOutput] =
                    static_cast<GByte>(nDataValue);
            }
            else if( eDataType == EPT_u1 )
            {
                if( nDataValue == 1 )
                    pabyDest[nPixelsOutput >> 3] |=
                        (1 << (nPixelsOutput & 0x7));
                else
                    pabyDest[nPixelsOutput >> 3] &=
                        ~(1 << (nPixelsOutput & 0x7));
            }
            else if( eDataType == EPT_u2 )
            {
                // nDataValue & 0x3 is just to avoid UBSAN warning on shifting
                // negative values
                if( (nPixelsOutput & 0x3) == 0 )
                    pabyDest[nPixelsOutput >> 2] =
                        static_cast<GByte>(nDataValue);
                else if( (nPixelsOutput & 0x3) == 1 )
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 2);
                else if( (nPixelsOutput & 0x3) == 2 )
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 4);
                else
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 6);
            }
            else if( eDataType == EPT_u4 )
            {
                // nDataValue & 0xF is just to avoid UBSAN warning on shifting
                // negative values
                if( (nPixelsOutput & 0x1) == 0)
                    pabyDest[nPixelsOutput >> 1] =
                        static_cast<GByte>(nDataValue);
                else
                    pabyDest[nPixelsOutput >> 1] |=
                        static_cast<GByte>((nDataValue & 0xF) << 4);
            }
            else if( eDataType == EPT_s8 )
            {
                ((GByte *)pabyDest)[nPixelsOutput] =
                    static_cast<GByte>(nDataValue);
            }
            else if( eDataType == EPT_u16 )
            {
                ((GUInt16 *)pabyDest)[nPixelsOutput] =
                    static_cast<GUInt16>(nDataValue);
            }
            else if( eDataType == EPT_s16 )
            {
                ((GInt16 *)pabyDest)[nPixelsOutput] =
                    static_cast<GInt16>(nDataValue);
            }
            else if( eDataType == EPT_s32 )
            {
                ((GInt32 *)pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if( eDataType == EPT_u32 )
            {
                ((GUInt32 *)pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if( eDataType == EPT_f32 )
            {
                // Note, floating point values are handled as if they were
                // signed 32-bit integers (bug #1000).
                memcpy(&(((float *)pabyDest)[nPixelsOutput]), &nDataValue,
                       sizeof(float));
            }
            else
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Attempt to uncompress an unsupported pixel data type.");
                return CE_Failure;
            }
        }

        return CE_None;
    }

    // Establish data pointers for runs.
    if( nNumRuns < 0 || nDataOffset < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nNumRuns=%d, nDataOffset=%d",
                 nNumRuns, nDataOffset);
        return CE_Failure;
    }

    if( nNumRuns != 0 &&
        (nNumBits > INT_MAX / nNumRuns ||
         nNumBits * nNumRuns > INT_MAX - 7 ||
         (nNumBits * nNumRuns + 7) / 8 > INT_MAX - nDataOffset) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Integer overflow: nDataOffset + (nNumBits * nNumRuns + 7)/8");
        return CE_Failure;
    }
    CHECK_ENOUGH_BYTES(nDataOffset + (nNumBits * nNumRuns + 7) / 8);

    GByte *pabyCounter = pabyCData + 13;
    int nCounterOffset = 13;
    pabyValues = pabyCData + nDataOffset;
    nValueBitOffset = 0;

    // Loop over runs.
    for( int iRun = 0; iRun < nNumRuns; iRun++ )
    {
        int nRepeatCount = 0;

        // Get the repeat count.  This can be stored as one, two, three
        // or four bytes depending on the low order two bits of the
        // first byte.
        CHECK_ENOUGH_BYTES(nCounterOffset + 1);
        if( (*pabyCounter & 0xc0) == 0x00)
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nCounterOffset++;
        }
        else if( ((*pabyCounter) & 0xc0) == 0x40 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 2);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 2;
        }
        else if( ((*pabyCounter) & 0xc0) == 0x80 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 3);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 3;
        }
        else if( ((*pabyCounter) & 0xc0) == 0xc0 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 4);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 4;
        }

        // Extract the data value in a way that depends on the number
        // of bits in it.
        int nDataValue = 0;

        if( nNumBits == 0 )
        {
            // nDataValue = 0;
        }
        else if( nNumBits == 1 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x1;
            nValueBitOffset++;
        }
        else if( nNumBits == 2 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x3;
            nValueBitOffset += 2;
        }
        else if( nNumBits == 4 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0xf;
            nValueBitOffset += 4;
        }
        else if( nNumBits == 8 )
        {
            nDataValue = *pabyValues;
            pabyValues++;
        }
        else if( nNumBits == 16 )
        {
            nDataValue = 256 * *(pabyValues++);
            nDataValue += *(pabyValues++);
        }
        else if( nNumBits == 32 )
        {
            memcpy(&nDataValue, pabyValues, 4);
            CPL_MSBPTR32(&nDataValue);
            pabyValues += 4;
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "nNumBits = %d", nNumBits );
            return CE_Failure;
        }

        // Offset by the minimum value.
        nDataValue += nDataMin;

        // Now apply to the output buffer in a type specific way.
        if( nRepeatCount > INT_MAX - nPixelsOutput ||
            nPixelsOutput + nRepeatCount > nMaxPixels )
        {
            CPLDebug("HFA", "Repeat count too big: %d", nRepeatCount);
            nRepeatCount = nMaxPixels - nPixelsOutput;
        }

        if( eDataType == EPT_u8 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue < 256);
#endif
                ((GByte *)pabyDest)[nPixelsOutput++] =
                    static_cast<GByte>(nDataValue);
            }
        }
        else if( eDataType == EPT_u16 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
#if DEBUG_VERBOSE
                CPLAssert(nDataValue >= 0);
                CPLAssert(nDataValue < 65536);
#endif
                ((GUInt16 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GUInt16>(nDataValue);
            }
        }
        else if( eDataType == EPT_s8 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= -127);
                CPLAssert(nDataValue < 128);
#endif
                ((GByte *)pabyDest)[nPixelsOutput++] =
                    static_cast<GByte>(nDataValue);
            }
        }
        else if( eDataType == EPT_s16 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= -32768);
                CPLAssert(nDataValue < 32768);
#endif
                ((GInt16 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GInt16>(nDataValue);
            }
        }
        else if( eDataType == EPT_u32 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
#if DEBUG_VERBOSE
                // TODO(schwehr): Do something smarter with out-of-range data.
                // Bad data can trigger this assert.  r23498
                CPLAssert(nDataValue >= 0);
#endif
                ((GUInt32 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GUInt32>(nDataValue);
            }
        }
        else if( eDataType == EPT_s32 )
        {
            for( int i = 0; i < nRepeatCount; i++ )
            {
                ((GInt32 *)pabyDest)[nPixelsOutput++] =
                    static_cast<GInt32>(nDataValue);
            }
        }
        else if( eDataType == EPT_f32 )
        {
            float fDataValue = 0.0f;

            memcpy(&fDataValue, &nDataValue, 4);
            for( int i = 0; i < nRepeatCount; i++ )
            {
                ((float *)pabyDest)[nPixelsOutput++] = fDataValue;
            }
        }
        else if( eDataType == EPT_u1 )
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue == 0 || nDataValue == 1);
#endif
            if( nDataValue == 1 )
            {
                for( int i = 0; i < nRepeatCount; i++ )
                {
                    pabyDest[nPixelsOutput >> 3] |=
                        (1 << (nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
            else
            {
                for( int i = 0; i < nRepeatCount; i++ )
                {
                    pabyDest[nPixelsOutput >> 3] &=
                        ~(1 << (nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
        }
        else if( eDataType == EPT_u2 )
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue >= 0 && nDataValue < 4);
#endif
            for( int i = 0; i < nRepeatCount; i++ )
            {
                if( (nPixelsOutput & 0x3) == 0 )
                    pabyDest[nPixelsOutput >> 2] =
                        static_cast<GByte>(nDataValue);
                else if( (nPixelsOutput & 0x3) == 1 )
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 2);
                else if( (nPixelsOutput & 0x3) == 2 )
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 4);
                else
                    pabyDest[nPixelsOutput >> 2] |=
                        static_cast<GByte>((nDataValue & 0x3) << 6);
                nPixelsOutput++;
            }
        }
        else if( eDataType == EPT_u4 )
        {
#ifdef DEBUG_VERBOSE
            CPLAssert(nDataValue >= 0 && nDataValue < 16);
#endif
            for( int i = 0; i < nRepeatCount; i++ )
            {
                if( (nPixelsOutput & 0x1) == 0 )
                    pabyDest[nPixelsOutput >> 1] =
                        static_cast<GByte>(nDataValue);
                else
                    pabyDest[nPixelsOutput >> 1] |=
                        static_cast<GByte>((nDataValue & 0xF) << 4);

                nPixelsOutput++;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Attempt to uncompress an unsupported pixel data type.");
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             NullBlock()                              */
/*                                                                      */
/*      Set the block buffer to zero or the nodata value as             */
/*      appropriate.                                                    */
/************************************************************************/

void HFABand::NullBlock( void *pData )

{
    const int nChunkSize = std::max(1, HFAGetDataTypeBits(eDataType) / 8);
    int nWords = nBlockXSize * nBlockYSize;

    if( !bNoDataSet )
    {
#ifdef ESRI_BUILD
        // We want special defaulting for 1 bit data in ArcGIS.
        if( eDataType >= EPT_u2 )
            memset(pData, 0, nChunkSize * nWords);
        else
            memset(pData, 255, nChunkSize * nWords);
#else
        memset(pData, 0, nChunkSize * nWords);
#endif
    }
    else
    {
        GByte abyTmp[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        switch( eDataType )
        {
        case EPT_u1:
        {
              nWords = (nWords + 7) / 8;
              if( dfNoData != 0.0 )
                  ((unsigned char *) abyTmp)[0] = 0xff;
              else
                  ((unsigned char *) abyTmp)[0] = 0x00;
        }
        break;

        case EPT_u2:
        {
              nWords = (nWords + 3) / 4;
              if( dfNoData == 0.0 )
                  ((unsigned char *) abyTmp)[0] = 0x00;
              else if( dfNoData == 1.0 )
                  ((unsigned char *) abyTmp)[0] = 0x55;
              else if( dfNoData == 2.0 )
                  ((unsigned char *) abyTmp)[0] = 0xaa;
              else
                  ((unsigned char *) abyTmp)[0] = 0xff;
        }
        break;

        case EPT_u4:
        {
            const unsigned char byVal = static_cast<unsigned char>(
                std::max(0, std::min(15, static_cast<int>(dfNoData))));

            nWords = (nWords + 1) / 2;

            ((unsigned char *)abyTmp)[0] = byVal + (byVal << 4);
        }
        break;

        case EPT_u8:
            ((unsigned char *)abyTmp)[0] = static_cast<unsigned char>(
                std::max(0, std::min(255, static_cast<int>(dfNoData))));
            break;

        case EPT_s8:
            ((signed char *)abyTmp)[0] = static_cast<signed char>(
                std::max(-128, std::min(127, static_cast<int>(dfNoData))));
            break;

        case EPT_u16:
        {
            GUInt16 nTmp = static_cast<GUInt16>(dfNoData);
            memcpy(abyTmp, &nTmp, sizeof(nTmp));
            break;
        }

        case EPT_s16:
        {
            GInt16 nTmp = static_cast<GInt16>(dfNoData);
            memcpy(abyTmp, &nTmp, sizeof(nTmp));
            break;
        }

        case EPT_u32:
        {
            GUInt32 nTmp = static_cast<GUInt32>(dfNoData);
            memcpy(abyTmp, &nTmp, sizeof(nTmp));
            break;
        }

        case EPT_s32:
        {
            GInt32 nTmp = static_cast<GInt32>(dfNoData);
            memcpy(abyTmp, &nTmp, sizeof(nTmp));
            break;
        }

        case EPT_f32:
        {
            float fTmp = static_cast<float>(dfNoData);
            memcpy(abyTmp, &fTmp, sizeof(fTmp));
            break;
        }

        case EPT_f64:
        {
            memcpy(abyTmp, &dfNoData, sizeof(dfNoData));
            break;
        }

        case EPT_c64:
        {
            float fTmp = static_cast<float>(dfNoData);
            memcpy(abyTmp, &fTmp, sizeof(fTmp));
            memset(abyTmp + 4, 0, sizeof(float));
            break;
        }

        case EPT_c128:
        {
            memcpy(abyTmp, &dfNoData, sizeof(dfNoData));
            memset(abyTmp + 8, 0, sizeof(double));
            break;
        }
        }

        for( int i = 0; i < nWords; i++ )
            memcpy(((GByte *)pData) + nChunkSize * i, abyTmp, nChunkSize);
    }
}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::GetRasterBlock( int nXBlock, int nYBlock,
                                void * pData, int nDataSize )

{
    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    const int iBlock = nXBlock + nYBlock * nBlocksPerRow;
    const int nDataTypeSizeBytes = std::max(1, HFAGetDataTypeBits(eDataType) / 8);
    const int nGDALBlockSize = nDataTypeSizeBytes * nBlockXSize * nBlockYSize;

    // If the block isn't valid, we just return all zeros, and an
    // indication of success.
    if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
    {
        NullBlock( pData );
        return CE_None;
    }

    // Otherwise we really read the data.
    vsi_l_offset nBlockOffset = 0;
    VSILFILE *fpData = NULL;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if( fpExternal )
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount +
                       nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

    if( VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0 )
    {
        // XXX: We will not report error here, because file just may be
        // in update state and data for this block will be available later.
        if( psInfo->eAccess == HFA_Update )
        {
            memset(pData, 0, nGDALBlockSize);
            return CE_None;
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Seek to %x:%08x on %p failed\n%s",
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff),
                     fpData, VSIStrerror(errno));
            return CE_Failure;
        }
    }

    // If the block is compressed, read into an intermediate buffer
    // and convert.
    if( panBlockFlag[iBlock] & BFLG_COMPRESSED )
    {
        GByte *pabyCData = static_cast<GByte *>(
            VSI_MALLOC_VERBOSE(static_cast<size_t>(nBlockSize)));
        if( pabyCData == NULL )
        {
            return CE_Failure;
        }

        if( VSIFReadL(pabyCData, static_cast<size_t>(nBlockSize),
                      1, fpData) != 1 )
        {
            CPLFree(pabyCData);

            // XXX: Suppose that file in update state
            if( psInfo->eAccess == HFA_Update )
            {
                memset(pData, 0, nGDALBlockSize);
                return CE_None;
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Read of %d bytes at %x:%08x on %p failed.\n%s",
                         static_cast<int>(nBlockSize),
                         static_cast<int>(nBlockOffset >> 32),
                         static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                         VSIStrerror(errno));
                return CE_Failure;
            }
        }

        CPLErr eErr = UncompressBlock(pabyCData, static_cast<int>(nBlockSize),
                                      static_cast<GByte *>(pData),
                                      nBlockXSize * nBlockYSize, eDataType);

        CPLFree(pabyCData);

        return eErr;
    }

    // Read uncompressed data directly into the return buffer.
    if( nDataSize != -1 && (nBlockSize > INT_MAX ||
                            static_cast<int>(nBlockSize) > nDataSize) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size: %d",
                 static_cast<int>(nBlockSize));
        return CE_Failure;
    }

    if( VSIFReadL(pData, static_cast<size_t>(nBlockSize), 1, fpData) != 1 )
    {
        memset(pData, 0, nGDALBlockSize);

        if( fpData != fpExternal )
            CPLDebug("HFABand", "Read of %x:%08x bytes at %d on %p failed.\n%s",
                     static_cast<int>(nBlockSize),
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));

        return CE_None;
    }

    // Byte swap to local byte order if required.  It appears that
    // raster data is always stored in Intel byte order in Imagine
    // files.

#ifdef CPL_MSB
    if( HFAGetDataTypeBits(eDataType) == 16 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
            CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
    }
    else if( HFAGetDataTypeBits(eDataType) == 32 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if( eDataType == EPT_f64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
    else if( eDataType == EPT_c64 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if( eDataType == EPT_c128 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8 );
    }
#endif  // def CPL_MSB

    return CE_None;
}

/************************************************************************/
/*                           ReAllocBlock()                             */
/************************************************************************/

void HFABand::ReAllocBlock( int iBlock, int nSize )
{
    // For compressed files - need to realloc the space for the block.

    // TODO: Should check to see if panBlockStart[iBlock] is not zero then do a
    // HFAFreeSpace() but that doesn't exist yet.
    // Instead as in interim measure it will reuse the existing block if
    // the new data will fit in.
    if( (panBlockStart[iBlock] != 0) && (nSize <= panBlockSize[iBlock]) )
    {
        panBlockSize[iBlock] = nSize;
        // fprintf( stderr, "Reusing block %d\n", iBlock );
        return;
    }

    panBlockStart[iBlock] = HFAAllocateSpace(psInfo, nSize);

    panBlockSize[iBlock] = nSize;

    // Need to rewrite this info to the RasterDMS node.
    HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");

    if( !poDMS )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
        return;
    }

    char szVarName[64];
    snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].offset", iBlock);
    poDMS->SetIntField(szVarName, static_cast<int>(panBlockStart[iBlock]));

    snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].size", iBlock);
    poDMS->SetIntField(szVarName, panBlockSize[iBlock]);
}

/************************************************************************/
/*                           SetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::SetRasterBlock( int nXBlock, int nYBlock, void * pData )

{
    if( psInfo->eAccess == HFA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Attempt to write block to read-only HFA file failed.");
        return CE_Failure;
    }

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    const int iBlock = nXBlock + nYBlock * nBlocksPerRow;

    // For now we don't support write invalid uncompressed blocks.
    // To do so we will need logic to make space at the end of the
    // file in the right size.
    if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 &&
        !(panBlockFlag[iBlock] & BFLG_COMPRESSED) &&
        panBlockStart[iBlock] == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to write to invalid tile with number %d "
                 "(X position %d, Y position %d).  This operation is "
                 "currently unsupported by HFABand::SetRasterBlock().",
                 iBlock, nXBlock, nYBlock);

        return CE_Failure;
    }

    // Move to the location that the data sits.
    VSILFILE *fpData = NULL;
    vsi_l_offset nBlockOffset = 0;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if( fpExternal )
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount
            + nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

    // Compressed Tile Handling.
    if( panBlockFlag[iBlock] & BFLG_COMPRESSED )
    {
        // Write compressed data.
        int nInBlockSize = static_cast<int>(
            (nBlockXSize * nBlockYSize *
                static_cast<GIntBig>(HFAGetDataTypeBits(eDataType)) + 7) / 8);

        // Create the compressor object.
        HFACompress compress(pData, nInBlockSize, eDataType);
        if( compress.getCounts() == NULL ||
            compress.getValues() == NULL)
        {
            return CE_Failure;
        }

        // Compress the data.
        if( compress.compressBlock() )
        {
            // Get the data out of the object.
            GByte *pCounts      = compress.getCounts();
            GUInt32 nSizeCount  = compress.getCountSize();
            GByte *pValues      = compress.getValues();
            GUInt32 nSizeValues = compress.getValueSize();
            GUInt32 nMin        = compress.getMin();
            GUInt32 nNumRuns    = compress.getNumRuns();
            GByte nNumBits      = compress.getNumBits();

            // Compensate for the header info.
            GUInt32 nDataOffset = nSizeCount + 13;
            int nTotalSize = nSizeCount + nSizeValues + 13;

            // Allocate space for the compressed block and seek to it.
            ReAllocBlock( iBlock, nTotalSize );

            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];

            // Seek to offset.
            if( VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Seek to %x:%08x on %p failed\n%s",
                         static_cast<int>(nBlockOffset >> 32),
                         static_cast<int>(nBlockOffset & 0xffffffff),
                         fpData, VSIStrerror(errno));
                return CE_Failure;
            }

            // Byte swap to local byte order if required.  It appears that
            // raster data is always stored in Intel byte order in Imagine
            // files.

#ifdef CPL_MSB
            CPL_SWAP32PTR(&nMin);
            CPL_SWAP32PTR(&nNumRuns);
            CPL_SWAP32PTR(&nDataOffset);
#endif  // def CPL_MSB

            // Write out the Minimum value.
            bool bRet = VSIFWriteL(&nMin, sizeof(nMin), 1, fpData) > 0;

            // The number of runs.
            bRet &= VSIFWriteL(&nNumRuns, sizeof(nNumRuns), 1, fpData) > 0;

            // The offset to the data.
            bRet &=
                VSIFWriteL(&nDataOffset, sizeof(nDataOffset), 1, fpData) > 0;

            // The number of bits.
            bRet &= VSIFWriteL(&nNumBits, sizeof(nNumBits), 1, fpData) > 0;

            // The counters - MSB stuff handled in HFACompress.
            bRet &= VSIFWriteL(pCounts, nSizeCount, 1, fpData) > 0;

            // The values - MSB stuff handled in HFACompress.
            bRet &= VSIFWriteL(pValues, nSizeValues, 1, fpData) > 0;

            if( !bRet )
                return CE_Failure;

            // Compressed data is freed in the HFACompress destructor.
        }
        else
        {
            // If we have actually made the block bigger - i.e. does not
            // compress well.
            panBlockFlag[iBlock] ^= BFLG_COMPRESSED;
            // Alloc more space for the uncompressed block.
            ReAllocBlock(iBlock, nInBlockSize);

            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];

            // Need to change the RasterDMS entry.
            HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");

            if( !poDMS )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
                return CE_Failure;
            }

            char szVarName[64] = {};
            snprintf(szVarName, sizeof(szVarName),
                     "blockinfo[%d].compressionType", iBlock);
            poDMS->SetIntField(szVarName, 0);
        }

        // If the block was previously invalid, mark it as valid now.
        if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
        {
            char szVarName[64];
            HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");

            if( !poDMS )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Unable to load RasterDMS");
                return CE_Failure;
            }

            snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].logvalid", iBlock);
            poDMS->SetStringField(szVarName, "true");

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }

    // Uncompressed TILE handling.
    if( (panBlockFlag[iBlock] & BFLG_COMPRESSED) == 0 )
    {

        if( VSIFSeekL(fpData, nBlockOffset, SEEK_SET) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Seek to %x:%08x on %p failed\n%s",
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));
            return CE_Failure;
        }

        // Byte swap to local byte order if required.  It appears that
        // raster data is always stored in Intel byte order in Imagine
        // files.

#ifdef CPL_MSB
        if( HFAGetDataTypeBits(eDataType) == 16 )
        {
            for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
                CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
        }
        else if( HFAGetDataTypeBits(eDataType) == 32 )
        {
            for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
                CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
        }
        else if( eDataType == EPT_f64 )
        {
            for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
                CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
        }
        else if( eDataType == EPT_c64 )
        {
            for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
                CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
        }
        else if( eDataType == EPT_c128 )
        {
            for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
                CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
        }
#endif  // def CPL_MSB

        // Write uncompressed data.
        if( VSIFWriteL(pData, static_cast<size_t>(nBlockSize),
                       1, fpData) != 1 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Write of %d bytes at %x:%08x on %p failed.\n%s",
                     static_cast<int>(nBlockSize),
                     static_cast<int>(nBlockOffset >> 32),
                     static_cast<int>(nBlockOffset & 0xffffffff), fpData,
                     VSIStrerror(errno));
            return CE_Failure;
        }

        // If the block was previously invalid, mark it as valid now.
        if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
        {
            char szVarName[64];
            HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");
            if( poDMS == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to get RasterDMS when trying to mark "
                         "block valid.");
                return CE_Failure;
            }
            snprintf(szVarName, sizeof(szVarName), "blockinfo[%d].logvalid",
                     iBlock);
            poDMS->SetStringField(szVarName, "true");

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }
    // Swap back, since we don't really have permission to change
    // the callers buffer.

#ifdef CPL_MSB
    if( HFAGetDataTypeBits(eDataType) == 16 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
            CPL_SWAP16PTR(((unsigned char *)pData) + ii * 2);
    }
    else if( HFAGetDataTypeBits(eDataType) == 32 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if( eDataType == EPT_f64 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize; ii++ )
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
    else if( eDataType == EPT_c64 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
            CPL_SWAP32PTR(((unsigned char *)pData) + ii * 4);
    }
    else if( eDataType == EPT_c128 )
    {
        for( int ii = 0; ii < nBlockXSize * nBlockYSize * 2; ii++ )
            CPL_SWAP64PTR(((unsigned char *)pData) + ii * 8);
    }
#endif  // def CPL_MSB

    return CE_None;
}

/************************************************************************/
/*                         GetBandName()                                */
/*                                                                      */
/*      Return the Layer Name                                           */
/************************************************************************/

const char *HFABand::GetBandName()
{
    if( strlen(poNode->GetName()) > 0 )
        return poNode->GetName();

    for( int iBand = 0; iBand < psInfo->nBands; iBand++ )
    {
        if( psInfo->papoBand[iBand] == this )
        {
            osOverName.Printf("Layer_%d", iBand + 1);
            return osOverName;
        }
    }

    osOverName.Printf("Layer_%x", poNode->GetFilePos());
    return osOverName;
}

/************************************************************************/
/*                         SetBandName()                                */
/*                                                                      */
/*      Set the Layer Name                                              */
/************************************************************************/

void HFABand::SetBandName(const char *pszName)
{
    if( psInfo->eAccess == HFA_Update )
    {
        poNode->SetName(pszName);
    }
}

/************************************************************************/
/*                         SetNoDataValue()                             */
/*                                                                      */
/*      Set the band no-data value                                      */
/************************************************************************/

CPLErr HFABand::SetNoDataValue( double dfValue )
{
    if( psInfo->eAccess != HFA_Update )
        return CE_Failure;

    HFAEntry *poNDNode = poNode->GetNamedChild("Eimg_NonInitializedValue");

    if( poNDNode == NULL )
    {
        poNDNode = HFAEntry::New(psInfo,
                                 "Eimg_NonInitializedValue",
                                 "Eimg_NonInitializedValue",
                                 poNode);
    }

    poNDNode->MakeData(8 + 12 + 8);
    poNDNode->SetPosition();

    poNDNode->SetIntField("valueBD[-3]", EPT_f64);
    poNDNode->SetIntField("valueBD[-2]", 1);
    poNDNode->SetIntField("valueBD[-1]", 1);

    if( poNDNode->SetDoubleField("valueBD[0]", dfValue) == CE_Failure )
        return CE_Failure;

    bNoDataSet = true;
    dfNoData = dfValue;
    return CE_None;
}

/************************************************************************/
/*                        HFAReadBFUniqueBins()                         */
/*                                                                      */
/*      Attempt to read the bins used for a PCT or RAT from a           */
/*      BinFunction node.  On failure just return NULL.                 */
/************************************************************************/

double *HFAReadBFUniqueBins( HFAEntry *poBinFunc, int nPCTColors )

{
    // First confirm this is a "BFUnique" bin function.  We don't
    // know what to do with any other types.
    const char *pszBinFunctionType =
        poBinFunc->GetStringField("binFunction.type.string");

    if( pszBinFunctionType == NULL ||
        !EQUAL(pszBinFunctionType, "BFUnique") )
        return NULL;

    // Process dictionary.
    const char *pszDict =
        poBinFunc->GetStringField("binFunction.MIFDictionary.string");
    if( pszDict == NULL )
        pszDict = poBinFunc->GetStringField("binFunction.MIFDictionary");
    if( pszDict == NULL )
        return NULL;

    HFADictionary oMiniDict(pszDict);

    HFAType *poBFUnique = oMiniDict.FindType("BFUnique");
    if( poBFUnique == NULL )
        return NULL;

    // Field the MIFObject raw data pointer.
    int nMIFObjectSize = 0;
    const GByte *pabyMIFObject =
        reinterpret_cast<const GByte *>(
            poBinFunc->GetStringField("binFunction.MIFObject",
                                      NULL, &nMIFObjectSize));

    if( pabyMIFObject == NULL ||
        nMIFObjectSize < 24 + static_cast<int>(sizeof(double)) * nPCTColors )
        return NULL;

    // Confirm that this is a 64bit floating point basearray.
    if( pabyMIFObject[20] != 0x0a || pabyMIFObject[21] != 0x00 )
    {
        CPLDebug("HFA", "HFAReadPCTBins(): "
                        "The basedata does not appear to be EGDA_TYPE_F64.");
        return NULL;
    }

    // Decode bins.
    double *padfBins =
        static_cast<double *>(CPLCalloc(sizeof(double), nPCTColors));

    memcpy(padfBins, pabyMIFObject + 24, sizeof(double) * nPCTColors);

    for( int i = 0; i < nPCTColors; i++ )
    {
        HFAStandard(8, padfBins + i);
#if DEBUG_VERBOSE
        CPLDebug("HFA", "Bin[%d] = %g", i, padfBins[i]);
#endif
    }

    return padfBins;
}

/************************************************************************/
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/

CPLErr HFABand::GetPCT( int * pnColors,
                        double **ppadfRed,
                        double **ppadfGreen,
                        double **ppadfBlue,
                        double **ppadfAlpha,
                        double **ppadfBins )

{
    *pnColors = 0;
    *ppadfRed = NULL;
    *ppadfGreen = NULL;
    *ppadfBlue = NULL;
    *ppadfAlpha = NULL;
    *ppadfBins = NULL;

    // If we haven't already tried to load the colors, do so now.
    if( nPCTColors == -1 )
    {

        nPCTColors = 0;

        HFAEntry *poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
        if( poColumnEntry == NULL )
            return CE_Failure;

        nPCTColors = poColumnEntry->GetIntField("numRows");
        if( nPCTColors < 0 || nPCTColors > 65536 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of colors: %d", nPCTColors);
            return CE_Failure;
        }

        for( int iColumn = 0; iColumn < 4; iColumn++ )
        {
            apadfPCT[iColumn] = static_cast<double *>(
                 VSI_MALLOC2_VERBOSE(sizeof(double), nPCTColors));
            if( apadfPCT[iColumn] == NULL )
            {
                return CE_Failure;
            }

            if( iColumn == 0 )
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
            }
            else if( iColumn == 1 )
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Green");
            }
            else if( iColumn == 2 )
            {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Blue");
            }
            else if( iColumn == 3 )
            {
                poColumnEntry =
                    poNode->GetNamedChild("Descriptor_Table.Opacity");
            }

            if( poColumnEntry == NULL )
            {
                double *pdCol = apadfPCT[iColumn];
                for( int i = 0; i < nPCTColors; i++ )
                    pdCol[i] = 1.0;
            }
            else
            {
                if( VSIFSeekL(psInfo->fp,
                              poColumnEntry->GetIntField("columnDataPtr"),
                              SEEK_SET) < 0 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "VSIFSeekL() failed in HFABand::GetPCT().");
                    return CE_Failure;
                }
                if( VSIFReadL(apadfPCT[iColumn], sizeof(double), nPCTColors,
                              psInfo->fp) != static_cast<size_t>(nPCTColors) )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "VSIFReadL() failed in HFABand::GetPCT().");
                    return CE_Failure;
                }

                for( int i = 0; i < nPCTColors; i++ )
                    HFAStandard(8, apadfPCT[iColumn] + i);
            }
        }

        // Do we have a custom binning function? If so, try reading it.
        HFAEntry *poBinFunc =
            poNode->GetNamedChild("Descriptor_Table.#Bin_Function840#");

        if( poBinFunc != NULL )
        {
            padfPCTBins = HFAReadBFUniqueBins(poBinFunc, nPCTColors);
        }
    }

    // Return the values.
    if( nPCTColors == 0 )
        return CE_Failure;

    *pnColors = nPCTColors;
    *ppadfRed = apadfPCT[0];
    *ppadfGreen = apadfPCT[1];
    *ppadfBlue = apadfPCT[2];
    *ppadfAlpha = apadfPCT[3];
    *ppadfBins = padfPCTBins;

    return CE_None;
}

/************************************************************************/
/*                               SetPCT()                               */
/*                                                                      */
/*      Set the PCT information for this band.                          */
/************************************************************************/

CPLErr HFABand::SetPCT( int nColors,
                        double *padfRed,
                        double *padfGreen,
                        double *padfBlue,
                        double *padfAlpha )

{
    static const char *const apszColNames[4] =
        {"Red", "Green", "Blue", "Opacity"};
    HFAEntry *poEdsc_Table;

    // Do we need to try and clear any existing color table?
    if( nColors == 0 )
    {
        poEdsc_Table = poNode->GetNamedChild("Descriptor_Table");
        if( poEdsc_Table == NULL )
            return CE_None;

        for( int iColumn = 0; iColumn < 4; iColumn++ )
        {
            HFAEntry *poEdsc_Column =
                poEdsc_Table->GetNamedChild(apszColNames[iColumn]);
            if( poEdsc_Column )
                poEdsc_Column->RemoveAndDestroy();
        }

        return CE_None;
    }

    // Create the Descriptor table.
    poEdsc_Table = poNode->GetNamedChild("Descriptor_Table");
    if( poEdsc_Table == NULL ||
        !EQUAL(poEdsc_Table->GetType(), "Edsc_Table") )
        poEdsc_Table =
            HFAEntry::New(psInfo, "Descriptor_Table", "Edsc_Table", poNode);

    poEdsc_Table->SetIntField( "numrows", nColors );

    // Create the Binning function node.  I am not sure that we
    // really need this though.
    HFAEntry *poEdsc_BinFunction =
        poEdsc_Table->GetNamedChild("#Bin_Function#");
    if( poEdsc_BinFunction == NULL ||
        !EQUAL(poEdsc_BinFunction->GetType(), "Edsc_BinFunction") )
        poEdsc_BinFunction = HFAEntry::New(psInfo, "#Bin_Function#",
                                           "Edsc_BinFunction",
                                           poEdsc_Table);

    // Because of the BaseData we have to hardcode the size.
    poEdsc_BinFunction->MakeData(30);

    poEdsc_BinFunction->SetIntField("numBins", nColors);
    poEdsc_BinFunction->SetStringField("binFunction", "direct");
    poEdsc_BinFunction->SetDoubleField("minLimit", 0.0);
    poEdsc_BinFunction->SetDoubleField("maxLimit", nColors - 1.0);

    // Process each color component.
    for( int iColumn = 0; iColumn < 4; iColumn++ )
    {
        double *padfValues=NULL;
        const char *pszName = apszColNames[iColumn];

        if( iColumn == 0 )
            padfValues = padfRed;
        else if( iColumn == 1 )
            padfValues = padfGreen;
        else if( iColumn == 2 )
            padfValues = padfBlue;
        else if( iColumn == 3 )
            padfValues = padfAlpha;

        // Create the Edsc_Column.
        HFAEntry *poEdsc_Column = poEdsc_Table->GetNamedChild(pszName);
        if( poEdsc_Column == NULL ||
            !EQUAL(poEdsc_Column->GetType(), "Edsc_Column") )
            poEdsc_Column =
                HFAEntry::New(psInfo, pszName, "Edsc_Column", poEdsc_Table);

        poEdsc_Column->SetIntField("numRows", nColors);
        poEdsc_Column->SetStringField("dataType", "real");
        poEdsc_Column->SetIntField("maxNumChars", 0);

        // Write the data out.
        const int nOffset = HFAAllocateSpace(psInfo, 8 * nColors);

        poEdsc_Column->SetIntField("columnDataPtr", nOffset);

        double *padfFileData =
            static_cast<double *>(CPLMalloc(nColors * sizeof(double)));
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            padfFileData[iColor] = padfValues[iColor];
            HFAStandard(8, padfFileData + iColor);
        }
        const bool bRet = VSIFSeekL(psInfo->fp, nOffset, SEEK_SET) >= 0 &&
                          VSIFWriteL(padfFileData, 8, nColors, psInfo->fp) ==
                              static_cast<size_t>(nColors);
        CPLFree(padfFileData);
        if( !bRet )
            return CE_Failure;
    }

    // Update the layer type to be thematic.
    poNode->SetStringField("layerType", "thematic");

    return CE_None;
}

/************************************************************************/
/*                           CreateOverview()                           */
/************************************************************************/

int HFABand::CreateOverview( int nOverviewLevel, const char *pszResampling )

{
    const int nOXSize = (psInfo->nXSize + nOverviewLevel - 1) / nOverviewLevel;
    const int nOYSize = (psInfo->nYSize + nOverviewLevel - 1) / nOverviewLevel;

    // Do we want to use a dependent file (.rrd) for the overviews?
    // Or just create them directly in this file?
    HFAInfo_t *psRRDInfo = psInfo;
    HFAEntry *poParent = poNode;

    if( CPLTestBool(CPLGetConfigOption("HFA_USE_RRD", "NO")) )
    {
        psRRDInfo = HFACreateDependent(psInfo);
        if( psRRDInfo == NULL )
            return -1;

        poParent = psRRDInfo->poRoot->GetNamedChild(GetBandName());

        // Need to create layer object.
        if( poParent == NULL )
        {
            poParent = HFAEntry::New(psRRDInfo, GetBandName(), "Eimg_Layer",
                                     psRRDInfo->poRoot);
        }
    }

    // What pixel type should we use for the overview.  Usually
    // this is the same as the base layer, but when
    // AVERAGE_BIT2GRAYSCALE is in effect we force it to u8 from u1.
    EPTType eOverviewDataType = eDataType;

    if( STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2GR") )
        eOverviewDataType = EPT_u8;

    // Eventually we need to decide on the whether to use the spill
    // file, primarily on the basis of whether the new overview
    // will drive our .img file size near 4GB.  For now, just base
    // it on the config options.
    bool bCreateLargeRaster =
        CPLTestBool(CPLGetConfigOption("USE_SPILL", "NO"));
    GIntBig nValidFlagsOffset = 0;
    GIntBig nDataOffset = 0;

    if( (psRRDInfo->nEndOfFile
         + (nOXSize * static_cast<double>(nOYSize))
         * (HFAGetDataTypeBits(eOverviewDataType) / 8)) > 2000000000.0 )
        bCreateLargeRaster = true;

    if( bCreateLargeRaster )
    {
        if( !HFACreateSpillStack(psRRDInfo, nOXSize, nOYSize, 1,
                                 64, eOverviewDataType,
                                 &nValidFlagsOffset, &nDataOffset) )
        {
            return -1;
        }
    }

    // Are we compressed? If so, overview should be too (unless
    // HFA_COMPRESS_OVR is defined).
    // Check RasterDMS like HFAGetBandInfo.
    bool bCompressionType = false;
    const char *pszCompressOvr = CPLGetConfigOption("HFA_COMPRESS_OVR", NULL);
    if( pszCompressOvr != NULL )
    {
        bCompressionType = CPLTestBool(pszCompressOvr);
    }
    else
    {
        HFAEntry *poDMS = poNode->GetNamedChild("RasterDMS");

        if( poDMS != NULL )
            bCompressionType = poDMS->GetIntField("compressionType") != 0;
    }

    // Create the layer.
    CPLString osLayerName;
    osLayerName.Printf("_ss_%d_", nOverviewLevel);

    if( !HFACreateLayer(psRRDInfo, poParent, osLayerName,
                        TRUE, 64, bCompressionType, bCreateLargeRaster, FALSE,
                        nOXSize, nOYSize, eOverviewDataType, NULL,
                        nValidFlagsOffset, nDataOffset, 1, 0) )
        return -1;

    HFAEntry *poOverLayer = poParent->GetNamedChild(osLayerName);
    if( poOverLayer == NULL )
        return -1;

    // Create RRDNamesList list if it does not yet exist.
    HFAEntry *poRRDNamesList = poNode->GetNamedChild("RRDNamesList");
    if( poRRDNamesList == NULL )
    {
        poRRDNamesList =
            HFAEntry::New(psInfo, "RRDNamesList", "Eimg_RRDNamesList", poNode);
        poRRDNamesList->MakeData(23 + 16 + 8 + 3000);  // Hack for growth room.

        // We need to hardcode file offset into the data, so locate it now.
        poRRDNamesList->SetPosition();

        poRRDNamesList->SetStringField("algorithm.string",
                                       "IMAGINE 2X2 Resampling");
    }

    // Add new overview layer to RRDNamesList.
    int iNextName = poRRDNamesList->GetFieldCount("nameList");
    char szName[50];
    CPLString osNodeName;

    snprintf(szName, sizeof(szName), "nameList[%d].string", iNextName);

    osLayerName.Printf("%s(:%s:_ss_%d_)",
                       psRRDInfo->pszFilename, GetBandName(),
                       nOverviewLevel);

    // TODO: Need to add to end of array (that is pretty hard).
    if( poRRDNamesList->SetStringField(szName, osLayerName) != CE_None )
    {
        poRRDNamesList->MakeData(poRRDNamesList->GetDataSize() + 3000);
        if( poRRDNamesList->SetStringField(szName, osLayerName) != CE_None )
            return -1;
    }

    // Add to the list of overviews for this band.
    papoOverviews = static_cast<HFABand **>(
        CPLRealloc(papoOverviews, sizeof(void *) * ++nOverviews));
    papoOverviews[nOverviews - 1] = new HFABand(psRRDInfo, poOverLayer);

    // If there is a nodata value, copy it to the overview band.
    if( bNoDataSet )
        papoOverviews[nOverviews - 1]->SetNoDataValue(dfNoData);

    return nOverviews - 1;
}
