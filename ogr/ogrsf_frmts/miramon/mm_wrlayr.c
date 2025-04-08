/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau, a.pau@creaf.uab.cat, based on the MiraMon codes,
 *           mainly written by Xavier Pons, Joan Maso (correctly written
 *           "Mas0xF3"), Abel Pau, Nuria Julia (N0xFAria Juli0xE0),
 *           Xavier Calaf, Lluis (Llu0xEDs) Pesquer and Alaitz Zabala, from
 *           CREAF and Universitat Autonoma (Aut0xF2noma) de Barcelona.
 *           For a complete list of contributors:
 *           https://www.miramon.cat/eng/QuiSom.htm
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "mm_wrlayr.h"
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#include "mm_rdlayr.h"    // For MM_ReadExtendedDBFHeader()
#include "gdal.h"         // For GDALDatasetH
#include "ogr_srs_api.h"  // For OSRGetAuthorityCode
#include "cpl_string.h"   // For CPL_ENC_UTF8

CPL_C_START  // Necessary for compiling in GDAL project

    /* -------------------------------------------------------------------- */
    /*      Header Functions                                                */
    /* -------------------------------------------------------------------- */
    int
    MMAppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo);
void MMInitBoundingBox(struct MMBoundingBox *dfBB);
int MMWriteAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                        MM_FILE_OFFSET DiskOffset);
int MMWriteNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_FILE_OFFSET DiskOffset);
int MMWritePHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            MM_FILE_OFFSET DiskOffset);
int MMAppendIntegerDependingOnVersion(
    struct MiraMonVectLayerInfo *hMiraMonLayer, struct MM_FLUSH_INFO *FlushInfo,
    uint32_t *nUL32, GUInt64 nUI64);
int MMMoveFromFileToFile(VSILFILE *pSrcFile, VSILFILE *pDestFile,
                         MM_FILE_OFFSET *nOffset);
int MMResizeZSectionDescrPointer(struct MM_ZD **pZDescription, GUInt64 *nMax,
                                 GUInt64 nNum, GUInt64 nIncr,
                                 GUInt64 nProposedMax);
int MMResizeArcHeaderPointer(struct MM_AH **pArcHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax);
int MMResizeNodeHeaderPointer(struct MM_NH **pNodeHeader, GUInt64 *nMax,
                              GUInt64 nNum, GUInt64 nIncr,
                              GUInt64 nProposedMax);
int MMResizePolHeaderPointer(struct MM_PH **pPolHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax);
void MMUpdateBoundingBoxXY(struct MMBoundingBox *dfBB,
                           struct MM_POINT_2D *pCoord);
void MMUpdateBoundingBox(struct MMBoundingBox *dfBBToBeAct,
                         struct MMBoundingBox *dfBBWithData);
int MMCheckVersionFor3DOffset(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              MM_INTERNAL_FID nElemCount,
                              MM_FILE_OFFSET nOffsetAL,
                              MM_FILE_OFFSET nZLOffset);
int MMCheckVersionOffset(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_FILE_OFFSET OffsetToCheck);
int MMCheckVersionForFID(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_INTERNAL_FID FID);

// Extended DBF functions
int MMCreateMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                 struct MM_POINT_2D *pFirstCoord);
int MMAddDBFRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         struct MiraMonFeature *hMMFeature);
int MMAddPointRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                           struct MiraMonFeature *hMMFeature,
                           MM_INTERNAL_FID nElemCount);
int MMAddArcRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         struct MiraMonFeature *hMMFeature,
                         MM_INTERNAL_FID nElemCount, struct MM_AH *pArcHeader);
int MMAddNodeRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                          MM_INTERNAL_FID nElemCount,
                          struct MM_NH *pNodeHeader);
int MMAddPolygonRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             struct MiraMonFeature *hMMFeature,
                             MM_INTERNAL_FID nElemCount,
                             MM_N_VERTICES_TYPE nVerticesCount,
                             struct MM_PH *pPolHeader);
int MMCloseMMBD_XP(struct MiraMonVectLayerInfo *hMiraMonLayer);
void MMDestroyMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer);

/* -------------------------------------------------------------------- */
/*      Managing errors and warnings                                    */
/* -------------------------------------------------------------------- */

// Checks for potential arithmetic overflow when performing multiplication
// operations between two GUInt64 values and converting the result to size_t.
// Important for 32 vs. 64 bit compiling compatibility.
int MMCheckSize_t(GUInt64 nCount, GUInt64 nSize)
{
    if ((size_t)nCount != nCount)
        return 1;

    if ((size_t)nSize != nSize)
        return 1;

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (nCount != 0 && nSize > SIZE_MAX / nCount)
#else
    if (nCount != 0 && nSize > (1000 * 1000 * 1000U) / nCount)
#endif
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Overflow in MMCheckSize_t()");
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Version                                         */
/* -------------------------------------------------------------------- */
int MMGetVectorVersion(struct MM_TH *pTopHeader)
{
    if ((pTopHeader->aLayerVersion[0] == ' ' ||
         pTopHeader->aLayerVersion[0] == '0') &&
        pTopHeader->aLayerVersion[1] == '1' &&
        pTopHeader->aLayerSubVersion == '1')
        return MM_32BITS_VERSION;

    if ((pTopHeader->aLayerVersion[0] == ' ' ||
         pTopHeader->aLayerVersion[0] == '0') &&
        pTopHeader->aLayerVersion[1] == '2' &&
        pTopHeader->aLayerSubVersion == '0')
        return MM_64BITS_VERSION;

    return MM_UNKNOWN_VERSION;
}

static void MMSet1_1Version(struct MM_TH *pTopHeader)
{
    pTopHeader->aLayerVersion[0] = ' ';
    pTopHeader->aLayerVersion[1] = '1';
    pTopHeader->aLayerSubVersion = '1';
}

static void MMSet2_0Version(struct MM_TH *pTopHeader)
{
    pTopHeader->aLayerVersion[0] = ' ';
    pTopHeader->aLayerVersion[1] = '2';
    pTopHeader->aLayerSubVersion = '0';
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Header                                         */
/* -------------------------------------------------------------------- */
int MMReadHeader(VSILFILE *pF, struct MM_TH *pMMHeader)
{
    char dot;
    uint32_t NCount;
    int32_t reservat4 = 0L;

    pMMHeader->Flag = 0x0;
    if (VSIFSeekL(pF, 0, SEEK_SET))
        return 1;
    if (VSIFReadL(pMMHeader->aFileType, 1, 3, pF) != 3)
        return 1;
    if (VSIFReadL(pMMHeader->aLayerVersion, 1, 2, pF) != 2)
        return 1;
    if (VSIFReadL(&dot, 1, 1, pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->aLayerSubVersion, 1, 1, pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->Flag, sizeof(pMMHeader->Flag), 1, pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->hBB.dfMinX, sizeof(pMMHeader->hBB.dfMinX), 1,
                  pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->hBB.dfMaxX, sizeof(pMMHeader->hBB.dfMaxX), 1,
                  pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->hBB.dfMinY, sizeof(pMMHeader->hBB.dfMinY), 1,
                  pF) != 1)
        return 1;
    if (VSIFReadL(&pMMHeader->hBB.dfMaxY, sizeof(pMMHeader->hBB.dfMaxY), 1,
                  pF) != 1)
        return 1;
    if (pMMHeader->aLayerVersion[0] == ' ' &&
        pMMHeader->aLayerVersion[1] == '1')
    {
        if (VSIFReadL(&NCount, sizeof(NCount), 1, pF) != 1)
            return 1;

        pMMHeader->nElemCount = (MM_INTERNAL_FID)NCount;

        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    else if (pMMHeader->aLayerVersion[0] == ' ' &&
             pMMHeader->aLayerVersion[1] == '2')
    {
        if (VSIFReadL(&(pMMHeader->nElemCount), sizeof(pMMHeader->nElemCount),
                      1, pF) != 1)
            return 1;

        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
    }

    if (pMMHeader->Flag & MM_LAYER_3D_INFO)
        pMMHeader->bIs3d = 1;

    if (pMMHeader->Flag & MM_LAYER_MULTIPOLYGON)
        pMMHeader->bIsMultipolygon = 1;

    return 0;
}

static int MMWriteHeader(VSILFILE *pF, struct MM_TH *pMMHeader)
{
    char dot = '.';
    uint32_t NCount;
    int32_t reservat4 = 0L;
    MM_INTERNAL_FID nNumber1 = 1, nNumber0 = 0;

    if (!pF)
        return 0;

    pMMHeader->Flag = MM_CREATED_USING_MIRAMON;  // Created from MiraMon
    if (pMMHeader->bIs3d)
        pMMHeader->Flag |= MM_LAYER_3D_INFO;  // 3D

    if (pMMHeader->bIsMultipolygon)
        pMMHeader->Flag |= MM_LAYER_MULTIPOLYGON;  // Multipolygon.

    if (pMMHeader->aFileType[0] == 'P' && pMMHeader->aFileType[1] == 'O' &&
        pMMHeader->aFileType[2] == 'L')
        pMMHeader->Flag |= MM_BIT_5_ON;  // Explicital polygons

    if (VSIFSeekL(pF, 0, SEEK_SET))
        return 1;
    if (VSIFWriteL(pMMHeader->aFileType, 1, 3, pF) != 3)
        return 1;
    if (VSIFWriteL(pMMHeader->aLayerVersion, 1, 2, pF) != 2)
        return 1;
    if (VSIFWriteL(&dot, 1, 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->aLayerSubVersion, 1, 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->Flag, sizeof(pMMHeader->Flag), 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->hBB.dfMinX, sizeof(pMMHeader->hBB.dfMinX), 1,
                   pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->hBB.dfMaxX, sizeof(pMMHeader->hBB.dfMaxX), 1,
                   pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->hBB.dfMinY, sizeof(pMMHeader->hBB.dfMinY), 1,
                   pF) != 1)
        return 1;
    if (VSIFWriteL(&pMMHeader->hBB.dfMaxY, sizeof(pMMHeader->hBB.dfMaxY), 1,
                   pF) != 1)
        return 1;
    if (pMMHeader->aLayerVersion[0] == ' ' &&
        pMMHeader->aLayerVersion[1] == '1')
    {
        NCount = (uint32_t)pMMHeader->nElemCount;
        if (VSIFWriteL(&NCount, sizeof(NCount), 1, pF) != 1)
            return 1;

        if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    else if (pMMHeader->aLayerVersion[0] == ' ' &&
             pMMHeader->aLayerVersion[1] == '2')
    {
        if (VSIFWriteL(&(pMMHeader->nElemCount), sizeof(pMMHeader->nElemCount),
                       1, pF) != 1)
            return 1;

        // Next part of the file (don't apply for the moment)
        if (VSIFWriteL(&nNumber1, sizeof(nNumber1), 1, pF) != 1)
            return 1;
        if (VSIFWriteL(&nNumber0, sizeof(nNumber0), 1, pF) != 1)
            return 1;

        // Reserved bytes
        if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
            return 1;
        if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Z section                                      */
/* -------------------------------------------------------------------- */
int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer, VSILFILE *pF,
                   struct MM_ZSection *pZSection)
{
    int32_t reservat4 = 0L;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPoint)
    {
        if (MMCheckSize_t(hMiraMonLayer->TopHeader.nElemCount, MM_SIZE_OF_TL))
            return 1;
        if (hMiraMonLayer->TopHeader.nElemCount * MM_SIZE_OF_TL >
            UINT64_MAX - hMiraMonLayer->nHeaderDiskSize)
            return 1;
        pZSection->ZSectionOffset =
            hMiraMonLayer->nHeaderDiskSize +
            hMiraMonLayer->TopHeader.nElemCount * MM_SIZE_OF_TL;
    }
    else if (hMiraMonLayer->bIsArc && !(hMiraMonLayer->bIsPolygon) &&
             hMiraMonLayer->TopHeader.nElemCount > 0)
    {
        const struct MM_AH *pArcHeader =
            &(hMiraMonLayer->MMArc
                  .pArcHeader[hMiraMonLayer->TopHeader.nElemCount - 1]);
        if (MMCheckSize_t(pArcHeader->nElemCount, MM_SIZE_OF_COORDINATE))
            return 1;
        if (pArcHeader->nElemCount * MM_SIZE_OF_COORDINATE >
            UINT64_MAX - pArcHeader->nOffset)
            return 1;
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset =
            pArcHeader->nOffset +
            pArcHeader->nElemCount * MM_SIZE_OF_COORDINATE;
    }
    else if (hMiraMonLayer->bIsPolygon &&
             hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount > 0)
    {
        const struct MM_AH *pArcHeader =
            &(hMiraMonLayer->MMPolygon.MMArc
                  .pArcHeader[hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount -
                              1]);
        if (MMCheckSize_t(pArcHeader->nElemCount, MM_SIZE_OF_COORDINATE))
            return 1;
        if (pArcHeader->nElemCount * MM_SIZE_OF_COORDINATE >
            UINT64_MAX - pArcHeader->nOffset)
            return 1;
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset =
            pArcHeader->nOffset +
            pArcHeader->nElemCount * MM_SIZE_OF_COORDINATE;
    }
    else
        return 1;

    if (pF)
    {
        if (VSIFSeekL(pF, pZSection->ZSectionOffset, SEEK_SET))
            return 1;

        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (VSIFReadL(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;

        if (VSIFReadL(&pZSection->ZHeader.dfBBminz,
                      sizeof(pZSection->ZHeader.dfBBminz), 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBminz);

        if (VSIFReadL(&pZSection->ZHeader.dfBBmaxz,
                      sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBmaxz);
    }
    return 0;
}

static int MMWriteZSection(VSILFILE *pF, struct MM_ZSection *pZSection)
{
    int32_t reservat4 = 0L;

    if (VSIFSeekL(pF, pZSection->ZSectionOffset, SEEK_SET))
        return 1;

    if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (VSIFWriteL(&reservat4, 4, 1, pF) != 1)
        return 1;

    pZSection->ZSectionOffset += 16;

    if (VSIFWriteL(&pZSection->ZHeader.dfBBminz,
                   sizeof(pZSection->ZHeader.dfBBminz), 1, pF) != 1)
        return 1;
    pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBminz);
    if (VSIFWriteL(&pZSection->ZHeader.dfBBmaxz,
                   sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF) != 1)
        return 1;
    pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBmaxz);
    return 0;
}

int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              VSILFILE *pF, MM_INTERNAL_FID nElements,
                              struct MM_ZSection *pZSection)
{
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    MM_INTERNAL_FID nIndex = 0;
    MM_FILE_OFFSET nBlockSize;
    struct MM_ZD *pZDescription;

    if (!hMiraMonLayer)
        return 1;

    if (!pZSection)
        return 1;

    if (!nElements)
        return 0;  // No elements to read

    pZDescription = pZSection->pZDescription;

    nBlockSize = nElements * pZSection->nZDDiskSize;

    if (MMInitFlush(&FlushTMP, pF, nBlockSize, &pBuffer,
                    pZSection->ZSectionOffset, 0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    for (nIndex = 0; nIndex < nElements; nIndex++)
    {
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBminz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBminz;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBmaxz;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->nZCount;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        if (hMiraMonLayer->LayerVersion == MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved = 4;
            FlushTMP.pBlockToBeSaved = (void *)nullptr;
            if (MMReadBlockFromBuffer(&FlushTMP))
            {
                if (pBuffer)
                    VSIFree(pBuffer);
                return 1;
            }
        }

        if (MMReadOffsetDependingOnVersion(hMiraMonLayer, &FlushTMP,
                                           &(pZDescription + nIndex)->nOffsetZ))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }
    if (pBuffer)
        VSIFree(pBuffer);

    return 0;
}

static int
MMWriteZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                           VSILFILE *pF, MM_INTERNAL_FID nElements,
                           struct MM_ZSection *pZSection)
{
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    uint32_t nUL32;
    MM_INTERNAL_FID nIndex = 0;
    MM_FILE_OFFSET nOffsetDiff;
    struct MM_ZD *pZDescription;

    if (!hMiraMonLayer)
        return 1;

    if (!pF)
        return 1;

    if (!pZSection)
        return 1;

    pZDescription = pZSection->pZDescription;

    nOffsetDiff =
        pZSection->ZSectionOffset + nElements * pZSection->nZDDiskSize;

    if (MMInitFlush(&FlushTMP, pF, MM_1MB, &pBuffer, pZSection->ZSectionOffset,
                    0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    for (nIndex = 0; nIndex < nElements; nIndex++)
    {
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBminz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBminz;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBmaxz;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->nZCount;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        if (hMiraMonLayer->LayerVersion == MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved = 4;
            FlushTMP.pBlockToBeSaved = (void *)nullptr;
            if (MMAppendBlockToBuffer(&FlushTMP))
            {
                if (pBuffer)
                    VSIFree(pBuffer);
                return 1;
            }
        }

        // Overflow?
        if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION &&
            (pZDescription + nIndex)->nOffsetZ >
                MAXIMUM_OFFSET_IN_2GB_VECTORS - nOffsetDiff)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Offset Overflow in V1.1");
            return 1;
        }

        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                (pZDescription + nIndex)->nOffsetZ + nOffsetDiff))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }
    pZSection->ZSectionOffset += FlushTMP.TotalSavedBytes;

    if (pBuffer)
        VSIFree(pBuffer);

    return 0;
}

static void MMDestroyZSectionDescription(struct MM_ZSection *pZSection)
{
    if (pZSection->pZL)
    {
        VSIFree(pZSection->pZL);
        pZSection->pZL = nullptr;
    }

    if (pZSection->pZDescription)
    {
        VSIFree(pZSection->pZDescription);
        pZSection->pZDescription = nullptr;
    }
}

static int MMInitZSectionDescription(struct MM_ZSection *pZSection)
{
    if (MMCheckSize_t(pZSection->nMaxZDescription,
                      sizeof(*pZSection->pZDescription)))
        return 1;

    if (!pZSection->nMaxZDescription)
    {
        pZSection->pZDescription = nullptr;
        return 0;  // No elements to read (or write)
    }

    pZSection->pZDescription = (struct MM_ZD *)VSICalloc(
        (size_t)pZSection->nMaxZDescription, sizeof(*pZSection->pZDescription));
    if (!pZSection->pZDescription)
        return 1;
    return 0;
}

static int MMInitZSectionLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                               VSILFILE *pF3d, struct MM_ZSection *pZSection)
{
    if (!hMiraMonLayer)
        return 1;

    // Zsection
    if (!hMiraMonLayer->TopHeader.bIs3d)
    {
        pZSection->pZDescription = nullptr;
        return 0;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        pZSection->ZHeader.dfBBminz = STATISTICAL_UNDEF_VALUE;
        pZSection->ZHeader.dfBBmaxz = -STATISTICAL_UNDEF_VALUE;
    }

    // ZH
    pZSection->ZHeader.nMyDiskSize = 32;
    pZSection->ZSectionOffset = 0;

    // ZD
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        pZSection->nMaxZDescription =
            MM_FIRST_NUMBER_OF_VERTICES * sizeof(double);
        if (MMInitZSectionDescription(pZSection))
            return 1;
    }
    else
    {
        if (hMiraMonLayer->bIsPolygon)
        {
            if (MMCheckSize_t(hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount,
                              sizeof(double)))
                return 1;

            pZSection->nMaxZDescription =
                hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount *
                sizeof(double);
        }
        else
        {
            if (MMCheckSize_t(hMiraMonLayer->TopHeader.nElemCount,
                              sizeof(double)))
                return 1;

            pZSection->nMaxZDescription =
                hMiraMonLayer->TopHeader.nElemCount * sizeof(double);
        }
        if (MMInitZSectionDescription(pZSection))
            return 1;
    }

    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pZSection->nZDDiskSize = MM_SIZE_OF_ZD_32_BITS;
    else
        pZSection->nZDDiskSize = MM_SIZE_OF_ZD_64_BITS;

    pZSection->ZDOffset = 0;

    // ZL
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        if (MMInitFlush(&pZSection->FlushZL, pF3d, MM_1MB, &pZSection->pZL, 0,
                        sizeof(double)))
            return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Extensions                                     */
/* -------------------------------------------------------------------- */

/* Find the last occurrence of pszFinalPart in pszName
    and changes it by pszNewPart.

    Examples of desired behavior
    AA.pnt -> AAT.rel
    AA.nod -> N.~idx
    AA.nod -> N.dbf
    AA.nod -> N.rel
*/

static int MMChangeFinalPartOfTheName(char *pszName, size_t nMaxSizeOfName,
                                      const char *pszFinalPart,
                                      const char *pszNewPart)
{
    char *pAux, *pszWhereToFind, *pszLastFound = nullptr;
    ;

    if (!pszName || !pszFinalPart || !pszNewPart)
        return 0;
    if (MMIsEmptyString(pszName) || MMIsEmptyString(pszFinalPart) ||
        MMIsEmptyString(pszNewPart))
        return 0;

    if (strlen(pszName) - strlen(pszFinalPart) + strlen(pszNewPart) >=
        nMaxSizeOfName)
        return 1;  // It's not possible to change the final part

    // It's the implementation on windows of the linux strrstr()
    // pszLastFound = strrstr(pszWhereToFind, pszFinalPart);
    pszWhereToFind = pszName;
    while (nullptr != (pAux = MM_stristr(pszWhereToFind, pszFinalPart)))
    {
        pszLastFound = pAux;
        pszWhereToFind = pAux + strlen(pAux);
    }

    if (!pszLastFound)
        return 1;  // Not found not changed

    memcpy(pszLastFound, pszNewPart, strlen(pszNewPart));

    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: initializing MiraMon layers                    */
/* -------------------------------------------------------------------- */
static int MMInitPointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    hMiraMonLayer->bIsPoint = 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // Geometrical part
        // Init header structure
        hMiraMonLayer->TopHeader.nElemCount = 0;
        MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);

        hMiraMonLayer->TopHeader.bIs3d = 1;  // Read description of bRealIs3d
        hMiraMonLayer->TopHeader.aFileType[0] = 'P';
        hMiraMonLayer->TopHeader.aFileType[1] = 'N';
        hMiraMonLayer->TopHeader.aFileType[2] = 'T';

        // Opening the binary file where sections TH, TL[...] and ZH-ZD[...]-ZL[...]
        // are going to be written.

        snprintf(hMiraMonLayer->MMPoint.pszLayerName,
                 sizeof(hMiraMonLayer->MMPoint.pszLayerName), "%s.pnt",
                 hMiraMonLayer->pszSrcLayerName);
    }
    if (nullptr == (hMiraMonLayer->MMPoint.pF =
                        VSIFOpenL(hMiraMonLayer->MMPoint.pszLayerName,
                                  hMiraMonLayer->pszFlags)))
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Error MMPoint.pF: Cannot open file %s.",
                 hMiraMonLayer->MMPoint.pszLayerName);
        return 1;
    }
    VSIFSeekL(hMiraMonLayer->MMPoint.pF, 0, SEEK_SET);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // TL
        snprintf(hMiraMonLayer->MMPoint.pszTLName,
                 sizeof(hMiraMonLayer->MMPoint.pszTLName), "%sT.~xy",
                 hMiraMonLayer->pszSrcLayerName);

        if (nullptr == (hMiraMonLayer->MMPoint.pFTL =
                            VSIFOpenL(hMiraMonLayer->MMPoint.pszTLName,
                                      hMiraMonLayer->pszFlags)))
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error MMPoint.pFTL: Cannot open file %s.",
                     hMiraMonLayer->MMPoint.pszTLName);
            return 1;
        }
        VSIFSeekL(hMiraMonLayer->MMPoint.pFTL, 0, SEEK_SET);

        if (MMInitFlush(&hMiraMonLayer->MMPoint.FlushTL,
                        hMiraMonLayer->MMPoint.pFTL, MM_1MB,
                        &hMiraMonLayer->MMPoint.pTL, 0, MM_SIZE_OF_TL))
            return 1;

        // 3D part
        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            snprintf(hMiraMonLayer->MMPoint.psz3DLayerName,
                     sizeof(hMiraMonLayer->MMPoint.psz3DLayerName), "%sT.~z",
                     hMiraMonLayer->pszSrcLayerName);

            if (nullptr == (hMiraMonLayer->MMPoint.pF3d =
                                VSIFOpenL(hMiraMonLayer->MMPoint.psz3DLayerName,
                                          hMiraMonLayer->pszFlags)))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Error MMPoint.pF3d: Cannot open file %s.",
                         hMiraMonLayer->MMPoint.psz3DLayerName);
                return 1;
            }
            VSIFSeekL(hMiraMonLayer->MMPoint.pF3d, 0, SEEK_SET);
        }
    }
    // Zsection
    if (hMiraMonLayer->TopHeader.bIs3d)
    {
        if (MMInitZSectionLayer(hMiraMonLayer, hMiraMonLayer->MMPoint.pF3d,
                                &hMiraMonLayer->MMPoint.pZSection))
            return 1;

        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if (MMReadZSection(hMiraMonLayer, hMiraMonLayer->MMPoint.pF,
                               &hMiraMonLayer->MMPoint.pZSection))
                return 1;

            if (MMReadZDescriptionHeaders(hMiraMonLayer,
                                          hMiraMonLayer->MMPoint.pF,
                                          hMiraMonLayer->TopHeader.nElemCount,
                                          &hMiraMonLayer->MMPoint.pZSection))
                return 1;
        }
    }

    // MiraMon metadata
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        snprintf(hMiraMonLayer->MMPoint.pszREL_LayerName,
                 sizeof(hMiraMonLayer->MMPoint.pszREL_LayerName), "%sT.rel",
                 hMiraMonLayer->pszSrcLayerName);
    }
    else
    {
        CPLStrlcpy(hMiraMonLayer->MMPoint.pszREL_LayerName,
                   hMiraMonLayer->pszSrcLayerName,
                   sizeof(hMiraMonLayer->MMPoint.pszREL_LayerName));
        if (MMChangeFinalPartOfTheName(hMiraMonLayer->MMPoint.pszREL_LayerName,
                                       MM_CPL_PATH_BUF_SIZE, ".pnt", "T.rel"))
            return 1;
    }

    hMiraMonLayer->pszMainREL_LayerName =
        hMiraMonLayer->MMPoint.pszREL_LayerName;

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        // This file has to exist and be the appropriate version.
        if (MMCheck_REL_FILE(hMiraMonLayer->MMPoint.pszREL_LayerName))
            return 1;
    }

    // MIRAMON DATA BASE
    // Creating the DBF file name
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        snprintf(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName,
                 sizeof(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName),
                 "%sT.dbf", hMiraMonLayer->pszSrcLayerName);
    }
    else
    {
        CPLStrlcpy(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName,
                   hMiraMonLayer->pszSrcLayerName,
                   sizeof(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName));

        if (MMChangeFinalPartOfTheName(
                hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName,
                MM_CPL_PATH_BUF_SIZE, ".pnt", "T.dbf"))
            return 1;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    return 0;
}

static int MMInitNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // Init header structure
        pMMArcLayer->TopNodeHeader.aFileType[0] = 'N';
        pMMArcLayer->TopNodeHeader.aFileType[1] = 'O';
        pMMArcLayer->TopNodeHeader.aFileType[2] = 'D';

        pMMArcLayer->TopNodeHeader.bIs3d = 1;  // Read description of bRealIs3d
        MMInitBoundingBox(&pMMArcLayer->TopNodeHeader.hBB);
    }

    // Opening the binary file where sections TH, NH and NL[...]
    // are going to be written.
    strcpy(pMMArcLayer->MMNode.pszLayerName, pMMArcLayer->pszLayerName);
    CPLStrlcpy(pMMArcLayer->MMNode.pszLayerName,
               CPLResetExtension(pMMArcLayer->MMNode.pszLayerName, "nod"),
               sizeof(pMMArcLayer->MMNode.pszLayerName));

    if (nullptr ==
        (pMMArcLayer->MMNode.pF = VSIFOpenL(pMMArcLayer->MMNode.pszLayerName,
                                            hMiraMonLayer->pszFlags)))
    {

        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Error MMNode.pF: Cannot open file %s.",
                 pMMArcLayer->MMNode.pszLayerName);
        return 1;
    }
    VSIFSeekL(pMMArcLayer->MMNode.pF, 0, SEEK_SET);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // Node Header
        pMMArcLayer->MMNode.nMaxNodeHeader = MM_FIRST_NUMBER_OF_NODES;
        if (MMCheckSize_t(pMMArcLayer->MMNode.nMaxNodeHeader,
                          sizeof(*pMMArcLayer->MMNode.pNodeHeader)))
            return 1;

        if (!pMMArcLayer->MMNode.nMaxNodeHeader)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Error in MiraMon "
                     "driver: no nodes to write?");
            return 1;
        }

        if (nullptr ==
            (pMMArcLayer->MMNode.pNodeHeader = (struct MM_NH *)VSICalloc(
                 (size_t)pMMArcLayer->MMNode.nMaxNodeHeader,
                 sizeof(*pMMArcLayer->MMNode.pNodeHeader))))
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMInitNodeLayer())");
            return 1;
        }

        if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
            pMMArcLayer->MMNode.nSizeNodeHeader = MM_SIZE_OF_NH_32BITS;
        else
            pMMArcLayer->MMNode.nSizeNodeHeader = MM_SIZE_OF_NH_64BITS;

        // NL Section
        strcpy(pMMArcLayer->MMNode.pszNLName, pMMArcLayer->MMNode.pszLayerName);
        if (MMChangeFinalPartOfTheName(pMMArcLayer->MMNode.pszNLName,
                                       MM_CPL_PATH_BUF_SIZE, ".nod", "N.~idx"))
            return 1;

        if (nullptr ==
            (pMMArcLayer->MMNode.pFNL = VSIFOpenL(pMMArcLayer->MMNode.pszNLName,
                                                  hMiraMonLayer->pszFlags)))
        {

            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error MMNode.pFNL: Cannot open file %s.",
                     pMMArcLayer->MMNode.pszNLName);
            return 1;
        }
        VSIFSeekL(pMMArcLayer->MMNode.pFNL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->MMNode.FlushNL, pMMArcLayer->MMNode.pFNL,
                        MM_1MB, &pMMArcLayer->MMNode.pNL, 0, 0))
            return 1;

        // Creating the DBF file name
        strcpy(pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName,
               pMMArcLayer->MMNode.pszLayerName);
        if (MMChangeFinalPartOfTheName(
                pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName,
                MM_CPL_PATH_BUF_SIZE, ".nod", "N.dbf"))
            return 1;

        // MiraMon metadata
        strcpy(pMMArcLayer->MMNode.pszREL_LayerName,
               pMMArcLayer->MMNode.pszLayerName);
        if (MMChangeFinalPartOfTheName(pMMArcLayer->MMNode.pszREL_LayerName,
                                       MM_CPL_PATH_BUF_SIZE, ".nod", "N.rel"))
            return 1;
    }
    return 0;
}

static int MMInitArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonArcLayer *pMMArcLayer;
    struct MM_TH *pArcTopHeader;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader = &hMiraMonLayer->MMPolygon.TopArcHeader;
    }
    else
    {
        pMMArcLayer = &hMiraMonLayer->MMArc;
        pArcTopHeader = &hMiraMonLayer->TopHeader;
    }

    // Init header structure
    hMiraMonLayer->bIsArc = 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        pArcTopHeader->bIs3d = 1;  // Read description of bRealIs3d
        MMInitBoundingBox(&pArcTopHeader->hBB);

        pArcTopHeader->aFileType[0] = 'A';
        pArcTopHeader->aFileType[1] = 'R';
        pArcTopHeader->aFileType[2] = 'C';

        if (hMiraMonLayer->bIsPolygon)
        {
            snprintf(pMMArcLayer->pszLayerName,
                     sizeof(pMMArcLayer->pszLayerName), "%s_bound.arc",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            snprintf(pMMArcLayer->pszLayerName,
                     sizeof(pMMArcLayer->pszLayerName), "%s.arc",
                     hMiraMonLayer->pszSrcLayerName);
        }
    }

    if (nullptr == (pMMArcLayer->pF = VSIFOpenL(pMMArcLayer->pszLayerName,
                                                hMiraMonLayer->pszFlags)))
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Error pMMArcLayer->pF: Cannot open file %s.",
                 pMMArcLayer->pszLayerName);
        return 1;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE &&
        hMiraMonLayer->bIsPolygon)
    {
        VSIFSeekL(pMMArcLayer->pF, 0, SEEK_SET);
        if (MMReadHeader(pMMArcLayer->pF,
                         &hMiraMonLayer->MMPolygon.TopArcHeader))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading the format in file %s.",
                     pMMArcLayer->pszLayerName);
            return 1;
        }
        // 3D information is in arcs file
        hMiraMonLayer->TopHeader.bIs3d =
            hMiraMonLayer->MMPolygon.TopArcHeader.bIs3d;
    }

    // AH
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMArcLayer->nSizeArcHeader = MM_SIZE_OF_AH_32BITS;
    else
        pMMArcLayer->nSizeArcHeader = MM_SIZE_OF_AH_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        pMMArcLayer->nMaxArcHeader = MM_FIRST_NUMBER_OF_ARCS;
    else
        pMMArcLayer->nMaxArcHeader = pArcTopHeader->nElemCount;

    if (pMMArcLayer->nMaxArcHeader)
    {
        if (MMCheckSize_t(pMMArcLayer->nMaxArcHeader,
                          sizeof(*pMMArcLayer->pArcHeader)))
            return 1;
        if (nullptr == (pMMArcLayer->pArcHeader = (struct MM_AH *)VSICalloc(
                            (size_t)pMMArcLayer->nMaxArcHeader,
                            sizeof(*pMMArcLayer->pArcHeader))))
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMInitArcLayer())");
            return 1;
        }
        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if (MMReadAHArcSection(hMiraMonLayer))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error reading the format in file %s.",
                         pMMArcLayer->pszLayerName);
                return 1;
            }
        }
    }
    else
        pMMArcLayer->pArcHeader = nullptr;

    // AL
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        pMMArcLayer->nALElementSize = MM_SIZE_OF_AL;

        if (hMiraMonLayer->bIsPolygon)
        {
            snprintf(pMMArcLayer->pszALName, sizeof(pMMArcLayer->pszALName),
                     "%s_boundA.~xy", hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            snprintf(pMMArcLayer->pszALName, sizeof(pMMArcLayer->pszALName),
                     "%sA.~xy", hMiraMonLayer->pszSrcLayerName);
        }

        if (nullptr == (pMMArcLayer->pFAL = VSIFOpenL(pMMArcLayer->pszALName,
                                                      hMiraMonLayer->pszFlags)))
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error pMMArcLayer->pFAL: Cannot open file %s.",
                     pMMArcLayer->pszALName);
            return 1;
        }
        VSIFSeekL(pMMArcLayer->pFAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->FlushAL, pMMArcLayer->pFAL, MM_1MB,
                        &pMMArcLayer->pAL, 0, 0))
            return 1;
    }

    // 3D
    if (pArcTopHeader->bIs3d)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            if (hMiraMonLayer->bIsPolygon)
            {
                snprintf(pMMArcLayer->psz3DLayerName,
                         sizeof(pMMArcLayer->psz3DLayerName), "%s_boundA.~z",
                         hMiraMonLayer->pszSrcLayerName);
            }
            else
            {
                snprintf(pMMArcLayer->psz3DLayerName,
                         sizeof(pMMArcLayer->psz3DLayerName), "%sA.~z",
                         hMiraMonLayer->pszSrcLayerName);
            }

            if (nullptr ==
                (pMMArcLayer->pF3d = VSIFOpenL(pMMArcLayer->psz3DLayerName,
                                               hMiraMonLayer->pszFlags)))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Error pMMArcLayer->pF3d: Cannot open file %s.",
                         pMMArcLayer->psz3DLayerName);
                return 1;
            }
            VSIFSeekL(pMMArcLayer->pF3d, 0, SEEK_SET);
        }

        if (MMInitZSectionLayer(hMiraMonLayer, pMMArcLayer->pF3d,
                                &pMMArcLayer->pZSection))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading the format in file %s %d.",
                     pMMArcLayer->pszLayerName, 6);
            return 1;
        }

        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if (MMReadZSection(hMiraMonLayer, pMMArcLayer->pF,
                               &pMMArcLayer->pZSection))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error reading the format in file %s.",
                         pMMArcLayer->pszLayerName);
                return 1;
            }

            if (MMReadZDescriptionHeaders(hMiraMonLayer, pMMArcLayer->pF,
                                          pArcTopHeader->nElemCount,
                                          &pMMArcLayer->pZSection))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error reading the format in file %s.",
                         pMMArcLayer->pszLayerName);
                return 1;
            }
        }
    }
    // MiraMon metadata
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMArcLayer->pszREL_LayerName,
                     sizeof(pMMArcLayer->pszREL_LayerName), "%s_boundA.rel",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            strcpy(pMMArcLayer->pszREL_LayerName, pMMArcLayer->pszLayerName);
            if (MMChangeFinalPartOfTheName(pMMArcLayer->pszREL_LayerName,
                                           MM_CPL_PATH_BUF_SIZE, ".arc",
                                           "A.rel"))
                return 1;
        }
    }
    else
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMArcLayer->pszREL_LayerName,
                     sizeof(pMMArcLayer->pszREL_LayerName), "%sA.rel",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            CPLStrlcpy(pMMArcLayer->pszREL_LayerName,
                       hMiraMonLayer->pszSrcLayerName,
                       sizeof(pMMArcLayer->pszREL_LayerName));
            if (MMChangeFinalPartOfTheName(pMMArcLayer->pszREL_LayerName,
                                           MM_CPL_PATH_BUF_SIZE, ".arc",
                                           "A.rel"))
                return 1;
        }
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        // This file has to exist and be the appropriate version.
        if (MMCheck_REL_FILE(pMMArcLayer->pszREL_LayerName))
            return 1;
    }

    if (!hMiraMonLayer->bIsPolygon)
        hMiraMonLayer->pszMainREL_LayerName = pMMArcLayer->pszREL_LayerName;

    // MIRAMON DATA BASE
    // Creating the DBF file name
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                     sizeof(pMMArcLayer->MMAdmDB.pszExtDBFLayerName),
                     "%s_boundA.dbf", hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            strcpy(pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                   pMMArcLayer->pszLayerName);
            if (MMChangeFinalPartOfTheName(
                    pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                    MM_CPL_PATH_BUF_SIZE, ".arc", "A.dbf"))
                return 1;
        }
    }
    else
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                     sizeof(pMMArcLayer->MMAdmDB.pszExtDBFLayerName), "%sA.dbf",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            CPLStrlcpy(pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                       hMiraMonLayer->pszSrcLayerName,
                       sizeof(pMMArcLayer->MMAdmDB.pszExtDBFLayerName));
            if (MMChangeFinalPartOfTheName(
                    pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
                    MM_CPL_PATH_BUF_SIZE, ".arc", "A.dbf"))
                return 1;
        }
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    // Node part
    if (MMInitNodeLayer(hMiraMonLayer))
        return 1;
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        MMSet1_1Version(&pMMArcLayer->TopNodeHeader);
    else
        MMSet2_0Version(&pMMArcLayer->TopNodeHeader);

    return 0;
}

static int MMInitPolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    // Init header structure
    hMiraMonLayer->bIsPolygon = 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d = 1;  // Read description of bRealIs3d
        MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);

        hMiraMonLayer->TopHeader.aFileType[0] = 'P';
        hMiraMonLayer->TopHeader.aFileType[1] = 'O';
        hMiraMonLayer->TopHeader.aFileType[2] = 'L';

        snprintf(pMMPolygonLayer->pszLayerName,
                 sizeof(pMMPolygonLayer->pszLayerName), "%s.pol",
                 hMiraMonLayer->pszSrcLayerName);
    }

    if (nullptr ==
        (pMMPolygonLayer->pF =
             VSIFOpenL(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszFlags)))
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Error pMMPolygonLayer->pF: Cannot open file %s.",
                 pMMPolygonLayer->pszLayerName);
        return 1;
    }

    // PS
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMPolygonLayer->nPSElementSize = MM_SIZE_OF_PS_32BITS;
    else
        pMMPolygonLayer->nPSElementSize = MM_SIZE_OF_PS_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        snprintf(pMMPolygonLayer->pszPSName, sizeof(pMMPolygonLayer->pszPSName),
                 "%sP.~PS", hMiraMonLayer->pszSrcLayerName);

        if (nullptr ==
            (pMMPolygonLayer->pFPS = VSIFOpenL(pMMPolygonLayer->pszPSName,
                                               hMiraMonLayer->pszFlags)))
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error pMMPolygonLayer->pFPS: Cannot open file %s.",
                     pMMPolygonLayer->pszPSName);
            return 1;
        }
        VSIFSeekL(pMMPolygonLayer->pFPS, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPS, pMMPolygonLayer->pFPS,
                        MM_1MB, &pMMPolygonLayer->pPS, 0,
                        pMMPolygonLayer->nPSElementSize))
            return 1;
    }

    // PH
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMPolygonLayer->nPHElementSize = MM_SIZE_OF_PH_32BITS;
    else
        pMMPolygonLayer->nPHElementSize = MM_SIZE_OF_PH_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        pMMPolygonLayer->nMaxPolHeader = MM_FIRST_NUMBER_OF_POLYGONS + 1;
    else
        pMMPolygonLayer->nMaxPolHeader = hMiraMonLayer->TopHeader.nElemCount;

    if (pMMPolygonLayer->nMaxPolHeader)
    {
        if (MMCheckSize_t(pMMPolygonLayer->nMaxPolHeader,
                          sizeof(*pMMPolygonLayer->pPolHeader)))
            return 1;
        if (nullptr == (pMMPolygonLayer->pPolHeader = (struct MM_PH *)VSICalloc(
                            (size_t)pMMPolygonLayer->nMaxPolHeader,
                            sizeof(*pMMPolygonLayer->pPolHeader))))
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMInitPolygonLayer())");
            return 1;
        }
    }
    else
        pMMPolygonLayer->pPolHeader = nullptr;

    // PAL
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMPolygonLayer->nPALElementSize = MM_SIZE_OF_PAL_32BITS;
    else
        pMMPolygonLayer->nPALElementSize = MM_SIZE_OF_PAL_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // Universal polygon.
        memset(pMMPolygonLayer->pPolHeader, 0,
               sizeof(*pMMPolygonLayer->pPolHeader));
        hMiraMonLayer->TopHeader.nElemCount = 1;

        // PAL
        snprintf(pMMPolygonLayer->pszPALName,
                 sizeof(pMMPolygonLayer->pszPALName), "%sP.~idx",
                 hMiraMonLayer->pszSrcLayerName);

        if (nullptr ==
            (pMMPolygonLayer->pFPAL = VSIFOpenL(pMMPolygonLayer->pszPALName,
                                                hMiraMonLayer->pszFlags)))
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Error pMMPolygonLayer->pFPAL: Cannot open file %s.",
                     pMMPolygonLayer->pszPALName);
            return 1;
        }
        VSIFSeekL(pMMPolygonLayer->pFPAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPAL, pMMPolygonLayer->pFPAL,
                        MM_1MB, &pMMPolygonLayer->pPAL, 0, 0))
            return 1;
    }

    // MiraMon metadata

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        snprintf(hMiraMonLayer->MMPolygon.pszREL_LayerName,
                 sizeof(hMiraMonLayer->MMPolygon.pszREL_LayerName), "%sP.rel",
                 hMiraMonLayer->pszSrcLayerName);
    }
    else
    {
        CPLStrlcpy(hMiraMonLayer->MMPolygon.pszREL_LayerName,
                   hMiraMonLayer->pszSrcLayerName,
                   sizeof(hMiraMonLayer->MMPolygon.pszREL_LayerName));

        if (MMChangeFinalPartOfTheName(
                hMiraMonLayer->MMPolygon.pszREL_LayerName, MM_CPL_PATH_BUF_SIZE,
                ".pol", "P.rel"))
            return 1;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        // This file has to exist and be the appropriate version.
        if (MMCheck_REL_FILE(hMiraMonLayer->MMPolygon.pszREL_LayerName))
            return 1;
    }

    hMiraMonLayer->pszMainREL_LayerName =
        hMiraMonLayer->MMPolygon.pszREL_LayerName;

    // MIRAMON DATA BASE
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        snprintf(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName,
                 sizeof(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName), "%sP.dbf",
                 hMiraMonLayer->pszSrcLayerName);
    }
    else
    {
        CPLStrlcpy(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName,
                   hMiraMonLayer->pszSrcLayerName,
                   sizeof(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName));
        if (MMChangeFinalPartOfTheName(
                pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName,
                MM_CPL_PATH_BUF_SIZE, ".pol", "P.dbf"))
            return 1;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    return 0;
}

int MMInitLayerByType(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->eLT == MM_LayerType_Point ||
        hMiraMonLayer->eLT == MM_LayerType_Point3d)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(hMiraMonLayer->MMPoint.pszLayerName,
                     sizeof(hMiraMonLayer->MMPoint.pszLayerName), "%s.pnt",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            CPLStrlcpy(hMiraMonLayer->MMPoint.pszLayerName,
                       hMiraMonLayer->pszSrcLayerName,
                       sizeof(hMiraMonLayer->MMPoint.pszLayerName));
        }
        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                        hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pnt\n",
                        CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
        }

        if (MMInitPointLayer(hMiraMonLayer))
        {
            // Error specified inside the function
            return 1;
        }
        return 0;
    }
    if (hMiraMonLayer->eLT == MM_LayerType_Arc ||
        hMiraMonLayer->eLT == MM_LayerType_Arc3d)
    {
        struct MiraMonArcLayer *pMMArcLayer = &hMiraMonLayer->MMArc;

        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMArcLayer->pszLayerName,
                     sizeof(pMMArcLayer->pszLayerName), "%s.arc",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            CPLStrlcpy(pMMArcLayer->pszLayerName,
                       hMiraMonLayer->pszSrcLayerName,
                       sizeof(pMMArcLayer->pszLayerName));
        }

        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                        hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.arc\n",
                        CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
        }

        if (MMInitArcLayer(hMiraMonLayer))
        {
            // Error specified inside the function
            return 1;
        }
        return 0;
    }
    if (hMiraMonLayer->eLT == MM_LayerType_Pol ||
        hMiraMonLayer->eLT == MM_LayerType_Pol3d)
    {
        struct MiraMonPolygonLayer *pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

        if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
        {
            snprintf(pMMPolygonLayer->pszLayerName,
                     sizeof(pMMPolygonLayer->pszLayerName), "%s.pol",
                     hMiraMonLayer->pszSrcLayerName);
        }
        else
        {
            CPLStrlcpy(pMMPolygonLayer->pszLayerName,
                       hMiraMonLayer->pszSrcLayerName,
                       sizeof(pMMPolygonLayer->pszLayerName));
        }

        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                        hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintfL(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pol\n",
                        CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
        }

        if (MMInitPolygonLayer(hMiraMonLayer))
        {
            // Error specified inside the function
            return 1;
        }

        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            char *pszArcLayerName;
            const char *pszExt;
            // StringLine associated to the polygon
            pszArcLayerName = MMReturnValueFromSectionINIFile(
                pMMPolygonLayer->pszREL_LayerName,
                SECTION_OVVW_ASPECTES_TECNICS, KEY_ArcSource);
            if (pszArcLayerName)
            {
                MM_RemoveInitial_and_FinalQuotationMarks(pszArcLayerName);

                // If extension is not specified ".arc" will be used
                pszExt = CPLGetExtension(pszArcLayerName);
                if (MMIsEmptyString(pszExt))
                {
                    char *pszArcLayerNameAux =
                        VSICalloc(1, strlen(pszArcLayerName) + 5);
                    if (!pszArcLayerNameAux)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Memory error in MiraMon "
                                 "driver (MMInitLayerByType())");
                        VSIFree(pszArcLayerName);
                        return 1;
                    }
                    snprintf(pszArcLayerNameAux, strlen(pszArcLayerName) + 5,
                             "%s.arc", pszArcLayerName);

                    VSIFree(pszArcLayerName);
                    pszArcLayerName = pszArcLayerNameAux;
                }

                CPLStrlcpy(
                    pMMPolygonLayer->MMArc.pszLayerName,
                    CPLFormFilename(CPLGetPath(hMiraMonLayer->pszSrcLayerName),
                                    pszArcLayerName, ""),
                    sizeof(pMMPolygonLayer->MMArc.pszLayerName));

                VSIFree(pszArcLayerName);
            }
            else
            {
                // There is no arc layer on the metada file
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Error reading the ARC file in the metadata file %s.",
                         pMMPolygonLayer->pszREL_LayerName);
                return 1;
            }

            if (nullptr == (hMiraMonLayer->MMPolygon.MMArc.pF =
                                VSIFOpenL(pMMPolygonLayer->MMArc.pszLayerName,
                                          hMiraMonLayer->pszFlags)))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Error pMMPolygonLayer.MMArc.pF: Cannot open file %s.",
                         pMMPolygonLayer->MMArc.pszLayerName);
                return 1;
            }

            if (MMReadHeader(hMiraMonLayer->MMPolygon.MMArc.pF,
                             &hMiraMonLayer->MMPolygon.TopArcHeader))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error reading the format in file %s.",
                         pMMPolygonLayer->MMArc.pszLayerName);
                return 1;
            }

            if (MMReadPHPolygonSection(hMiraMonLayer))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error reading the format in file %s.",
                         pMMPolygonLayer->MMArc.pszLayerName);
                return 1;
            }

            fclose_and_nullify(&hMiraMonLayer->MMPolygon.MMArc.pF);
        }
        else
        {
            // Creating the stringLine file associated to the polygon
            snprintf(pMMPolygonLayer->MMArc.pszLayerName,
                     sizeof(pMMPolygonLayer->MMArc.pszLayerName), "%s.arc",
                     hMiraMonLayer->pszSrcLayerName);
        }

        if (MMInitArcLayer(hMiraMonLayer))
        {
            // Error specified inside the function
            return 1;
        }

        // Polygon is 3D if Arc is 3D, by definition.
        hMiraMonLayer->TopHeader.bIs3d =
            hMiraMonLayer->MMPolygon.TopArcHeader.bIs3d;

        if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
            MMSet1_1Version(&pMMPolygonLayer->TopArcHeader);
        else
            MMSet2_0Version(&pMMPolygonLayer->TopArcHeader);
    }
    else if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        // Trying to get DBF information
        snprintf(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName,
                 sizeof(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName),
                 "%s.dbf", hMiraMonLayer->pszSrcLayerName);
    }

    return 0;
}

int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                const char *pzFileName, int LayerVersion, char nMMRecode,
                char nMMLanguage, struct MiraMonDataBase *pLayerDB,
                MM_BOOLEAN ReadOrWrite, struct MiraMonVectMapInfo *MMMap)
{
    if (!hMiraMonLayer)
        return 1;

    // Some variables must be initialized
    MM_FillFieldDescriptorByLanguage();

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));

    //hMiraMonLayer->Version = MM_VECTOR_LAYER_LAST_VERSION;

    hMiraMonLayer->ReadOrWrite = ReadOrWrite;
    hMiraMonLayer->MMMap = MMMap;

    // Don't free in destructor
    hMiraMonLayer->pLayerDB = pLayerDB;

    // Opening mode
    strcpy(hMiraMonLayer->pszFlags, "wb+");

    if (LayerVersion == MM_UNKNOWN_VERSION)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unknown version in MiraMon driver.");
        return 1;
    }
    if (LayerVersion == MM_LAST_VERSION)
    {
        MMSet1_1Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize = MM_HEADER_SIZE_64_BITS;
        hMiraMonLayer->LayerVersion = MM_64BITS_VERSION;
    }
    else if (LayerVersion == MM_32BITS_VERSION)
    {
        MMSet1_1Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize = MM_HEADER_SIZE_32_BITS;
        hMiraMonLayer->LayerVersion = MM_32BITS_VERSION;
    }
    else
    {
        MMSet2_0Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize = MM_HEADER_SIZE_64_BITS;
        hMiraMonLayer->LayerVersion = MM_64BITS_VERSION;
    }

    hMiraMonLayer->pszSrcLayerName = CPLStrdup(pzFileName);
    hMiraMonLayer->szLayerTitle = CPLStrdup(CPLGetFilename(pzFileName));

    if (!hMiraMonLayer->bIsBeenInit &&
        hMiraMonLayer->eLT != MM_LayerType_Unknown)
    {
        if (MMInitLayerByType(hMiraMonLayer))
        {
            // Error specified inside the function
            return 1;
        }
        hMiraMonLayer->bIsBeenInit = 1;
    }

    // If more nNumStringToOperate is needed, it'll be increased.
    hMiraMonLayer->nNumStringToOperate = 0;
    if (MMResizeStringToOperateIfNeeded(hMiraMonLayer, 500))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMInitLayer())");
        return 1;
    }

    hMiraMonLayer->nMMLanguage = nMMLanguage;

    if (nMMRecode == MM_RECODE_UTF8)
        hMiraMonLayer->nCharSet = MM_JOC_CARAC_UTF8_DBF;
    else  //if(nMMRecode==MM_RECODE_ANSI)
        hMiraMonLayer->nCharSet = MM_JOC_CARAC_ANSI_DBASE;
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Closing MiraMon layers                         */
/* -------------------------------------------------------------------- */
static int MMClose3DSectionLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                 MM_INTERNAL_FID nElements, VSILFILE *pF,
                                 VSILFILE *pF3d, const char *pszF3d,
                                 struct MM_ZSection *pZSection,
                                 MM_FILE_OFFSET FinalOffset)
{
    int ret_code = 1;
    if (!hMiraMonLayer)
        return 1;

    // Avoid closing when it has no sense. But it's not an error.
    // Just return elegantly.
    if (!pF || !pF3d || !pszF3d || !pZSection)
        return 0;

    if (hMiraMonLayer->bIsReal3d)
    {
        pZSection->ZSectionOffset = FinalOffset;
        if (MMWriteZSection(pF, pZSection))
            goto end_label;

        // Header 3D. Writes it after header
        if (MMWriteZDescriptionHeaders(hMiraMonLayer, pF, nElements, pZSection))
            goto end_label;

        // ZL section
        pZSection->FlushZL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pZSection->FlushZL))
            goto end_label;

        if (MMMoveFromFileToFile(pF3d, pF, &pZSection->ZSectionOffset))
            goto end_label;
    }

    ret_code = 0;
end_label:
    fclose_and_nullify(&pF3d);
    if (pszF3d && *pszF3d != '\0')
        VSIUnlink(pszF3d);

    return ret_code;
}

static int MMClosePointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 1;
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->TopHeader))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     hMiraMonLayer->MMPoint.pszLayerName);
            goto end_label;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // TL Section
        hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     hMiraMonLayer->MMPoint.pszLayerName);
            goto end_label;
        }
        if (MMMoveFromFileToFile(hMiraMonLayer->MMPoint.pFTL,
                                 hMiraMonLayer->MMPoint.pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     hMiraMonLayer->MMPoint.pszLayerName);
            goto end_label;
        }

        fclose_and_nullify(&hMiraMonLayer->MMPoint.pFTL);

        if (*hMiraMonLayer->MMPoint.pszTLName != '\0')
            VSIUnlink(hMiraMonLayer->MMPoint.pszTLName);

        if (MMClose3DSectionLayer(
                hMiraMonLayer, hMiraMonLayer->TopHeader.nElemCount,
                hMiraMonLayer->MMPoint.pF, hMiraMonLayer->MMPoint.pF3d,
                hMiraMonLayer->MMPoint.psz3DLayerName,
                &hMiraMonLayer->MMPoint.pZSection, hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     hMiraMonLayer->MMPoint.pszLayerName);
            goto end_label;
        }
    }

    ret_code = 0;
end_label:
    fclose_and_nullify(&hMiraMonLayer->MMPoint.pF);
    return ret_code;
}

static int MMCloseNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 1;
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->MMNode.pF, &pMMArcLayer->TopNodeHeader))
            goto end_label;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // NH Section
        if (MMWriteNHNodeSection(hMiraMonLayer, hMiraMonLayer->nHeaderDiskSize))
            goto end_label;

        // NL Section
        pMMArcLayer->MMNode.FlushNL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMArcLayer->MMNode.FlushNL))
            goto end_label;
        if (MMMoveFromFileToFile(pMMArcLayer->MMNode.pFNL,
                                 pMMArcLayer->MMNode.pF,
                                 &hMiraMonLayer->OffsetCheck))
            goto end_label;

        fclose_and_nullify(&pMMArcLayer->MMNode.pFNL);
        if (*pMMArcLayer->MMNode.pszNLName != '\0')
            VSIUnlink(pMMArcLayer->MMNode.pszNLName);
    }

    ret_code = 0;
end_label:
    fclose_and_nullify(&pMMArcLayer->MMNode.pFNL);

    fclose_and_nullify(&pMMArcLayer->MMNode.pF);

    return ret_code;
}

static int MMCloseArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 0;
    struct MiraMonArcLayer *pMMArcLayer;
    struct MM_TH *pArcTopHeader;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader = &hMiraMonLayer->MMPolygon.TopArcHeader;
    }
    else
    {
        pMMArcLayer = &hMiraMonLayer->MMArc;
        pArcTopHeader = &hMiraMonLayer->TopHeader;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = pArcTopHeader->nElemCount;
        pArcTopHeader->bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->pF, pArcTopHeader))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMArcLayer->pszLayerName);
            goto end_label;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // AH Section
        if (MMWriteAHArcSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMArcLayer->pszLayerName);
            goto end_label;
        }

        // AL Section
        pMMArcLayer->FlushAL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMArcLayer->FlushAL))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMArcLayer->pszLayerName);
            goto end_label;
        }
        if (MMMoveFromFileToFile(pMMArcLayer->pFAL, pMMArcLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMArcLayer->pszLayerName);
            goto end_label;
        }
        fclose_and_nullify(&pMMArcLayer->pFAL);

        if (*pMMArcLayer->pszALName != '\0')
            VSIUnlink(pMMArcLayer->pszALName);

        // 3D Section
        if (MMClose3DSectionLayer(
                hMiraMonLayer, pArcTopHeader->nElemCount, pMMArcLayer->pF,
                pMMArcLayer->pF3d, pMMArcLayer->psz3DLayerName,
                &pMMArcLayer->pZSection, hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMArcLayer->pszLayerName);
            goto end_label;
        }
    }

    ret_code = 0;
end_label:
    fclose_and_nullify(&pMMArcLayer->pF);

    fclose_and_nullify(&pMMArcLayer->pFAL);

    if (MMCloseNodeLayer(hMiraMonLayer))
        ret_code = 1;

    return ret_code;
}

static int MMClosePolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 0;
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    MMCloseArcLayer(hMiraMonLayer);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMPolygonLayer->pF, &hMiraMonLayer->TopHeader))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // PS Section
        pMMPolygonLayer->FlushPS.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPS))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPS, pMMPolygonLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }

        fclose_and_nullify(&pMMPolygonLayer->pFPS);
        if (*pMMPolygonLayer->pszPSName != '\0')
            VSIUnlink(pMMPolygonLayer->pszPSName);

        // AH Section
        if (MMWritePHPolygonSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }

        // PAL Section
        pMMPolygonLayer->FlushPAL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPAL))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPAL, pMMPolygonLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess, "Error writing to file %s",
                     pMMPolygonLayer->pszLayerName);
            goto end_label;
        }
        fclose_and_nullify(&pMMPolygonLayer->pFPAL);

        if (*pMMPolygonLayer->pszPALName != '\0')
            VSIUnlink(pMMPolygonLayer->pszPALName);
    }

    ret_code = 0;

end_label:
    fclose_and_nullify(&pMMPolygonLayer->pF);

    fclose_and_nullify(&pMMPolygonLayer->pFPAL);

    return ret_code;
}

int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 0;
    //CheckMMVectorLayerVersion(hMiraMonLayer, 1)

    if (!hMiraMonLayer)
        return 0;

    if (hMiraMonLayer->bIsPoint)
    {
        ret_code = MMClosePointLayer(hMiraMonLayer);
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        ret_code = MMCloseArcLayer(hMiraMonLayer);
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        ret_code = MMClosePolygonLayer(hMiraMonLayer);
    }
    else if (hMiraMonLayer->bIsDBF)
    {
        // If no geometry, remove all created files
        if (hMiraMonLayer->pszSrcLayerName)
            VSIUnlink(hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->szLayerTitle)
            VSIUnlink(hMiraMonLayer->szLayerTitle);
    }

    // MiraMon metadata files
    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        if (MMWriteVectorMetadata(hMiraMonLayer))
        {
            CPLError(CE_Failure, CPLE_NoWriteAccess,
                     "Some error writing in metadata file of the layer");
            ret_code = 1;
        }
    }

    // MiraMon database files
    if (MMCloseMMBD_XP(hMiraMonLayer))
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Some error writing in DBF file of the layer");
        ret_code = 1;
    }
    return ret_code;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Destroying (allocated memory)                  */
/* -------------------------------------------------------------------- */
static void MMDestroyMMAdmDB(struct MMAdmDatabase *pMMAdmDB)
{
    if (pMMAdmDB->pRecList)
    {
        VSIFree(pMMAdmDB->pRecList);
        pMMAdmDB->pRecList = nullptr;
    }

    if (pMMAdmDB->szRecordOnCourse)
    {
        VSIFree(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse = nullptr;
        pMMAdmDB->nNumRecordOnCourse = 0;
    }
}

static int MMDestroyPointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->MMPoint.pTL)
    {
        VSIFree(hMiraMonLayer->MMPoint.pTL);
        hMiraMonLayer->MMPoint.pTL = nullptr;
    }

    MMDestroyZSectionDescription(&hMiraMonLayer->MMPoint.pZSection);
    MMDestroyMMAdmDB(&hMiraMonLayer->MMPoint.MMAdmDB);

    return 0;
}

static int MMDestroyNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (pMMArcLayer->MMNode.pNL)
    {
        VSIFree(pMMArcLayer->MMNode.pNL);
        pMMArcLayer->MMNode.pNL = nullptr;
    }

    if (pMMArcLayer->MMNode.pNodeHeader)
    {
        VSIFree(pMMArcLayer->MMNode.pNodeHeader);
        pMMArcLayer->MMNode.pNodeHeader = nullptr;
    }

    MMDestroyMMAdmDB(&hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    return 0;
}

static int MMDestroyArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (pMMArcLayer->pAL)
    {
        VSIFree(pMMArcLayer->pAL);
        pMMArcLayer->pAL = nullptr;
    }
    if (pMMArcLayer->pArcHeader)
    {
        VSIFree(pMMArcLayer->pArcHeader);
        pMMArcLayer->pArcHeader = nullptr;
    }

    MMDestroyZSectionDescription(&pMMArcLayer->pZSection);
    MMDestroyMMAdmDB(&pMMArcLayer->MMAdmDB);

    MMDestroyNodeLayer(hMiraMonLayer);
    return 0;
}

static int MMDestroyPolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    MMDestroyArcLayer(hMiraMonLayer);

    if (pMMPolygonLayer->pPAL)
    {
        VSIFree(pMMPolygonLayer->pPAL);
        pMMPolygonLayer->pPAL = nullptr;
    }

    if (pMMPolygonLayer->pPS)
    {
        VSIFree(pMMPolygonLayer->pPS);
        pMMPolygonLayer->pPS = nullptr;
    }

    if (pMMPolygonLayer->pPolHeader)
    {
        VSIFree(pMMPolygonLayer->pPolHeader);
        pMMPolygonLayer->pPolHeader = nullptr;
    }

    MMDestroyMMAdmDB(&pMMPolygonLayer->MMAdmDB);

    return 0;
}

int MMDestroyLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    //CheckMMVectorLayerVersion(hMiraMonLayer, 1)

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPoint)
        MMDestroyPointLayer(hMiraMonLayer);
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
        MMDestroyArcLayer(hMiraMonLayer);
    else if (hMiraMonLayer->bIsPolygon)
        MMDestroyPolygonLayer(hMiraMonLayer);

    if (hMiraMonLayer->pszSrcLayerName)
    {
        VSIFree(hMiraMonLayer->pszSrcLayerName);
        hMiraMonLayer->pszSrcLayerName = nullptr;
    }
    if (hMiraMonLayer->szLayerTitle)
    {
        VSIFree(hMiraMonLayer->szLayerTitle);
        hMiraMonLayer->szLayerTitle = nullptr;
    }
    if (hMiraMonLayer->pSRS)
    {
        VSIFree(hMiraMonLayer->pSRS);
        hMiraMonLayer->pSRS = nullptr;
    }

    if (hMiraMonLayer->pZUnit)
    {
        VSIFree(hMiraMonLayer->pZUnit);
        hMiraMonLayer->pZUnit = nullptr;
    }

    if (hMiraMonLayer->pMultRecordIndex)
    {
        VSIFree(hMiraMonLayer->pMultRecordIndex);
        hMiraMonLayer->pMultRecordIndex = nullptr;
    }

    if (hMiraMonLayer->ReadFeature.pNCoordRing)
    {
        free(hMiraMonLayer->ReadFeature.pNCoordRing);
        hMiraMonLayer->ReadFeature.pNCoordRing = nullptr;
    }
    if (hMiraMonLayer->ReadFeature.pCoord)
    {
        free(hMiraMonLayer->ReadFeature.pCoord);
        hMiraMonLayer->ReadFeature.pCoord = nullptr;
    }
    if (hMiraMonLayer->ReadFeature.pZCoord)
    {
        free(hMiraMonLayer->ReadFeature.pZCoord);
        hMiraMonLayer->ReadFeature.pZCoord = nullptr;
    }
    if (hMiraMonLayer->ReadFeature.pRecords)
    {
        free(hMiraMonLayer->ReadFeature.pRecords);
        hMiraMonLayer->ReadFeature.pRecords = nullptr;
    }
    if (hMiraMonLayer->ReadFeature.flag_VFG)
    {
        free(hMiraMonLayer->ReadFeature.flag_VFG);
        hMiraMonLayer->ReadFeature.flag_VFG = nullptr;
    }

    if (hMiraMonLayer->pArcs)
    {
        VSIFree(hMiraMonLayer->pArcs);
        hMiraMonLayer->pArcs = nullptr;
    }

    if (hMiraMonLayer->szStringToOperate)
    {
        VSIFree(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate = nullptr;
        hMiraMonLayer->nNumStringToOperate = 0;
    }

    if (hMiraMonLayer->pLayerDB)
    {
        if (hMiraMonLayer->pLayerDB->pFields)
        {
            VSIFree(hMiraMonLayer->pLayerDB->pFields);
            hMiraMonLayer->pLayerDB->pFields = nullptr;
        }
        VSIFree(hMiraMonLayer->pLayerDB);
        hMiraMonLayer->pLayerDB = nullptr;
    }

    // Destroys all database objects
    MMDestroyMMDB(hMiraMonLayer);

    return 0;
}

/* -------------------------------------------------------------------- */
/*      Flush Layer Functions                                           */
/* -------------------------------------------------------------------- */

// Initializes a MM_FLUSH_INFO structure, which is used for buffering
// data before writing it to a file.
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, VSILFILE *pF, GUInt64 nBlockSize,
                char **pBuffer, MM_FILE_OFFSET DiskOffsetWhereToFlush,
                GInt32 nMyDiskSize)
{
    memset(pFlush, 0, sizeof(*pFlush));
    *pBuffer = nullptr;

    pFlush->nMyDiskSize = nMyDiskSize;
    pFlush->pF = pF;
    pFlush->nBlockSize = nBlockSize;
    pFlush->nNumBytes = 0;
    if (MMCheckSize_t(nBlockSize, 1))
        return 1;

    if (!nBlockSize)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Error in MiraMon "
                 "driver: MMInitFlush() with no bytes to process");
        return 1;
    }

    if (nullptr == (*pBuffer = (char *)VSICalloc(1, (size_t)nBlockSize)))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMInitFlush())");
        return 1;
    }
    pFlush->OffsetWhereToFlush = DiskOffsetWhereToFlush;
    pFlush->CurrentOffset = 0;
    return 0;
}

// Reads data from a file into a buffer.
int MMReadFlush(struct MM_FLUSH_INFO *pFlush)
{
    VSIFSeekL(pFlush->pF, pFlush->OffsetWhereToFlush, SEEK_SET);
    if (pFlush->nBlockSize !=
        (GUInt64)(VSIFReadL(pFlush->pBlockWhereToSaveOrRead, 1,
                            (size_t)pFlush->nBlockSize, pFlush->pF)))
        return 1;
    return 0;
}

// Flushes data from a buffer to a disk file.
static int MMFlushToDisk(struct MM_FLUSH_INFO *FlushInfo)
{
    if (!FlushInfo->nNumBytes)
        return 0;
    // Just flush to the disk at the correct place.
    VSIFSeekL(FlushInfo->pF, FlushInfo->OffsetWhereToFlush, SEEK_SET);

    if (FlushInfo->nNumBytes !=
        (GUInt64)VSIFWriteL(FlushInfo->pBlockWhereToSaveOrRead, 1,
                            (size_t)FlushInfo->nNumBytes, FlushInfo->pF))
        return 1;
    FlushInfo->OffsetWhereToFlush += FlushInfo->nNumBytes;
    FlushInfo->NTimesFlushed++;
    FlushInfo->TotalSavedBytes += FlushInfo->nNumBytes;
    FlushInfo->nNumBytes = 0;

    return 0;
}

// Reads a block of data from a buffer in memory
int MMReadBlockFromBuffer(struct MM_FLUSH_INFO *FlushInfo)
{
    if (!FlushInfo->SizeOfBlockToBeSaved)
        return 0;

    if (FlushInfo->pBlockToBeSaved)
    {
        memcpy(FlushInfo->pBlockToBeSaved,
               (void *)((char *)FlushInfo->pBlockWhereToSaveOrRead +
                        FlushInfo->CurrentOffset),
               FlushInfo->SizeOfBlockToBeSaved);
    }
    FlushInfo->CurrentOffset += FlushInfo->SizeOfBlockToBeSaved;

    return 0;
}

// Appends a block of data to a buffer in memory, which is
// used for later flushing to disk.
int MMAppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo)
{
    if (FlushInfo->SizeOfBlockToBeSaved)
    {
        // If all the bloc itself does not fit to the buffer,
        // then all the block is written directly to the disk
        if (FlushInfo->nNumBytes == 0 &&
            FlushInfo->SizeOfBlockToBeSaved >= FlushInfo->nBlockSize)
        {
            if (MMFlushToDisk(FlushInfo))
                return 1;
            return 0;
        }

        // There is space in FlushInfo->pBlockWhereToSaveOrRead?
        if (FlushInfo->nNumBytes + FlushInfo->SizeOfBlockToBeSaved <=
            FlushInfo->nBlockSize)
        {
            if (FlushInfo->pBlockToBeSaved)
            {
                memcpy((void *)((char *)FlushInfo->pBlockWhereToSaveOrRead +
                                FlushInfo->nNumBytes),
                       FlushInfo->pBlockToBeSaved,
                       FlushInfo->SizeOfBlockToBeSaved);
            }
            else  // Add zero characters
            {
                char zero_caracters[8] = {0, 0, 0, 0, 0, 0, 0, 0};
                memcpy((char *)FlushInfo->pBlockWhereToSaveOrRead +
                           FlushInfo->nNumBytes,
                       zero_caracters, FlushInfo->SizeOfBlockToBeSaved);
            }

            FlushInfo->nNumBytes += FlushInfo->SizeOfBlockToBeSaved;
        }
        else
        {
            // Empty the buffer
            if (MMFlushToDisk(FlushInfo))
                return 1;
            // Append the pendant bytes
            if (MMAppendBlockToBuffer(FlushInfo))
                return 1;
        }
        return 0;
    }
    // Just flush to the disc.
    return MMFlushToDisk(FlushInfo);
}

// Copy the contents of a temporary file to a final file.
// Used everywhere when closing layers.
int MMMoveFromFileToFile(VSILFILE *pSrcFile, VSILFILE *pDestFile,
                         MM_FILE_OFFSET *pnOffset)
{
    size_t bufferSize = 1024 * 1024;  // 1 MB buffer;
    unsigned char *buffer;
    size_t bytesRead, bytesWritten;

    if (!pSrcFile || !pDestFile || !pnOffset)
        return 0;

    buffer = (unsigned char *)VSICalloc(1, bufferSize);

    if (!buffer)
        return 1;

    VSIFSeekL(pSrcFile, 0, SEEK_SET);
    VSIFSeekL(pDestFile, *pnOffset, SEEK_SET);
    while ((bytesRead = VSIFReadL(buffer, sizeof(unsigned char), bufferSize,
                                  pSrcFile)) > 0)
    {
        bytesWritten =
            VSIFWriteL(buffer, sizeof(unsigned char), bytesRead, pDestFile);
        if (bytesWritten != bytesRead)
        {
            VSIFree(buffer);
            return 1;
        }
        (*pnOffset) += bytesWritten;
    }
    VSIFree(buffer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer: Offsets and variables types managing                     */
/* -------------------------------------------------------------------- */

// Alineation described in format documents.
static void MMGetOffsetAlignedTo8(MM_FILE_OFFSET *Offset)
{
    MM_FILE_OFFSET reajust;

    if ((*Offset) % 8L)
    {
        reajust = 8 - ((*Offset) % 8L);
        (*Offset) += reajust;
    }
}

// Reading integers depending on the version being read.
int MMReadGUInt64DependingOnVersion(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                    struct MM_FLUSH_INFO *FlushInfo,
                                    GUInt64 *pnUI64)
{
    uint32_t nUL32;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
    {
        FlushInfo->pBlockToBeSaved = (void *)&nUL32;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(nUL32);
        if (MMReadBlockFromBuffer(FlushInfo))
        {
            FlushInfo->pBlockToBeSaved = nullptr;
            return 1;
        }
        *pnUI64 = (GUInt64)nUL32;
    }
    else
    {
        FlushInfo->pBlockToBeSaved = (void *)pnUI64;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(*pnUI64);
        if (MMReadBlockFromBuffer(FlushInfo))
        {
            FlushInfo->pBlockToBeSaved = nullptr;
            return 1;
        }
    }
    FlushInfo->pBlockToBeSaved = nullptr;
    return 0;
}

// Reading offsets depending on the version is being read.
int MMReadOffsetDependingOnVersion(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                   struct MM_FLUSH_INFO *FlushInfo,
                                   MM_FILE_OFFSET *pnUI64)
{
    uint32_t nUL32;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
    {
        FlushInfo->pBlockToBeSaved = (void *)&nUL32;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(nUL32);
        if (MMReadBlockFromBuffer(FlushInfo))
        {
            FlushInfo->pBlockToBeSaved = nullptr;
            return 1;
        }
        *pnUI64 = (MM_FILE_OFFSET)nUL32;
    }
    else
    {
        FlushInfo->pBlockToBeSaved = (void *)pnUI64;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(*pnUI64);
        if (MMReadBlockFromBuffer(FlushInfo))
        {
            FlushInfo->pBlockToBeSaved = nullptr;
            return 1;
        }
    }
    FlushInfo->pBlockToBeSaved = nullptr;
    return 0;
}

// Appending integers depending on the version.
int MMAppendIntegerDependingOnVersion(
    struct MiraMonVectLayerInfo *hMiraMonLayer, struct MM_FLUSH_INFO *FlushInfo,
    uint32_t *nUL32, GUInt64 nUI64)
{
    int result;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
    {
        *nUL32 = (uint32_t)nUI64;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(*nUL32);
        hMiraMonLayer->OffsetCheck += FlushInfo->SizeOfBlockToBeSaved;
        FlushInfo->pBlockToBeSaved = (void *)nUL32;
    }
    else
    {
        FlushInfo->SizeOfBlockToBeSaved = sizeof(nUI64);
        hMiraMonLayer->OffsetCheck += FlushInfo->SizeOfBlockToBeSaved;
        FlushInfo->pBlockToBeSaved = (void *)&nUI64;
    }
    result = MMAppendBlockToBuffer(FlushInfo);
    FlushInfo->pBlockToBeSaved = nullptr;
    return result;
}

/* -------------------------------------------------------------------- */
/*      Layer: Reading and writing layer sections                       */
/*      This code follows the specifications of the following document: */
/*             https://www.miramon.cat/new_note/eng/notes/   \          */
/*              FormatFitxersTopologicsMiraMon.pdf                      */
/* -------------------------------------------------------------------- */
int MMReadAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    MM_INTERNAL_FID iElem, nElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    MM_FILE_OFFSET nBlockSize;
    struct MiraMonArcLayer *pMMArcLayer;
    MM_N_VERTICES_TYPE nElementCount;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
        nElem = hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount;
    }
    else
    {
        pMMArcLayer = &hMiraMonLayer->MMArc;
        nElem = hMiraMonLayer->TopHeader.nElemCount;
    }

    if (MMCheckSize_t(nElem, pMMArcLayer->nSizeArcHeader))
    {
        return 1;
    }

    nBlockSize = nElem * (pMMArcLayer->nSizeArcHeader);

    if (MMInitFlush(&FlushTMP, pMMArcLayer->pF, nBlockSize, &pBuffer,
                    hMiraMonLayer->nHeaderDiskSize, 0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    for (iElem = 0; iElem < nElem; iElem++)
    {
        // Bounding box
        FlushTMP.pBlockToBeSaved =
            (void *)&(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        nElementCount = pMMArcLayer->pArcHeader[iElem].nElemCount;
        if (MMReadGUInt64DependingOnVersion(hMiraMonLayer, &FlushTMP,
                                            &nElementCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        pMMArcLayer->pArcHeader[iElem].nElemCount = nElementCount;

        // Offset: offset of the first vertice of the arc
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nOffset))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Length of the arc
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLength;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLength);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}

int MMWriteAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                        MM_FILE_OFFSET DiskOffset)
{
    MM_INTERNAL_FID iElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    uint32_t nUL32;
    MM_FILE_OFFSET nOffsetDiff;
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    nOffsetDiff =
        hMiraMonLayer->nHeaderDiskSize +
        hMiraMonLayer->nFinalElemCount * (pMMArcLayer->nSizeArcHeader);

    if (MMInitFlush(&FlushTMP, pMMArcLayer->pF, MM_1MB, &pBuffer, DiskOffset,
                    0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    for (iElem = 0; iElem < hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nElemCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertice of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Length of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLength);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLength;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}

#ifdef JUST_IN_CASE_WE_NEED_IT_SOMEDAY
static int MMReadNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    MM_INTERNAL_FID iElem, nElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    MM_FILE_OFFSET nBlockSize;
    struct MiraMonArcLayer *pMMArcLayer;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    nElem = pMMArcLayer->TopNodeHeader.nElemCount;

    nBlockSize = nElem * pMMArcLayer->MMNode.nSizeNodeHeader;

    if (MMInitFlush(&FlushTMP, pMMArcLayer->MMNode.pF, nBlockSize, &pBuffer,
                    hMiraMonLayer->nHeaderDiskSize, 0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    for (iElem = 0; iElem < nElem; iElem++)
    {
        // Arcs count
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Node type
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved = 1;
        FlushTMP.pBlockToBeSaved = (void *)nullptr;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Offset: offset of the first arc to the node
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}
#endif  // JUST_IN_CASE_WE_NEED_IT_SOMEDAY

int MMWriteNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_FILE_OFFSET DiskOffset)
{
    MM_INTERNAL_FID iElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    uint32_t nUL32;
    MM_FILE_OFFSET nOffsetDiff;
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    nOffsetDiff = hMiraMonLayer->nHeaderDiskSize +
                  (pMMArcLayer->TopNodeHeader.nElemCount *
                   pMMArcLayer->MMNode.nSizeNodeHeader);

    if (MMInitFlush(&FlushTMP, pMMArcLayer->MMNode.pF, MM_1MB, &pBuffer,
                    DiskOffset, 0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    for (iElem = 0; iElem < pMMArcLayer->TopNodeHeader.nElemCount; iElem++)
    {
        // Arcs count
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        // Node type
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved = 1;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved = (void *)nullptr;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Offset: offset of the first arc to the node
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}

int MMReadPHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    MM_INTERNAL_FID iElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    MM_FILE_OFFSET nBlockSize;
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    if (MMCheckSize_t(hMiraMonLayer->TopHeader.nElemCount,
                      pMMPolygonLayer->nPHElementSize) ||
        MMCheckSize_t(hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount,
                      hMiraMonLayer->MMPolygon.nPSElementSize))
    {
        return 1;
    }
    nBlockSize =
        hMiraMonLayer->TopHeader.nElemCount * (pMMPolygonLayer->nPHElementSize);

    if (MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, nBlockSize, &pBuffer,
                    hMiraMonLayer->nHeaderDiskSize +
                        (hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount *
                         hMiraMonLayer->MMPolygon.nPSElementSize),
                    0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    for (iElem = 0; iElem < hMiraMonLayer->TopHeader.nElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.pBlockToBeSaved =
            (void *)&(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Arcs count: number of arcs of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // External arcs count: number of external arcs of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Rings count: number of rings of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertex of the arc
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nOffset))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Perimeter of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfPerimeter);
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfPerimeter;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Area of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfArea);
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfArea;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}

int MMWritePHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            MM_FILE_OFFSET DiskOffset)
{
    MM_INTERNAL_FID iElem;
    struct MM_FLUSH_INFO FlushTMP;
    char *pBuffer = nullptr;
    uint32_t nUL32;
    MM_FILE_OFFSET nOffsetDiff;
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    if (!pMMPolygonLayer->pF)
        return 0;

    if (!hMiraMonLayer->nFinalElemCount)
        return 0;

    nOffsetDiff = DiskOffset + hMiraMonLayer->TopHeader.nElemCount *
                                   (pMMPolygonLayer->nPHElementSize);

    if (MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, MM_1MB, &pBuffer,
                    DiskOffset, 0))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    for (iElem = 0; iElem < hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Arcs count: number of the arcs of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // External arcs count: number of external arcs of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Rings count: number of rings of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertex of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Perimeter of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfPerimeter);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfPerimeter;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }

        // Area of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfArea);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfArea;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                VSIFree(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            VSIFree(pBuffer);
        return 1;
    }

    if (pBuffer)
        VSIFree(pBuffer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Feature Functions                                               */
/* -------------------------------------------------------------------- */
int MMInitFeature(struct MiraMonFeature *hMMFeature)
{
    memset(hMMFeature, 0, sizeof(*hMMFeature));

    hMMFeature->nMaxMRecords = MM_INIT_NUMBER_OF_RECORDS;
    if (MMCheckSize_t(hMMFeature->nMaxMRecords,
                      sizeof(*(hMMFeature->pRecords))))
        return 1;

    if (!hMMFeature->nMaxMRecords)
        return 0;  // No elements nothing to do.

    if ((hMMFeature->pRecords = VSICalloc((size_t)hMMFeature->nMaxMRecords,
                                          sizeof(*(hMMFeature->pRecords)))) ==
        nullptr)
        return 1;

    hMMFeature->pRecords[0].nMaxField = MM_INIT_NUMBER_OF_FIELDS;
    hMMFeature->pRecords[0].nNumField = 0;
    if (MMCheckSize_t(hMMFeature->pRecords[0].nMaxField,
                      sizeof(*(hMMFeature->pRecords[0].pField))))
        return 1;
    if (nullptr == (hMMFeature->pRecords[0].pField =
                        VSICalloc((size_t)hMMFeature->pRecords[0].nMaxField,
                                  sizeof(*(hMMFeature->pRecords[0].pField)))))
        return 1;

    return 0;
}

// Conserves all allocated memory but resets the information
void MMResetFeatureGeometry(struct MiraMonFeature *hMMFeature)
{
    if (hMMFeature->pNCoordRing)
    {
        memset(hMMFeature->pNCoordRing, 0,
               (size_t)hMMFeature->nMaxpNCoordRing *
                   sizeof(*(hMMFeature->pNCoordRing)));
    }
    if (hMMFeature->pCoord)
    {
        memset(hMMFeature->pCoord, 0,
               (size_t)hMMFeature->nMaxpCoord * sizeof(*(hMMFeature->pCoord)));
    }
    hMMFeature->nICoord = 0;
    if (hMMFeature->pZCoord)
    {
        memset(hMMFeature->pZCoord, 0,
               (size_t)hMMFeature->nMaxpZCoord *
                   sizeof(*(hMMFeature->pZCoord)));
    }
    hMMFeature->nNRings = 0;
    hMMFeature->nIRing = 0;

    if (hMMFeature->flag_VFG)
    {
        memset(hMMFeature->flag_VFG, 0,
               (size_t)hMMFeature->nMaxVFG * sizeof(*(hMMFeature->flag_VFG)));
    }
}

// Preserves all allocated memory but initializes it to zero.
void MMResetFeatureRecord(struct MiraMonFeature *hMMFeature)
{
    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
    MM_EXT_DBF_N_FIELDS nIField;

    if (!hMMFeature->pRecords)
        return;

    for (nIRecord = 0; nIRecord < hMMFeature->nMaxMRecords; nIRecord++)
    {
        if (!hMMFeature->pRecords[nIRecord].pField)
            continue;
        for (nIField = 0; nIField < hMMFeature->pRecords[nIRecord].nMaxField;
             nIField++)
        {
            if (hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)
                *(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue) =
                    '\0';
            hMMFeature->pRecords[nIRecord].pField[nIField].bIsValid = 0;
        }
        hMMFeature->pRecords[nIRecord].nNumField = 0;
    }
    hMMFeature->nNumMRecords = 0;
}

// Destroys all allocated memory
void MMDestroyFeature(struct MiraMonFeature *hMMFeature)
{
    if (hMMFeature->pCoord)
    {
        VSIFree(hMMFeature->pCoord);
        hMMFeature->pCoord = nullptr;
    }
    if (hMMFeature->pZCoord)
    {
        VSIFree(hMMFeature->pZCoord);
        hMMFeature->pZCoord = nullptr;
    }
    if (hMMFeature->pNCoordRing)
    {
        VSIFree(hMMFeature->pNCoordRing);
        hMMFeature->pNCoordRing = nullptr;
    }

    if (hMMFeature->flag_VFG)
    {
        VSIFree(hMMFeature->flag_VFG);
        hMMFeature->flag_VFG = nullptr;
    }

    if (hMMFeature->pRecords)
    {
        MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
        MM_EXT_DBF_N_FIELDS nIField;

        for (nIRecord = 0; nIRecord < hMMFeature->nMaxMRecords; nIRecord++)
        {
            if (!hMMFeature->pRecords[nIRecord].pField)
                continue;
            for (nIField = 0;
                 nIField < hMMFeature->pRecords[nIRecord].nMaxField; nIField++)
            {
                if (hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)
                    VSIFree(hMMFeature->pRecords[nIRecord]
                                .pField[nIField]
                                .pDinValue);
            }
            VSIFree(hMMFeature->pRecords[nIRecord].pField);
        }
        VSIFree(hMMFeature->pRecords);
        hMMFeature->pRecords = nullptr;
    }

    hMMFeature->nNRings = 0;
    hMMFeature->nNumMRecords = 0;
    hMMFeature->nMaxMRecords = 0;
}

// Creates a MiraMon polygon, multipolygon, or linestring (arc) feature.
static int MMCreateFeaturePolOrArc(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                   struct MiraMonFeature *hMMFeature)
{
    double *pZ = nullptr;
    struct MM_POINT_2D *pCoord, *pCoordReal;
    MM_POLYGON_RINGS_COUNT nIPart;
    MM_N_VERTICES_TYPE nIVertice;
    double dtempx, dtempy;
    MM_POLYGON_RINGS_COUNT nExternalRingsCount;
    struct MM_PH *pCurrentPolHeader = nullptr;
    struct MM_AH *pCurrentArcHeader;
    // To access how many points have been stored in the last stringline
    struct MM_AH *pLastArcHeader = nullptr;
    struct MM_NH *pCurrentNodeHeader, *pCurrentNodeHeaderPlus1 = nullptr;
    uint32_t UnsignedLongNumber;
    struct MiraMonArcLayer *pMMArc;
    struct MiraMonNodeLayer *pMMNode;
    struct MM_TH *pArcTopHeader;
    struct MM_TH *pNodeTopHeader;
    char VFG = 0;
    MM_FILE_OFFSET nOffsetTmp;
    struct MM_ZD *pZDesc = nullptr;
    struct MM_FLUSH_INFO *pFlushAL, *pFlushNL, *pFlushZL, *pFlushPS, *pFlushPAL;
    MM_N_VERTICES_TYPE nPolVertices = 0;
    MM_BOOLEAN bReverseArc;
    int prevCoord = -1;
    MM_BOOLEAN bPolZeroJustCreated = FALSE;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Setting pointer to 3D structure (if exists).
    if (hMiraMonLayer->TopHeader.bIs3d)
        pZ = hMMFeature->pZCoord;

    // Setting pointers to arc/node structures.
    if (hMiraMonLayer->bIsPolygon)
    {
        pMMArc = &hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader = &hMiraMonLayer->MMPolygon.TopArcHeader;

        pMMNode = &hMiraMonLayer->MMPolygon.MMArc.MMNode;
        pNodeTopHeader = &hMiraMonLayer->MMPolygon.MMArc.TopNodeHeader;
    }
    else
    {
        pMMArc = &hMiraMonLayer->MMArc;
        pArcTopHeader = &hMiraMonLayer->TopHeader;

        pMMNode = &hMiraMonLayer->MMArc.MMNode;
        pNodeTopHeader = &hMiraMonLayer->MMArc.TopNodeHeader;
    }

    // Setting pointers to polygon structures
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMResizePolHeaderPointer(&hMiraMonLayer->MMPolygon.pPolHeader,
                                     &hMiraMonLayer->MMPolygon.nMaxPolHeader,
                                     hMiraMonLayer->TopHeader.nElemCount,
                                     MM_INCR_NUMBER_OF_POLYGONS, 0))
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMResizePolHeaderPointer())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        pCurrentPolHeader = hMiraMonLayer->MMPolygon.pPolHeader +
                            hMiraMonLayer->TopHeader.nElemCount;
        MMInitBoundingBox(&pCurrentPolHeader->dfBB);

        pCurrentPolHeader->dfPerimeter = 0;
        pCurrentPolHeader->dfArea = 0L;
    }

    // Setting flushes to all sections described in
    // format specifications document.
    pFlushAL = &pMMArc->FlushAL;
    pFlushNL = &pMMNode->FlushNL;
    pFlushZL = &pMMArc->pZSection.FlushZL;
    pFlushPS = &hMiraMonLayer->MMPolygon.FlushPS;
    pFlushPAL = &hMiraMonLayer->MMPolygon.FlushPAL;

    pFlushNL->pBlockWhereToSaveOrRead = (void *)pMMNode->pNL;
    pFlushAL->pBlockWhereToSaveOrRead = (void *)pMMArc->pAL;
    if (hMiraMonLayer->TopHeader.bIs3d)
        pFlushZL->pBlockWhereToSaveOrRead = (void *)pMMArc->pZSection.pZL;
    if (hMiraMonLayer->bIsPolygon)
    {
        pFlushPS->pBlockWhereToSaveOrRead =
            (void *)hMiraMonLayer->MMPolygon.pPS;
        pFlushPAL->pBlockWhereToSaveOrRead =
            (void *)hMiraMonLayer->MMPolygon.pPAL;
    }

    // Creation of the MiraMon extended database
    if (!hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->TopHeader.nElemCount == 0)
        {
            CPLDebugOnly("MiraMon", "Creating MiraMon database");
            if (MMCreateMMDB(hMiraMonLayer, hMMFeature->pCoord))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            CPLDebugOnly("MiraMon", "MiraMon database created. "
                                    "Creating features...");
        }
    }
    else
    {  // Universal polygon has been created
        if (hMiraMonLayer->TopHeader.nElemCount == 1)
        {
            CPLDebugOnly("MiraMon", "Creating MiraMon database");
            if (MMCreateMMDB(hMiraMonLayer, hMMFeature->pCoord))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            CPLDebugOnly("MiraMon", "MiraMon database created. "
                                    "Creating features...");

            // Universal polygon have a record with ID_GRAFIC=0 and blancs
            if (MMAddPolygonRecordToMMDB(hMiraMonLayer, nullptr, 0, 0, nullptr))
            {
                // Rare case where polygon zero creates the database
                // but some error occurs in the creation of the polygon zero.
                // The database must be destroyed and created again
                // next time the polygon zero is created.
                MMCloseMMBD_XP(hMiraMonLayer);
                MMDestroyMMDB(hMiraMonLayer);
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
            bPolZeroJustCreated = TRUE;
        }
    }

    // Checking if its possible continue writing the file due
    // to version limitations.
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
    {
        MM_FILE_OFFSET nNodeOffset, nArcOffset;
        MM_INTERNAL_FID nArcElemCount, nNodeElemCount;
        nNodeOffset = pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes;
        nArcOffset = pMMArc->nOffsetArc;

        nArcElemCount = pArcTopHeader->nElemCount;
        nNodeElemCount = pNodeTopHeader->nElemCount;
        for (nIPart = 0; nIPart < hMMFeature->nNRings; nIPart++,
            nArcElemCount++,
            nNodeElemCount += (hMiraMonLayer->bIsPolygon ? 1 : 2))
        {
            // There is space for the element that is going to be written?
            // Polygon or arc
            if (MMCheckVersionForFID(hMiraMonLayer,
                                     hMiraMonLayer->TopHeader.nElemCount))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error in MMCheckVersionForFID() (1)");
                return MM_STOP_WRITING_FEATURES;
            }

            // Arc if there is no polygon
            if (MMCheckVersionForFID(hMiraMonLayer, nArcElemCount))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error in MMCheckVersionForFID() (2)");
                return MM_STOP_WRITING_FEATURES;
            }

            // Nodes
            if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Error in MMCheckVersionForFID() (3)");
                return MM_STOP_WRITING_FEATURES;
            }

            // There is space for the last node(s) that is(are) going to be written?
            if (!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount + 1))
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Error in MMCheckVersionForFID() (4)");
                    return MM_STOP_WRITING_FEATURES;
                }
            }

            // Checking offsets
            // AL: check the last point
            if (MMCheckVersionOffset(hMiraMonLayer, nArcOffset))
            {
                CPLDebugOnly("MiraMon", "Error in MMCheckVersionOffset() (0)");
                return MM_STOP_WRITING_FEATURES;
            }
            // Setting next offset
            nArcOffset +=
                (hMMFeature->pNCoordRing[nIPart]) * pMMArc->nALElementSize;

            // NL: check the last node
            if (hMiraMonLayer->bIsPolygon)
                nNodeOffset += (hMMFeature->nNRings) * MM_SIZE_OF_NL_32BITS;
            else
                nNodeOffset += (2 * hMMFeature->nNRings) * MM_SIZE_OF_NL_32BITS;

            if (MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
            {
                CPLDebugOnly("MiraMon", "Error in MMCheckVersionOffset() (1)");
                return MM_STOP_WRITING_FEATURES;
            }
            // Setting next offset
            nNodeOffset += MM_SIZE_OF_NL_32BITS;

            if (!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
                {
                    CPLDebugOnly("MiraMon",
                                 "Error in MMCheckVersionOffset() (2)");
                    return MM_STOP_WRITING_FEATURES;
                }
                // Setting next offset
                nNodeOffset += MM_SIZE_OF_NL_32BITS;
            }

            // Where 3D part is going to start
            if (hMiraMonLayer->TopHeader.bIs3d)
            {
                if (nArcElemCount == 0)
                {
                    if (MMCheckVersionFor3DOffset(hMiraMonLayer,
                                                  nArcElemCount + 1, 0, 0))
                        return MM_STOP_WRITING_FEATURES;
                }
                else
                {
                    pZDesc = pMMArc->pZSection.pZDescription;
                    if (!pZDesc)
                    {
                        CPLError(CE_Failure, CPLE_ObjectNull,
                                 "Error: pZDescription should not be nullptr");
                        return MM_STOP_WRITING_FEATURES;
                    }

                    if (pZDesc[nArcElemCount - 1].nZCount < 0)
                    {
                        // One altitude was written on last element
                        if (MMCheckVersionFor3DOffset(
                                hMiraMonLayer, nArcElemCount + 1, nArcOffset,
                                pZDesc[nArcElemCount - 1].nOffsetZ +
                                    sizeof(*pZ)))
                            return MM_STOP_WRITING_FEATURES;
                    }
                    else
                    {
                        // One for each vertice altitude was written on last element
                        if (MMCheckVersionFor3DOffset(
                                hMiraMonLayer, nArcElemCount + 1, nArcOffset,
                                pZDesc[nArcElemCount - 1].nOffsetZ +
                                    sizeof(*pZ) * (pMMArc->pArcHeader +
                                                   (nArcElemCount - 1))
                                                      ->nElemCount))
                            return MM_STOP_WRITING_FEATURES;
                    }
                }
            }
        }
    }

    // Going through parts of the feature.
    nExternalRingsCount = 0;
    pCoord = hMMFeature->pCoord;

    if (!pCoord)
    {
        if (bPolZeroJustCreated)
        {
            // Rare case where polygon zero creates the database
            // but some error occurs in the creation of the polygon zero.
            // The database must be destroyed and created again
            // next time the polygon zero is created.
            MMCloseMMBD_XP(hMiraMonLayer);
            MMDestroyMMDB(hMiraMonLayer);
        }
        return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    // Doing real job
    for (nIPart = 0; nIPart < hMMFeature->nNRings; nIPart++,
        pArcTopHeader->nElemCount++,
        pNodeTopHeader->nElemCount += (hMiraMonLayer->bIsPolygon ? 1 : 2))
    {
        // Resize structures if necessary
        if (MMResizeArcHeaderPointer(
                &pMMArc->pArcHeader, &pMMArc->nMaxArcHeader,
                pArcTopHeader->nElemCount + 1, MM_INCR_NUMBER_OF_ARCS, 0))
        {
            CPLDebugOnly("MiraMon", "Error in MMResizeArcHeaderPointer()");
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMCreateFeaturePolOrArc())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        if (MMResizeNodeHeaderPointer(
                &pMMNode->pNodeHeader, &pMMNode->nMaxNodeHeader,
                hMiraMonLayer->bIsPolygon ? pNodeTopHeader->nElemCount + 1
                                          : pNodeTopHeader->nElemCount + 2,
                MM_INCR_NUMBER_OF_NODES, 0))
        {
            CPLDebugOnly("MiraMon", "Error in MMResizeNodeHeaderPointer()");
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMCreateFeaturePolOrArc())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if (MMResizeZSectionDescrPointer(
                    &pMMArc->pZSection.pZDescription,
                    &pMMArc->pZSection.nMaxZDescription, pMMArc->nMaxArcHeader,
                    MM_INCR_NUMBER_OF_ARCS, 0))
            {
                CPLDebugOnly("MiraMon",
                             "Error in MMResizeZSectionDescrPointer()");
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Memory error in MiraMon "
                         "driver (MMCreateFeaturePolOrArc())");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
            pZDesc = pMMArc->pZSection.pZDescription;
        }

        // Setting pointers to current headers
        pCurrentArcHeader = pMMArc->pArcHeader + pArcTopHeader->nElemCount;
        MMInitBoundingBox(&pCurrentArcHeader->dfBB);

        pCurrentNodeHeader = pMMNode->pNodeHeader + pNodeTopHeader->nElemCount;
        if (!hMiraMonLayer->bIsPolygon)
            pCurrentNodeHeaderPlus1 = pCurrentNodeHeader + 1;

        // Initializing feature information (section AH/PH)
        pCurrentArcHeader->nElemCount = hMMFeature->pNCoordRing[nIPart];
        pCurrentArcHeader->dfLength = 0.0;
        pCurrentArcHeader->nOffset =
            pFlushAL->TotalSavedBytes + pFlushAL->nNumBytes;

        // Dumping vertices and calculating stuff that
        // MiraMon needs (longitude/perimeter, area)
        bReverseArc = FALSE;
        if (hMiraMonLayer->bIsPolygon)
        {
            VFG = hMMFeature->flag_VFG[nIPart];
            bReverseArc = (VFG & MM_ROTATE_ARC) ? TRUE : FALSE;
        }

        if (bReverseArc)
        {
            prevCoord = 1;  // to find previous coordinate
            pCoordReal = pCoord + pCurrentArcHeader->nElemCount - 1;
        }
        else
        {
            prevCoord = -1;  // to find previous coordinate
            pCoordReal = pCoord;
        }

        for (nIVertice = 0; nIVertice < pCurrentArcHeader->nElemCount;
             nIVertice++, (bReverseArc) ? pCoordReal-- : pCoordReal++)
        {
            // Writing the arc in the normal way
            pFlushAL->SizeOfBlockToBeSaved = sizeof(pCoordReal->dfX);
            pFlushAL->pBlockToBeSaved = (void *)&(pCoord + nIVertice)->dfX;
            if (MMAppendBlockToBuffer(pFlushAL))
            {
                CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer() (1)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            pFlushAL->pBlockToBeSaved = (void *)&(pCoord + nIVertice)->dfY;
            if (MMAppendBlockToBuffer(pFlushAL))
            {
                CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer() (2)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // Calculating stuff using the inverse coordinates if it's needed
            MMUpdateBoundingBoxXY(&pCurrentArcHeader->dfBB, pCoordReal);
            if (nIVertice == 0 ||
                nIVertice == pCurrentArcHeader->nElemCount - 1)
                MMUpdateBoundingBoxXY(&pNodeTopHeader->hBB, pCoordReal);
            if (nIVertice > 0)
            {
                dtempx = pCoordReal->dfX - (pCoordReal + prevCoord)->dfX;
                dtempy = pCoordReal->dfY - (pCoordReal + prevCoord)->dfY;
                pCurrentArcHeader->dfLength +=
                    sqrt(dtempx * dtempx + dtempy * dtempy);
                if (hMiraMonLayer->bIsPolygon && pCurrentPolHeader)
                {
                    pCurrentPolHeader->dfArea +=
                        (pCoordReal->dfX * (pCoordReal + prevCoord)->dfY -
                         (pCoordReal + prevCoord)->dfX * pCoordReal->dfY);
                }
            }
        }
        if (bReverseArc)
            pCoord = pCoordReal + pCurrentArcHeader->nElemCount + 1;
        else
            pCoord += pCurrentArcHeader->nElemCount;

        nPolVertices += pCurrentArcHeader->nElemCount;

        // Updating bounding boxes
        MMUpdateBoundingBox(&pArcTopHeader->hBB, &pCurrentArcHeader->dfBB);
        if (hMiraMonLayer->bIsPolygon)
            MMUpdateBoundingBox(&hMiraMonLayer->TopHeader.hBB,
                                &pCurrentArcHeader->dfBB);

        pMMArc->nOffsetArc +=
            (pCurrentArcHeader->nElemCount) * pMMArc->nALElementSize;

        pCurrentArcHeader->nFirstIdNode = (2 * pArcTopHeader->nElemCount);
        if (hMiraMonLayer->bIsPolygon)
        {
            pCurrentArcHeader->nFirstIdNode = pArcTopHeader->nElemCount;
            pCurrentArcHeader->nLastIdNode = pArcTopHeader->nElemCount;
        }
        else
        {
            pCurrentArcHeader->nFirstIdNode = (2 * pArcTopHeader->nElemCount);
            pCurrentArcHeader->nLastIdNode =
                (2 * pArcTopHeader->nElemCount + 1);
        }
        if (MMAddArcRecordToMMDB(hMiraMonLayer, hMMFeature,
                                 pArcTopHeader->nElemCount, pCurrentArcHeader))
        {
            CPLDebugOnly("MiraMon", "Error in MMAddArcRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        // Node Stuff: writing NL section
        pCurrentNodeHeader->nArcsCount = 1;
        if (hMiraMonLayer->bIsPolygon)
            pCurrentNodeHeader->cNodeType = MM_RING_NODE;
        else
            pCurrentNodeHeader->cNodeType = MM_FINAL_NODE;

        pCurrentNodeHeader->nOffset =
            pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes;
        if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushNL,
                                              &UnsignedLongNumber,
                                              pArcTopHeader->nElemCount))
        {
            CPLDebugOnly("MiraMon",
                         "Error in MMAppendIntegerDependingOnVersion()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        // 8bytes alignment
        nOffsetTmp = pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes;
        MMGetOffsetAlignedTo8(&nOffsetTmp);
        if (nOffsetTmp != pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes)
        {
            pFlushNL->SizeOfBlockToBeSaved =
                (size_t)(nOffsetTmp -
                         (pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes));
            pFlushNL->pBlockToBeSaved = (void *)nullptr;
            if (MMAppendBlockToBuffer(pFlushNL))
            {
                CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer() (3)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        }
        if (MMAddNodeRecordToMMDB(hMiraMonLayer, pNodeTopHeader->nElemCount,
                                  pCurrentNodeHeader))
        {
            CPLDebugOnly("MiraMon", "Error in MMAddNodeRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        if (!hMiraMonLayer->bIsPolygon)
        {
            pCurrentNodeHeaderPlus1->nArcsCount = 1;
            if (hMiraMonLayer->bIsPolygon)
                pCurrentNodeHeaderPlus1->cNodeType = MM_RING_NODE;
            else
                pCurrentNodeHeaderPlus1->cNodeType = MM_FINAL_NODE;

            pCurrentNodeHeaderPlus1->nOffset =
                pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes;

            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushNL,
                                                  &UnsignedLongNumber,
                                                  pArcTopHeader->nElemCount))
            {
                CPLDebugOnly("MiraMon",
                             "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // 8bytes alignment
            nOffsetTmp = pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes;
            MMGetOffsetAlignedTo8(&nOffsetTmp);
            if (nOffsetTmp != pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes)
            {
                pFlushNL->SizeOfBlockToBeSaved =
                    (size_t)(nOffsetTmp -
                             (pFlushNL->TotalSavedBytes + pFlushNL->nNumBytes));
                pFlushNL->pBlockToBeSaved = (void *)nullptr;
                if (MMAppendBlockToBuffer(pFlushNL))
                {
                    CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }
            }
            if (MMAddNodeRecordToMMDB(hMiraMonLayer,
                                      pNodeTopHeader->nElemCount + 1,
                                      pCurrentNodeHeaderPlus1))
            {
                CPLDebugOnly("MiraMon", "Error in MMAddNodeRecordToMMDB()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        }

        // 3D stuff
        if (hMiraMonLayer->TopHeader.bIs3d && pZDesc)
        {
            pZDesc[pArcTopHeader->nElemCount].dfBBminz =
                STATISTICAL_UNDEF_VALUE;
            pZDesc[pArcTopHeader->nElemCount].dfBBmaxz =
                -STATISTICAL_UNDEF_VALUE;

            // Number of arc altitudes. If the number of altitudes is
            // positive this indicates the number of altitudes for
            // each vertex of the arc, and all the altitudes of the
            // vertex 0 are written first, then those of the vertex 1,
            // etc. If the number of altitudes is negative this
            // indicates the number of arc altitudes, understanding
            // that all the vertices have the same altitude (it is
            // the case of a contour line, for example).

            for (nIVertice = 0; nIVertice < pCurrentArcHeader->nElemCount;
                 nIVertice++, pZ++)
            {
                pFlushZL->SizeOfBlockToBeSaved = sizeof(*pZ);
                pFlushZL->pBlockToBeSaved = (void *)pZ;
                if (MMAppendBlockToBuffer(pFlushZL))
                {
                    CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }

                if (pZDesc[pArcTopHeader->nElemCount].dfBBminz > *pZ)
                    pZDesc[pArcTopHeader->nElemCount].dfBBminz = *pZ;
                if (pZDesc[pArcTopHeader->nElemCount].dfBBmaxz < *pZ)
                    pZDesc[pArcTopHeader->nElemCount].dfBBmaxz = *pZ;

                if (pMMArc->pZSection.ZHeader.dfBBminz > *pZ)
                    pMMArc->pZSection.ZHeader.dfBBminz = *pZ;
                if (pMMArc->pZSection.ZHeader.dfBBmaxz < *pZ)
                    pMMArc->pZSection.ZHeader.dfBBmaxz = *pZ;

                // Only one altitude (the same for all vertices) is written
                if (hMMFeature->bAllZHaveSameValue)
                    break;
            }
            if (hMMFeature->bAllZHaveSameValue)
            {
                // Same altitude for all vertices
                pZDesc[pArcTopHeader->nElemCount].nZCount = -1;
            }
            else
            {
                // One different altitude for each vertice
                pZDesc[pArcTopHeader->nElemCount].nZCount = 1;
            }

            if (pArcTopHeader->nElemCount == 0)
                pZDesc[pArcTopHeader->nElemCount].nOffsetZ = 0;
            else
            {
                pLastArcHeader =
                    pMMArc->pArcHeader + pArcTopHeader->nElemCount - 1;

                if (pZDesc[pArcTopHeader->nElemCount - 1].nZCount < 0)
                {
                    pZDesc[pArcTopHeader->nElemCount].nOffsetZ =
                        pZDesc[pArcTopHeader->nElemCount - 1].nOffsetZ +
                        sizeof(*pZ);
                }
                else
                {
                    pZDesc[pArcTopHeader->nElemCount].nOffsetZ =
                        pZDesc[pArcTopHeader->nElemCount - 1].nOffsetZ +
                        sizeof(*pZ) * (pLastArcHeader->nElemCount);
                }
            }
        }

        // Exclusive polygon stuff
        if (hMiraMonLayer->bIsPolygon && pCurrentPolHeader)
        {
            // PS SECTION
            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPS,
                                                  &UnsignedLongNumber, 0))
            {
                CPLDebugOnly("MiraMon",
                             "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            if (MMAppendIntegerDependingOnVersion(
                    hMiraMonLayer, pFlushPS, &UnsignedLongNumber,
                    hMiraMonLayer->TopHeader.nElemCount))
            {
                CPLDebugOnly("MiraMon",
                             "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // PAL SECTION
            // Vertices of rings defining
            // holes in polygons are in a counterclockwise direction.
            // Holes are at the end of all external rings that contain the holes!!
            if (VFG & MM_EXTERIOR_ARC_SIDE)
                nExternalRingsCount++;

            pCurrentPolHeader->nArcsCount++;
            //(MM_POLYGON_ARCS_COUNT)hMMFeature->nNRings;
            if (VFG & MM_EXTERIOR_ARC_SIDE)
                pCurrentPolHeader
                    ->nExternalRingsCount++;  //= nExternalRingsCount;

            if (VFG & MM_END_ARC_IN_RING)
                pCurrentPolHeader->nRingsCount++;  //= hMMFeature->nNRings;
            if (nIPart == 0)
            {
                pCurrentPolHeader->nOffset =
                    pFlushPAL->TotalSavedBytes + pFlushPAL->nNumBytes;
            }

            if (nIPart == hMMFeature->nNRings - 1)
                pCurrentPolHeader->dfArea /= 2;

            pFlushPAL->SizeOfBlockToBeSaved = 1;
            pFlushPAL->pBlockToBeSaved = (void *)&VFG;
            if (MMAppendBlockToBuffer(pFlushPAL))
            {
                CPLDebugOnly("MiraMon", "Error in MMAppendBlockToBuffer()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPAL,
                                                  &UnsignedLongNumber,
                                                  pArcTopHeader->nElemCount))
            {
                CPLDebugOnly("MiraMon",
                             "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // 8bytes alignment
            if (nIPart == hMMFeature->nNRings - 1)
            {
                nOffsetTmp = pFlushPAL->TotalSavedBytes + pFlushPAL->nNumBytes;
                MMGetOffsetAlignedTo8(&nOffsetTmp);

                if (nOffsetTmp !=
                    pFlushPAL->TotalSavedBytes + pFlushPAL->nNumBytes)
                {
                    pFlushPAL->SizeOfBlockToBeSaved =
                        (size_t)(nOffsetTmp - (pFlushPAL->TotalSavedBytes +
                                               pFlushPAL->nNumBytes));
                    pFlushPAL->pBlockToBeSaved = (void *)nullptr;
                    if (MMAppendBlockToBuffer(pFlushPAL))
                    {
                        CPLDebugOnly("MiraMon",
                                     "Error in MMAppendBlockToBuffer()");
                        return MM_FATAL_ERROR_WRITING_FEATURES;
                    }
                }
            }

            MMUpdateBoundingBox(&pCurrentPolHeader->dfBB,
                                &pCurrentArcHeader->dfBB);
            pCurrentPolHeader->dfPerimeter += pCurrentArcHeader->dfLength;
        }
    }

    // Updating element count and if the polygon is multipart.
    // MiraMon does not accept multipoints or multilines, only multipolygons.
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMAddPolygonRecordToMMDB(hMiraMonLayer, hMMFeature,
                                     hMiraMonLayer->TopHeader.nElemCount,
                                     nPolVertices, pCurrentPolHeader))
        {
            CPLDebugOnly("MiraMon", "Error in MMAddPolygonRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        hMiraMonLayer->TopHeader.nElemCount++;

        if (nExternalRingsCount > 1)
            hMiraMonLayer->TopHeader.bIsMultipolygon = TRUE;
    }

    return MM_CONTINUE_WRITING_FEATURES;
}  // End of de MMCreateFeaturePolOrArc()

// Creates a MiraMon DBF record when not associated with a geometric feature.
static int MMCreateRecordDBF(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             struct MiraMonFeature *hMMFeature)
{
    int result;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (hMiraMonLayer->TopHeader.nElemCount == 0)
    {
        if (MMCreateMMDB(hMiraMonLayer, nullptr))
        {
            MMDestroyMMDB(hMiraMonLayer);
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
    }

    result = MMAddDBFRecordToMMDB(hMiraMonLayer, hMMFeature);
    if (result == MM_FATAL_ERROR_WRITING_FEATURES ||
        result == MM_STOP_WRITING_FEATURES)
        return result;

    // Everything OK.
    return MM_CONTINUE_WRITING_FEATURES;
}  // End of de MMCreateRecordDBF()

// Creates a MiraMon point feature.
static int MMCreateFeaturePoint(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                struct MiraMonFeature *hMMFeature)
{
    double *pZ = nullptr;
    struct MM_POINT_2D *pCoord;
    MM_POLYGON_RINGS_COUNT nIPart;
    MM_N_VERTICES_TYPE nIVertice, nCoord;
    struct MM_ZD *pZDescription = nullptr;
    MM_INTERNAL_FID nElemCount;
    int result;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
        return MM_STOP_WRITING_FEATURES;

    if (hMiraMonLayer->TopHeader.bIs3d)
        pZ = hMMFeature->pZCoord;

    nElemCount = hMiraMonLayer->TopHeader.nElemCount;
    for (nIPart = 0, pCoord = hMMFeature->pCoord; nIPart < hMMFeature->nNRings;
         nIPart++, nElemCount++)
    {
        nCoord = hMMFeature->pNCoordRing[nIPart];

        // Checking if its possible continue writing the file due
        // to version limitations.
        if (MMCheckVersionForFID(hMiraMonLayer,
                                 hMiraMonLayer->TopHeader.nElemCount + nCoord))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error in MMCheckVersionForFID() (5)");
            return MM_STOP_WRITING_FEATURES;
        }

        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if (nElemCount == 0)
            {
                if (MMCheckVersionFor3DOffset(hMiraMonLayer, (nElemCount + 1),
                                              0, 0))
                    return MM_STOP_WRITING_FEATURES;
            }
            else
            {
                pZDescription = hMiraMonLayer->MMPoint.pZSection.pZDescription;
                if (!pZDescription)
                {
                    CPLError(CE_Failure, CPLE_ObjectNull,
                             "Error: pZDescription should not be nullptr");
                    return MM_STOP_WRITING_FEATURES;
                }

                if (MMCheckVersionFor3DOffset(
                        hMiraMonLayer, nElemCount + 1, 0,
                        pZDescription[nElemCount - 1].nOffsetZ + sizeof(*pZ)))
                    return MM_STOP_WRITING_FEATURES;
            }
        }

        // Doing real job
        // Memory issues
        if (hMiraMonLayer->TopHeader.bIs3d && pZ)
        {
            if (MMResizeZSectionDescrPointer(
                    &hMiraMonLayer->MMPoint.pZSection.pZDescription,
                    &hMiraMonLayer->MMPoint.pZSection.nMaxZDescription,
                    nElemCount, MM_INCR_NUMBER_OF_POINTS, 0))
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Memory error in MiraMon "
                         "driver (MMCreateFeaturePoint())");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            pZDescription = hMiraMonLayer->MMPoint.pZSection.pZDescription;
            if (!pZDescription)
            {
                CPLError(CE_Failure, CPLE_ObjectNull,
                         "Error: pZDescription should not be nullptr");
                return MM_STOP_WRITING_FEATURES;
            }

            pZDescription[nElemCount].dfBBminz = *pZ;
            pZDescription[nElemCount].dfBBmaxz = *pZ;

            if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz > *pZ)
                hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz = *pZ;
            if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz < *pZ)
                hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz = *pZ;

            // Specification ask for a negative number.
            pZDescription[nElemCount].nZCount = -1;
            if (nElemCount == 0)
                pZDescription[nElemCount].nOffsetZ = 0;
            else
                pZDescription[nElemCount].nOffsetZ =
                    pZDescription[nElemCount - 1].nOffsetZ + sizeof(*pZ);
        }

        // Flush settings
        hMiraMonLayer->MMPoint.FlushTL.pBlockWhereToSaveOrRead =
            (void *)hMiraMonLayer->MMPoint.pTL;
        if (hMiraMonLayer->TopHeader.bIs3d)
            hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockWhereToSaveOrRead =
                (void *)hMiraMonLayer->MMPoint.pZSection.pZL;

        // Dump point or points (MiraMon does not have multiple points)
        for (nIVertice = 0; nIVertice < nCoord; nIVertice++, pCoord++)
        {
            // Updating the bounding box of the layer
            MMUpdateBoundingBoxXY(&hMiraMonLayer->TopHeader.hBB, pCoord);

            // Adding the point at the memory block
            hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved =
                sizeof(pCoord->dfX);
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved =
                (void *)&pCoord->dfX;
            if (MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved =
                (void *)&pCoord->dfY;
            if (MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return MM_FATAL_ERROR_WRITING_FEATURES;

            // Adding the 3D part, if exists, at the memory block
            if (hMiraMonLayer->TopHeader.bIs3d && pZ)
            {
                hMiraMonLayer->MMPoint.pZSection.FlushZL.SizeOfBlockToBeSaved =
                    sizeof(*pZ);
                hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockToBeSaved =
                    (void *)pZ;
                if (MMAppendBlockToBuffer(
                        &hMiraMonLayer->MMPoint.pZSection.FlushZL))
                    return MM_FATAL_ERROR_WRITING_FEATURES;

                if (!pZDescription)
                {
                    CPLError(CE_Failure, CPLE_ObjectNull,
                             "Error: pZDescription should not be nullptr");
                    return MM_STOP_WRITING_FEATURES;
                }

                if (pZDescription[nElemCount].dfBBminz > *pZ)
                    pZDescription[nElemCount].dfBBminz = *pZ;
                if (pZDescription[nElemCount].dfBBmaxz < *pZ)
                    pZDescription[nElemCount].dfBBmaxz = *pZ;

                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz > *pZ)
                    hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz = *pZ;
                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz < *pZ)
                    hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz = *pZ;

                pZ++;
            }
        }

        if (hMiraMonLayer->TopHeader.nElemCount == 0)
        {
            if (MMCreateMMDB(hMiraMonLayer, hMMFeature->pCoord))
                return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        result = MMAddPointRecordToMMDB(hMiraMonLayer, hMMFeature, nElemCount);
        if (result == MM_FATAL_ERROR_WRITING_FEATURES ||
            result == MM_STOP_WRITING_FEATURES)
            return result;
    }
    // Updating nElemCount at the header of the layer
    hMiraMonLayer->TopHeader.nElemCount = nElemCount;

    // Everything OK.
    return MM_CONTINUE_WRITING_FEATURES;
}  // End of de MMCreateFeaturePoint()

// Checks whether a given Feature ID (FID) exceeds the maximum allowed
// index for 2 GB vectors in a specific MiraMon layer.
int MMCheckVersionForFID(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_INTERNAL_FID FID)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->LayerVersion != MM_32BITS_VERSION)
        return 0;

    if (FID >= MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS)
        return 1;
    return 0;
}

// Checks whether a given offset exceeds the maximum allowed
// index for 2 GB vectors in a specific MiraMon layer.
int MMCheckVersionOffset(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_FILE_OFFSET OffsetToCheck)
{
    if (!hMiraMonLayer)
        return 1;

    // Checking if the final version is 1.1 or 2.0
    if (hMiraMonLayer->LayerVersion != MM_32BITS_VERSION)
        return 0;

    // User decided that if necessary, output version can be 2.0
    if (OffsetToCheck < MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;

    return 1;
}

// Checks whether a given offset in 3D section exceeds the maximum allowed
// index for 2 GB vectors in a specific MiraMon layer.
int MMCheckVersionFor3DOffset(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              MM_INTERNAL_FID nElemCount,
                              MM_FILE_OFFSET nOffsetAL,
                              MM_FILE_OFFSET nZLOffset)
{
    MM_FILE_OFFSET LastOffset;

    if (!hMiraMonLayer)
        return 1;

    // Checking if the final version is 1.1 or 2.0
    if (hMiraMonLayer->LayerVersion != MM_32BITS_VERSION)
        return 0;

    // User decided that if necessary, output version can be 2.0
    if (hMiraMonLayer->bIsPoint)
        LastOffset = MM_HEADER_SIZE_32_BITS + nElemCount * MM_SIZE_OF_TL;
    else  // Arc case
    {
        LastOffset = MM_HEADER_SIZE_32_BITS +
                     nElemCount * MM_SIZE_OF_AH_32BITS + +nOffsetAL;
    }

    LastOffset += MM_SIZE_OF_ZH;
    LastOffset += nElemCount * MM_SIZE_OF_ZD_32_BITS;
    LastOffset += nZLOffset;

    if (LastOffset < MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;

    return 1;
}

// Adds a feature in a MiraMon layer.
int MMAddFeature(struct MiraMonVectLayerInfo *hMiraMonLayer,
                 struct MiraMonFeature *hMiraMonFeature)
{
    int re;
    MM_INTERNAL_FID previousFID = 0;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMiraMonLayer->bIsBeenInit)
    {
        if (MMInitLayerByType(hMiraMonLayer))
        {
            CPLDebugOnly("MiraMon", "Error in MMInitLayerByType()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        hMiraMonLayer->bIsBeenInit = 1;
    }

    if (hMiraMonFeature)
        previousFID = hMiraMonLayer->TopHeader.nElemCount;

    if (hMiraMonLayer->bIsPoint)
    {
        re = MMCreateFeaturePoint(hMiraMonLayer, hMiraMonFeature);
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previousFID;
        }
        return re;
    }
    if (hMiraMonLayer->bIsArc || hMiraMonLayer->bIsPolygon)
    {
        re = MMCreateFeaturePolOrArc(hMiraMonLayer, hMiraMonFeature);
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previousFID;
        }
        return re;
    }
    if (hMiraMonLayer->bIsDBF)
    {
        // Adding a record to DBF file
        re = MMCreateRecordDBF(hMiraMonLayer, hMiraMonFeature);
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previousFID;
        }
        return re;
    }

    return MM_CONTINUE_WRITING_FEATURES;
}

/* -------------------------------------------------------------------- */
/*      Tools used by MiraMon.                                          */
/* -------------------------------------------------------------------- */

void MMInitBoundingBox(struct MMBoundingBox *dfBB)
{
    if (!dfBB)
        return;
    dfBB->dfMinX = STATISTICAL_UNDEF_VALUE;
    dfBB->dfMaxX = -STATISTICAL_UNDEF_VALUE;
    dfBB->dfMinY = STATISTICAL_UNDEF_VALUE;
    dfBB->dfMaxY = -STATISTICAL_UNDEF_VALUE;
}

void MMUpdateBoundingBox(struct MMBoundingBox *dfBBToBeAct,
                         struct MMBoundingBox *dfBBWithData)
{
    if (!dfBBToBeAct)
        return;

    if (dfBBToBeAct->dfMinX > dfBBWithData->dfMinX)
        dfBBToBeAct->dfMinX = dfBBWithData->dfMinX;

    if (dfBBToBeAct->dfMinY > dfBBWithData->dfMinY)
        dfBBToBeAct->dfMinY = dfBBWithData->dfMinY;

    if (dfBBToBeAct->dfMaxX < dfBBWithData->dfMaxX)
        dfBBToBeAct->dfMaxX = dfBBWithData->dfMaxX;

    if (dfBBToBeAct->dfMaxY < dfBBWithData->dfMaxY)
        dfBBToBeAct->dfMaxY = dfBBWithData->dfMaxY;
}

void MMUpdateBoundingBoxXY(struct MMBoundingBox *dfBB,
                           struct MM_POINT_2D *pCoord)
{
    if (!pCoord)
        return;

    if (pCoord->dfX < dfBB->dfMinX)
        dfBB->dfMinX = pCoord->dfX;

    if (pCoord->dfY < dfBB->dfMinY)
        dfBB->dfMinY = pCoord->dfY;

    if (pCoord->dfX > dfBB->dfMaxX)
        dfBB->dfMaxX = pCoord->dfX;

    if (pCoord->dfY > dfBB->dfMaxY)
        dfBB->dfMaxY = pCoord->dfY;
}

/* -------------------------------------------------------------------- */
/*      Resize structures for reuse                                     */
/* -------------------------------------------------------------------- */
int MMResizeMiraMonFieldValue(struct MiraMonFieldValue **pFieldValue,
                              MM_EXT_DBF_N_MULTIPLE_RECORDS *nMax,
                              MM_EXT_DBF_N_MULTIPLE_RECORDS nNum,
                              MM_EXT_DBF_N_MULTIPLE_RECORDS nIncr,
                              MM_EXT_DBF_N_MULTIPLE_RECORDS nProposedMax)
{
    MM_EXT_DBF_N_MULTIPLE_RECORDS nPrevMax;
    MM_EXT_DBF_N_MULTIPLE_RECORDS nNewMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pFieldValue)))
    {
        return 1;
    }
    if ((pTmp = VSIRealloc(*pFieldValue,
                           (size_t)nNewMax * sizeof(**pFieldValue))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeMiraMonFieldValue())");
        return 1;
    }
    *nMax = nNewMax;
    *pFieldValue = pTmp;

    memset((*pFieldValue) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pFieldValue));
    return 0;
}

int MMResizeMiraMonPolygonArcs(struct MM_PAL_MEM **pFID,
                               MM_POLYGON_ARCS_COUNT *nMax,
                               MM_POLYGON_ARCS_COUNT nNum,
                               MM_POLYGON_ARCS_COUNT nIncr,
                               MM_POLYGON_ARCS_COUNT nProposedMax)
{
    MM_POLYGON_ARCS_COUNT nPrevMax;
    MM_POLYGON_ARCS_COUNT nNewMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pFID)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pFID)
        return 0;
    if ((pTmp = VSIRealloc(*pFID, (size_t)nNewMax * sizeof(**pFID))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeMiraMonPolygonArcs())");
        return 1;
    }
    *nMax = nNewMax;
    *pFID = pTmp;

    memset((*pFID) + nPrevMax, 0, (size_t)(*nMax - nPrevMax) * sizeof(**pFID));
    return 0;
}

int MMResizeMiraMonRecord(struct MiraMonRecord **pMiraMonRecord,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS *nMax,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nNum,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nIncr,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nProposedMax)
{
    MM_EXT_DBF_N_MULTIPLE_RECORDS nPrevMax;
    MM_EXT_DBF_N_MULTIPLE_RECORDS nNewMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pMiraMonRecord)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pMiraMonRecord)
        return 0;
    if ((pTmp = VSIRealloc(*pMiraMonRecord,
                           (size_t)nNewMax * sizeof(**pMiraMonRecord))) ==
        nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeMiraMonRecord())");
        return 1;
    }
    *nMax = nNewMax;
    *pMiraMonRecord = pTmp;

    memset((*pMiraMonRecord) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pMiraMonRecord));
    return 0;
}

int MMResizeZSectionDescrPointer(struct MM_ZD **pZDescription, GUInt64 *nMax,
                                 GUInt64 nNum, GUInt64 nIncr,
                                 GUInt64 nProposedMax)
{
    GUInt64 nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pZDescription)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pZDescription)
        return 0;
    if ((pTmp = VSIRealloc(*pZDescription,
                           (size_t)nNewMax * sizeof(**pZDescription))) ==
        nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeZSectionDescrPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pZDescription = pTmp;

    memset((*pZDescription) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pZDescription));
    return 0;
}

int MMResizeNodeHeaderPointer(struct MM_NH **pNodeHeader, GUInt64 *nMax,
                              GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    GUInt64 nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pNodeHeader)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pNodeHeader)
        return 0;
    if ((pTmp = VSIRealloc(*pNodeHeader,
                           (size_t)nNewMax * sizeof(**pNodeHeader))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeNodeHeaderPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pNodeHeader = pTmp;

    memset((*pNodeHeader) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pNodeHeader));
    return 0;
}

int MMResizeArcHeaderPointer(struct MM_AH **pArcHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    GUInt64 nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pArcHeader)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pArcHeader)
        return 0;
    if ((pTmp = VSIRealloc(*pArcHeader,
                           (size_t)nNewMax * sizeof(**pArcHeader))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeArcHeaderPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pArcHeader = pTmp;

    memset((*pArcHeader) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pArcHeader));
    return 0;
}

int MMResizePolHeaderPointer(struct MM_PH **pPolHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    GUInt64 nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pPolHeader)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pPolHeader)
        return 0;
    if ((pTmp = VSIRealloc(*pPolHeader,
                           (size_t)nNewMax * sizeof(**pPolHeader))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizePolHeaderPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pPolHeader = pTmp;

    memset((*pPolHeader) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pPolHeader));
    return 0;
}

int MMResize_MM_N_VERTICES_TYPE_Pointer(MM_N_VERTICES_TYPE **pVrt,
                                        MM_N_VERTICES_TYPE *nMax,
                                        MM_N_VERTICES_TYPE nNum,
                                        MM_N_VERTICES_TYPE nIncr,
                                        MM_N_VERTICES_TYPE nProposedMax)
{
    MM_N_VERTICES_TYPE nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pVrt)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pVrt)
        return 0;
    if ((pTmp = VSIRealloc(*pVrt, (size_t)nNewMax * sizeof(**pVrt))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResize_MM_N_VERTICES_TYPE_Pointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pVrt = pTmp;

    memset((*pVrt) + nPrevMax, 0, (size_t)(*nMax - nPrevMax) * sizeof(**pVrt));
    return 0;
}

int MMResizeVFGPointer(char **pInt, MM_INTERNAL_FID *nMax, MM_INTERNAL_FID nNum,
                       MM_INTERNAL_FID nIncr, MM_INTERNAL_FID nProposedMax)
{
    MM_N_VERTICES_TYPE nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pInt)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pInt)
        return 0;
    if ((pTmp = VSIRealloc(*pInt, (size_t)nNewMax * sizeof(**pInt))) == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeVFGPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pInt = pTmp;

    memset((*pInt) + nPrevMax, 0, (size_t)(*nMax - nPrevMax) * sizeof(**pInt));
    return 0;
}

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D,
                              MM_N_VERTICES_TYPE *nMax, MM_N_VERTICES_TYPE nNum,
                              MM_N_VERTICES_TYPE nIncr,
                              MM_N_VERTICES_TYPE nProposedMax)
{
    MM_N_VERTICES_TYPE nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pPoint2D)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pPoint2D)
        return 0;
    if ((pTmp = VSIRealloc(*pPoint2D, (size_t)nNewMax * sizeof(**pPoint2D))) ==
        nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeMM_POINT2DPointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pPoint2D = pTmp;

    memset((*pPoint2D) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pPoint2D));
    return 0;
}

int MMResizeDoublePointer(MM_COORD_TYPE **pDouble, MM_N_VERTICES_TYPE *nMax,
                          MM_N_VERTICES_TYPE nNum, MM_N_VERTICES_TYPE nIncr,
                          MM_N_VERTICES_TYPE nProposedMax)
{
    MM_N_VERTICES_TYPE nNewMax, nPrevMax;
    void *pTmp;

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    nNewMax = MAX(nNum + nIncr, nProposedMax);
    if (MMCheckSize_t(nNewMax, sizeof(**pDouble)))
    {
        return 1;
    }
    if (nNewMax == 0 && *pDouble)
        return 0;
    if ((pTmp = VSIRealloc(*pDouble, (size_t)nNewMax * sizeof(**pDouble))) ==
        nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMResizeDoublePointer())");
        return 1;
    }
    *nMax = nNewMax;
    *pDouble = pTmp;

    memset((*pDouble) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pDouble));
    return 0;
}

int MMResizeStringToOperateIfNeeded(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                    MM_EXT_DBF_N_FIELDS nNewSize)
{
    if (!hMiraMonLayer)
        return 1;

    if (nNewSize >= hMiraMonLayer->nNumStringToOperate)
    {
        char *p;
        if (MMCheckSize_t(nNewSize, 1))
            return 1;
        p = (char *)VSICalloc(1, (size_t)nNewSize);
        if (!p)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Memory error in MiraMon "
                     "driver (MMResizeStringToOperateIfNeeded())");
            return 1;
        }
        VSIFree(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate = p;
        hMiraMonLayer->nNumStringToOperate = nNewSize;
    }
    return 0;
}

// Checks if a string is empty
int MMIsEmptyString(const char *string)
{
    const char *ptr = string;

    for (; *ptr; ptr++)
        if (*ptr != ' ' && *ptr != '\t')
            return 0;

    return 1;
}

/* -------------------------------------------------------------------- */
/*      Metadata Functions                                              */
/* -------------------------------------------------------------------- */

#define LineReturn "\r\n"

// Generates an idientifier that REL 4 MiraMon metadata needs.
static void MMGenerateFileIdentifierFromMetadataFileName(char *pMMFN,
                                                         char *aFileIdentifier)
{
    char aCharRand[8];
    static const char aCharset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i, len_charset;

    memset(aFileIdentifier, '\0', MM_MAX_LEN_LAYER_IDENTIFIER);

    aCharRand[0] = '_';
    len_charset = (int)strlen(aCharset);
    for (i = 1; i < 7; i++)
    {
        // coverity[dont_call]
        aCharRand[i] = aCharset[rand() % (len_charset - 1)];
    }
    aCharRand[7] = '\0';
    CPLStrlcpy(aFileIdentifier, pMMFN, MM_MAX_LEN_LAYER_IDENTIFIER - 7);
    strcat(aFileIdentifier, aCharRand);
    return;
}

// Converts a string from UTF-8 to ANSI to be written in a REL 4 file
static void
MMWrite_ANSI_MetadataKeyDescriptor(struct MiraMonVectorMetaData *hMMMD,
                                   VSILFILE *pF, const char *pszEng,
                                   const char *pszCat, const char *pszEsp)
{
    char *pszString = nullptr;

    switch (hMMMD->nMMLanguage)
    {
        case MM_CAT_LANGUAGE:
            pszString = CPLRecode(pszCat, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
        case MM_SPA_LANGUAGE:
            pszString = CPLRecode(pszEsp, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
        default:
        case MM_ENG_LANGUAGE:
            pszString = CPLRecode(pszEng, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
    }
    if (pszString)
    {
        VSIFPrintfL(pF, "%s", KEY_descriptor);
        VSIFPrintfL(pF, "=");
        VSIFPrintfL(pF, "%s", pszString);
        VSIFPrintfL(pF, "%s", LineReturn);
        CPLFree(pszString);
    }
}

/*
    Writes a MiraMon REL 4 metadata file. Next sections are included:
    VERSION, METADADES, IDENTIFICATION, EXTENT, OVERVIEW,
    TAULA_PRINCIPAL and GEOMETRIA_I_TOPOLOGIA

    Please, consult the meaning of all them at:
    https://www.miramon.cat/help/eng/GeMPlus/ClausREL.htm
*/
static int MMWriteMetadataFile(struct MiraMonVectorMetaData *hMMMD)
{
    char aMessage[MM_MESSAGE_LENGTH],
        aFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER], aMMIDSRS[MM_MAX_ID_SNY];
    MM_EXT_DBF_N_FIELDS nIField;
    VSILFILE *pF;
    time_t currentTime;
    char aTimeString[200];

    if (!hMMMD->aLayerName)
        return 0;

    if (nullptr == (pF = VSIFOpenL(hMMMD->aLayerName, "wb")))
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "The file %s must exist.",
                 hMMMD->aLayerName);
        return 1;
    }

    // Writing MiraMon version section
    VSIFPrintfL(pF, "[%s]" LineReturn, SECTION_VERSIO);

    VSIFPrintfL(pF, "%s=%u" LineReturn, KEY_Vers, (unsigned)MM_VERS);
    VSIFPrintfL(pF, "%s=%u" LineReturn, KEY_SubVers, (unsigned)MM_SUBVERS);

    VSIFPrintfL(pF, "%s=%u" LineReturn, KEY_VersMetaDades,
                (unsigned)MM_VERS_METADADES);
    VSIFPrintfL(pF, "%s=%u" LineReturn, KEY_SubVersMetaDades,
                (unsigned)MM_SUBVERS_METADADES);

    // Writing METADADES section
    VSIFPrintfL(pF, "\r\n[%s]" LineReturn, SECTION_METADADES);
    CPLStrlcpy(aMessage, hMMMD->aLayerName, sizeof(aMessage));
    MMGenerateFileIdentifierFromMetadataFileName(aMessage, aFileIdentifier);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_FileIdentifier, aFileIdentifier);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_language, KEY_Value_eng);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_MDIdiom, KEY_Value_eng);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_characterSet,
                KEY_Value_characterSet);

    // Writing IDENTIFICATION section
    VSIFPrintfL(pF, LineReturn "[%s]" LineReturn, SECTION_IDENTIFICATION);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_code, aFileIdentifier);
    VSIFPrintfL(pF, "%s=" LineReturn, KEY_codeSpace);
    if (hMMMD->szLayerTitle && !MMIsEmptyString(hMMMD->szLayerTitle))
    {
        if (hMMMD->ePlainLT == MM_LayerType_Point)
            VSIFPrintfL(pF, "%s=%s (pnt)" LineReturn, KEY_DatasetTitle,
                        hMMMD->szLayerTitle);
        if (hMMMD->ePlainLT == MM_LayerType_Arc)
            VSIFPrintfL(pF, "%s=%s (arc)" LineReturn, KEY_DatasetTitle,
                        hMMMD->szLayerTitle);
        if (hMMMD->ePlainLT == MM_LayerType_Pol)
            VSIFPrintfL(pF, "%s=%s (pol)" LineReturn, KEY_DatasetTitle,
                        hMMMD->szLayerTitle);
    }
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_language, KEY_Value_eng);

    if (hMMMD->ePlainLT != MM_LayerType_Node &&
        hMMMD->ePlainLT != MM_LayerType_Pol)
    {
        // SPATIAL_REFERENCE_SYSTEM:HORIZONTAL
        VSIFPrintfL(pF, LineReturn "[%s:%s]" LineReturn,
                    SECTION_SPATIAL_REFERENCE_SYSTEM, SECTION_HORIZONTAL);
        if (!ReturnMMIDSRSFromEPSGCodeSRS(hMMMD->pSRS, aMMIDSRS) &&
            !MMIsEmptyString(aMMIDSRS))
            VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_HorizontalSystemIdentifier,
                        aMMIDSRS);
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "The MiraMon driver cannot assign any HRS.");
            // Horizontal Reference System
            VSIFPrintfL(pF, "%s=plane" LineReturn,
                        KEY_HorizontalSystemIdentifier);
            VSIFPrintfL(pF, "%s=local" LineReturn,
                        KEY_HorizontalSystemDefinition);
            if (hMMMD->pXUnit)
                VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_unitats, hMMMD->pXUnit);
            if (hMMMD->pYUnit)
            {
                if (!hMMMD->pXUnit || strcasecmp(hMMMD->pXUnit, hMMMD->pYUnit))
                    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_unitatsY,
                                hMMMD->pYUnit);
            }
        }

        // SPATIAL_REFERENCE_SYSTEM:VERTICAL
        // Llegim les unitats del Sist. ref. vertical
        if (hMMMD->pZUnit)
        {
            VSIFPrintfL(pF, LineReturn "[%s:%s]" LineReturn,
                        SECTION_SPATIAL_REFERENCE_SYSTEM, SECTION_VERTICAL);
            VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_unitats, hMMMD->pZUnit);
        }
    }

    if (hMMMD->ePlainLT == MM_LayerType_Pol && hMMMD->aArcFile)
    {
        // Writing OVERVIEW:ASPECTES_TECNICS in polygon metadata file.
        // ArcSource=fitx_pol.arc
        VSIFPrintfL(pF, LineReturn "[%s]" LineReturn,
                    SECTION_OVVW_ASPECTES_TECNICS);
        VSIFPrintfL(pF, "%s=\"%s\"" LineReturn, KEY_ArcSource, hMMMD->aArcFile);
    }
    else if (hMMMD->ePlainLT == MM_LayerType_Arc && hMMMD->aArcFile)
    {
        // Writing OVERVIEW:ASPECTES_TECNICS in arc metadata file.
        // Ciclat1=fitx_arc.pol
        VSIFPrintfL(pF, LineReturn "[%s]" LineReturn,
                    SECTION_OVVW_ASPECTES_TECNICS);
        VSIFPrintfL(pF, "Ciclat1=\"%s\"" LineReturn, hMMMD->aArcFile);
    }

    // Writing EXTENT section
    VSIFPrintfL(pF, LineReturn "[%s]" LineReturn, SECTION_EXTENT);
    VSIFPrintfL(pF, "%s=0" LineReturn, KEY_toler_env);

    if (hMMMD->hBB.dfMinX != MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxX != -MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMinY != MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxY != -MM_UNDEFINED_STATISTICAL_VALUE)
    {
        VSIFPrintfL(pF, "%s=%lf" LineReturn, KEY_MinX, hMMMD->hBB.dfMinX);
        VSIFPrintfL(pF, "%s=%lf" LineReturn, KEY_MaxX, hMMMD->hBB.dfMaxX);
        VSIFPrintfL(pF, "%s=%lf" LineReturn, KEY_MinY, hMMMD->hBB.dfMinY);
        VSIFPrintfL(pF, "%s=%lf" LineReturn, KEY_MaxY, hMMMD->hBB.dfMaxY);
    }

    // Writing OVERVIEW section
    VSIFPrintfL(pF, LineReturn "[%s]" LineReturn, SECTION_OVERVIEW);

    currentTime = time(nullptr);

    struct tm ltime;
    VSILocalTime(&currentTime, &ltime);
    snprintf(aTimeString, sizeof(aTimeString), "%04d%02d%02d %02d%02d%02d%02d",
             ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday,
             ltime.tm_hour, ltime.tm_min, ltime.tm_sec, 0);
    VSIFPrintfL(pF, "%s=%s" LineReturn, KEY_CreationDate, aTimeString);
    VSIFPrintfL(pF, LineReturn);

    // Writing TAULA_PRINCIPAL section
    VSIFPrintfL(pF, "[%s]" LineReturn, SECTION_TAULA_PRINCIPAL);
    VSIFPrintfL(pF, "IdGrafic=%s" LineReturn, szMMNomCampIdGraficDefecte);
    VSIFPrintfL(pF, "TipusRelacio=RELACIO_1_1_DICC" LineReturn);

    VSIFPrintfL(pF, LineReturn);
    VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                szMMNomCampIdGraficDefecte);
    VSIFPrintfL(pF, "visible=0" LineReturn);
    VSIFPrintfL(pF, "simbolitzable=0" LineReturn);

    MMWrite_ANSI_MetadataKeyDescriptor(
        hMMMD, pF, szInternalGraphicIdentifierEng,
        szInternalGraphicIdentifierCat, szInternalGraphicIdentifierSpa);

    if (hMMMD->ePlainLT == MM_LayerType_Arc)
    {
        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNVertexsDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfVerticesEng,
                                           szNumberOfVerticesCat,
                                           szNumberOfVerticesSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampLongitudArcDefecte);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szLengthOfAarcEng, szLengthOfAarcCat, szLengthOfAarcSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNodeIniDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szInitialNodeEng,
                                           szInitialNodeCat, szInitialNodeSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNodeFiDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szFinalNodeEng,
                                           szFinalNodeCat, szFinalNodeSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[GEOMETRIA_I_TOPOLOGIA]" LineReturn);
        VSIFPrintfL(pF, "NomCampNVertexs=%s" LineReturn,
                    szMMNomCampNVertexsDefecte);
        VSIFPrintfL(pF, "NomCampLongitudArc=%s" LineReturn,
                    szMMNomCampLongitudArcDefecte);
        VSIFPrintfL(pF, "NomCampNodeIni=%s" LineReturn,
                    szMMNomCampNodeIniDefecte);
        VSIFPrintfL(pF, "NomCampNodeFi=%s" LineReturn,
                    szMMNomCampNodeFiDefecte);
    }
    else if (hMMMD->ePlainLT == MM_LayerType_Node)
    {
        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampArcsANodeDefecte);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfArcsToNodeEng,
                                           szNumberOfArcsToNodeCat,
                                           szNumberOfArcsToNodeSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampTipusNodeDefecte);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNodeTypeEng,
                                           szNodeTypeCat, szNodeTypeSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[GEOMETRIA_I_TOPOLOGIA]" LineReturn);
        VSIFPrintfL(pF, "NomCampArcsANode=%s" LineReturn,
                    szMMNomCampArcsANodeDefecte);
        VSIFPrintfL(pF, "NomCampTipusNode=%s" LineReturn,
                    szMMNomCampTipusNodeDefecte);
    }
    else if (hMMMD->ePlainLT == MM_LayerType_Pol)
    {
        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNVertexsDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfVerticesEng,
                                           szNumberOfVerticesCat,
                                           szNumberOfVerticesSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampPerimetreDefecte);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szPerimeterOfThePolygonEng, szPerimeterOfThePolygonCat,
            szPerimeterOfThePolygonSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampAreaDefecte);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szAreaOfThePolygonEng,
                                           szAreaOfThePolygonCat,
                                           szAreaOfThePolygonSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNArcsDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szNumberOfArcsEng, szNumberOfArcsCat, szNumberOfArcsSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                    szMMNomCampNPoligonsDefecte);
        VSIFPrintfL(pF, "visible=0" LineReturn);
        VSIFPrintfL(pF, "simbolitzable=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szNumberOfElementaryPolygonsEng,
            szNumberOfElementaryPolygonsCat, szNumberOfElementaryPolygonsSpa);

        VSIFPrintfL(pF, LineReturn);
        VSIFPrintfL(pF, "[GEOMETRIA_I_TOPOLOGIA]" LineReturn);
        VSIFPrintfL(pF, "NomCampNVertexs=%s" LineReturn,
                    szMMNomCampNVertexsDefecte);
        VSIFPrintfL(pF, "NomCampPerimetre=%s" LineReturn,
                    szMMNomCampPerimetreDefecte);
        VSIFPrintfL(pF, "NomCampArea=%s" LineReturn, szMMNomCampAreaDefecte);
        VSIFPrintfL(pF, "NomCampNArcs=%s" LineReturn, szMMNomCampNArcsDefecte);
        VSIFPrintfL(pF, "NomCampNPoligons=%s" LineReturn,
                    szMMNomCampNPoligonsDefecte);
    }

    if (hMMMD->pLayerDB && hMMMD->pLayerDB->nNFields > 0)
    {
        // For each field of the databes
        for (nIField = 0; nIField < hMMMD->pLayerDB->nNFields; nIField++)
        {
            VSIFPrintfL(pF, LineReturn "[%s:%s]" LineReturn,
                        SECTION_TAULA_PRINCIPAL,
                        hMMMD->pLayerDB->pFields[nIField].pszFieldName);

            if (!MMIsEmptyString(
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription) &&
                !MMIsEmptyString(
                    hMMMD->pLayerDB->pFields[nIField].pszFieldName))
            {
                MMWrite_ANSI_MetadataKeyDescriptor(
                    hMMMD, pF,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription);
            }

            // Exception in a particular case: "altura" is a catalan word that means
            // "height". Its unit by default will be "m" instead of "unknown".
            // The same goes for "z", which easily means height.
            if (EQUAL("altura",
                      hMMMD->pLayerDB->pFields[nIField].pszFieldName) ||
                EQUAL("z", hMMMD->pLayerDB->pFields[nIField].pszFieldName))
            {
                VSIFPrintfL(pF, "unitats=m" LineReturn);
            }
            else
            {
                // By default units of field values will not be shown.
                VSIFPrintfL(pF, "MostrarUnitats=0" LineReturn);
            }
        }
    }
    VSIFCloseL(pF);
    return 0;
}

// Writes metadata files for MiraMon vector layers
static int MMWriteVectorMetadataFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                     int layerPlainType, int layerMainPlainType)
{
    struct MiraMonVectorMetaData hMMMD;

    if (!hMiraMonLayer)
        return 1;

    // MiraMon writes a REL file of each .pnt, .arc, .nod or .pol
    memset(&hMMMD, 0, sizeof(hMMMD));
    hMMMD.ePlainLT = layerPlainType;
    hMMMD.pSRS = hMiraMonLayer->pSRS;
    hMMMD.pZUnit = hMiraMonLayer->pZUnit;
    hMMMD.nMMLanguage = hMiraMonLayer->nMMLanguage;

    hMMMD.szLayerTitle = hMiraMonLayer->szLayerTitle;
    if (layerPlainType == MM_LayerType_Point)
    {
        hMMMD.aLayerName = hMiraMonLayer->MMPoint.pszREL_LayerName;
        if (MMIsEmptyString(hMMMD.aLayerName))
            return 0;  // If no file, no error. Just continue.
        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB = hMiraMonLayer->pLayerDB;
        return MMWriteMetadataFile(&hMMMD);
    }
    else if (layerPlainType == MM_LayerType_Arc)
    {
        int nResult;

        // Arcs and not polygons
        if (layerMainPlainType == MM_LayerType_Arc)
        {
            hMMMD.aLayerName = hMiraMonLayer->MMArc.pszREL_LayerName;
            if (MMIsEmptyString(hMMMD.aLayerName))
                return 0;  // If no file, no error. Just continue.
            memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB,
                   sizeof(hMMMD.hBB));
            hMMMD.pLayerDB = hMiraMonLayer->pLayerDB;
        }
        // Arcs and polygons
        else
        {
            // Arc from polygon
            hMMMD.aLayerName = hMiraMonLayer->MMPolygon.MMArc.pszREL_LayerName;
            if (MMIsEmptyString(hMMMD.aLayerName))
                return 0;  // If no file, no error. Just continue.

            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMPolygon.TopArcHeader.hBB,
                   sizeof(hMMMD.hBB));
            hMMMD.pLayerDB = nullptr;
            hMMMD.aArcFile = CPLStrdup(
                CPLGetFilename(hMiraMonLayer->MMPolygon.pszLayerName));
        }
        nResult = MMWriteMetadataFile(&hMMMD);
        VSIFree(hMMMD.aArcFile);
        return nResult;
    }
    else if (layerPlainType == MM_LayerType_Pol)
    {
        int nResult;

        hMMMD.aLayerName = hMiraMonLayer->MMPolygon.pszREL_LayerName;

        if (MMIsEmptyString(hMMMD.aLayerName))
            return 0;  // If no file, no error. Just continue.

        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB = hMiraMonLayer->pLayerDB;
        hMMMD.aArcFile = CPLStrdup(
            CPLGetFilename(hMiraMonLayer->MMPolygon.MMArc.pszLayerName));
        nResult = MMWriteMetadataFile(&hMMMD);
        VSIFree(hMMMD.aArcFile);
        return nResult;
    }
    else if (layerPlainType == MM_LayerType_Node)
    {
        // Node from arc
        if (layerMainPlainType == MM_LayerType_Arc)
        {
            hMMMD.aLayerName = hMiraMonLayer->MMArc.MMNode.pszREL_LayerName;
            if (MMIsEmptyString(hMMMD.aLayerName))
                return 0;  // If no file, no error. Just continue.
            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMArc.TopNodeHeader.hBB,
                   sizeof(hMMMD.hBB));
        }
        else  // Node from polygon
        {
            hMMMD.aLayerName =
                hMiraMonLayer->MMPolygon.MMArc.MMNode.pszREL_LayerName;
            if (MMIsEmptyString(hMMMD.aLayerName))
                return 0;  // If no file, no error. Just continue.
            memcpy(&hMMMD.hBB,
                   &hMiraMonLayer->MMPolygon.MMArc.TopNodeHeader.hBB,
                   sizeof(hMMMD.hBB));
        }
        hMMMD.pLayerDB = nullptr;
        return MMWriteMetadataFile(&hMMMD);
    }
    return 0;
}

int MMWriteVectorMetadata(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPoint)
        return MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Point,
                                         MM_LayerType_Point);
    if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Node,
                                      MM_LayerType_Arc))
            return 1;
        return MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Arc,
                                         MM_LayerType_Arc);
    }
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Node,
                                      MM_LayerType_Pol))
            return 1;
        if (MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Arc,
                                      MM_LayerType_Pol))
            return 1;
        return MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Pol,
                                         MM_LayerType_Pol);
    }
    if (hMiraMonLayer->bIsDBF)
    {
        return MMWriteVectorMetadataFile(hMiraMonLayer, MM_LayerType_Unknown,
                                         MM_LayerType_Unknown);
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      MiraMon database functions                                      */
/* -------------------------------------------------------------------- */

// Initializes a MiraMon database associated with a vector layer:
// Sets the usual fields that MiraMon needs and after them, adds
// all fields of the input layer
static int MMInitMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                      struct MMAdmDatabase *pMMAdmDB)
{
    if (!hMiraMonLayer || !pMMAdmDB)
        return 1;

    if (MMIsEmptyString(pMMAdmDB->pszExtDBFLayerName))
        return 0;  // No file, no error. Just continue

    strcpy(pMMAdmDB->pMMBDXP->ReadingMode, "wb+");
    if (FALSE == MM_CreateAndOpenDBFFile(pMMAdmDB->pMMBDXP,
                                         pMMAdmDB->pszExtDBFLayerName))
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Error pMMAdmDB: Cannot create or open file %s.",
                 pMMAdmDB->pszExtDBFLayerName);
        return 1;
    }

    VSIFSeekL(pMMAdmDB->pMMBDXP->pfDataBase,
              pMMAdmDB->pMMBDXP->FirstRecordOffset, SEEK_SET);

    if (MMInitFlush(&pMMAdmDB->FlushRecList, pMMAdmDB->pMMBDXP->pfDataBase,
                    MM_1MB, &pMMAdmDB->pRecList,
                    pMMAdmDB->pMMBDXP->FirstRecordOffset, 0))
        return 1;

    pMMAdmDB->nNumRecordOnCourse =
        (GUInt64)pMMAdmDB->pMMBDXP->BytesPerRecord + 1;
    if (MMCheckSize_t(pMMAdmDB->nNumRecordOnCourse, 1))
        return 1;
    pMMAdmDB->szRecordOnCourse =
        VSICalloc(1, (size_t)pMMAdmDB->nNumRecordOnCourse);
    if (!pMMAdmDB->szRecordOnCourse)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Memory error in MiraMon "
                 "driver (MMInitMMDB())");
        return 1;
    }
    return 0;
}

// Creates a MiraMon database associated with a vector layer.
// It determines the number of fields and initializes the database header
// accordingly. Depending on the layer type (point, arc, polygon, or generic),
// it defines the fields and initializes the corresponding MiraMon database
// structures.
int MMCreateMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                 struct MM_POINT_2D *pFirstCoord)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr, *pBD_XP_Aux = nullptr;
    struct MM_FIELD MMField;
    size_t nIFieldLayer;
    MM_EXT_DBF_N_FIELDS nIField = 0;
    MM_EXT_DBF_N_FIELDS nNFields;

    if (!hMiraMonLayer)
        return 1;

    // If the SRS is unknown, we attempt to deduce the appropriate number
    // of decimals to be used in the reserved fields as LONG_ARC, PERIMETRE,
    // or AREA using the coordinate values. It's not 100% reliable, but it's a
    // good approximation.
    if (hMiraMonLayer->nSRSType == MM_SRS_LAYER_IS_UNKNOWN_TYPE && pFirstCoord)
    {
        if (pFirstCoord->dfX < -360 || pFirstCoord->dfX > 360)
            hMiraMonLayer->nSRSType = MM_SRS_LAYER_IS_PROJECTED_TYPE;
        else
            hMiraMonLayer->nSRSType = MM_SRS_LAYER_IS_GEOGRAPHIC_TYPE;
    }

    if (hMiraMonLayer->bIsPoint)
    {
        if (hMiraMonLayer->pLayerDB)
            nNFields =
                MM_PRIVATE_POINT_DB_FIELDS + hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields = MM_PRIVATE_POINT_DB_FIELDS;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(&hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP);

        pBD_XP = hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;

        if (0 == (nIField = (MM_EXT_DBF_N_FIELDS)MM_DefineFirstPointFieldsDB_XP(
                      pBD_XP)))
            return 1;
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->pLayerDB)
            nNFields =
                MM_PRIVATE_ARC_DB_FIELDS + hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields = MM_PRIVATE_ARC_DB_FIELDS;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(&hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP);

        pBD_XP = hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;

        if (0 == (nIField = (MM_EXT_DBF_N_FIELDS)MM_DefineFirstArcFieldsDB_XP(
                      pBD_XP,
                      hMiraMonLayer->nSRSType == MM_SRS_LAYER_IS_PROJECTED_TYPE
                          ? 3
                          : 9)))
            return 1;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMArc.MMNode.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(&hMiraMonLayer->MMArc.MMNode.MMAdmDB.pMMBDXP);

        pBD_XP_Aux = hMiraMonLayer->MMArc.MMNode.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(3, hMiraMonLayer->nCharSet);

        if (!pBD_XP_Aux)
            return 1;

        if (0 == MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
            return 1;
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->pLayerDB)
            nNFields = MM_PRIVATE_POLYGON_DB_FIELDS +
                       hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields = MM_PRIVATE_POLYGON_DB_FIELDS;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(&hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP);

        pBD_XP = hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;

        if (0 ==
            (nIField = (MM_EXT_DBF_N_FIELDS)MM_DefineFirstPolygonFieldsDB_XP(
                 pBD_XP,
                 hMiraMonLayer->nSRSType == MM_SRS_LAYER_IS_PROJECTED_TYPE ? 3
                                                                           : 9,
                 hMiraMonLayer->nSRSType == MM_SRS_LAYER_IS_PROJECTED_TYPE
                     ? 3
                     : 12)))
            return 1;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(
                &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP);

        pBD_XP_Aux = hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(5, hMiraMonLayer->nCharSet);

        if (!pBD_XP_Aux)
            return 1;

        if (0 == MM_DefineFirstArcFieldsDB_XP(
                     pBD_XP_Aux,
                     hMiraMonLayer->nSRSType == MM_SRS_LAYER_IS_PROJECTED_TYPE
                         ? 3
                         : 9))
            return 1;

        // Before allocating new memory, there might be some previously allocated but unused memory.
        // Let's free that memory first.
        if (hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB.pMMBDXP)
            MM_ReleaseDBFHeader(
                &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB.pMMBDXP);

        pBD_XP_Aux = hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(3, hMiraMonLayer->nCharSet);

        if (!pBD_XP_Aux)
            return 1;

        if (0 == MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
            return 1;
    }
    else if (hMiraMonLayer->bIsDBF)
    {
        // Creating only a DBF
        if (hMiraMonLayer->pLayerDB)
            nNFields = hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields = 0;

        pBD_XP = hMiraMonLayer->MMAdmDBWriting.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;
    }
    else
        return 0;

    // After private MiraMon fields, other fields are added.
    // If names are no compatible, some changes are done.
    if (hMiraMonLayer->pLayerDB)
    {
        for (nIFieldLayer = 0; nIField < nNFields; nIField++, nIFieldLayer++)
        {
            MM_InitializeField(&MMField);
            CPLStrlcpy(
                MMField.FieldName,
                hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].pszFieldName,
                MM_MAX_LON_FIELD_NAME_DBF);

            CPLStrlcpy(MMField.FieldDescription[0],
                       hMiraMonLayer->pLayerDB->pFields[nIFieldLayer]
                           .pszFieldDescription,
                       MM_MAX_BYTES_FIELD_DESC);

            MMField.BytesPerField =
                hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].nFieldSize;
            switch (hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].eFieldType)
            {
                case MM_Numeric:
                    MMField.FieldType = 'N';
                    if (hMiraMonLayer->pLayerDB->pFields[nIFieldLayer]
                            .bIs64BitInteger)
                        MMField.Is64 = 1;
                    if (MMField.BytesPerField == 0)
                        MMField.BytesPerField = MM_MAX_AMPLADA_CAMP_N_DBF;
                    break;
                case MM_Character:
                    MMField.FieldType = 'C';
                    if (MMField.BytesPerField == 0)
                        MMField.BytesPerField = MM_MAX_AMPLADA_CAMP_C_DBF;
                    break;
                case MM_Data:
                    MMField.FieldType = 'D';
                    if (MMField.BytesPerField == 0)
                        MMField.BytesPerField = MM_MAX_AMPLADA_CAMP_D_DBF;
                    break;
                case MM_Logic:
                    MMField.FieldType = 'L';
                    if (MMField.BytesPerField == 0)
                        MMField.BytesPerField = 1;
                    break;
                default:
                    MMField.FieldType = 'C';
                    if (MMField.BytesPerField == 0)
                        MMField.BytesPerField = MM_MAX_AMPLADA_CAMP_C_DBF;
            };

            MMField.DecimalsIfFloat =
                (MM_BYTE)hMiraMonLayer->pLayerDB->pFields[nIFieldLayer]
                    .nNumberOfDecimals;

            MM_DuplicateFieldDBXP(pBD_XP->pField + nIField, &MMField);
            MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(
                pBD_XP->pField + nIField, pBD_XP, FALSE, 0);
            if (pBD_XP->pField[nIField].FieldType == 'F')
                pBD_XP->pField[nIField].FieldType = 'N';
        }
    }

    if (hMiraMonLayer->bIsPoint)
    {
        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPoint.MMAdmDB))
            return 1;
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMArc.MMAdmDB))
            return 1;

        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMArc.MMNode.MMAdmDB))
            return 1;
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;

        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;

        if (MMInitMMDB(hMiraMonLayer,
                       &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB))
            return 1;
    }
    else if (hMiraMonLayer->bIsDBF)
    {
        if (MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMAdmDBWriting))
            return 1;
    }
    return 0;
}

// Checks and fits the width of a specific field in a MiraMon database
// associated with a vector layer. It examines the length of the provided
// value and resizes the field width, if necessary, to accommodate the new
// value. If the new width exceeds the current width of the field,
// it updates the database structure, including the field width and
// the size of the record. Additionally, it reallocates memory if needed
// for the record handling buffer.

static int
MMTestAndFixValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              struct MMAdmDatabase *pMMAdmDB,
                              MM_EXT_DBF_N_FIELDS nIField, char *szValue)
{
    struct MM_FIELD *camp;
    MM_BYTES_PER_FIELD_TYPE_DBF nNewWidth;

    if (!hMiraMonLayer || !pMMAdmDB || !pMMAdmDB->pMMBDXP ||
        !pMMAdmDB->pMMBDXP->pField || !pMMAdmDB->pMMBDXP->pfDataBase)
        return 1;

    camp = pMMAdmDB->pMMBDXP->pField + nIField;

    if (!szValue)
        return 0;

    nNewWidth = (MM_BYTES_PER_FIELD_TYPE_DBF)strlen(szValue);
    if (MMResizeStringToOperateIfNeeded(hMiraMonLayer, nNewWidth + 1))
        return 1;

    if (nNewWidth > camp->BytesPerField)
    {
        if (MM_WriteNRecordsMMBD_XPFile(pMMAdmDB))
            return 1;

        // Flushing all to be flushed
        pMMAdmDB->FlushRecList.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMAdmDB->FlushRecList))
            return 1;

        if (MM_ChangeDBFWidthField(
                pMMAdmDB->pMMBDXP, nIField, nNewWidth,
                pMMAdmDB->pMMBDXP->pField[nIField].DecimalsIfFloat))
            return 1;

        // The record on course also has to change its size.
        if ((GUInt64)pMMAdmDB->pMMBDXP->BytesPerRecord + 1 >=
            pMMAdmDB->nNumRecordOnCourse)
        {
            void *pTmp;
            if (nullptr == (pTmp = VSIRealloc(
                                pMMAdmDB->szRecordOnCourse,
                                (size_t)pMMAdmDB->pMMBDXP->BytesPerRecord + 1)))
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Memory error in MiraMon "
                         "driver (MMTestAndFixValueToRecordDBXP())");
                return 1;
            }
            pMMAdmDB->szRecordOnCourse = pTmp;
        }

        // File has changed its size, so it has to be updated
        // at the Flush tool
        VSIFSeekL(pMMAdmDB->pMMBDXP->pfDataBase, 0, SEEK_END);
        pMMAdmDB->FlushRecList.OffsetWhereToFlush =
            VSIFTellL(pMMAdmDB->pMMBDXP->pfDataBase);
    }
    return 0;
}

static int
MMWriteValueToszStringToOperate(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                const struct MM_FIELD *camp, const void *valor,
                                MM_BOOLEAN is_64)
{
    if (!hMiraMonLayer)
        return 1;

    if (!camp)
        return 0;

    if (MMResizeStringToOperateIfNeeded(hMiraMonLayer,
                                        camp->BytesPerField + 10))
        return 1;

    if (!valor)
        *hMiraMonLayer->szStringToOperate = '\0';
    else
    {
        if (camp->FieldType == 'N')
        {
            if (!is_64)
            {
                snprintf(hMiraMonLayer->szStringToOperate,
                         (size_t)hMiraMonLayer->nNumStringToOperate, "%*.*f",
                         camp->BytesPerField, camp->DecimalsIfFloat,
                         *(const double *)valor);
            }
            else
            {
                snprintf(hMiraMonLayer->szStringToOperate,
                         (size_t)hMiraMonLayer->nNumStringToOperate, "%*lld",
                         camp->BytesPerField, *(const GInt64 *)valor);
            }
        }
        else
        {
            snprintf(hMiraMonLayer->szStringToOperate,
                     (size_t)hMiraMonLayer->nNumStringToOperate, "%-*s",
                     camp->BytesPerField, (const char *)valor);
        }
    }

    return 0;
}

int MMWritePreformatedNumberValueToRecordDBXP(
    struct MiraMonVectLayerInfo *hMiraMonLayer, char *registre,
    const struct MM_FIELD *camp, const char *valor)
{
    if (!hMiraMonLayer)
        return 1;

    if (!camp)
        return 0;

    if (MMResizeStringToOperateIfNeeded(hMiraMonLayer,
                                        camp->BytesPerField + 10))
        return 1;

    if (!valor)
        memset(hMiraMonLayer->szStringToOperate, 0, camp->BytesPerField);
    else
    {
        snprintf(hMiraMonLayer->szStringToOperate,
                 (size_t)hMiraMonLayer->nNumStringToOperate, "%*s",
                 camp->BytesPerField, valor);
    }

    memcpy(registre + camp->AccumulatedBytes, hMiraMonLayer->szStringToOperate,
           camp->BytesPerField);
    return 0;
}

int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             char *registre, const struct MM_FIELD *camp,
                             const void *valor, MM_BOOLEAN is_64)
{
    if (!hMiraMonLayer)
        return 1;

    if (!camp)
        return 0;

    if (MMWriteValueToszStringToOperate(hMiraMonLayer, camp, valor, is_64))
        return 1;

    memcpy(registre + camp->AccumulatedBytes, hMiraMonLayer->szStringToOperate,
           camp->BytesPerField);
    return 0;
}

static int MMAddFeatureRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                    struct MiraMonFeature *hMMFeature,
                                    struct MMAdmDatabase *pMMAdmDB,
                                    char *pszRecordOnCourse,
                                    struct MM_FLUSH_INFO *pFlushRecList,
                                    MM_EXT_DBF_N_RECORDS *nNumRecords,
                                    MM_EXT_DBF_N_FIELDS nNumPrivateMMField)
{
    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
    MM_EXT_DBF_N_FIELDS nIField;
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;

    if (!hMiraMonLayer)
        return 1;

    if (!hMMFeature)
        return 1;

    pBD_XP = pMMAdmDB->pMMBDXP;
    for (nIRecord = 0; nIRecord < hMMFeature->nNumMRecords; nIRecord++)
    {
        for (nIField = 0; nIField < hMMFeature->pRecords[nIRecord].nNumField;
             nIField++)
        {
            // A field with no valid value is written as blank
            if (!hMMFeature->pRecords[nIRecord].pField[nIField].bIsValid)
            {
                memset(
                    pszRecordOnCourse +
                        pBD_XP->pField[nIField + nNumPrivateMMField]
                            .AccumulatedBytes,
                    ' ',
                    pBD_XP->pField[nIField + nNumPrivateMMField].BytesPerField);

                continue;
            }
            if (pBD_XP->pField[nIField + nNumPrivateMMField].FieldType == 'C' ||
                pBD_XP->pField[nIField + nNumPrivateMMField].FieldType == 'L' ||
                pBD_XP->pField[nIField + nNumPrivateMMField].FieldType == 'D')
            {
                if (MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                                             pBD_XP->pField + nIField +
                                                 nNumPrivateMMField,
                                             hMMFeature->pRecords[nIRecord]
                                                 .pField[nIField]
                                                 .pDinValue,
                                             FALSE))
                    return 1;
            }
            else if (pBD_XP->pField[nIField + nNumPrivateMMField].FieldType ==
                         'N' &&
                     !pBD_XP->pField[nIField + nNumPrivateMMField].Is64)
            {
                if (MMWritePreformatedNumberValueToRecordDBXP(
                        hMiraMonLayer, pszRecordOnCourse,
                        pBD_XP->pField + nIField + nNumPrivateMMField,
                        hMMFeature->pRecords[nIRecord]
                            .pField[nIField]
                            .pDinValue))
                    return 1;
            }
            else if (pBD_XP->pField[nIField + nNumPrivateMMField].FieldType ==
                     'N')
            {
                if (pBD_XP->pField[nIField + nNumPrivateMMField].Is64)
                {
                    if (MMWriteValueToRecordDBXP(
                            hMiraMonLayer, pszRecordOnCourse,
                            pBD_XP->pField + nIField + nNumPrivateMMField,
                            &hMMFeature->pRecords[nIRecord]
                                 .pField[nIField]
                                 .iValue,
                            TRUE))
                        return 1;
                }
            }
        }

        if (MMAppendBlockToBuffer(pFlushRecList))
            return 1;

        (*nNumRecords)++;
    }
    return 0;
}

// Adds feature records to a MiraMon database associated with a vector layer.
static int MMDetectAndFixDBFWidthChange(
    struct MiraMonVectLayerInfo *hMiraMonLayer,
    struct MiraMonFeature *hMMFeature, struct MMAdmDatabase *pMMAdmDB,
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField,
    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord, MM_EXT_DBF_N_FIELDS nIField)
{
    if (!hMiraMonLayer)
        return 1;

    if (!hMMFeature)
        return 1;

    if (nIRecord >= hMMFeature->nNumMRecords)
        return 1;

    if (nIField >= hMMFeature->pRecords[nIRecord].nNumField)
        return 1;

    if (MMTestAndFixValueToRecordDBXP(
            hMiraMonLayer, pMMAdmDB, nIField + nNumPrivateMMField,
            hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue))
        return 1;

    // We analyze next fields
    if (nIField == hMMFeature->pRecords[nIRecord].nNumField - 1)
    {
        if (nIRecord + 1 < hMMFeature->nNumMRecords)
        {
            if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                             pMMAdmDB, nNumPrivateMMField,
                                             nIRecord + 1, 0))
                return 1;
        }
        else
            return 0;
    }
    else
    {
        if (nIField + 1 < hMMFeature->pRecords[nIRecord].nNumField)
        {
            if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                             pMMAdmDB, nNumPrivateMMField,
                                             nIRecord, nIField + 1))
                return 1;
        }
        else
            return 0;
    }
    return 0;
}  // End of MMDetectAndFixDBFWidthChange()

// Adds a DBF record to a MiraMon table associated with a vector layer.
// It sets up flush settings for writing to the table and initializes
// variables needed for the process. Then, it checks and fixes the width
// change if necessary.
int MMAddDBFRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         struct MiraMonFeature *hMMFeature)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = 0;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    pBD_XP = hMiraMonLayer->MMAdmDBWriting.pMMBDXP;

    // Test length
    if (hMMFeature && hMMFeature->nNumMRecords &&
        hMMFeature->pRecords[0].nNumField)
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                         &hMiraMonLayer->MMAdmDBWriting,
                                         nNumPrivateMMField, 0, 0))
            return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMAdmDBWriting.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMAdmDBWriting.pRecList;

    pFlushRecList->pBlockToBeSaved =
        (void *)hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse;
    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;

    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMAdmDBWriting,
            hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse, pFlushRecList,
            &hMiraMonLayer->MMAdmDBWriting.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // In this case, the number of features is also updated
    hMiraMonLayer->TopHeader.nElemCount =
        hMiraMonLayer->MMAdmDBWriting.pMMBDXP->nRecords;

    return MM_CONTINUE_WRITING_FEATURES;
}

// Adds a point record to a MiraMon table associated with a vector layer.
int MMAddPointRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                           struct MiraMonFeature *hMMFeature,
                           MM_INTERNAL_FID nElemCount)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = MM_PRIVATE_POINT_DB_FIELDS;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(hMiraMonLayer,
                             hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nRecords +
                                 hMMFeature->nNumMRecords))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Error in MMCheckVersionForFID() (6)");
        return MM_STOP_WRITING_FEATURES;
    }

    pBD_XP = hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;

    // Test length
    // Private fields
    // ID_GRAFIC
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField,
                                        &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                      &hMiraMonLayer->MMPoint.MMAdmDB, 0,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // GDAL fields
    if (hMMFeature->nNumMRecords && hMMFeature->pRecords[0].nNumField)
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                         &hMiraMonLayer->MMPoint.MMAdmDB,
                                         nNumPrivateMMField, 0, 0))
            return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    // Now length is sure, write
    memset(hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse, 0,
           pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField, &nElemCount, TRUE);

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMPoint.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMPoint.MMAdmDB.pRecList;

    pFlushRecList->pBlockToBeSaved =
        (void *)hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse;
    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;

    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMPoint.MMAdmDB,
            hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse, pFlushRecList,
            &hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Adds a stringline record to a MiraMon table associated with a vector layer.
int MMAddArcRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         struct MiraMonFeature *hMMFeature,
                         MM_INTERNAL_FID nElemCount, struct MM_AH *pArcHeader)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    struct MiraMonArcLayer *pMMArcLayer;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = MM_PRIVATE_ARC_DB_FIELDS;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    // In V1.1 only _UI32_MAX records number is allowed
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMCheckVersionForFID(hMiraMonLayer,
                                 pMMArcLayer->MMAdmDB.pMMBDXP->nRecords + 1))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error in MMCheckVersionForFID() (7)");
            return MM_STOP_WRITING_FEATURES;
        }
    }
    else
    {
        if (MMCheckVersionForFID(hMiraMonLayer,
                                 pMMArcLayer->MMAdmDB.pMMBDXP->nRecords +
                                     hMMFeature->nNumMRecords))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error in MMCheckVersionForFID() (8)");
            return MM_STOP_WRITING_FEATURES;
        }
    }

    pBD_XP = pMMArcLayer->MMAdmDB.pMMBDXP;

    // Test length
    // Private fields
    // ID_GRAFIC
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField,
                                        &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMArcLayer->MMAdmDB, 0,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // N_VERTEXS
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 1,
                                        &pArcHeader->nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMArcLayer->MMAdmDB, 1,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // LENGTH
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 2,
                                        &pArcHeader->dfLength, FALSE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMArcLayer->MMAdmDB, 2,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // NODE_INI
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 3,
                                        &pArcHeader->nFirstIdNode, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMArcLayer->MMAdmDB, 3,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // NODE_FI
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 4,
                                        &pArcHeader->nLastIdNode, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMArcLayer->MMAdmDB, 4,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // GDAL fields
    if (!hMiraMonLayer->bIsPolygon)
    {
        if (hMMFeature->nNumMRecords && hMMFeature->pRecords[0].nNumField)
        {
            if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                             &pMMArcLayer->MMAdmDB,
                                             nNumPrivateMMField, 0, 0))
                return MM_FATAL_ERROR_WRITING_FEATURES;
        }
    }

    // Now length is sure, write
    memset(pMMArcLayer->MMAdmDB.szRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMArcLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField, &nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMArcLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField + 1, &pArcHeader->nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMArcLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField + 2, &pArcHeader->dfLength, FALSE);

    MMWriteValueToRecordDBXP(
        hMiraMonLayer, pMMArcLayer->MMAdmDB.szRecordOnCourse,
        pBD_XP->pField + 3, &pArcHeader->nFirstIdNode, TRUE);

    MMWriteValueToRecordDBXP(
        hMiraMonLayer, pMMArcLayer->MMAdmDB.szRecordOnCourse,
        pBD_XP->pField + 4, &pArcHeader->nLastIdNode, TRUE);

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &pMMArcLayer->MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)pMMArcLayer->MMAdmDB.pRecList;

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    pFlushRecList->pBlockToBeSaved =
        (void *)pMMArcLayer->MMAdmDB.szRecordOnCourse;

    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMAppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        pMMArcLayer->MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &pMMArcLayer->MMAdmDB,
            pMMArcLayer->MMAdmDB.szRecordOnCourse, pFlushRecList,
            &pMMArcLayer->MMAdmDB.pMMBDXP->nRecords, nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Adds a node record to a MiraMon table associated with a vector layer.
int MMAddNodeRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                          MM_INTERNAL_FID nElemCount, struct MM_NH *pNodeHeader)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    struct MiraMonNodeLayer *pMMNodeLayer;
    double nDoubleValue;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMNodeLayer = &hMiraMonLayer->MMPolygon.MMArc.MMNode;
    else
        pMMNodeLayer = &hMiraMonLayer->MMArc.MMNode;

    if (!pMMNodeLayer)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Error in pMMNodeLayer() (1)");
        return MM_STOP_WRITING_FEATURES;
    }

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(hMiraMonLayer,
                             pMMNodeLayer->MMAdmDB.pMMBDXP->nRecords + 1))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Error in MMCheckVersionForFID() (9)");
        return MM_STOP_WRITING_FEATURES;
    }

    // Test length
    // Private fields
    // ID_GRAFIC
    if (MMWriteValueToszStringToOperate(hMiraMonLayer,
                                        pMMNodeLayer->MMAdmDB.pMMBDXP->pField,
                                        &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMNodeLayer->MMAdmDB, 0,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // ARCS_A_NOD
    nDoubleValue = pNodeHeader->nArcsCount;
    if (MMWriteValueToszStringToOperate(
            hMiraMonLayer, pMMNodeLayer->MMAdmDB.pMMBDXP->pField + 1,
            &nDoubleValue, FALSE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMNodeLayer->MMAdmDB, 1,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // TIPUS_NODE
    nDoubleValue = pNodeHeader->cNodeType;
    if (MMWriteValueToszStringToOperate(
            hMiraMonLayer, pMMNodeLayer->MMAdmDB.pMMBDXP->pField + 2,
            &nDoubleValue, FALSE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer, &pMMNodeLayer->MMAdmDB, 2,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockWhereToSaveOrRead =
        (void *)pMMNodeLayer->MMAdmDB.pRecList;

    pBD_XP = pMMNodeLayer->MMAdmDB.pMMBDXP;

    pMMNodeLayer->MMAdmDB.FlushRecList.SizeOfBlockToBeSaved =
        pBD_XP->BytesPerRecord;
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockToBeSaved =
        (void *)pMMNodeLayer->MMAdmDB.szRecordOnCourse;

    memset(pMMNodeLayer->MMAdmDB.szRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMNodeLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField, &nElemCount, TRUE);

    nDoubleValue = pNodeHeader->nArcsCount;
    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMNodeLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField + 1, &nDoubleValue, FALSE);

    nDoubleValue = pNodeHeader->cNodeType;
    MMWriteValueToRecordDBXP(hMiraMonLayer,
                             pMMNodeLayer->MMAdmDB.szRecordOnCourse,
                             pBD_XP->pField + 2, &nDoubleValue, FALSE);

    if (MMAppendBlockToBuffer(&pMMNodeLayer->MMAdmDB.FlushRecList))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    pMMNodeLayer->MMAdmDB.pMMBDXP->nRecords++;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Adds a polygon or multipolygon record to a MiraMon table
// associated with a vector layer.
int MMAddPolygonRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             struct MiraMonFeature *hMMFeature,
                             MM_INTERNAL_FID nElemCount,
                             MM_N_VERTICES_TYPE nVerticesCount,
                             struct MM_PH *pPolHeader)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = MM_PRIVATE_POLYGON_DB_FIELDS;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords +
                               (hMMFeature ? hMMFeature->nNumMRecords : 0)))
        return MM_STOP_WRITING_FEATURES;

    pBD_XP = hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;

    // Test length
    // Private fields
    // ID_GRAFIC
    if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField,
                                        &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                      &hMiraMonLayer->MMPolygon.MMAdmDB, 0,
                                      hMiraMonLayer->szStringToOperate))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // The other fields are valid if pPolHeader exists (it is not
    // the universal polygon)
    if (pPolHeader)
    {
        // N_VERTEXS
        if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 1,
                                            &nVerticesCount, TRUE))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                          &hMiraMonLayer->MMPolygon.MMAdmDB, 1,
                                          hMiraMonLayer->szStringToOperate))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // PERIMETER
        if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 2,
                                            &pPolHeader->dfPerimeter, FALSE))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                          &hMiraMonLayer->MMPolygon.MMAdmDB, 2,
                                          hMiraMonLayer->szStringToOperate))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // AREA
        if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 3,
                                            &pPolHeader->dfArea, FALSE))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                          &hMiraMonLayer->MMPolygon.MMAdmDB, 3,
                                          hMiraMonLayer->szStringToOperate))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // N_ARCS
        if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 4,
                                            &pPolHeader->nArcsCount, TRUE))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                          &hMiraMonLayer->MMPolygon.MMAdmDB, 4,
                                          hMiraMonLayer->szStringToOperate))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // N_POLIG
        if (MMWriteValueToszStringToOperate(hMiraMonLayer, pBD_XP->pField + 5,
                                            &pPolHeader->nRingsCount, TRUE))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        if (MMTestAndFixValueToRecordDBXP(hMiraMonLayer,
                                          &hMiraMonLayer->MMPolygon.MMAdmDB, 5,
                                          hMiraMonLayer->szStringToOperate))
            return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    // GDAL fields
    if (hMMFeature && hMMFeature->nNumMRecords &&
        hMMFeature->pRecords[0].nNumField)
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                         &hMiraMonLayer->MMPolygon.MMAdmDB,
                                         nNumPrivateMMField, 0, 0))
            return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMPolygon.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMPolygon.MMAdmDB.pRecList;

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    pFlushRecList->pBlockToBeSaved =
        (void *)hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;

    // Now length is sure, write
    memset(hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse, ' ',
           pBD_XP->BytesPerRecord);
    if (MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField, &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
    {
        if (MMAppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    if (pPolHeader)
    {
        MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField + 1, &nVerticesCount, TRUE);

        MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField + 2, &pPolHeader->dfPerimeter, FALSE);

        MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField + 3, &pPolHeader->dfArea, FALSE);

        MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField + 4, &pPolHeader->nArcsCount, TRUE);

        MMWriteValueToRecordDBXP(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse,
            pBD_XP->pField + 5, &pPolHeader->nRingsCount, TRUE);
    }

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMPolygon.MMAdmDB,
            hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse, pFlushRecList,
            &hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Close the MiraMon database associated with a vector layer.
static int MMCloseMMBD_XPFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              struct MMAdmDatabase *MMAdmDB)
{
    int ret_code = 1;
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITING_MODE)
    {
        if (!MMAdmDB->pMMBDXP ||
            (MMAdmDB->pMMBDXP && !MMAdmDB->pMMBDXP->pfDataBase))
        {
            // In case of 0 elements created we have to
            // create an empty DBF
            if (hMiraMonLayer->bIsPolygon)
            {
                if (hMiraMonLayer->TopHeader.nElemCount <= 1)
                {
                    if (MMCreateMMDB(hMiraMonLayer, nullptr))
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Memory error in MiraMon "
                                 "driver (MMCreateMMDB())");
                        goto end_label;
                    }
                }
            }
            else if (hMiraMonLayer->bIsPoint || hMiraMonLayer->bIsArc)
            {
                if (hMiraMonLayer->TopHeader.nElemCount == 0)
                {
                    if (MMCreateMMDB(hMiraMonLayer, nullptr))
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Memory error in MiraMon "
                                 "driver (MMCreateMMDB())");
                        goto end_label;
                    }
                }
            }
        }

        if (MM_WriteNRecordsMMBD_XPFile(MMAdmDB))
            goto end_label;

        // Flushing all to be flushed
        MMAdmDB->FlushRecList.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&MMAdmDB->FlushRecList))
            goto end_label;
    }

    ret_code = 0;
end_label:
    // Closing database files
    if (MMAdmDB && MMAdmDB->pMMBDXP && MMAdmDB->pMMBDXP->pfDataBase)
        fclose_and_nullify(&MMAdmDB->pMMBDXP->pfDataBase);

    return ret_code;
}

int MMCloseMMBD_XP(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int ret_code = 0;
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->pMMBDXP && hMiraMonLayer->pMMBDXP->pfDataBase)
    {
        fclose_and_nullify(&hMiraMonLayer->pMMBDXP->pfDataBase);
    }

    if (hMiraMonLayer->bIsPoint)
        ret_code =
            MMCloseMMBD_XPFile(hMiraMonLayer, &hMiraMonLayer->MMPoint.MMAdmDB);
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (MMCloseMMBD_XPFile(hMiraMonLayer, &hMiraMonLayer->MMArc.MMAdmDB))
            ret_code = 1;
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMArc.MMNode.MMAdmDB))
            ret_code = 1;
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMPolygon.MMAdmDB))
            ret_code = 1;
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            ret_code = 1;
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB))
            ret_code = 1;
    }
    else if (hMiraMonLayer->bIsDBF)
        ret_code =
            MMCloseMMBD_XPFile(hMiraMonLayer, &hMiraMonLayer->MMAdmDBWriting);

    return ret_code;
}

// Destroys the memory used to create a MiraMon table associated
// with a vector layer.
static void MMDestroyMMDBFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              struct MMAdmDatabase *pMMAdmDB)
{
    if (!hMiraMonLayer)
        return;

    if (pMMAdmDB && pMMAdmDB->szRecordOnCourse)
    {
        VSIFree(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse = nullptr;
    }
    if (hMiraMonLayer->szStringToOperate)
    {
        VSIFree(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate = nullptr;
        hMiraMonLayer->nNumStringToOperate = 0;
    }

    if (pMMAdmDB && pMMAdmDB->pMMBDXP)
    {
        MM_ReleaseDBFHeader(&pMMAdmDB->pMMBDXP);
        hMiraMonLayer->pMMBDXP = nullptr;
    }
    if (pMMAdmDB && pMMAdmDB->pRecList)
    {
        VSIFree(pMMAdmDB->pRecList);
        pMMAdmDB->pRecList = nullptr;
    }
}

void MMDestroyMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return;

    if (hMiraMonLayer->bIsPoint)
    {
        MMDestroyMMDBFile(hMiraMonLayer, &hMiraMonLayer->MMPoint.MMAdmDB);
        return;
    }
    if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(hMiraMonLayer, &hMiraMonLayer->MMArc.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer, &hMiraMonLayer->MMArc.MMNode.MMAdmDB);
        return;
    }
    if (hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer,
                          &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer,
                          &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
    if (hMiraMonLayer->bIsDBF)
        MMDestroyMMDBFile(hMiraMonLayer, &hMiraMonLayer->MMAdmDBWriting);
}

CPL_C_END  // Necessary for compiling in GDAL project
