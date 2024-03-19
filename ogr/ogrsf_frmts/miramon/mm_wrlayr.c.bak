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

#ifdef GDAL_COMPILATION
#include "mm_wrlayr.h"
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#include "mm_rdlayr.h"    // For MM_ReadExtendedDBFHeader()
#include "gdal.h"         // For GDALDatasetH
#include "ogr_srs_api.h"  // For OSRGetAuthorityCode
#else
#include "CmptCmp.h"                    // Compatibility between compilers
#include "PrjMMVGl.h"                   // For a DirectoriPrograma
#include "mm_gdal\mm_wrlayr.h"          // For fseek_function()
#include "mm_gdal\mm_gdal_functions.h"  // For MM_strnzcpy()
#include "mm_gdal\mm_rdlayr.h"          // For MM_ReadExtendedDBFHeader()
#include "msg.h"                        // For ErrorMsg()
#endif

#include "cpl_string.h"  // For CPL_ENC_UTF8

static char local_message[MAX_LOCAL_MESSAGE];

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling in GDAL project
#endif       // GDAL_COMPILATION

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
int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile,
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
                              MM_FILE_OFFSET nOffset,
                              MM_INTERNAL_FID nElemCount);
int MMCheckVersionOffset(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_FILE_OFFSET OffsetToCheck);
int MMCheckVersionForFID(struct MiraMonVectLayerInfo *hMiraMonLayer,
                         MM_INTERNAL_FID FID);

// Extended DBF functions
int MMCreateMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer);
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

void MMCPLError(int level, int code, const char *message)
{
#ifdef GDAL_COMPILATION
    CPLError(level, code, "%s", message);
#else
    sprintf(local_message, "level(%d), code(%d)", level, code);
    InfoMsg(local_message);
    ErrorMsg(message);
#endif
}

void MMCPLWarning(int level, int code, const char *message)
{
#ifdef GDAL_COMPILATION
    CPLError(level, code, "%s", message);
#else
    sprintf(local_message, "level(%d), code(%d)", level, code);
    InfoMsg(local_message);
    InfoMsg(message);
#endif
}

void MMCPLDebug(const char *c, const char *message)
{
#ifdef GDAL_COMPILATION
    CPLDebug(c, "%s", message);
#else
    sprintf(local_message, "Code(%s)\n", c);
    printf(local_message); /*ok*/
    printf(message);       /*ok*/
    printf("\n");          /*ok*/
#endif
}

#ifndef GDAL_COMPILATION
int snprintf(char *str, size_t size, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}
#endif

// Checks for potential arithmetic overflow when performing multiplication
// operations between two GUInt64 values and converting the result to size_t.
// Important for 32 vs. 64 bit compiling compatibility.
int MMCheckSize_t(GUInt64 nCount, GUInt64 nSize)
{
    size_t nMul;
    if ((size_t)nCount != nCount)
        return 1;

    if ((size_t)nSize != nSize)
        return 1;

    nMul = (size_t)(nCount * nSize);
    if (nCount != 0 && nMul / nCount != nSize)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory, "Overflow in MMCheckSize_t()");
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
int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader)
{
    char dot;
    uint32_t NCount;
    int32_t reservat4 = 0L;

    pMMHeader->Flag = 0x0;
    if (fseek_function(pF, 0, SEEK_SET))
        return 1;
    if (fread_function(pMMHeader->aFileType, 1, 3, pF) != 3)
        return 1;
    if (fread_function(pMMHeader->aLayerVersion, 1, 2, pF) != 2)
        return 1;
    if (fread_function(&dot, 1, 1, pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->aLayerSubVersion, 1, 1, pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->Flag, sizeof(pMMHeader->Flag), 1, pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->hBB.dfMinX, sizeof(pMMHeader->hBB.dfMinX), 1,
                       pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->hBB.dfMaxX, sizeof(pMMHeader->hBB.dfMaxX), 1,
                       pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->hBB.dfMinY, sizeof(pMMHeader->hBB.dfMinY), 1,
                       pF) != 1)
        return 1;
    if (fread_function(&pMMHeader->hBB.dfMaxY, sizeof(pMMHeader->hBB.dfMaxY), 1,
                       pF) != 1)
        return 1;
    if (pMMHeader->aLayerVersion[0] == ' ' &&
        pMMHeader->aLayerVersion[1] == '1')
    {
        if (fread_function(&NCount, sizeof(NCount), 1, pF) != 1)
            return 1;

        pMMHeader->nElemCount = (MM_INTERNAL_FID)NCount;

        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    else if (pMMHeader->aLayerVersion[0] == ' ' &&
             pMMHeader->aLayerVersion[1] == '2')
    {
        if (fread_function(&(pMMHeader->nElemCount),
                           sizeof(pMMHeader->nElemCount), 1, pF) != 1)
            return 1;

        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
    }

    if (pMMHeader->Flag & MM_LAYER_3D_INFO)
        pMMHeader->bIs3d = 1;

    if (pMMHeader->Flag & MM_LAYER_MULTIPOLYGON)
        pMMHeader->bIsMultipolygon = 1;

    return 0;
}

static int MMWriteHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader)
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

    if (fseek_function(pF, 0, SEEK_SET))
        return 1;
    if (fwrite_function(pMMHeader->aFileType, 1, 3, pF) != 3)
        return 1;
    if (fwrite_function(pMMHeader->aLayerVersion, 1, 2, pF) != 2)
        return 1;
    if (fwrite_function(&dot, 1, 1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->aLayerSubVersion, 1, 1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->Flag, sizeof(pMMHeader->Flag), 1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMinX, sizeof(pMMHeader->hBB.dfMinX),
                        1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMaxX, sizeof(pMMHeader->hBB.dfMaxX),
                        1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMinY, sizeof(pMMHeader->hBB.dfMinY),
                        1, pF) != 1)
        return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMaxY, sizeof(pMMHeader->hBB.dfMaxY),
                        1, pF) != 1)
        return 1;
    if (pMMHeader->aLayerVersion[0] == ' ' &&
        pMMHeader->aLayerVersion[1] == '1')
    {
        NCount = (uint32_t)pMMHeader->nElemCount;
        if (fwrite_function(&NCount, sizeof(NCount), 1, pF) != 1)
            return 1;

        if (fwrite_function(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    else if (pMMHeader->aLayerVersion[0] == ' ' &&
             pMMHeader->aLayerVersion[1] == '2')
    {
        if (fwrite_function(&(pMMHeader->nElemCount),
                            sizeof(pMMHeader->nElemCount), 1, pF) != 1)
            return 1;

        // Next part of the file (don't apply for the moment)
        if (fwrite_function(&nNumber1, sizeof(nNumber1), 1, pF) != 1)
            return 1;
        if (fwrite_function(&nNumber0, sizeof(nNumber0), 1, pF) != 1)
            return 1;

        // Reserved bytes
        if (fwrite_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        if (fwrite_function(&reservat4, 4, 1, pF) != 1)
            return 1;
    }
    return 0;
}

void MMInitHeader(struct MM_TH *pMMHeader, int layerType, int nVersion)
{
    memset(pMMHeader, 0, sizeof(*pMMHeader));
    switch (nVersion)
    {
        case MM_32BITS_VERSION:
            pMMHeader->aLayerVersion[0] = '0';
            pMMHeader->aLayerVersion[1] = '1';
            pMMHeader->aLayerSubVersion = '1';
            break;
        case MM_64BITS_VERSION:
        case MM_LAST_VERSION:
        default:
            pMMHeader->aLayerVersion[0] = '0';
            pMMHeader->aLayerVersion[1] = '2';
            pMMHeader->aLayerSubVersion = '0';
            break;
    }
    switch (layerType)
    {
        case MM_LayerType_Point:
            pMMHeader->aFileType[0] = 'P';
            pMMHeader->aFileType[1] = 'N';
            pMMHeader->aFileType[2] = 'T';
            break;
        case MM_LayerType_Point3d:
            pMMHeader->aFileType[0] = 'P';
            pMMHeader->aFileType[1] = 'N';
            pMMHeader->aFileType[2] = 'T';
            pMMHeader->bIs3d = 1;
            break;
        case MM_LayerType_Arc:
            pMMHeader->aFileType[0] = 'A';
            pMMHeader->aFileType[1] = 'R';
            pMMHeader->aFileType[2] = 'C';
            break;
        case MM_LayerType_Arc3d:
            pMMHeader->aFileType[0] = 'A';
            pMMHeader->aFileType[1] = 'R';
            pMMHeader->aFileType[2] = 'C';
            pMMHeader->bIs3d = 1;
            break;
        case MM_LayerType_Pol:
            pMMHeader->aFileType[0] = 'P';
            pMMHeader->aFileType[1] = 'O';
            pMMHeader->aFileType[2] = 'L';
            break;
        case MM_LayerType_Pol3d:
            pMMHeader->aFileType[0] = 'P';
            pMMHeader->aFileType[1] = 'O';
            pMMHeader->aFileType[2] = 'L';
            pMMHeader->bIs3d = 1;
            break;
        default:
            break;
    }
    pMMHeader->nElemCount = 0;
    pMMHeader->hBB.dfMinX = MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMaxX = -MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMinY = MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMaxY = -MM_UNDEFINED_STATISTICAL_VALUE;

    pMMHeader->Flag = MM_CREATED_USING_MIRAMON;  // Created from MiraMon
    if (pMMHeader->bIs3d)
        pMMHeader->Flag |= MM_LAYER_3D_INFO;  // 3D

    if (pMMHeader->bIsMultipolygon)
        pMMHeader->Flag |= MM_LAYER_MULTIPOLYGON;  // Multipolygon.

    if (pMMHeader->aFileType[0] == 'P' && pMMHeader->aFileType[1] == 'O' &&
        pMMHeader->aFileType[2] == 'L')
        pMMHeader->Flag |= MM_BIT_5_ON;  // Explicital polygons
}

int MMWriteEmptyHeader(FILE_TYPE *pF, int layerType, int nVersion)
{
    struct MM_TH pMMHeader;

    memset(&pMMHeader, 0, sizeof(pMMHeader));
    switch (nVersion)
    {
        case MM_32BITS_VERSION:
            pMMHeader.aLayerVersion[0] = '0';
            pMMHeader.aLayerVersion[1] = '1';
            pMMHeader.aLayerSubVersion = '1';
            break;
        case MM_64BITS_VERSION:
        case MM_LAST_VERSION:
        default:
            pMMHeader.aLayerVersion[0] = '0';
            pMMHeader.aLayerVersion[1] = '2';
            pMMHeader.aLayerSubVersion = '0';
            break;
    }
    switch (layerType)
    {
        case MM_LayerType_Point:
            pMMHeader.aFileType[0] = 'P';
            pMMHeader.aFileType[1] = 'N';
            pMMHeader.aFileType[2] = 'T';
            break;
        case MM_LayerType_Point3d:
            pMMHeader.aFileType[0] = 'P';
            pMMHeader.aFileType[1] = 'N';
            pMMHeader.aFileType[2] = 'T';
            pMMHeader.bIs3d = 1;
            break;
        case MM_LayerType_Arc:
            pMMHeader.aFileType[0] = 'A';
            pMMHeader.aFileType[1] = 'R';
            pMMHeader.aFileType[2] = 'C';
            break;
        case MM_LayerType_Arc3d:
            pMMHeader.aFileType[0] = 'A';
            pMMHeader.aFileType[1] = 'R';
            pMMHeader.aFileType[2] = 'C';
            pMMHeader.bIs3d = 1;
            break;
        case MM_LayerType_Pol:
            pMMHeader.aFileType[0] = 'P';
            pMMHeader.aFileType[1] = 'O';
            pMMHeader.aFileType[2] = 'L';
            break;
        case MM_LayerType_Pol3d:
            pMMHeader.aFileType[0] = 'P';
            pMMHeader.aFileType[1] = 'O';
            pMMHeader.aFileType[2] = 'L';
            pMMHeader.bIs3d = 1;
            break;
        default:
            break;
    }
    pMMHeader.nElemCount = 0;
    pMMHeader.hBB.dfMinX = MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMaxX = -MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMinY = MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMaxY = -MM_UNDEFINED_STATISTICAL_VALUE;

    return MMWriteHeader(pF, &pMMHeader);
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Z section                                      */
/* -------------------------------------------------------------------- */
int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *pF,
                   struct MM_ZSection *pZSection)
{
    int32_t reservat4 = 0L;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPoint)
    {
        pZSection->ZSectionOffset =
            hMiraMonLayer->nHeaderDiskSize +
            hMiraMonLayer->TopHeader.nElemCount * MM_SIZE_OF_TL;
    }
    else if (hMiraMonLayer->bIsArc && !(hMiraMonLayer->bIsPolygon) &&
             hMiraMonLayer->TopHeader.nElemCount > 0)
    {
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset =
            hMiraMonLayer->MMArc
                .pArcHeader[hMiraMonLayer->TopHeader.nElemCount - 1]
                .nOffset +
            hMiraMonLayer->MMArc
                    .pArcHeader[hMiraMonLayer->TopHeader.nElemCount - 1]
                    .nElemCount *
                MM_SIZE_OF_COORDINATE;
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset =
            hMiraMonLayer->MMPolygon.MMArc
                .pArcHeader[hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount -
                            1]
                .nOffset +
            hMiraMonLayer->MMPolygon.MMArc
                    .pArcHeader
                        [hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount - 1]
                    .nElemCount *
                MM_SIZE_OF_COORDINATE;
    }
    else
        return 1;

    if (pF)
    {
        if (fseek_function(pF, pZSection->ZSectionOffset, SEEK_SET))
            return 1;

        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;
        if (fread_function(&reservat4, 4, 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += 4;

        if (fread_function(&pZSection->ZHeader.dfBBminz,
                           sizeof(pZSection->ZHeader.dfBBminz), 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBminz);

        if (fread_function(&pZSection->ZHeader.dfBBmaxz,
                           sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF) != 1)
            return 1;
        pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBmaxz);
    }
    return 0;
}

static int MMWriteZSection(FILE_TYPE *pF, struct MM_ZSection *pZSection)
{
    int32_t reservat4 = 0L;

    if (fseek_function(pF, pZSection->ZSectionOffset, SEEK_SET))
        return 1;

    if (fwrite_function(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (fwrite_function(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (fwrite_function(&reservat4, 4, 1, pF) != 1)
        return 1;
    if (fwrite_function(&reservat4, 4, 1, pF) != 1)
        return 1;

    pZSection->ZSectionOffset += 16;

    if (fwrite_function(&pZSection->ZHeader.dfBBminz,
                        sizeof(pZSection->ZHeader.dfBBminz), 1, pF) != 1)
        return 1;
    pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBminz);
    if (fwrite_function(&pZSection->ZHeader.dfBBmaxz,
                        sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF) != 1)
        return 1;
    pZSection->ZSectionOffset += sizeof(pZSection->ZHeader.dfBBmaxz);
    return 0;
}

int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              FILE_TYPE *pF, MM_INTERNAL_FID nElements,
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
            free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBmaxz;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->nZCount;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        if (hMiraMonLayer->LayerVersion == MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved = 4;
            FlushTMP.pBlockToBeSaved = (void *)nullptr;
            if (MMReadBlockFromBuffer(&FlushTMP))
            {
                if (pBuffer)
                    free_function(pBuffer);
                return 1;
            }
        }

        if (MMReadOffsetDependingOnVersion(hMiraMonLayer, &FlushTMP,
                                           &(pZDescription + nIndex)->nOffsetZ))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }
    if (pBuffer)
        free_function(pBuffer);

    return 0;
}

static int
MMWriteZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                           FILE_TYPE *pF, MM_INTERNAL_FID nElements,
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
        pZSection->ZSectionOffset +
        nElements *
            (sizeof(pZDescription->dfBBminz) + sizeof(pZDescription->dfBBmaxz) +
             sizeof(pZDescription->nZCount) +
             ((hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
                  ? sizeof(nUL32)
                  : sizeof(pZDescription->nOffsetZ)));

    if (MMInitFlush(&FlushTMP, pF,
                    (hMiraMonLayer->nMemoryRatio != 1)
                        ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                        : MM_25MB,
                    &pBuffer, pZSection->ZSectionOffset, 0))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->dfBBmaxz;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved =
            sizeof((pZDescription + nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved = (void *)&(pZDescription + nIndex)->nZCount;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        if (hMiraMonLayer->LayerVersion == MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved = 4;
            FlushTMP.pBlockToBeSaved = (void *)nullptr;
            if (MMAppendBlockToBuffer(&FlushTMP))
            {
                if (pBuffer)
                    free_function(pBuffer);
                return 1;
            }
        }

        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                (pZDescription + nIndex)->nOffsetZ + nOffsetDiff))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }
    pZSection->ZSectionOffset += FlushTMP.TotalSavedBytes;

    if (pBuffer)
        free_function(pBuffer);

    return 0;
}

static void MMDestroyZSectionDescription(struct MM_ZSection *pZSection)
{
    if (pZSection->pZL)
    {
        free_function(pZSection->pZL);
        pZSection->pZL = nullptr;
    }

    if (pZSection->pZDescription)
    {
        free_function(pZSection->pZDescription);
        pZSection->pZDescription = nullptr;
    }
}

static int MMInitZSectionDescription(struct MM_ZSection *pZSection)
{
    if (MMCheckSize_t(
            pZSection->nMaxZDescription * sizeof(*pZSection->pZDescription), 1))
        return 1;

    if (!pZSection->nMaxZDescription)
    {
        pZSection->pZDescription = nullptr;
        return 0;  // No elements to read (or write)
    }

    pZSection->pZDescription =
        (struct MM_ZD *)calloc_function((size_t)pZSection->nMaxZDescription *
                                        sizeof(*pZSection->pZDescription));
    if (!pZSection->pZDescription)
        return 1;
    return 0;
}

static int MMInitZSectionLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                               FILE_TYPE *pF3d, struct MM_ZSection *pZSection)
{
    if (!hMiraMonLayer)
        return 1;

    // Zsection
    if (!hMiraMonLayer->TopHeader.bIs3d)
    {
        pZSection->pZDescription = nullptr;
        return 0;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pZSection->ZHeader.dfBBminz = STATISTICAL_UNDEF_VALUE;
        pZSection->ZHeader.dfBBmaxz = -STATISTICAL_UNDEF_VALUE;
    }

    // ZH
    pZSection->ZHeader.nMyDiskSize = 32;
    pZSection->ZSectionOffset = 0;

    // ZD
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pZSection->nMaxZDescription =
            MM_FIRST_NUMBER_OF_VERTICES * sizeof(double);
        if (MMInitZSectionDescription(pZSection))
            return 1;
    }
    else
    {
        pZSection->nMaxZDescription =
            hMiraMonLayer->TopHeader.nElemCount * sizeof(double);
        if (MMInitZSectionDescription(pZSection))
            return 1;
    }

    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pZSection->nZDDiskSize = MM_SIZE_OF_ZD_32_BITS;
    else
        pZSection->nZDDiskSize = MM_SIZE_OF_ZD_64_BITS;

    pZSection->ZDOffset = 0;

    // ZL
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        if (MMInitFlush(&pZSection->FlushZL, pF3d,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &pZSection->pZL, 0, sizeof(double)))
            return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Extensions                                     */
/* -------------------------------------------------------------------- */

/* Find the last ocurrence of pszFinalPart in pszName
    and changes it by pszNewPart.
 
    Examples of desired behaviour
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

    pszWhereToFind = pszName;
    while (nullptr != (pAux = strstr(pszWhereToFind, pszFinalPart)))
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
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

        strcpy(hMiraMonLayer->MMPoint.pszLayerName,
               hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMPoint.pszLayerName, ".pnt");
    }
    if (nullptr == (hMiraMonLayer->MMPoint.pF =
                        fopen_function(hMiraMonLayer->MMPoint.pszLayerName,
                                       hMiraMonLayer->pszFlags)))
    {
        sprintf(local_message, "Error MMPoint.pF: Cannot open file %s.",
                hMiraMonLayer->MMPoint.pszLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }
    fseek_function(hMiraMonLayer->MMPoint.pF, 0, SEEK_SET);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // TL
        strcpy(hMiraMonLayer->MMPoint.pszTLName,
               hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMPoint.pszTLName, "T.~xy");

        if (nullptr == (hMiraMonLayer->MMPoint.pFTL =
                            fopen_function(hMiraMonLayer->MMPoint.pszTLName,
                                           hMiraMonLayer->pszFlags)))
        {
            sprintf(local_message, "Error MMPoint.pFTL: Cannot open file %s.",
                    hMiraMonLayer->MMPoint.pszTLName);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        fseek_function(hMiraMonLayer->MMPoint.pFTL, 0, SEEK_SET);

        if (MMInitFlush(&hMiraMonLayer->MMPoint.FlushTL,
                        hMiraMonLayer->MMPoint.pFTL,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &hMiraMonLayer->MMPoint.pTL, 0, MM_SIZE_OF_TL))
            return 1;

        // 3D part
        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            strcpy(hMiraMonLayer->MMPoint.psz3DLayerName,
                   hMiraMonLayer->pszSrcLayerName);
            strcat(hMiraMonLayer->MMPoint.psz3DLayerName, "T.~z");

            if (nullptr == (hMiraMonLayer->MMPoint.pF3d = fopen_function(
                                hMiraMonLayer->MMPoint.psz3DLayerName,
                                hMiraMonLayer->pszFlags)))
            {
                sprintf(local_message,
                        "Error MMPoint.pF3d: Cannot open file %s.",
                        hMiraMonLayer->MMPoint.psz3DLayerName);
                MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
                return 1;
            }
            fseek_function(hMiraMonLayer->MMPoint.pF3d, 0, SEEK_SET);
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
    strcpy(hMiraMonLayer->MMPoint.pszREL_LayerName,
           hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPoint.pszREL_LayerName, "T.rel");
    else
    {
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
    strcpy(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName,
           hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName, "T.dbf");
    else
    {
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
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
    strcpy(pMMArcLayer->MMNode.pszLayerName,
           reset_extension(pMMArcLayer->MMNode.pszLayerName, "nod"));

    if (nullptr == (pMMArcLayer->MMNode.pF =
                        fopen_function(pMMArcLayer->MMNode.pszLayerName,
                                       hMiraMonLayer->pszFlags)))
    {
        sprintf(local_message, "Error MMNode.pF: Cannot open file %s.",
                pMMArcLayer->MMNode.pszLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }
    fseek_function(pMMArcLayer->MMNode.pF, 0, SEEK_SET);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Node Header
        pMMArcLayer->MMNode.nMaxNodeHeader = MM_FIRST_NUMBER_OF_NODES;
        if (MMCheckSize_t(pMMArcLayer->MMNode.nMaxNodeHeader *
                              sizeof(*pMMArcLayer->MMNode.pNodeHeader),
                          1))
            return 1;

        if (!pMMArcLayer->MMNode.nMaxNodeHeader)
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
                       "Error in MiraMon "
                       "driver: no nodes to write?");
            return 1;
        }

        if (nullptr ==
            (pMMArcLayer->MMNode.pNodeHeader = (struct MM_NH *)calloc_function(
                 (size_t)pMMArcLayer->MMNode.nMaxNodeHeader *
                 sizeof(*pMMArcLayer->MMNode.pNodeHeader))))
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
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

        if (nullptr == (pMMArcLayer->MMNode.pFNL =
                            fopen_function(pMMArcLayer->MMNode.pszNLName,
                                           hMiraMonLayer->pszFlags)))
        {
            sprintf(local_message, "Error MMNode.pFNL: Cannot open file %s.",
                    pMMArcLayer->MMNode.pszNLName);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        fseek_function(pMMArcLayer->MMNode.pFNL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->MMNode.FlushNL, pMMArcLayer->MMNode.pFNL,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &pMMArcLayer->MMNode.pNL, 0, 0))
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pArcTopHeader->bIs3d = 1;  // Read description of bRealIs3d
        MMInitBoundingBox(&pArcTopHeader->hBB);

        pArcTopHeader->aFileType[0] = 'A';
        pArcTopHeader->aFileType[1] = 'R';
        pArcTopHeader->aFileType[2] = 'C';

        strcpy(pMMArcLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->bIsPolygon)
            strcat(pMMArcLayer->pszLayerName, "_bound.arc");
        else
            strcat(pMMArcLayer->pszLayerName, ".arc");
    }

    if (nullptr == (pMMArcLayer->pF = fopen_function(pMMArcLayer->pszLayerName,
                                                     hMiraMonLayer->pszFlags)))
    {
        sprintf(local_message, "Error pMMArcLayer->pF: Cannot open file %s.",
                pMMArcLayer->pszLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE &&
        hMiraMonLayer->bIsPolygon)
    {
        fseek_function(pMMArcLayer->pF, 0, SEEK_SET);
        if (MMReadHeader(pMMArcLayer->pF,
                         &hMiraMonLayer->MMPolygon.TopArcHeader))
        {
            sprintf(local_message, "Error reading the format in file %s.",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        pMMArcLayer->nMaxArcHeader = MM_FIRST_NUMBER_OF_ARCS;
    else
        pMMArcLayer->nMaxArcHeader = pArcTopHeader->nElemCount;

    if (pMMArcLayer->nMaxArcHeader)
    {
        if (MMCheckSize_t(pMMArcLayer->nMaxArcHeader *
                              sizeof(*pMMArcLayer->pArcHeader),
                          1))
            return 1;
        if (nullptr == (pMMArcLayer->pArcHeader = (struct MM_AH *)
                            calloc_function((size_t)pMMArcLayer->nMaxArcHeader *
                                            sizeof(*pMMArcLayer->pArcHeader))))
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
                       "Memory error in MiraMon "
                       "driver (MMInitArcLayer())");
            return 1;
        }
        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if (MMReadAHArcSection(hMiraMonLayer))
            {
                sprintf(local_message, "Error reading the format in file %s.",
                        pMMArcLayer->pszLayerName);
                MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
                return 1;
            }
        }
    }
    else
        pMMArcLayer->pArcHeader = nullptr;

    // AL
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pMMArcLayer->nALElementSize = MM_SIZE_OF_AL;

        strcpy(pMMArcLayer->pszALName, hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->bIsPolygon)
            strcat(pMMArcLayer->pszALName, "_boundA.~xy");
        else
            strcat(pMMArcLayer->pszALName, "A.~xy");

        if (nullptr == (pMMArcLayer->pFAL = fopen_function(
                            pMMArcLayer->pszALName, hMiraMonLayer->pszFlags)))
        {
            sprintf(local_message,
                    "Error pMMArcLayer->pFAL: Cannot open file %s.",
                    pMMArcLayer->pszALName);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        fseek_function(pMMArcLayer->pFAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->FlushAL, pMMArcLayer->pFAL,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &pMMArcLayer->pAL, 0, 0))
            return 1;
    }

    // 3D
    if (pArcTopHeader->bIs3d)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        {
            strcpy(pMMArcLayer->psz3DLayerName, hMiraMonLayer->pszSrcLayerName);
            if (hMiraMonLayer->bIsPolygon)
                strcat(pMMArcLayer->psz3DLayerName, "_boundA.~z");
            else
                strcat(pMMArcLayer->psz3DLayerName, "A.~z");

            if (nullptr ==
                (pMMArcLayer->pF3d = fopen_function(pMMArcLayer->psz3DLayerName,
                                                    hMiraMonLayer->pszFlags)))
            {
                sprintf(local_message,
                        "Error pMMArcLayer->pF3d: Cannot open file %s.",
                        pMMArcLayer->psz3DLayerName);
                MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
                return 1;
            }
            fseek_function(pMMArcLayer->pF3d, 0, SEEK_SET);
        }

        if (MMInitZSectionLayer(hMiraMonLayer, pMMArcLayer->pF3d,
                                &pMMArcLayer->pZSection))
        {
            sprintf(local_message, "Error reading the format in file %s %d.",
                    pMMArcLayer->pszLayerName, 6);
            MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
            return 1;
        }

        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if (MMReadZSection(hMiraMonLayer, pMMArcLayer->pF,
                               &pMMArcLayer->pZSection))
            {
                sprintf(local_message, "Error reading the format in file %s.",
                        pMMArcLayer->pszLayerName);
                MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
                return 1;
            }

            if (MMReadZDescriptionHeaders(hMiraMonLayer, pMMArcLayer->pF,
                                          pArcTopHeader->nElemCount,
                                          &pMMArcLayer->pZSection))
            {
                sprintf(local_message, "Error reading the format in file %s.",
                        pMMArcLayer->pszLayerName);
                MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
                return 1;
            }
        }
    }
    // MiraMon metadata
    strcpy(pMMArcLayer->pszREL_LayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszREL_LayerName, "_boundA.rel");
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
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszREL_LayerName, "A.rel");
        else
        {
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
    strcpy(pMMArcLayer->MMAdmDB.pszExtDBFLayerName,
           hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, "_boundA.dbf");
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
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, "A.dbf");
        else
        {
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d = 1;  // Read description of bRealIs3d
        MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);

        hMiraMonLayer->TopHeader.aFileType[0] = 'P';
        hMiraMonLayer->TopHeader.aFileType[1] = 'O';
        hMiraMonLayer->TopHeader.aFileType[2] = 'L';

        strcpy(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszLayerName, ".pol");
    }

    if (nullptr ==
        (pMMPolygonLayer->pF = fopen_function(pMMPolygonLayer->pszLayerName,
                                              hMiraMonLayer->pszFlags)))
    {
        sprintf(local_message,
                "Error pMMPolygonLayer->pF: Cannot open file %s.",
                pMMPolygonLayer->pszLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    // PS
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMPolygonLayer->nPSElementSize = MM_SIZE_OF_PS_32BITS;
    else
        pMMPolygonLayer->nPSElementSize = MM_SIZE_OF_PS_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        strcpy(pMMPolygonLayer->pszPSName, hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszPSName, "P.~PS");

        if (nullptr ==
            (pMMPolygonLayer->pFPS = fopen_function(pMMPolygonLayer->pszPSName,
                                                    hMiraMonLayer->pszFlags)))
        {
            sprintf(local_message,
                    "Error pMMPolygonLayer->pFPS: Cannot open file %s.",
                    pMMPolygonLayer->pszPSName);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        fseek_function(pMMPolygonLayer->pFPS, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPS, pMMPolygonLayer->pFPS,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &pMMPolygonLayer->pPS, 0,
                        pMMPolygonLayer->nPSElementSize))
            return 1;
    }

    // PH
    if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
        pMMPolygonLayer->nPHElementSize = MM_SIZE_OF_PH_32BITS;
    else
        pMMPolygonLayer->nPHElementSize = MM_SIZE_OF_PH_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        pMMPolygonLayer->nMaxPolHeader = MM_FIRST_NUMBER_OF_POLYGONS + 1;
    else
        pMMPolygonLayer->nMaxPolHeader = hMiraMonLayer->TopHeader.nElemCount;

    if (pMMPolygonLayer->nMaxPolHeader)
    {
        if (MMCheckSize_t(pMMPolygonLayer->nMaxPolHeader *
                              sizeof(*pMMPolygonLayer->pPolHeader),
                          1))
            return 1;
        if (nullptr ==
            (pMMPolygonLayer->pPolHeader = (struct MM_PH *)calloc_function(
                 (size_t)pMMPolygonLayer->nMaxPolHeader *
                 sizeof(*pMMPolygonLayer->pPolHeader))))
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Universal polygon.
        memset(pMMPolygonLayer->pPolHeader, 0,
               sizeof(*pMMPolygonLayer->pPolHeader));
        hMiraMonLayer->TopHeader.nElemCount = 1;

        // PAL
        strcpy(pMMPolygonLayer->pszPALName, hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszPALName, "P.~idx");

        if (nullptr == (pMMPolygonLayer->pFPAL =
                            fopen_function(pMMPolygonLayer->pszPALName,
                                           hMiraMonLayer->pszFlags)))
        {
            sprintf(local_message,
                    "Error pMMPolygonLayer->pFPAL: Cannot open file %s.",
                    pMMPolygonLayer->pszPALName);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        fseek_function(pMMPolygonLayer->pFPAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPAL, pMMPolygonLayer->pFPAL,
                        (hMiraMonLayer->nMemoryRatio != 1)
                            ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                            : MM_25MB,
                        &pMMPolygonLayer->pPAL, 0, 0))
            return 1;
    }

    // MiraMon metadata
    strcpy(hMiraMonLayer->MMPolygon.pszREL_LayerName,
           hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPolygon.pszREL_LayerName, "P.rel");
    else
    {
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
    strcpy(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName,
           hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName, "P.dbf");
    else
    {
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
        strcpy(hMiraMonLayer->MMPoint.pszLayerName,
               hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(hMiraMonLayer->MMPoint.pszLayerName, ".pnt");
        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                             hMiraMonLayer->MMMap->nNumberOfLayers);
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pnt\n",
                             MM_CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
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

        strcpy(pMMArcLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszLayerName, ".arc");

        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                             hMiraMonLayer->MMMap->nNumberOfLayers);
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.arc\n",
                             MM_CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
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

        strcpy(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMPolygonLayer->pszLayerName, ".pol");

        if (hMiraMonLayer->MMMap && hMiraMonLayer->MMMap->fMMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n",
                             hMiraMonLayer->MMMap->nNumberOfLayers);
            fprintf_function(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pol\n",
                             MM_CPLGetBasename(hMiraMonLayer->pszSrcLayerName));
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
            MM_RemoveInitial_and_FinalQuotationMarks(pszArcLayerName);

            // If extension is not specified ".arc" will be used
            pszExt = get_extension_function(pszArcLayerName);
            if (MMIsEmptyString(pszExt))
            {
                char *pszArcLayerNameAux =
                    calloc_function(strlen(pszArcLayerName) + 5);
                if (!pszArcLayerNameAux)
                {
                    MMCPLError(CE_Failure, CPLE_OutOfMemory,
                               "Memory error in MiraMon "
                               "driver (MMInitLayerByType())");
                    return 1;
                }
                strcpy(pszArcLayerNameAux, pszArcLayerName);
                strcat(pszArcLayerNameAux, ".arc");
                free_function(pszArcLayerName);
                pszArcLayerName = pszArcLayerNameAux;
            }

            strcpy(pMMPolygonLayer->MMArc.pszLayerName,
                   form_filename_function(
                       get_path_function(hMiraMonLayer->pszSrcLayerName),
                       pszArcLayerName));

            free_function(pszArcLayerName);

            if (nullptr == (hMiraMonLayer->MMPolygon.MMArc.pF = fopen_function(
                                pMMPolygonLayer->MMArc.pszLayerName,
                                hMiraMonLayer->pszFlags)))
            {
                sprintf(local_message,
                        "Error pMMPolygonLayer.MMArc.pF: Cannot open file %s.",
                        pMMPolygonLayer->MMArc.pszLayerName);
                MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
                return 1;
            }

            if (MMReadHeader(hMiraMonLayer->MMPolygon.MMArc.pF,
                             &hMiraMonLayer->MMPolygon.TopArcHeader))
            {
                sprintf(local_message, "Error reading the format in file %s.",
                        pMMPolygonLayer->MMArc.pszLayerName);
                MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
                return 1;
            }

            if (MMReadPHPolygonSection(hMiraMonLayer))
            {
                sprintf(local_message, "Error reading the format in file %s.",
                        pMMPolygonLayer->MMArc.pszLayerName);
                MMCPLError(CE_Failure, CPLE_NotSupported, local_message);
                return 1;
            }

            fclose_function(hMiraMonLayer->MMPolygon.MMArc.pF);
            hMiraMonLayer->MMPolygon.MMArc.pF = nullptr;
        }
        else
        {
            // Creating the stringLine file associated to the polygon
            strcpy(pMMPolygonLayer->MMArc.pszLayerName,
                   hMiraMonLayer->pszSrcLayerName);
            strcat(pMMPolygonLayer->MMArc.pszLayerName, ".arc");
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
    else if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Trying to get DBF information
        strcpy(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName,
               hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName, ".dbf");
    }

    return 0;
}

int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                const char *pzFileName, int LayerVersion, char nMMRecode,
                char nMMLanguage, double nMMMemoryRatio,
                struct MiraMonDataBase *pLayerDB, MM_BOOLEAN ReadOrWrite,
                struct MiraMonVectMapInfo *MMMap)
{
    if (!hMiraMonLayer)
        return 1;

    // Some variables must be initialized
    MM_FillFieldDescriptorByLanguage();

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));

    //hMiraMonLayer->Version = MM_VECTOR_LAYER_LAST_VERSION;
    hMiraMonLayer->nMemoryRatio = nMMMemoryRatio;

    hMiraMonLayer->ReadOrWrite = ReadOrWrite;
    hMiraMonLayer->MMMap = MMMap;

    // Don't free in destructor
    hMiraMonLayer->pLayerDB = pLayerDB;

    // Opening mode
    strcpy(hMiraMonLayer->pszFlags, "wb+");

    if (LayerVersion == MM_UNKNOWN_VERSION)
    {
        MMCPLError(CE_Failure, CPLE_NotSupported,
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

    hMiraMonLayer->pszSrcLayerName = strdup_function(pzFileName);
    hMiraMonLayer->szLayerTitle =
        strdup_function(get_filename_function(pzFileName));

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
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
                                 MM_INTERNAL_FID nElements, FILE_TYPE *pF,
                                 FILE_TYPE *pF3d, const char *pszF3d,
                                 struct MM_ZSection *pZSection,
                                 MM_FILE_OFFSET FinalOffset)
{
    if (!hMiraMonLayer)
        return 1;

    // Flushing if there is something to flush on the disk
    if (!pF || !pF3d || !pszF3d || !pZSection)
        return 0;

    if (hMiraMonLayer->bIsReal3d)
    {
        pZSection->ZSectionOffset = FinalOffset;
        if (MMWriteZSection(pF, pZSection))
            return 1;

        // Header 3D. Writes it after header
        if (MMWriteZDescriptionHeaders(hMiraMonLayer, pF, nElements, pZSection))
            return 1;

        // ZL section
        pZSection->FlushZL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pZSection->FlushZL))
            return 1;

        if (MMMoveFromFileToFile(pF3d, pF, &pZSection->ZSectionOffset))
            return 1;
    }

    if (pF3d)
        fclose_function(pF3d);
    if (pszF3d && *pszF3d != '\0')
        remove_function(pszF3d);

    return 0;
}

static int MMClosePointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->TopHeader))
        {
            sprintf(local_message, "Error writing to file %s",
                    hMiraMonLayer->MMPoint.pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // TL Section
        hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
        {
            sprintf(local_message, "Error writing to file %s",
                    hMiraMonLayer->MMPoint.pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (MMMoveFromFileToFile(hMiraMonLayer->MMPoint.pFTL,
                                 hMiraMonLayer->MMPoint.pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    hMiraMonLayer->MMPoint.pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }

        fclose_function(hMiraMonLayer->MMPoint.pFTL);
        if (*hMiraMonLayer->MMPoint.pszTLName != '\0')
            remove_function(hMiraMonLayer->MMPoint.pszTLName);

        if (MMClose3DSectionLayer(
                hMiraMonLayer, hMiraMonLayer->TopHeader.nElemCount,
                hMiraMonLayer->MMPoint.pF, hMiraMonLayer->MMPoint.pF3d,
                hMiraMonLayer->MMPoint.psz3DLayerName,
                &hMiraMonLayer->MMPoint.pZSection, hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    hMiraMonLayer->MMPoint.pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
    }
    if (hMiraMonLayer->MMPoint.pF)
        fclose_function(hMiraMonLayer->MMPoint.pF);
    return 0;
}

static int MMCloseNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonArcLayer *pMMArcLayer;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->MMNode.pF, &pMMArcLayer->TopNodeHeader))
            return 1;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // NH Section
        if (MMWriteNHNodeSection(hMiraMonLayer, hMiraMonLayer->nHeaderDiskSize))
            return 1;

        // NL Section
        pMMArcLayer->MMNode.FlushNL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMArcLayer->MMNode.FlushNL))
            return 1;
        if (MMMoveFromFileToFile(pMMArcLayer->MMNode.pFNL,
                                 pMMArcLayer->MMNode.pF,
                                 &hMiraMonLayer->OffsetCheck))
            return 1;

        if (pMMArcLayer->MMNode.pFNL)
            fclose_function(pMMArcLayer->MMNode.pFNL);
        if (*pMMArcLayer->MMNode.pszNLName != '\0')
            remove_function(pMMArcLayer->MMNode.pszNLName);
    }

    if (pMMArcLayer->MMNode.pF)
        fclose_function(pMMArcLayer->MMNode.pF);

    return 0;
}

static int MMCloseArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
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

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = pArcTopHeader->nElemCount;
        pArcTopHeader->bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->pF, pArcTopHeader))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // AH Section
        if (MMWriteAHArcSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }

        // AL Section
        pMMArcLayer->FlushAL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMArcLayer->FlushAL))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (MMMoveFromFileToFile(pMMArcLayer->pFAL, pMMArcLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (pMMArcLayer->pFAL)
            fclose_function(pMMArcLayer->pFAL);
        if (*pMMArcLayer->pszALName != '\0')
            remove_function(pMMArcLayer->pszALName);

        // 3D Section
        if (MMClose3DSectionLayer(
                hMiraMonLayer, pArcTopHeader->nElemCount, pMMArcLayer->pF,
                pMMArcLayer->pF3d, pMMArcLayer->psz3DLayerName,
                &pMMArcLayer->pZSection, hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMArcLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
    }

    if (pMMArcLayer->pF)
        fclose_function(pMMArcLayer->pF);

    if (MMCloseNodeLayer(hMiraMonLayer))
        return 1;

    return 0;
}

static int MMClosePolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MiraMonPolygonLayer *pMMPolygonLayer;

    if (!hMiraMonLayer)
        return 1;

    pMMPolygonLayer = &hMiraMonLayer->MMPolygon;

    MMCloseArcLayer(hMiraMonLayer);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d = hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMPolygonLayer->pF, &hMiraMonLayer->TopHeader))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // PS Section
        pMMPolygonLayer->FlushPS.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPS))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPS, pMMPolygonLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }

        if (pMMPolygonLayer->pFPS)
            fclose_function(pMMPolygonLayer->pFPS);
        if (*pMMPolygonLayer->pszPSName != '\0')
            remove_function(pMMPolygonLayer->pszPSName);

        // AH Section
        if (MMWritePHPolygonSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }

        // PAL Section
        pMMPolygonLayer->FlushPAL.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPAL))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPAL, pMMPolygonLayer->pF,
                                 &hMiraMonLayer->OffsetCheck))
        {
            sprintf(local_message, "Error writing to file %s",
                    pMMPolygonLayer->pszLayerName);
            MMCPLError(CE_Failure, CPLE_NoWriteAccess, local_message);
            return 1;
        }
        if (pMMPolygonLayer->pFPAL)
            fclose_function(pMMPolygonLayer->pFPAL);
        if (*pMMPolygonLayer->pszPALName != '\0')
            remove_function(pMMPolygonLayer->pszPALName);
    }

    if (pMMPolygonLayer->pF)
        fclose_function(pMMPolygonLayer->pF);

    return 0;
}

int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    //CheckMMVectorLayerVersion(hMiraMonLayer, 1)

    if (!hMiraMonLayer)
        return 0;

    if (hMiraMonLayer->bIsPoint)
    {
        if (MMClosePointLayer(hMiraMonLayer))
            return 1;
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (MMCloseArcLayer(hMiraMonLayer))
            return 1;
    }
    else if (hMiraMonLayer->bIsPolygon)
    {
        if (MMClosePolygonLayer(hMiraMonLayer))
            return 1;
    }
    else if (hMiraMonLayer->bIsDBF)
    {
        // If no geometry, remove all created files
        if (hMiraMonLayer->pszSrcLayerName)
            remove_function(hMiraMonLayer->pszSrcLayerName);
        if (hMiraMonLayer->szLayerTitle)
            remove_function(hMiraMonLayer->szLayerTitle);
    }

    // MiraMon metadata files
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        if (MMWriteVectorMetadata(hMiraMonLayer))
        {
            MMCPLError(CE_Failure, CPLE_NoWriteAccess,
                       "Some error writing in metadata file of the layer");
            return 1;
        }
    }

    // MiraMon database files
    if (MMCloseMMBD_XP(hMiraMonLayer))
    {
        MMCPLError(CE_Failure, CPLE_NoWriteAccess,
                   "Some error writing in DBF file of the layer");
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Destroying (allocated memory)                  */
/* -------------------------------------------------------------------- */
static void MMDestroyMMAdmDB(struct MMAdmDatabase *pMMAdmDB)
{
    if (pMMAdmDB->pRecList)
    {
        free_function(pMMAdmDB->pRecList);
        pMMAdmDB->pRecList = nullptr;
    }

    if (pMMAdmDB->szRecordOnCourse)
    {
        free_function(pMMAdmDB->szRecordOnCourse);
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
        free_function(hMiraMonLayer->MMPoint.pTL);
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
        free_function(pMMArcLayer->MMNode.pNL);
        pMMArcLayer->MMNode.pNL = nullptr;
    }

    if (pMMArcLayer->MMNode.pNodeHeader)
    {
        free_function(pMMArcLayer->MMNode.pNodeHeader);
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
        free_function(pMMArcLayer->pAL);
        pMMArcLayer->pAL = nullptr;
    }
    if (pMMArcLayer->pArcHeader)
    {
        free_function(pMMArcLayer->pArcHeader);
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
        free_function(pMMPolygonLayer->pPAL);
        pMMPolygonLayer->pPAL = nullptr;
    }

    if (pMMPolygonLayer->pPS)
    {
        free_function(pMMPolygonLayer->pPS);
        pMMPolygonLayer->pPS = nullptr;
    }

    if (pMMPolygonLayer->pPolHeader)
    {
        free_function(pMMPolygonLayer->pPolHeader);
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
        free_function(hMiraMonLayer->pszSrcLayerName);
        hMiraMonLayer->pszSrcLayerName = nullptr;
    }
    if (hMiraMonLayer->szLayerTitle)
    {
        free_function(hMiraMonLayer->szLayerTitle);
        hMiraMonLayer->szLayerTitle = nullptr;
    }
    if (hMiraMonLayer->pSRS)
    {
        free_function(hMiraMonLayer->pSRS);
        hMiraMonLayer->pSRS = nullptr;
    }

    if (hMiraMonLayer->pMultRecordIndex)
    {
        free_function(hMiraMonLayer->pMultRecordIndex);
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
        free_function(hMiraMonLayer->pArcs);
        hMiraMonLayer->pArcs = nullptr;
    }

    if (hMiraMonLayer->szStringToOperate)
    {
        free_function(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate = nullptr;
        hMiraMonLayer->nNumStringToOperate = 0;
    }

    if (hMiraMonLayer->pLayerDB)
    {
        if (hMiraMonLayer->pLayerDB->pFields)
        {
            free_function(hMiraMonLayer->pLayerDB->pFields);
            hMiraMonLayer->pLayerDB->pFields = nullptr;
        }
        free_function(hMiraMonLayer->pLayerDB);
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
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, GUInt64 nBlockSize,
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
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Error in MiraMon "
                   "driver: MMInitFlush() with no bytes to process");
        return 1;
    }

    if (nullptr == (*pBuffer = (char *)calloc_function((size_t)nBlockSize)))
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
    fseek_function(pFlush->pF, pFlush->OffsetWhereToFlush, SEEK_SET);
    if (pFlush->nBlockSize !=
        (GUInt64)(fread_function(pFlush->pBlockWhereToSaveOrRead, 1,
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
    fseek_function(FlushInfo->pF, FlushInfo->OffsetWhereToFlush, SEEK_SET);

    if (FlushInfo->nNumBytes !=
        (GUInt64)fwrite_function(FlushInfo->pBlockWhereToSaveOrRead, 1,
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
        // If all the bloc itselfs does not fit to the buffer,
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
            else  // Add zero caracters
            {
                char zero_caracters[8] = "\0\0\0\0\0\0\0";
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
int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile,
                         MM_FILE_OFFSET *nOffset)
{
    size_t bufferSize = 100 * 1024 * 1024;  // 100 MB buffer;
    unsigned char *buffer;
    size_t bytesRead, bytesWritten;
    MM_FILE_OFFSET size_of_dst_file;

    if (!pSrcFile || !pDestFile || !nOffset)
        return 0;

    fseek_function(pDestFile, 0, SEEK_END);
    size_of_dst_file = ftell_function(pDestFile);

    if (!size_of_dst_file)
        return 0;

    if (size_of_dst_file < bufferSize)
        bufferSize = (size_t)size_of_dst_file;

    buffer = (unsigned char *)calloc_function(bufferSize);

    if (!buffer)
        return 1;

    fseek_function(pSrcFile, 0, SEEK_SET);
    while ((bytesRead = fread_function(buffer, sizeof(unsigned char),
                                       bufferSize, pSrcFile)) > 0)
    {
        bytesWritten = fwrite_function(buffer, sizeof(unsigned char), bytesRead,
                                       pDestFile);
        if (bytesWritten != bytesRead)
        {
            free_function(buffer);
            return 1;
        }
        if (nOffset)
            (*nOffset) += bytesWritten;
    }
    free_function(buffer);
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
                                    GUInt64 *nUI64)
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
        *nUI64 = (GUInt64)nUL32;
    }
    else
    {
        FlushInfo->pBlockToBeSaved = (void *)nUI64;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(&nUI64);
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
                                   MM_FILE_OFFSET *nUI64)
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
        *nUI64 = (MM_FILE_OFFSET)nUL32;
    }
    else
    {
        FlushInfo->pBlockToBeSaved = (void *)nUI64;
        FlushInfo->SizeOfBlockToBeSaved = sizeof(&nUI64);
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
/*             https://www.miramon.cat/new_note/usa/notes/   \          */
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

    nBlockSize = nElem * (pMMArcLayer->nSizeArcHeader);

    if (MMInitFlush(&FlushTMP, pMMArcLayer->pF, nBlockSize, &pBuffer,
                    hMiraMonLayer->nHeaderDiskSize, 0))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        nElementCount = pMMArcLayer->pArcHeader[iElem].nElemCount;
        if (MMReadGUInt64DependingOnVersion(hMiraMonLayer, &FlushTMP,
                                            &nElementCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        pMMArcLayer->pArcHeader[iElem].nElemCount = nElementCount;

        // Offset: offset of the first vertice of the arc
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nOffset))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // Lenght of the arc
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLenght;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLenght);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        free_function(pBuffer);
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

    if (MMInitFlush(&FlushTMP, pMMArcLayer->pF,
                    (hMiraMonLayer->nMemoryRatio != 1)
                        ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                        : MM_25MB,
                    &pBuffer, DiskOffset, 0))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nElemCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertice of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        // Lenght of the arc
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLenght);
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLenght;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }

    if (pBuffer)
        free_function(pBuffer);
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
            free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved = 1;
        FlushTMP.pBlockToBeSaved = (void *)nullptr;
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Offset: offset of the first arc to the node
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        free_function(pBuffer);
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

    if (MMInitFlush(&FlushTMP, pMMArcLayer->MMNode.pF,
                    (hMiraMonLayer->nMemoryRatio != 1)
                        ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                        : MM_25MB,
                    &pBuffer, DiskOffset, 0))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved = 1;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved = (void *)nullptr;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Offset: offset of the first arc to the node
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }

    if (pBuffer)
        free_function(pBuffer);
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

    nBlockSize =
        hMiraMonLayer->TopHeader.nElemCount * (pMMPolygonLayer->nPHElementSize);

    if (MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, nBlockSize, &pBuffer,
                    hMiraMonLayer->nHeaderDiskSize +
                        (hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount *
                         hMiraMonLayer->MMPolygon.nPSElementSize),
                    0))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead = (void *)pBuffer;
    if (MMReadFlush(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved =
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY);
        if (MMReadBlockFromBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Arcs count: number of arcs of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // External arcs count: number of external arcs of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Rings count: number of rings of the polygon
        if (MMReadGUInt64DependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertex of the arc
        if (MMReadOffsetDependingOnVersion(
                hMiraMonLayer, &FlushTMP,
                &pMMPolygonLayer->pPolHeader[iElem].nOffset))
        {
            if (pBuffer)
                free_function(pBuffer);
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
                free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
    }

    if (pBuffer)
        free_function(pBuffer);
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

    if (MMInitFlush(&FlushTMP, pMMPolygonLayer->pF,
                    (hMiraMonLayer->nMemoryRatio != 1)
                        ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                        : MM_25MB,
                    &pBuffer, DiskOffset, 0))
    {
        if (pBuffer)
            free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved =
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck += FlushTMP.SizeOfBlockToBeSaved;
        if (MMAppendBlockToBuffer(&FlushTMP))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Arcs count: number of the arcs of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // External arcs count: number of external arcs of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Rings count: number of rings of the polygon
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if (pBuffer)
                free_function(pBuffer);
            return 1;
        }

        // Offset: offset of the first vertex of the arc
        if (MMAppendIntegerDependingOnVersion(
                hMiraMonLayer, &FlushTMP, &nUL32,
                pMMPolygonLayer->pPolHeader[iElem].nOffset + nOffsetDiff))
        {
            if (pBuffer)
                free_function(pBuffer);
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
                free_function(pBuffer);
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
                free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved = 0;
    if (MMAppendBlockToBuffer(&FlushTMP))
    {
        if (pBuffer)
            free_function(pBuffer);
        return 1;
    }

    if (pBuffer)
        free_function(pBuffer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Feature Functions                                               */
/* -------------------------------------------------------------------- */
int MMInitFeature(struct MiraMonFeature *hMMFeature)
{
    memset(hMMFeature, 0, sizeof(*hMMFeature));

    hMMFeature->nMaxMRecords = MM_INIT_NUMBER_OF_RECORDS;
    if (MMCheckSize_t(
            hMMFeature->nMaxMRecords * sizeof(*(hMMFeature->pRecords)), 1))
        return 1;

    if (!hMMFeature->nMaxMRecords)
        return 0;  // No elements nothing to do.

    if ((hMMFeature->pRecords =
             calloc_function((size_t)hMMFeature->nMaxMRecords *
                             sizeof(*(hMMFeature->pRecords)))) == nullptr)
        return 1;

    hMMFeature->pRecords[0].nMaxField = MM_INIT_NUMBER_OF_FIELDS;
    hMMFeature->pRecords[0].nNumField = 0;
    if (MMCheckSize_t(hMMFeature->pRecords[0].nMaxField *
                          sizeof(*(hMMFeature->pRecords[0].pField)),
                      1))
        return 1;
    if (nullptr == (hMMFeature->pRecords[0].pField = calloc_function(
                        (size_t)hMMFeature->pRecords[0].nMaxField *
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
    }
}

// Destroys all allocated memory
void MMDestroyFeature(struct MiraMonFeature *hMMFeature)
{
    if (hMMFeature->pCoord)
    {
        free_function(hMMFeature->pCoord);
        hMMFeature->pCoord = nullptr;
    }
    if (hMMFeature->pZCoord)
    {
        free_function(hMMFeature->pZCoord);
        hMMFeature->pZCoord = nullptr;
    }
    if (hMMFeature->pNCoordRing)
    {
        free_function(hMMFeature->pNCoordRing);
        hMMFeature->pNCoordRing = nullptr;
    }

    if (hMMFeature->flag_VFG)
    {
        free_function(hMMFeature->flag_VFG);
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
                    free_function(hMMFeature->pRecords[nIRecord]
                                      .pField[nIField]
                                      .pDinValue);
            }
            free_function(hMMFeature->pRecords[nIRecord].pField);
        }
        free_function(hMMFeature->pRecords);
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

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature->pCoord)
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
                                     pNodeTopHeader->nElemCount + 2,
                                     MM_INCR_NUMBER_OF_POLYGONS, 0))
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
            MMCPLDebug("MiraMon", "Creating MiraMon database");
            if (MMCreateMMDB(hMiraMonLayer))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            MMCPLDebug("MiraMon", "MiraMon database created");
        }
    }
    else
    {  // Universal polygon has been created
        if (hMiraMonLayer->TopHeader.nElemCount == 1)
        {
            MMCPLDebug("MiraMon", "Creating MiraMon database");
            if (MMCreateMMDB(hMiraMonLayer))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            MMCPLDebug("MiraMon", "MiraMon database created");

            // Universal polygon have a record with ID_GRAFIC=0 and blancs
            if (MMAddPolygonRecordToMMDB(hMiraMonLayer, nullptr, 0, 0, nullptr))
                return MM_FATAL_ERROR_WRITING_FEATURES;
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
                MMCPLDebug("MiraMon", "Error in MMCheckVersionForFID() (1)");
                return MM_STOP_WRITING_FEATURES;
            }

            // Arc if there is no polygon
            if (MMCheckVersionForFID(hMiraMonLayer, nArcElemCount))
            {
                MMCPLDebug("MiraMon", "Error in MMCheckVersionForFID() (2)");
                return MM_STOP_WRITING_FEATURES;
            }

            // Nodes
            if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount))
            {
                MMCPLDebug("MiraMon", "Error in MMCheckVersionForFID() (3)");
                return MM_STOP_WRITING_FEATURES;
            }

            // There is space for the last node(s) that is(are) going to be written?
            if (!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount + 1))
                {
                    MMCPLDebug("MiraMon",
                               "Error in MMCheckVersionForFID() (4)");
                    return MM_STOP_WRITING_FEATURES;
                }
            }

            // Checking offsets
            // AL: check the last point
            if (MMCheckVersionOffset(hMiraMonLayer, nArcOffset))
            {
                MMCPLDebug("MiraMon", "Error in MMCheckVersionOffset() (0)");
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
                MMCPLDebug("MiraMon", "Error in MMCheckVersionOffset() (1)");
                return MM_STOP_WRITING_FEATURES;
            }
            // Setting next offset
            nNodeOffset += MM_SIZE_OF_NL_32BITS;

            if (!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
                {
                    MMCPLDebug("MiraMon",
                               "Error in MMCheckVersionOffset() (2)");
                    return MM_STOP_WRITING_FEATURES;
                }
                // Setting next offset
                nNodeOffset += MM_SIZE_OF_NL_32BITS;
            }

            // Where 3D part is going to start
            if (hMiraMonLayer->TopHeader.bIs3d)
            {
                nArcOffset +=
                    hMMFeature->pNCoordRing[nIPart] * pMMArc->nALElementSize;
                if (MMCheckVersionFor3DOffset(
                        hMiraMonLayer, nArcOffset,
                        hMiraMonLayer->TopHeader.nElemCount +
                            hMMFeature->nNRings))
                {
                    MMCPLDebug("MiraMon",
                               "Error in MMCheckVersionFor3DOffset()");
                    return MM_STOP_WRITING_FEATURES;
                }
            }
        }
    }

    // Going through parts of the feature.
    nExternalRingsCount = 0;
    pCoord = hMMFeature->pCoord;

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
            MMCPLDebug("MiraMon", "Error in MMResizeArcHeaderPointer()");
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
            MMCPLDebug("MiraMon", "Error in MMResizeNodeHeaderPointer()");
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
                MMCPLDebug("MiraMon",
                           "Error in MMResizeZSectionDescrPointer()");
                MMCPLError(CE_Failure, CPLE_OutOfMemory,
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

        // Initialiting feature information (section AH/PH)
        pCurrentArcHeader->nElemCount = hMMFeature->pNCoordRing[nIPart];
        pCurrentArcHeader->dfLenght = 0.0;
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
                MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer() (1)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            pFlushAL->pBlockToBeSaved = (void *)&(pCoord + nIVertice)->dfY;
            if (MMAppendBlockToBuffer(pFlushAL))
            {
                MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer() (2)");
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
                pCurrentArcHeader->dfLenght +=
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
            pCoord = pCoordReal + pCurrentArcHeader->nElemCount;
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
            MMCPLDebug("MiraMon", "Error in MMAddArcRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        // Node Stuff: writting NL section
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
            MMCPLDebug("MiraMon",
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
                MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer() (3)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        }
        if (MMAddNodeRecordToMMDB(hMiraMonLayer, pNodeTopHeader->nElemCount,
                                  pCurrentNodeHeader))
        {
            MMCPLDebug("MiraMon", "Error in MMAddNodeRecordToMMDB()");
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
                MMCPLDebug("MiraMon",
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
                    MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }
            }
            if (MMAddNodeRecordToMMDB(hMiraMonLayer,
                                      pNodeTopHeader->nElemCount + 1,
                                      pCurrentNodeHeaderPlus1))
            {
                MMCPLDebug("MiraMon", "Error in MMAddNodeRecordToMMDB()");
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
            for (nIVertice = 0; nIVertice < pCurrentArcHeader->nElemCount;
                 nIVertice++, pZ++)
            {
                pFlushZL->SizeOfBlockToBeSaved = sizeof(*pZ);
                pFlushZL->pBlockToBeSaved = (void *)pZ;
                if (MMAppendBlockToBuffer(pFlushZL))
                {
                    MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }

                if (pZDesc[pArcTopHeader->nElemCount].dfBBminz > *pZ)
                    pZDesc[pArcTopHeader->nElemCount].dfBBminz = *pZ;
                if (pZDesc[pArcTopHeader->nElemCount].dfBBmaxz < *pZ)
                    pZDesc[pArcTopHeader->nElemCount].dfBBmaxz = *pZ;
            }
            pZDesc[pArcTopHeader->nElemCount].nZCount = 1;
            if (hMiraMonLayer->TopHeader.nElemCount == 0)
                pZDesc[hMiraMonLayer->TopHeader.nElemCount].nOffsetZ = 0;
            else
                pZDesc[hMiraMonLayer->TopHeader.nElemCount].nOffsetZ =
                    pZDesc[hMiraMonLayer->TopHeader.nElemCount - 1].nOffsetZ +
                    sizeof(*pZ);
        }

        // Exclusive polygon stuff
        if (hMiraMonLayer->bIsPolygon && pCurrentPolHeader)
        {
            // PS SECTION
            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPS,
                                                  &UnsignedLongNumber, 0))
            {
                MMCPLDebug("MiraMon",
                           "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            if (MMAppendIntegerDependingOnVersion(
                    hMiraMonLayer, pFlushPS, &UnsignedLongNumber,
                    hMiraMonLayer->TopHeader.nElemCount))
            {
                MMCPLDebug("MiraMon",
                           "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // PAL SECTION
            // Vertices of rings defining
            // holes in polygons are in a counterclockwise direction.
            // Holes are at the end of all external rings that contain the holes!!
            if (VFG & MM_EXTERIOR_ARC_SIDE)
                nExternalRingsCount++;

            pCurrentPolHeader->nArcsCount =
                (MM_POLYGON_ARCS_COUNT)hMMFeature->nNRings;
            pCurrentPolHeader->nExternalRingsCount = nExternalRingsCount;
            pCurrentPolHeader->nRingsCount = hMMFeature->nNRings;
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
                MMCPLDebug("MiraMon", "Error in MMAppendBlockToBuffer()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPAL,
                                                  &UnsignedLongNumber,
                                                  pArcTopHeader->nElemCount))
            {
                MMCPLDebug("MiraMon",
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
                        MMCPLDebug("MiraMon",
                                   "Error in MMAppendBlockToBuffer()");
                        return MM_FATAL_ERROR_WRITING_FEATURES;
                    }
                }
            }

            MMUpdateBoundingBox(&pCurrentPolHeader->dfBB,
                                &pCurrentArcHeader->dfBB);
            pCurrentPolHeader->dfPerimeter += pCurrentArcHeader->dfLenght;
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
            MMCPLDebug("MiraMon", "Error in MMAddPolygonRecordToMMDB()");
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
        if (MMCreateMMDB(hMiraMonLayer))
            return MM_FATAL_ERROR_WRITING_FEATURES;
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
            return MM_STOP_WRITING_FEATURES;

        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if (nElemCount == 0)
            {
                if (MMCheckVersionFor3DOffset(hMiraMonLayer, 0, nElemCount + 1))
                    return MM_STOP_WRITING_FEATURES;
            }
            else
            {
                pZDescription = hMiraMonLayer->MMPoint.pZSection.pZDescription;
                if (MMCheckVersionFor3DOffset(
                        hMiraMonLayer,
                        pZDescription[nElemCount - 1].nOffsetZ + sizeof(*pZ),
                        nElemCount + 1))
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
                MMCPLError(CE_Failure, CPLE_OutOfMemory,
                           "Memory error in MiraMon "
                           "driver (MMCreateFeaturePoint())");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            pZDescription = hMiraMonLayer->MMPoint.pZSection.pZDescription;

            pZDescription[nElemCount].dfBBminz = *pZ;
            pZDescription[nElemCount].dfBBmaxz = *pZ;
            pZDescription[nElemCount].nZCount = 1;
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
        for (nIVertice = 0; nIVertice < nCoord; nIVertice++, pCoord++, pZ++)
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

                if (pZDescription[nElemCount].dfBBminz > *pZ)
                    pZDescription[nElemCount].dfBBminz = *pZ;
                if (pZDescription[nElemCount].dfBBmaxz < *pZ)
                    pZDescription[nElemCount].dfBBmaxz = *pZ;

                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz > *pZ)
                    hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz = *pZ;
                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz < *pZ)
                    hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz = *pZ;
            }
        }

        if (hMiraMonLayer->TopHeader.nElemCount == 0)
        {
            if (MMCreateMMDB(hMiraMonLayer))
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
                              MM_FILE_OFFSET nOffset,
                              MM_INTERNAL_FID nElemCount)
{
    MM_FILE_OFFSET LastOffset;

    if (!hMiraMonLayer)
        return 1;

    // Checking if the final version is 1.1 or 2.0
    if (hMiraMonLayer->LayerVersion != MM_32BITS_VERSION)
        return 0;

    // User decided that if necessary, output version can be 2.0
    LastOffset = nOffset + MM_HEADER_SIZE_32_BITS + nElemCount * MM_SIZE_OF_TL;

    LastOffset += MM_SIZE_OF_ZH;
    LastOffset += nElemCount * MM_SIZE_OF_ZD_32_BITS;

    if (LastOffset < MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;

    return 1;
}

// Adds a feature in a MiraMon layer.
int MMAddFeature(struct MiraMonVectLayerInfo *hMiraMonLayer,
                 struct MiraMonFeature *hMiraMonFeature)
{
    int re;
    MM_INTERNAL_FID previusFID = 0;

    if (!hMiraMonLayer)
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMiraMonLayer->bIsBeenInit)
    {
        if (MMInitLayerByType(hMiraMonLayer))
        {
            MMCPLDebug("MiraMon", "Error in MMInitLayerByType()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        hMiraMonLayer->bIsBeenInit = 1;
    }

    if (hMiraMonFeature)
        previusFID = hMiraMonLayer->TopHeader.nElemCount;

    if (hMiraMonLayer->bIsPoint)
    {
        re = LOG_ACTION(MMCreateFeaturePoint(hMiraMonLayer, hMiraMonFeature));
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previusFID;
        }
        return re;
    }
    if (hMiraMonLayer->bIsArc || hMiraMonLayer->bIsPolygon)
    {
        re =
            LOG_ACTION(MMCreateFeaturePolOrArc(hMiraMonLayer, hMiraMonFeature));
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previusFID;
        }
        return re;
    }
    if (hMiraMonLayer->bIsDBF)
    {
        // Adding a record to DBF file
        re = LOG_ACTION(MMCreateRecordDBF(hMiraMonLayer, hMiraMonFeature));
        if (hMiraMonFeature)
        {
            hMiraMonFeature->nReadFeatures =
                hMiraMonLayer->TopHeader.nElemCount - previusFID;
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

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pFieldValue) = realloc_function(
             *pFieldValue, (size_t)*nMax * sizeof(**pFieldValue))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeMiraMonFieldValue())");
        return 1;
    }

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

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pFID) = realloc_function(*pFID, (size_t)*nMax * sizeof(**pFID))) ==
        nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeMiraMonPolygonArcs())");
        return 1;
    }

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

    if (nNum < *nMax)
        return 0;

    nPrevMax = *nMax;
    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pMiraMonRecord) = realloc_function(
             *pMiraMonRecord, (size_t)*nMax * sizeof(**pMiraMonRecord))) ==
        nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeMiraMonRecord())");
        return 1;
    }

    memset((*pMiraMonRecord) + nPrevMax, 0,
           (size_t)(*nMax - nPrevMax) * sizeof(**pMiraMonRecord));
    return 0;
}

int MMResizeZSectionDescrPointer(struct MM_ZD **pZDescription, GUInt64 *nMax,
                                 GUInt64 nNum, GUInt64 nIncr,
                                 GUInt64 nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pZDescription) = realloc_function(
             *pZDescription, (size_t)*nMax * sizeof(**pZDescription))) ==
        nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeZSectionDescrPointer())");
        return 1;
    }
    return 0;
}

int MMResizeNodeHeaderPointer(struct MM_NH **pNodeHeader, GUInt64 *nMax,
                              GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pNodeHeader) = realloc_function(
             *pNodeHeader, (size_t)*nMax * sizeof(**pNodeHeader))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeNodeHeaderPointer())");
        return 1;
    }
    return 0;
}

int MMResizeArcHeaderPointer(struct MM_AH **pArcHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pArcHeader) = realloc_function(
             *pArcHeader, (size_t)*nMax * sizeof(**pArcHeader))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeArcHeaderPointer())");
        return 1;
    }
    return 0;
}

int MMResizePolHeaderPointer(struct MM_PH **pPolHeader, GUInt64 *nMax,
                             GUInt64 nNum, GUInt64 nIncr, GUInt64 nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pPolHeader) = realloc_function(
             *pPolHeader, (size_t)*nMax * sizeof(**pPolHeader))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizePolHeaderPointer())");
        return 1;
    }
    return 0;
}

int MMResize_MM_N_VERTICES_TYPE_Pointer(MM_N_VERTICES_TYPE **pVrt,
                                        MM_N_VERTICES_TYPE *nMax,
                                        MM_N_VERTICES_TYPE nNum,
                                        MM_N_VERTICES_TYPE nIncr,
                                        MM_N_VERTICES_TYPE nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pVrt) = realloc_function(*pVrt, (size_t)*nMax * sizeof(**pVrt))) ==
        nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResize_MM_N_VERTICES_TYPE_Pointer())");
        return 1;
    }
    return 0;
}

int MMResizeVFGPointer(char **pInt, MM_INTERNAL_FID *nMax, MM_INTERNAL_FID nNum,
                       MM_INTERNAL_FID nIncr, MM_INTERNAL_FID nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pInt) = realloc_function(*pInt, (size_t)*nMax * sizeof(**pInt))) ==
        nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeVFGPointer())");
        return 1;
    }
    return 0;
}

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D,
                              MM_N_VERTICES_TYPE *nMax, MM_N_VERTICES_TYPE nNum,
                              MM_N_VERTICES_TYPE nIncr,
                              MM_N_VERTICES_TYPE nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pPoint2D) = realloc_function(
             *pPoint2D, (size_t)*nMax * sizeof(**pPoint2D))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeMM_POINT2DPointer())");
        return 1;
    }
    return 0;
}

int MMResizeDoublePointer(MM_COORD_TYPE **pDouble, MM_N_VERTICES_TYPE *nMax,
                          MM_N_VERTICES_TYPE nNum, MM_N_VERTICES_TYPE nIncr,
                          MM_N_VERTICES_TYPE nProposedMax)
{
    if (nNum < *nMax)
        return 0;

    *nMax = max_function(nNum + nIncr, nProposedMax);
    if (((*pDouble) = realloc_function(
             *pDouble, (size_t)*nMax * sizeof(**pDouble))) == nullptr)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMResizeDoublePointer())");
        return 1;
    }
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
        p = (char *)calloc_function((size_t)nNewSize);
        if (!p)
        {
            MMCPLError(CE_Failure, CPLE_OutOfMemory,
                       "Memory error in MiraMon "
                       "driver (MMResizeStringToOperateIfNeeded())");
            return 1;
        }
        hMiraMonLayer->szStringToOperate = p;
        hMiraMonLayer->nNumStringToOperate = nNewSize;
    }
    return 0;
}

// Checks if a string is empty
int MMIsEmptyString(const char *string)
{
    char *ptr;

    for (ptr = (char *)string; *ptr; ptr++)
        if (*ptr != ' ' && *ptr != '\t')
            return 0;

    return 1;
}

/* -------------------------------------------------------------------- */
/*      Metadata Functions                                              */
/* -------------------------------------------------------------------- */

// Returns the value of an INI file. Used to read MiraMon metadata
char *MMReturnValueFromSectionINIFile(const char *filename, const char *section,
                                      const char *key)
{
    char *value = nullptr;
    char line[16000];
    char *section_header = nullptr;
    size_t key_len = 0;
    size_t bytes_read;
    char *pszString;
    char *start;
    char *end;

    FILE_TYPE *file = fopen_function(filename, "rb");
    if (file == nullptr)
    {
        sprintf(local_message, "Cannot open INI file %s.", filename);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return nullptr;
    }

    if (key)
        key_len = strlen(key);

    while ((bytes_read = fread_function(line, 1, sizeof(line), file)) > 0)
    {
        // As MiraMon is written in ANSI convertion to UTF-8 must be done
        line[bytes_read - 1] = '\0';
        pszString = CPLRecode_function(line, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
        MM_strnzcpy(line, pszString, 16000);
        CPLFree_function(pszString);

        // Iterate through the read buffer
        start = line;
        end = line + bytes_read;
        while (start < end)
        {
            if (!start)
                return nullptr;

            // Skip comments and empty lines
            if (*start == ';' || *start == '#' || *start == '\n' ||
                *start == '\r')
            {
                while (start < end && *start != '\n')
                    start++;
                start++;  // Move to next line
                continue;
            }

            if (!start)
                return nullptr;

            // Check for section header
            if (*start == '[')
            {
                char *section_end = strchr(start, ']');
                if (section_end != nullptr)
                {
                    *section_end = '\0';  // Terminate the string at ']'
                    if (section_header)
                        free_function(section_header);
                    section_header =
                        strdup_function(start + 1);  // Skip the '['
                }
                start = section_end + 1;
                continue;
            }

            if (!start)
                return nullptr;

            if (key)
            {
                // If the current line belongs to the desired section
                if (section_header != nullptr &&
                    strcmp(section_header, section) == 0)
                {
                    // Check if the line contains the desired key
                    if (strncmp(start, key, key_len) == 0 &&
                        start[key_len] == '=')
                    {
                        // Extract the value
                        char *value_start = start + key_len + 1;
                        char *value_end = strstr(value_start, "\r\n");
                        if (value_end != nullptr)
                        {
                            *value_end =
                                '\0';  // Terminate the string at newline character if found
                        }
                        else
                        {
                            value_end = strstr(value_start, "\n");
                            if (value_end != nullptr)
                            {
                                *value_end =
                                    '\0';  // Terminate the string at newline character if found
                            }
                            else
                            {
                                value_end = strstr(value_start, "\r");
                                if (value_end != nullptr)
                                {
                                    *value_end =
                                        '\0';  // Terminate the string at newline character if found
                                }
                            }
                        }

                        value = strdup_function(value_start);
                        fclose_function(file);
                        free_function(section_header);  // Free allocated memory
                        return value;
                    }
                }
            }
            else
            {
                value = section_header;  // Freed out
                fclose_function(file);
                return value;
            }

            // Move to the next line
            while (start < end && *start != '\n')
                start++;
            start++;
        }
    }

    if (section_header)
        free_function(section_header);  // Free allocated memory
    fclose_function(file);
    return value;
}

// Retrieves EPSG codes from a CSV file based on provided geodetic identifiers.
int MMReturnCodeFromMM_m_idofic(char *pMMSRS_or_pSRS, char *szResult,
                                MM_BYTE direction)
{
    static char aEPSGCodeSRS[MM_MAX_ID_SNY];
    char *aMMIDDBFFile = nullptr;  //m_idofic.dbf
    FILE *pfMMSRS;
    size_t nLong;
    int nLongBuffer = 5000;
    char *pszBuffer = calloc_function(nLongBuffer);
    char *id_geodes, *psidgeodes, *epsg;

    if (!pszBuffer)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                   "Memory error in MiraMon "
                   "driver (MMReturnCodeFromMM_m_idofic())");
        return 1;
    }
    if (!pMMSRS_or_pSRS)
    {
        free_function(pszBuffer);
        return 1;
    }
    memset(aEPSGCodeSRS, '\0', sizeof(*aEPSGCodeSRS));

#ifdef GDAL_COMPILATION
    aMMIDDBFFile = strdup_function(CPLFindFile("gdal", "MM_m_idofic.csv"));
#else
    {
        char temp_file[MM_CPL_PATH_BUF_SIZE];
        MuntaPath(DirectoriPrograma, strcpy(temp_file, "m_idofic.csv"), TRUE);
        aMMIDDBFFile = strdup_function(temp_file);
    }
#endif

    if (!aMMIDDBFFile)
    {
        free_function(pszBuffer);
        MMCPLError(CE_Failure, CPLE_OpenFailed,
                   "Error opening data\\MM_m_idofic.csv.\n");
        return 1;
    }

    // Opening the file with SRS information
    if (nullptr == (pfMMSRS = fopen(aMMIDDBFFile, "r")))
    {
        free_function(aMMIDDBFFile);
        free_function(pszBuffer);
        MMCPLError(CE_Failure, CPLE_OpenFailed,
                   "Error opening data\\MM_m_idofic.csv.\n");
        return 1;
    }
    free_function(aMMIDDBFFile);

    // Checking the header of the csv file
    memset(pszBuffer, 0, nLongBuffer);
    if (!fgets(pszBuffer, nLongBuffer, pfMMSRS))
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        MMCPLError(CE_Failure, CPLE_NotSupported,
                   "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    id_geodes = strstr(pszBuffer, "ID_GEODES");
    if (!id_geodes)
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        MMCPLError(CE_Failure, CPLE_NotSupported,
                   "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    id_geodes[strlen("ID_GEODES")] = '\0';
    psidgeodes = strstr(pszBuffer, "PSIDGEODES");
    if (!psidgeodes)
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        MMCPLError(CE_Failure, CPLE_NotSupported,
                   "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    psidgeodes[strlen("PSIDGEODES")] = '\0';

    // Is PSIDGEODES in first place?
    if (strncmp(pszBuffer, psidgeodes, strlen("PSIDGEODES")))
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        MMCPLError(CE_Failure, CPLE_NotSupported,
                   "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }
    // Is ID_GEODES after PSIDGEODES?
    if (strncmp(pszBuffer + strlen("PSIDGEODES") + 1, "ID_GEODES",
                strlen("ID_GEODES")))
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        MMCPLError(CE_Failure, CPLE_NotSupported,
                   "Wrong format in data\\MM_m_idofic.csv.\n");
        return 1;
    }

    // Looking for the information
    while (fgets(pszBuffer, nLongBuffer, pfMMSRS))
    {
        id_geodes = strstr(pszBuffer, ";");
        if (!id_geodes || (id_geodes + 1)[0] == '\n')
        {
            free_function(pszBuffer);
            fclose(pfMMSRS);
            MMCPLError(CE_Failure, CPLE_NotSupported,
                       "Wrong format in data\\MM_m_idofic.csv.\n");
            return 1;
        }

        psidgeodes = strstr(id_geodes + 1, ";");
        if (!psidgeodes)
        {
            free_function(pszBuffer);
            fclose(pfMMSRS);
            MMCPLError(CE_Failure, CPLE_NotSupported,
                       "Wrong format in data\\MM_m_idofic.csv.\n");
            return 1;
        }

        id_geodes[(ptrdiff_t)psidgeodes - (ptrdiff_t)id_geodes] = '\0';
        psidgeodes = pszBuffer;
        psidgeodes[(ptrdiff_t)id_geodes - (ptrdiff_t)psidgeodes] = '\0';
        id_geodes++;

        if (direction == EPSG_FROM_MMSRS)
        {
            // I have pMMSRS and I want pSRS
            if (strcmp(pMMSRS_or_pSRS, id_geodes))
                continue;

            epsg = strstr(psidgeodes, "EPSG:");
            nLong = strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg[nLong] != '\0')
                {
                    strcpy(szResult, epsg + nLong);
                    free_function(pszBuffer);
                    fclose(pfMMSRS);
                    return 0;  // found
                }
                else
                {
                    free_function(pszBuffer);
                    fclose(pfMMSRS);
                    *szResult = '\0';
                    return 1;  // not found
                }
            }
        }
        else
        {
            // I have pSRS and I want pMMSRS
            epsg = strstr(psidgeodes, "EPSG:");
            nLong = strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg[nLong] != '\0')
                {
                    if (!strcmp(pMMSRS_or_pSRS, epsg + nLong))
                    {
                        strcpy(szResult, id_geodes);
                        free_function(pszBuffer);
                        fclose(pfMMSRS);
                        return 0;  // found
                    }
                }
            }
        }
    }

    free_function(pszBuffer);
    fclose(pfMMSRS);
    return 1;  // not found
}

#define LineReturn "\r\n"

// Generates an idientifier that REL 4 MiraMon metadata needs.
static char *MMGenerateFileIdentifierFromMetadataFileName(char *pMMFN)
{
    static char aCharRand[8],
        aCharset[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i, len_charset;
    static char aFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER];

    memset(aFileIdentifier, '\0', MM_MAX_LEN_LAYER_IDENTIFIER);

    aCharRand[0] = '_';
    len_charset = (int)strlen(aCharset);
    for (i = 1; i < 7; i++)
        aCharRand[i] = aCharset[rand() % (len_charset - 1)];
    aCharRand[7] = '\0';
    MM_strnzcpy(aFileIdentifier, pMMFN, MM_MAX_LEN_LAYER_IDENTIFIER - 7);
    strcat(aFileIdentifier, aCharRand);
    return aFileIdentifier;
}

// Converts a string from UTF-8 to ANSI to be written in a REL 4 file
static void
MMWrite_ANSI_MetadataKeyDescriptor(struct MiraMonVectorMetaData *hMMMD,
                                   FILE_TYPE *pF, const char *pszEng,
                                   const char *pszCat, const char *pszEsp)
{
    char *pszString = nullptr;

    switch (hMMMD->nMMLanguage)
    {
        case MM_CAT_LANGUAGE:
            pszString =
                CPLRecode_function(pszCat, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
        case MM_SPA_LANGUAGE:
            pszString =
                CPLRecode_function(pszEsp, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
        default:
        case MM_ENG_LANGUAGE:
            pszString =
                CPLRecode_function(pszEng, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            break;
    }
    if (pszString)
    {
        fprintf_function(pF, "%s", KEY_descriptor);
        fprintf_function(pF, "=");
        fprintf_function(pF, "%s", pszString);
        fprintf_function(pF, "%s", LineReturn);
        CPLFree_function(pszString);
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
    char aMessage[MM_MESSAGE_LENGHT],
        aFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER], aMMIDSRS[MM_MAX_ID_SNY];
    MM_EXT_DBF_N_FIELDS nIField;
    FILE_TYPE *pF;
    time_t currentTime;
    struct tm *pLocalTime;
    char aTimeString[200];

    if (!hMMMD->aLayerName)
        return 0;

    if (nullptr == (pF = fopen_function(hMMMD->aLayerName, "wb")))
    {
        sprintf(local_message, "The file %s must exist.", hMMMD->aLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    // Writing MiraMon version section
    fprintf_function(pF, "[%s]" LineReturn, SECTION_VERSIO);

    fprintf_function(pF, "%s=%u" LineReturn, KEY_Vers, (unsigned)MM_VERS);
    fprintf_function(pF, "%s=%u" LineReturn, KEY_SubVers, (unsigned)MM_SUBVERS);

    fprintf_function(pF, "%s=%u" LineReturn, KEY_VersMetaDades,
                     (unsigned)MM_VERS_METADADES);
    fprintf_function(pF, "%s=%u" LineReturn, KEY_SubVersMetaDades,
                     (unsigned)MM_SUBVERS_METADADES);

    // Writing METADADES section
    fprintf_function(pF, "\r\n[%s]" LineReturn, SECTION_METADADES);
    strcpy(aMessage, hMMMD->aLayerName);
    strcpy(aFileIdentifier,
           MMGenerateFileIdentifierFromMetadataFileName(aMessage));
    fprintf_function(pF, "%s=%s" LineReturn, KEY_FileIdentifier,
                     aFileIdentifier);
    fprintf_function(pF, "%s=%s" LineReturn, KEY_language, KEY_Value_eng);
    fprintf_function(pF, "%s=%s" LineReturn, KEY_MDIdiom, KEY_Value_eng);
    fprintf_function(pF, "%s=%s" LineReturn, KEY_characterSet,
                     KEY_Value_characterSet);

    // Writing IDENTIFICATION section
    fprintf_function(pF, LineReturn "[%s]" LineReturn, SECTION_IDENTIFICATION);
    fprintf_function(pF, "%s=%s" LineReturn, KEY_code, aFileIdentifier);
    fprintf_function(pF, "%s=" LineReturn, KEY_codeSpace);
    if (hMMMD->szLayerTitle && !MMIsEmptyString(hMMMD->szLayerTitle))
    {
        if (hMMMD->ePlainLT == MM_LayerType_Point)
            fprintf_function(pF, "%s=%s (pnt)" LineReturn, KEY_DatasetTitle,
                             hMMMD->szLayerTitle);
        if (hMMMD->ePlainLT == MM_LayerType_Arc)
            fprintf_function(pF, "%s=%s (arc)" LineReturn, KEY_DatasetTitle,
                             hMMMD->szLayerTitle);
        if (hMMMD->ePlainLT == MM_LayerType_Pol)
            fprintf_function(pF, "%s=%s (pol)" LineReturn, KEY_DatasetTitle,
                             hMMMD->szLayerTitle);
    }
    fprintf_function(pF, "%s=%s" LineReturn, KEY_language, KEY_Value_eng);

    if (hMMMD->ePlainLT != MM_LayerType_Node)
    {
        if (hMMMD->pSRS && hMMMD->ePlainLT != MM_LayerType_Pol)
        {
            fprintf_function(pF, LineReturn "[%s:%s]" LineReturn,
                             SECTION_SPATIAL_REFERENCE_SYSTEM,
                             SECTION_HORIZONTAL);
            if (!ReturnMMIDSRSFromEPSGCodeSRS(hMMMD->pSRS, aMMIDSRS) &&
                !MMIsEmptyString(aMMIDSRS))
                fprintf_function(pF, "%s=%s" LineReturn,
                                 KEY_HorizontalSystemIdentifier, aMMIDSRS);
            else
            {
                MMCPLWarning(CE_Warning, CPLE_NotSupported,
                             "The MiraMon driver cannot assign any HRS.");
                // Horizontal Reference System
                fprintf_function(pF, "%s=plane" LineReturn,
                                 KEY_HorizontalSystemIdentifier);
                fprintf_function(pF, "%s=local" LineReturn,
                                 KEY_HorizontalSystemDefinition);
                if (hMMMD->pXUnit)
                    fprintf_function(pF, "%s=%s" LineReturn, KEY_unitats,
                                     hMMMD->pXUnit);
                if (hMMMD->pYUnit)
                {
                    if (!hMMMD->pXUnit ||
                        strcasecmp(hMMMD->pXUnit, hMMMD->pYUnit))
                        fprintf_function(pF, "%s=%s" LineReturn, KEY_unitatsY,
                                         hMMMD->pYUnit);
                }
            }
        }
        else
        {
            fprintf_function(pF, "%s=plane" LineReturn,
                             KEY_HorizontalSystemIdentifier);
            fprintf_function(pF, "%s=local" LineReturn,
                             KEY_HorizontalSystemDefinition);
            if (hMMMD->pXUnit)
            {
                fprintf_function(pF, "%s=%s" LineReturn, KEY_unitats,
                                 hMMMD->pXUnit);
                if (hMMMD->pYUnit)
                {
                    if (!hMMMD->pXUnit ||
                        strcasecmp(hMMMD->pXUnit, hMMMD->pYUnit))
                        fprintf_function(pF, "%s=%s" LineReturn, KEY_unitatsY,
                                         hMMMD->pYUnit);
                }
            }
        }
    }

    // Writing OVERVIEW:ASPECTES_TECNICS in polygon metadata file.
    // ArcSource=fitx_pol.arc
    if (hMMMD->ePlainLT == MM_LayerType_Pol)
    {
        fprintf_function(pF, LineReturn "[%s]" LineReturn,
                         SECTION_OVVW_ASPECTES_TECNICS);
        fprintf_function(pF, "%s=\"%s\"" LineReturn, KEY_ArcSource,
                         hMMMD->aArcFile);
    }

    // Writing EXTENT section
    fprintf_function(pF, LineReturn "[%s]" LineReturn, SECTION_EXTENT);
    fprintf_function(pF, "%s=0" LineReturn, KEY_toler_env);

    if (hMMMD->hBB.dfMinX != MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxX != -MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMinY != MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxY != -MM_UNDEFINED_STATISTICAL_VALUE)
    {
        fprintf_function(pF, "%s=%lf" LineReturn, KEY_MinX, hMMMD->hBB.dfMinX);
        fprintf_function(pF, "%s=%lf" LineReturn, KEY_MaxX, hMMMD->hBB.dfMaxX);
        fprintf_function(pF, "%s=%lf" LineReturn, KEY_MinY, hMMMD->hBB.dfMinY);
        fprintf_function(pF, "%s=%lf" LineReturn, KEY_MaxY, hMMMD->hBB.dfMaxY);
    }

    // Writing OVERVIEW section
    fprintf_function(pF, LineReturn "[%s]" LineReturn, SECTION_OVERVIEW);

    currentTime = time(nullptr);
    pLocalTime = localtime(&currentTime);
    snprintf(aTimeString, sizeof(aTimeString),
             "%04d%02d%02d %02d%02d%02d%02d+00:00", pLocalTime->tm_year + 1900,
             pLocalTime->tm_mon + 1, pLocalTime->tm_mday, pLocalTime->tm_hour,
             pLocalTime->tm_min, pLocalTime->tm_sec, 0);
    fprintf_function(pF, "%s=%s" LineReturn, KEY_CreationDate, aTimeString);

    fprintf_function(pF, LineReturn);

    // Writing TAULA_PRINCIPAL section
    fprintf_function(pF, "[%s]" LineReturn, SECTION_TAULA_PRINCIPAL);
    fprintf_function(pF, "IdGrafic=%s" LineReturn, szMMNomCampIdGraficDefecte);
    fprintf_function(pF, "TipusRelacio=RELACIO_1_1_DICC" LineReturn);

    fprintf_function(pF, LineReturn);
    fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                     szMMNomCampIdGraficDefecte);
    fprintf_function(pF, "visible=1" LineReturn);
    fprintf_function(pF, "MostrarUnitats=0" LineReturn);

    MMWrite_ANSI_MetadataKeyDescriptor(
        hMMMD, pF, szInternalGraphicIdentifierEng,
        szInternalGraphicIdentifierCat, szInternalGraphicIdentifierEsp);

    if (hMMMD->ePlainLT == MM_LayerType_Arc)
    {
        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNVertexsDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfVerticesEng,
                                           szNumberOfVerticesCat,
                                           szNumberOfVerticesEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampLongitudArcDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szLenghtOfAarcEng, szLenghtOfAarcCat, szLenghtOfAarcEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNodeIniDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szInitialNodeEng,
                                           szInitialNodeCat, szInitialNodeEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNodeFiDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szFinalNodeEng,
                                           szFinalNodeCat, szFinalNodeEsp);

        fprintf_function(pF, "[GEOMETRIA_I_TOPOLOGIA]" LineReturn);
        fprintf_function(pF, "NomCampNVertexs=%s" LineReturn,
                         szMMNomCampNVertexsDefecte);
        fprintf_function(pF, "NomCampLongitudArc=%s" LineReturn,
                         szMMNomCampLongitudArcDefecte);
        fprintf_function(pF, "NomCampNodeIni=%s" LineReturn,
                         szMMNomCampNodeIniDefecte);
        fprintf_function(pF, "NomCampNodeFi=%s" LineReturn,
                         szMMNomCampNodeFiDefecte);
    }
    else if (hMMMD->ePlainLT == MM_LayerType_Node)
    {
        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampArcsANodeDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfArcsToNodeEng,
                                           szNumberOfArcsToNodeCat,
                                           szNumberOfArcsToNodeEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampTipusNodeDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNodeTypeEng,
                                           szNodeTypeCat, szNodeTypeEsp);
    }
    else if (hMMMD->ePlainLT == MM_LayerType_Pol)
    {
        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNVertexsDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szNumberOfVerticesEng,
                                           szNumberOfVerticesCat,
                                           szNumberOfVerticesEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampPerimetreDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szPerimeterOfThePolygonEng, szPerimeterOfThePolygonCat,
            szPerimeterOfThePolygonEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampAreaDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(hMMMD, pF, szAreaOfThePolygonEng,
                                           szAreaOfThePolygonCat,
                                           szAreaOfThePolygonEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNArcsDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szNumberOfArcsEng, szNumberOfArcsCat, szNumberOfArcsEsp);

        fprintf_function(pF, LineReturn);
        fprintf_function(pF, "[%s:%s]" LineReturn, SECTION_TAULA_PRINCIPAL,
                         szMMNomCampNPoligonsDefecte);
        fprintf_function(pF, "visible=0" LineReturn);
        fprintf_function(pF, "simbolitzable=0" LineReturn);
        fprintf_function(pF, "MostrarUnitats=0" LineReturn);
        MMWrite_ANSI_MetadataKeyDescriptor(
            hMMMD, pF, szNumberOfElementaryPolygonsEng,
            szNumberOfElementaryPolygonsCat, szNumberOfElementaryPolygonsEsp);

        fprintf_function(pF, "[GEOMETRIA_I_TOPOLOGIA]" LineReturn);
        fprintf_function(pF, "NomCampNVertexs=%s" LineReturn,
                         szMMNomCampNVertexsDefecte);
        fprintf_function(pF, "NomCampPerimetre=%s" LineReturn,
                         szMMNomCampPerimetreDefecte);
        fprintf_function(pF, "NomCampArea=%s" LineReturn,
                         szMMNomCampAreaDefecte);
        fprintf_function(pF, "NomCampNArcs=%s" LineReturn,
                         szMMNomCampNArcsDefecte);
        fprintf_function(pF, "NomCampNPoligons=%s" LineReturn,
                         szMMNomCampNPoligonsDefecte);
    }

    if (hMMMD->pLayerDB && hMMMD->pLayerDB->nNFields > 0)
    {
        // For each field of the databes
        for (nIField = 0; nIField < hMMMD->pLayerDB->nNFields; nIField++)
        {
            if (!MMIsEmptyString(
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription) &&
                !MMIsEmptyString(
                    hMMMD->pLayerDB->pFields[nIField].pszFieldName))
            {
                fprintf_function(
                    pF, LineReturn "[%s:%s]" LineReturn,
                    SECTION_TAULA_PRINCIPAL,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldName);

                MMWrite_ANSI_MetadataKeyDescriptor(
                    hMMMD, pF,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription,
                    hMMMD->pLayerDB->pFields[nIField].pszFieldDescription);
            }
        }
    }
    fclose_function(pF);
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
        }
        return MMWriteMetadataFile(&hMMMD);
    }
    else if (layerPlainType == MM_LayerType_Pol)
    {
        int nResult;

        hMMMD.aLayerName = hMiraMonLayer->MMPolygon.pszREL_LayerName;

        if (MMIsEmptyString(hMMMD.aLayerName))
            return 0;  // If no file, no error. Just continue.

        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB = hMiraMonLayer->pLayerDB;
        hMMMD.aArcFile = strdup_function(
            get_filename_function(hMiraMonLayer->MMPolygon.MMArc.pszLayerName));
        nResult = MMWriteMetadataFile(&hMMMD);
        free_function(hMMMD.aArcFile);
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

// Verifies the version of a MiraMon REL 4 file.
int MMCheck_REL_FILE(char *szREL_file)
{
    char *pszLine;
    FILE_TYPE *pF;

    // Does the REL file exist?
    pF = fopen_function(szREL_file, "r");
    if (!pF)
    {
        sprintf(local_message, "The file %s must exist.", szREL_file);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }
    fclose_function(pF);

    // Does the REL file have VERSION?
    pszLine =
        MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, nullptr);
    if (!pszLine)
    {
        sprintf(local_message,
                "The file \"%s\" must be REL4. "
                "You can use ConvREL.exe from MiraMon software "
                "to convert this file to REL4.",
                szREL_file);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }
    free_function(pszLine);

    // Does the REL file have the correct VERSION?
    // Vers>=4?
    pszLine =
        MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_Vers);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_VERS)
        {
            sprintf(local_message, "The file \"%s\" must have %s>=%d.",
                    szREL_file, KEY_Vers, MM_VERS);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        free_function(pszLine);
    }
    else
    {
        sprintf(local_message, "The file \"%s\" must have %s>=%d.", szREL_file,
                KEY_Vers, MM_VERS);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    // SubVers>=3?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_SubVers);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_SUBVERS)
        {
            sprintf(local_message, "The file \"%s\" must have %s>=%d.",
                    szREL_file, KEY_SubVers, MM_SUBVERS);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        free_function(pszLine);
    }
    else
    {
        sprintf(local_message, "The file \"%s\" must have %s>=%d.", szREL_file,
                KEY_SubVers, MM_SUBVERS);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    // VersMetaDades>=5?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_VersMetaDades);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_VERS_METADADES)
        {
            sprintf(local_message, "The file \"%s\" must have %s>=%d.",
                    szREL_file, KEY_VersMetaDades, MM_VERS_METADADES);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
            return 1;
        }
        free_function(pszLine);
    }
    else
    {
        sprintf(local_message, "The file \"%s\" must have %s>=%d.", szREL_file,
                KEY_VersMetaDades, MM_VERS_METADADES);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }

    // SubVersMetaDades>=0?
    pszLine = MMReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO,
                                              KEY_SubVersMetaDades);
    if (pszLine)
    {
        if (*pszLine == '\0' || atoi(pszLine) < (int)MM_SUBVERS_METADADES)
        {
            sprintf(local_message, "The file \"%s\" must have %s>=%d.",
                    szREL_file, KEY_SubVersMetaDades, MM_SUBVERS_METADADES);
            MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);

            return 1;
        }
        free_function(pszLine);
    }
    else
    {
        sprintf(local_message, "The file \"%s\" must have %s>=%d.", szREL_file,
                KEY_SubVersMetaDades, MM_SUBVERS_METADADES);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
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
    if (!hMiraMonLayer)
        return 1;

    if (!pMMAdmDB)
        return 1;

    if (MMIsEmptyString(pMMAdmDB->pszExtDBFLayerName))
        return 0;  // No file, no error. Just continue

    strcpy(pMMAdmDB->pMMBDXP->ReadingMode, "wb+");
    if (FALSE ==
        MM_CreateDBFFile(pMMAdmDB->pMMBDXP, pMMAdmDB->pszExtDBFLayerName))
        return 1;

    // Opening the file
    if (nullptr == (pMMAdmDB->pFExtDBF =
                        fopen_function(pMMAdmDB->pszExtDBFLayerName,
                                       "r+b")))  //hMiraMonLayer->pszFlags)))
    {
        sprintf(local_message, "Error pMMAdmDB: Cannot open file %s.",
                pMMAdmDB->pszExtDBFLayerName);
        MMCPLError(CE_Failure, CPLE_OpenFailed, local_message);
        return 1;
    }
    fseek_function(pMMAdmDB->pFExtDBF, pMMAdmDB->pMMBDXP->FirstRecordOffset,
                   SEEK_SET);

    if (MMInitFlush(&pMMAdmDB->FlushRecList, pMMAdmDB->pFExtDBF,
                    (hMiraMonLayer->nMemoryRatio != 1)
                        ? (GUInt64)(hMiraMonLayer->nMemoryRatio * MM_25MB)
                        : MM_25MB,
                    &pMMAdmDB->pRecList, pMMAdmDB->pMMBDXP->FirstRecordOffset,
                    0))
        return 1;

    pMMAdmDB->nNumRecordOnCourse =
        (GUInt64)pMMAdmDB->pMMBDXP->BytesPerRecord + 1;
    if (MMCheckSize_t(pMMAdmDB->nNumRecordOnCourse, 1))
        return 1;
    pMMAdmDB->szRecordOnCourse =
        calloc_function((size_t)pMMAdmDB->nNumRecordOnCourse);
    if (!pMMAdmDB->szRecordOnCourse)
    {
        MMCPLError(CE_Failure, CPLE_OutOfMemory,
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
int MMCreateMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr, *pBD_XP_Aux = nullptr;
    struct MM_FIELD MMField;
    size_t nIFieldLayer;
    MM_EXT_DBF_N_FIELDS nIField = 0;
    MM_EXT_DBF_N_FIELDS nNFields;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPoint)
    {
        if (hMiraMonLayer->pLayerDB)
            nNFields =
                MM_PRIVATE_POINT_DB_FIELDS + hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields = MM_PRIVATE_POINT_DB_FIELDS;
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

        pBD_XP = hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;

        if (0 == (nIField = (MM_EXT_DBF_N_FIELDS)MM_DefineFirstArcFieldsDB_XP(
                      pBD_XP, 0)))
            return 1;

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

        pBD_XP = hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(nNFields, hMiraMonLayer->nCharSet);

        if (!pBD_XP)
            return 1;

        if (0 == (nIField = (MM_EXT_DBF_N_FIELDS)
                      MM_DefineFirstPolygonFieldsDB_XP(pBD_XP, 6)))
            return 1;

        pBD_XP_Aux = hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP =
            MM_CreateDBFHeader(5, hMiraMonLayer->nCharSet);

        if (!pBD_XP_Aux)
            return 1;

        if (0 == MM_DefineFirstArcFieldsDB_XP(pBD_XP_Aux, 6))
            return 1;

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
            MM_strnzcpy(
                MMField.FieldName,
                hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].pszFieldName,
                MM_MAX_LON_FIELD_NAME_DBF);

            MM_strnzcpy(MMField.FieldDescription[0],
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

    if (!hMiraMonLayer)
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

        pMMAdmDB->pMMBDXP->pfDataBase = pMMAdmDB->pFExtDBF;

        if (MM_ChangeDBFWidthField(
                pMMAdmDB->pMMBDXP, nIField, nNewWidth,
                pMMAdmDB->pMMBDXP->pField[nIField].DecimalsIfFloat,
                (MM_BYTE)MM_NOU_N_DECIMALS_NO_APLICA))
            return 1;

        // The record on course also has to change its size.
        if ((GUInt64)pMMAdmDB->pMMBDXP->BytesPerRecord + 1 >=
            pMMAdmDB->nNumRecordOnCourse)
        {
            if (nullptr == (pMMAdmDB->szRecordOnCourse = realloc_function(
                                pMMAdmDB->szRecordOnCourse,
                                (size_t)pMMAdmDB->pMMBDXP->BytesPerRecord + 1)))
            {
                MMCPLError(CE_Failure, CPLE_OutOfMemory,
                           "Memory error in MiraMon "
                           "driver (MMTestAndFixValueToRecordDBXP())");
                return 1;
            }
        }

        // File has changed it's size, so it has to be updated
        // at the Flush tool
        fseek_function(pMMAdmDB->pFExtDBF, 0, SEEK_END);
        pMMAdmDB->FlushRecList.OffsetWhereToFlush =
            ftell_function(pMMAdmDB->pFExtDBF);
    }
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
                sprintf(hMiraMonLayer->szStringToOperate, "%*.*f",
                        camp->BytesPerField, camp->DecimalsIfFloat,
                        *(const double *)valor);
            }
            else
            {
                sprintf(hMiraMonLayer->szStringToOperate, "%*lld",
                        camp->BytesPerField, *(const GInt64 *)valor);
            }
        }
        else
        {
            sprintf(hMiraMonLayer->szStringToOperate, "%-*s",
                    camp->BytesPerField, (const char *)valor);
        }
    }

    memcpy(registre + camp->AcumulatedBytes, hMiraMonLayer->szStringToOperate,
           camp->BytesPerField);
    return 0;
}

// Gets the n-th value of the format (number_of_values:val1,val2,...,valN)
char *MMGetNFieldValue(const char *pszStringList, GUInt32 nIRecord)
{
    char *p, *q;
    GUInt32 nNValues, nIValues;
    char *pszAux;

    if (!pszStringList)
        return nullptr;

    pszAux = strdup_function(pszStringList);
    p = strstr(pszAux, "(");
    if (!p)
        return nullptr;
    p++;
    if (!p)
        return nullptr;
    q = strstr(p, ":");
    p[(ptrdiff_t)q - (ptrdiff_t)p] = '\0';
    nNValues = atoi(p);
    if (nIRecord > nNValues)
        return nullptr;

    q++;
    nIValues = 0;
    while (nIValues <= nIRecord)
    {
        if (!q)
            return nullptr;
        p = strstr(q, ",");
        if (!p)
        {
            p = strstr(q, ")");
            if (!p)
                return nullptr;
            q[(ptrdiff_t)p - (ptrdiff_t)q] = '\0';
            return q;
        }
        if (nIValues == nIRecord)
        {
            p = strstr(q, ",");
            q[(ptrdiff_t)p - (ptrdiff_t)q] = '\0';

            return q;
        }
        q = p + 1;
    }

    return q;
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
                MM_ACUMULATED_BYTES_TYPE_DBF i = 0;
                while (
                    i <
                    pBD_XP->pField[nIField + nNumPrivateMMField].BytesPerField)
                {
                    memcpy(pszRecordOnCourse +
                               pBD_XP->pField[nIField + nNumPrivateMMField]
                                   .AcumulatedBytes +
                               i,
                           " ", 1);
                    i++;
                }
                continue;
            }
            if (pBD_XP->pField[nIField + nNumPrivateMMField].FieldType == 'C')
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
                else
                {
                    if (MMWriteValueToRecordDBXP(
                            hMiraMonLayer, pszRecordOnCourse,
                            pBD_XP->pField + nIField + nNumPrivateMMField,
                            &hMMFeature->pRecords[nIRecord]
                                 .pField[nIField]
                                 .dValue,
                            FALSE))
                        return 1;
                }
            }
            else if (pBD_XP->pField[nIField + nNumPrivateMMField].FieldType ==
                     'D')
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
    struct MM_FLUSH_INFO *pFlushRecList, MM_EXT_DBF_N_FIELDS nNumPrivateMMField,
    MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord, MM_EXT_DBF_N_FIELDS nIField)
{
    if (!hMiraMonLayer)
        return 1;

    if (!hMMFeature)
        return 0;

    if (nIRecord >= hMMFeature->nNumMRecords)
        return 0;

    if (nIField >= hMMFeature->pRecords[nIRecord].nNumField)
        return 0;

    if (MMTestAndFixValueToRecordDBXP(
            hMiraMonLayer, pMMAdmDB, nIField + nNumPrivateMMField,
            hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue))
        return 1;

    // We analize next fields
    if (nIField == hMMFeature->pRecords[nIRecord].nNumField - 1)
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature, pMMAdmDB,
                                         pFlushRecList, nNumPrivateMMField,
                                         nIRecord + 1, 0))
            return 1;
    }
    else
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature, pMMAdmDB,
                                         pFlushRecList, nNumPrivateMMField,
                                         nIRecord, nIField + 1))
            return 1;
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
    char *pszRecordOnCourse;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return 1;

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMAdmDBWriting.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMAdmDBWriting.pRecList;

    pszRecordOnCourse = hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    pBD_XP = hMiraMonLayer->MMAdmDBWriting.pMMBDXP;

    // Test lenght
    if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                     &hMiraMonLayer->MMAdmDBWriting,
                                     pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse = hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMAdmDBWriting,
            pszRecordOnCourse, pFlushRecList,
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
    char *pszRecordOnCourse;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return 1;

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(hMiraMonLayer,
                             hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nRecords +
                                 hMMFeature->nNumMRecords))
        return MM_STOP_WRITING_FEATURES;

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMPoint.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMPoint.MMAdmDB.pRecList;

    pBD_XP = hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;

    // Test lenght
    if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                     &hMiraMonLayer->MMPoint.MMAdmDB,
                                     pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse = hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse, pBD_XP->pField,
                             &nElemCount, TRUE);

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMPoint.MMAdmDB,
            pszRecordOnCourse, pFlushRecList,
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
    char *pszRecordOnCourse;
    struct MiraMonArcLayer *pMMArcLayer;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = MM_PRIVATE_ARC_DB_FIELDS;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (!pMMArcLayer)
        return 1;

    // In V1.1 only _UI32_MAX records number is allowed
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMCheckVersionForFID(hMiraMonLayer,
                                 pMMArcLayer->MMAdmDB.pMMBDXP->nRecords + 1))
            return MM_STOP_WRITING_FEATURES;
    }
    else
    {
        if (MMCheckVersionForFID(hMiraMonLayer,
                                 pMMArcLayer->MMAdmDB.pMMBDXP->nRecords +
                                     hMMFeature->nNumMRecords))
            return MM_STOP_WRITING_FEATURES;
    }

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &pMMArcLayer->MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)pMMArcLayer->MMAdmDB.pRecList;

    pBD_XP = pMMArcLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse = pMMArcLayer->MMAdmDB.szRecordOnCourse;

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    // Test lenght
    if (!hMiraMonLayer->bIsPolygon)
    {
        if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                         &pMMArcLayer->MMAdmDB, pFlushRecList,
                                         nNumPrivateMMField, 0, 0))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // Reassign the point because the function can realloc it.
        pszRecordOnCourse = pMMArcLayer->MMAdmDB.szRecordOnCourse;
        pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;
    }

    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse, pBD_XP->pField,
                             &nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 1, &pArcHeader->nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 2, &pArcHeader->dfLenght, FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 3, &pArcHeader->nFirstIdNode,
                             TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 4, &pArcHeader->nLastIdNode,
                             TRUE);

    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMAppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        pMMArcLayer->MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &pMMArcLayer->MMAdmDB, pszRecordOnCourse,
            pFlushRecList, &pMMArcLayer->MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Adds a node record to a MiraMon table associated with a vector layer.
int MMAddNodeRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                          MM_INTERNAL_FID nElemCount, struct MM_NH *pNodeHeader)
{
    struct MM_DATA_BASE_XP *pBD_XP = nullptr;
    char *pszRecordOnCourse;
    struct MiraMonNodeLayer *pMMNodeLayer;
    double nDoubleValue;

    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->bIsPolygon)
        pMMNodeLayer = &hMiraMonLayer->MMPolygon.MMArc.MMNode;
    else
        pMMNodeLayer = &hMiraMonLayer->MMArc.MMNode;

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(hMiraMonLayer,
                             pMMNodeLayer->MMAdmDB.pMMBDXP->nRecords + 1))
        return MM_STOP_WRITING_FEATURES;

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockWhereToSaveOrRead =
        (void *)pMMNodeLayer->MMAdmDB.pRecList;

    pBD_XP = pMMNodeLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse = pMMNodeLayer->MMAdmDB.szRecordOnCourse;

    pMMNodeLayer->MMAdmDB.FlushRecList.SizeOfBlockToBeSaved =
        pBD_XP->BytesPerRecord;
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockToBeSaved =
        (void *)pszRecordOnCourse;

    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse, pBD_XP->pField,
                             &nElemCount, TRUE);

    nDoubleValue = pNodeHeader->nArcsCount;
    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 1, &nDoubleValue, FALSE);

    nDoubleValue = pNodeHeader->cNodeType;
    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
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
    char *pszRecordOnCourse;
    MM_EXT_DBF_N_FIELDS nNumPrivateMMField = MM_PRIVATE_POLYGON_DB_FIELDS;
    struct MM_FLUSH_INFO *pFlushRecList;

    if (!hMiraMonLayer)
        return 1;

    // In V1.1 only _UI32_MAX records number is allowed
    if (MMCheckVersionForFID(
            hMiraMonLayer, hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords +
                               (hMMFeature ? hMMFeature->nNumMRecords : 0)))
        return MM_STOP_WRITING_FEATURES;

    // Adding record to the MiraMon table (extended DBF)
    // Flush settings
    pFlushRecList = &hMiraMonLayer->MMPolygon.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead =
        (void *)hMiraMonLayer->MMPolygon.MMAdmDB.pRecList;

    pBD_XP = hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;
    pszRecordOnCourse = hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    // Test lenght
    if (MMDetectAndFixDBFWidthChange(hMiraMonLayer, hMMFeature,
                                     &hMiraMonLayer->MMPolygon.MMAdmDB,
                                     pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse = hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved = (void *)pszRecordOnCourse;

    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerRecord);
    if (MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                                 pBD_XP->pField, &nElemCount, TRUE))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    if (!hMMFeature)
    {
        if (MMAppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 1, &nVerticesCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 2, &pPolHeader->dfPerimeter,
                             FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 3, &pPolHeader->dfArea, FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 4, &pPolHeader->nArcsCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, pszRecordOnCourse,
                             pBD_XP->pField + 5, &pPolHeader->nRingsCount,
                             TRUE);

    pFlushRecList->SizeOfBlockToBeSaved = pBD_XP->BytesPerRecord;
    if (MMAddFeatureRecordToMMDB(
            hMiraMonLayer, hMMFeature, &hMiraMonLayer->MMPolygon.MMAdmDB,
            pszRecordOnCourse, pFlushRecList,
            &hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

// Close the MiraMon database associated with a vector layer.
static int MMCloseMMBD_XPFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              struct MMAdmDatabase *MMAdmDB)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        if (!MMAdmDB->pFExtDBF)
        {
            // In case of 0 elements created we have to
            // create an empty DBF
            if (hMiraMonLayer->bIsPolygon)
            {
                if (hMiraMonLayer->TopHeader.nElemCount <= 1)
                {
                    if (MMCreateMMDB(hMiraMonLayer))
                    {
                        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                                   "Memory error in MiraMon "
                                   "driver (MMCreateMMDB())");
                        return 1;
                    }
                }
            }
            else if (hMiraMonLayer->bIsPoint || hMiraMonLayer->bIsArc)
            {
                if (hMiraMonLayer->TopHeader.nElemCount == 0)
                {
                    if (MMCreateMMDB(hMiraMonLayer))
                    {
                        MMCPLError(CE_Failure, CPLE_OutOfMemory,
                                   "Memory error in MiraMon "
                                   "driver (MMCreateMMDB())");
                        return 1;
                    }
                }
            }
        }

        if (MM_WriteNRecordsMMBD_XPFile(MMAdmDB))
            return 1;

        // Flushing all to be flushed
        MMAdmDB->FlushRecList.SizeOfBlockToBeSaved = 0;
        if (MMAppendBlockToBuffer(&MMAdmDB->FlushRecList))
            return 1;
    }

    // Closing database files
    if (MMAdmDB->pFExtDBF)
    {
        if (fclose_function(MMAdmDB->pFExtDBF))
            return 1;
        MMAdmDB->pFExtDBF = nullptr;
    }

    return 0;
}

int MMCloseMMBD_XP(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (!hMiraMonLayer)
        return 1;

    if (hMiraMonLayer->pMMBDXP && hMiraMonLayer->pMMBDXP->pfDataBase)
    {
        fclose_function(hMiraMonLayer->pMMBDXP->pfDataBase);
        hMiraMonLayer->pMMBDXP->pfDataBase = nullptr;
    }

    if (hMiraMonLayer->bIsPoint)
        return MMCloseMMBD_XPFile(hMiraMonLayer,
                                  &hMiraMonLayer->MMPoint.MMAdmDB);
    if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if (MMCloseMMBD_XPFile(hMiraMonLayer, &hMiraMonLayer->MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(hMiraMonLayer,
                                  &hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    }
    if (hMiraMonLayer->bIsPolygon)
    {
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;
        if (MMCloseMMBD_XPFile(hMiraMonLayer,
                               &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(
            hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
    if (hMiraMonLayer->bIsDBF)
        return MMCloseMMBD_XPFile(hMiraMonLayer,
                                  &hMiraMonLayer->MMAdmDBWriting);

    return 0;
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
        free_function(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse = nullptr;
    }
    if (hMiraMonLayer->szStringToOperate)
    {
        free_function(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate = nullptr;
        hMiraMonLayer->nNumStringToOperate = 0;
    }

    if (pMMAdmDB && pMMAdmDB->pMMBDXP)
    {
        MM_ReleaseDBFHeader(pMMAdmDB->pMMBDXP);
        hMiraMonLayer->pMMBDXP = pMMAdmDB->pMMBDXP = nullptr;
    }
    if (pMMAdmDB && pMMAdmDB->pRecList)
    {
        free_function(pMMAdmDB->pRecList);
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
#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
