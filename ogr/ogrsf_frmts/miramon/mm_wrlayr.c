/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau, a.pau@creaf.uab.cat, based on the MiraMon codes, 
 *           mainly written by Xavier Pons, Joan Masó, Abel Pau, Núria Julià,
 *           Xavier Calaf, Lluís Pesquer and Alaitz Zabala, from CREAF and
 *           Universitat Autònoma de Barcelona. For a complete list of
 *           contributors: https://www.miramon.cat/USA/QuiSom.htm
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

#ifndef GDAL_COMPILATION
#include "CmptCmp.h"                    // Compatibility between compilators
#include "mm_gdal\mm_wrlayr.h"          // For fseek_function()
#include "mm_gdal\mm_gdal_functions.h"  // For MM_strnzcpy()
#include "mm_gdal\mmrdlayr.h"           // For MM_ReadExtendedDBFHeader()
#include "msg.h"		                // For ErrorMsg()
#else
#include "mm_wrlayr.h" 
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#include "mmrdlayr.h" // For MM_ReadExtendedDBFHeader()
#endif

#include "gdal.h"			// For GDALDatasetH
#include "ogr_srs_api.h"	// For OSRGetAuthorityCode
CPL_C_START  // Necessary for compiling in GDAL project

/* -------------------------------------------------------------------- */
/*      Header Functions                                                */
/* -------------------------------------------------------------------- */
int MM_AppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo);
void MMInitBoundingBox(struct MMBoundingBox *dfBB);
int MMWriteAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMWriteNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMWritePHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMAppendIntegerDependingOnVersion(
            struct MiraMonVectLayerInfo *hMiraMonLayer,
            struct MM_FLUSH_INFO *FlushInfo, 
            unsigned long *nUL32, 
            unsigned __int64 nUI64);
int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile, 
            MM_FILE_OFFSET *nOffset);
int MMResizeZSectionDescrPointer(struct MM_ZD **pZDescription, 
            unsigned __int64 *nMax, 
            unsigned __int64 nNum, 
            unsigned __int64 nIncr,
            unsigned __int64 nProposedMax);
int MMResizeArcHeaderPointer(struct MM_AH **pArcHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);
int MMResizeNodeHeaderPointer(struct MM_NH **pNodeHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);
int MMResizePolHeaderPointer(struct MM_PH **pPolHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);
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
int MM_WriteNRecordsMMBD_XPFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
    struct MMAdmDatabase *MMAdmDB);
int MMAddDBFRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature);
int MMAddPointRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
            struct MiraMonFeature *hMMFeature,MM_INTERNAL_FID nElemCount);
int MMAddArcRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount, 
                        struct MM_AH *pArcHeader);
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

int MMTestAndFixValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            struct MMAdmDatabase  *pMMAdmDB,
                            MM_EXT_DBF_N_FIELDS nIField, 
                            const void *valor);


/* -------------------------------------------------------------------- */
/*      Functions to be used in GDAL and in MiraMon                     */
/* -------------------------------------------------------------------- */
void MM_CPLError(
    int level, int code,
    const char* format, ...)
{
    #ifdef GDAL_COMPILATION
    CPLError(level, code, format);
    #else
    ErrorMsg(format);
    #endif
}

void MM_CPLWarning(
    int level, int code,
    const char* format, ...)
{
    #ifdef GDAL_COMPILATION
    CPLError(level, code, format);
    #else
    InfoMsg(format);
    #endif
}

void MM_CPLDebug(
    const char *c,
    const char* format, ...)
{
    #ifdef GDAL_COMPILATION
    CPLDebug(c, format);
    #else
    printf(format);
    printf("\n");
    #endif
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Header                                         */
/* -------------------------------------------------------------------- */
int MMGetVectorVersion(struct MM_TH *pTopHeader)
{
    if((pTopHeader->aLayerVersion[0]==' ' ||
        pTopHeader->aLayerVersion[0]=='0') && 
            pTopHeader->aLayerVersion[1]=='1'&&
            pTopHeader->aLayerSubVersion=='1')
        return MM_32BITS_VERSION;

    if((pTopHeader->aLayerVersion[0]==' ' ||
        pTopHeader->aLayerVersion[0]=='0') && 
            pTopHeader->aLayerVersion[1]=='2'&&
            pTopHeader->aLayerSubVersion=='0')
        return MM_64BITS_VERSION;

    return MM_UNKNOWN_VERSION;
}

void MMSet1_1Version(struct MM_TH *pTopHeader)
{
    pTopHeader->aLayerVersion[0]=' ';
    pTopHeader->aLayerVersion[1]='1';
    pTopHeader->aLayerSubVersion='1';
}

void MMSet2_0Version(struct MM_TH *pTopHeader)
{
    pTopHeader->aLayerVersion[0]=' ';
    pTopHeader->aLayerVersion[1]='2';
    pTopHeader->aLayerSubVersion='0';
}

int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader)
{
char dot;
unsigned long NCount;
long reservat4=0L;

    pMMHeader->Flag=0x0;
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
    if (fread_function(&pMMHeader->Flag, 
            sizeof(pMMHeader->Flag), 1, pF)!=1)
		return 1;
    if (fread_function(&pMMHeader->hBB.dfMinX, 
            sizeof(pMMHeader->hBB.dfMinX), 1, pF)!=1)
		return 1;
    if (fread_function(&pMMHeader->hBB.dfMaxX, 
            sizeof(pMMHeader->hBB.dfMaxX), 1, pF)!=1)
		return 1;
    if (fread_function(&pMMHeader->hBB.dfMinY, 
            sizeof(pMMHeader->hBB.dfMinY), 1, pF)!=1)
		return 1;
    if (fread_function(&pMMHeader->hBB.dfMaxY, 
            sizeof(pMMHeader->hBB.dfMaxY), 1, pF)!=1)
		return 1;
    if (pMMHeader->aLayerVersion[0]==' ' && 
        pMMHeader->aLayerVersion[1]=='1')
    {
        if (fread_function(&NCount, sizeof(NCount), 1, pF)!=1)
	    	return 1;

        pMMHeader->nElemCount=(MM_INTERNAL_FID)NCount;
    
	    if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
    }
    else if (pMMHeader->aLayerVersion[0]==' ' && 
        pMMHeader->aLayerVersion[1]=='2')
    {
        if (fread_function(&(pMMHeader->nElemCount), 
            sizeof(pMMHeader->nElemCount), 1, pF)!=1)
	    	return 1;

        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
    }

    if(pMMHeader->Flag&MM_LAYER_3D_INFO)
        pMMHeader->bIs3d=1;

    if(pMMHeader->Flag&MM_LAYER_MULTIPOLYGON)
        pMMHeader->bIsMultipolygon=1;

    return 0;
}


int MMWriteHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader)
{
char dot='.';
unsigned long NCount;
long reservat4=0L;
MM_INTERNAL_FID nNumber1=1, nNumber0=0;

    if(!pF)
        return 0;

    pMMHeader->Flag=MM_CREATED_USING_MIRAMON; // Created from MiraMon
    if(pMMHeader->bIs3d)
        pMMHeader->Flag|=MM_LAYER_3D_INFO; // 3D

    if(pMMHeader->bIsMultipolygon)
        pMMHeader->Flag|=MM_LAYER_MULTIPOLYGON; // Multipolygon.

    if(pMMHeader->aFileType[0]=='P' &&
            pMMHeader->aFileType[1]=='O' && 
            pMMHeader->aFileType[2]=='L')
        pMMHeader->Flag|=MM_BIT_5_ON; // Explicital polygons

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
    if (fwrite_function(&pMMHeader->Flag, sizeof(pMMHeader->Flag), 1, pF)!=1)
		return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMinX, 
        sizeof(pMMHeader->hBB.dfMinX), 1, pF)!=1)
		return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMaxX, 
        sizeof(pMMHeader->hBB.dfMaxX), 1, pF)!=1)
		return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMinY, 
        sizeof(pMMHeader->hBB.dfMinY), 1, pF)!=1)
		return 1;
    if (fwrite_function(&pMMHeader->hBB.dfMaxY, 
        sizeof(pMMHeader->hBB.dfMaxY), 1, pF)!=1)
		return 1;
    if (pMMHeader->aLayerVersion[0]==' ' && 
        pMMHeader->aLayerVersion[1]=='1')
    {
        NCount=(unsigned long)pMMHeader->nElemCount;
        if (fwrite_function(&NCount, sizeof(NCount), 1, pF)!=1)
	    	return 1;
    
	    if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
    }
    else if (pMMHeader->aLayerVersion[0]==' ' && 
        pMMHeader->aLayerVersion[1]=='2')
    {
        if (fwrite_function(&(pMMHeader->nElemCount), 
            sizeof(pMMHeader->nElemCount), 1, pF)!=1)
	    	return 1;

        // Next part of the file (don't apply for the moment)
        if (fwrite_function(&nNumber1, sizeof(nNumber1), 1, pF)!=1)
	    	return 1;
        if (fwrite_function(&nNumber0, sizeof(nNumber0), 1, pF)!=1)
	    	return 1;

        // Reserved bytes
        if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
    }
	return 0;
}

void MMInitHeader(struct MM_TH *pMMHeader, int layerType, int nVersion)
{
    memset(pMMHeader, 0, sizeof(*pMMHeader));
    switch(nVersion)
    {
        case MM_32BITS_VERSION:
            pMMHeader->aLayerVersion[0]='0';
            pMMHeader->aLayerVersion[1]='1';
            pMMHeader->aLayerSubVersion='1';
            break;
        case MM_64BITS_VERSION:
        case MM_LAST_VERSION:
        default:
            pMMHeader->aLayerVersion[0]='0';
            pMMHeader->aLayerVersion[1]='2';
            pMMHeader->aLayerSubVersion='0';
            break;
    }
    switch (layerType)
    {
        case MM_LayerType_Point:
            pMMHeader->aFileType[0]='P';
            pMMHeader->aFileType[1]='N';
            pMMHeader->aFileType[2]='T';
            break;
        case MM_LayerType_Point3d:
            pMMHeader->aFileType[0]='P';
            pMMHeader->aFileType[1]='N';
            pMMHeader->aFileType[2]='T';
            pMMHeader->bIs3d=1;
            break;
        case MM_LayerType_Arc:
            pMMHeader->aFileType[0]='A';
            pMMHeader->aFileType[1]='R';
            pMMHeader->aFileType[2]='C';
            break;
        case MM_LayerType_Arc3d:
            pMMHeader->aFileType[0]='A';
            pMMHeader->aFileType[1]='R';
            pMMHeader->aFileType[2]='C';
            pMMHeader->bIs3d=1;
            break;
        case MM_LayerType_Pol:
            pMMHeader->aFileType[0]='P';
            pMMHeader->aFileType[1]='O';
            pMMHeader->aFileType[2]='L';
            break;
        case MM_LayerType_Pol3d:
            pMMHeader->aFileType[0]='P';
            pMMHeader->aFileType[1]='O';
            pMMHeader->aFileType[2]='L';
            pMMHeader->bIs3d=1;
            break;
        default:
		    break;
    }
    pMMHeader->nElemCount=0;
    pMMHeader->hBB.dfMinX=MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMaxX=-MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMinY=MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader->hBB.dfMaxY=-MM_UNDEFINED_STATISTICAL_VALUE;

    pMMHeader->Flag=MM_CREATED_USING_MIRAMON; // Created from MiraMon
    if(pMMHeader->bIs3d)
        pMMHeader->Flag|=MM_LAYER_3D_INFO; // 3D

    if(pMMHeader->bIsMultipolygon)
        pMMHeader->Flag|=MM_LAYER_MULTIPOLYGON; // Multipolygon.

    if(pMMHeader->aFileType[0]=='P' &&
            pMMHeader->aFileType[1]=='O' && 
            pMMHeader->aFileType[2]=='L')
        pMMHeader->Flag|=MM_BIT_5_ON; // Explicital polygons
}

int MMWriteEmptyHeader(FILE_TYPE *pF, int layerType, int nVersion)
{
struct MM_TH pMMHeader;

    memset(&pMMHeader, 0, sizeof(pMMHeader));
    switch(nVersion)
    {
        case MM_32BITS_VERSION:
            pMMHeader.aLayerVersion[0]='0';
            pMMHeader.aLayerVersion[1]='1';
            pMMHeader.aLayerSubVersion='1';
            break;
        case MM_64BITS_VERSION:
        case MM_LAST_VERSION:
        default:
            pMMHeader.aLayerVersion[0]='0';
            pMMHeader.aLayerVersion[1]='2';
            pMMHeader.aLayerSubVersion='0';
            break;
    }
    switch (layerType)
    {
        case MM_LayerType_Point:
            pMMHeader.aFileType[0]='P';
            pMMHeader.aFileType[1]='N';
            pMMHeader.aFileType[2]='T';
            break;
        case MM_LayerType_Point3d:
            pMMHeader.aFileType[0]='P';
            pMMHeader.aFileType[1]='N';
            pMMHeader.aFileType[2]='T';
            pMMHeader.bIs3d=1;
            break;
        case MM_LayerType_Arc:
            pMMHeader.aFileType[0]='A';
            pMMHeader.aFileType[1]='R';
            pMMHeader.aFileType[2]='C';
            break;
        case MM_LayerType_Arc3d:
            pMMHeader.aFileType[0]='A';
            pMMHeader.aFileType[1]='R';
            pMMHeader.aFileType[2]='C';
            pMMHeader.bIs3d=1;
            break;
        case MM_LayerType_Pol:
            pMMHeader.aFileType[0]='P';
            pMMHeader.aFileType[1]='O';
            pMMHeader.aFileType[2]='L';
            break;
        case MM_LayerType_Pol3d:
            pMMHeader.aFileType[0]='P';
            pMMHeader.aFileType[1]='O';
            pMMHeader.aFileType[2]='L';
            pMMHeader.bIs3d=1;
            break;
        default:
		    break;
    }
    pMMHeader.nElemCount=0;
    pMMHeader.hBB.dfMinX=MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMaxX=-MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMinY=MM_UNDEFINED_STATISTICAL_VALUE;
    pMMHeader.hBB.dfMaxY=-MM_UNDEFINED_STATISTICAL_VALUE;

	return MMWriteHeader(pF, &pMMHeader);
}

int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                   FILE_TYPE *pF, 
                   struct MM_ZSection *pZSection)
{
long reservat4=0L;

    if(hMiraMonLayer->bIsPoint)
    {
        pZSection->ZSectionOffset=hMiraMonLayer->nHeaderDiskSize+
            hMiraMonLayer->TopHeader.nElemCount*MM_SIZE_OF_TL;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset=hMiraMonLayer->MMArc.pArcHeader[hMiraMonLayer->TopHeader.nElemCount-1].nOffset+
        hMiraMonLayer->MMArc.pArcHeader[hMiraMonLayer->TopHeader.nElemCount-1].nElemCount*
            MM_SIZE_OF_COORDINATE;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        // Z section begins just after last coordinate of the last arc
        pZSection->ZSectionOffset=hMiraMonLayer->MMPolygon.MMArc.pArcHeader[hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount-1].nOffset+
        hMiraMonLayer->MMPolygon.MMArc.pArcHeader[hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount-1].nElemCount*
            MM_SIZE_OF_COORDINATE;
    }
    else 
        return 1;

    if(pF)
    {
        if (fseek_function(pF, pZSection->ZSectionOffset, SEEK_SET))
    	    return 1;

        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=4;
        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=4;
        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=4;
        if (fread_function(&reservat4, 4, 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=4;
        
        if (fread_function(&pZSection->ZHeader.dfBBminz, 
                            sizeof(pZSection->ZHeader.dfBBminz), 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=sizeof(pZSection->ZHeader.dfBBminz);
        
        if (fread_function(&pZSection->ZHeader.dfBBmaxz, 
                            sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=sizeof(pZSection->ZHeader.dfBBmaxz);
        
    }
	return 0;
}

int MMWriteZSection(FILE_TYPE *pF, struct MM_ZSection *pZSection)
{
long reservat4=0L;

    if (fseek_function(pF, pZSection->ZSectionOffset, SEEK_SET))
    	return 1;

    if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		return 1;
    if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		return 1;
    if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		return 1;
    if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		return 1;

    pZSection->ZSectionOffset+=16;

    if (fwrite_function(&pZSection->ZHeader.dfBBminz, 
                        sizeof(pZSection->ZHeader.dfBBminz), 1, pF)!=1)
		return 1;
    pZSection->ZSectionOffset+=sizeof(pZSection->ZHeader.dfBBminz);
    if (fwrite_function(&pZSection->ZHeader.dfBBmaxz, 
                        sizeof(pZSection->ZHeader.dfBBmaxz), 1, pF)!=1)
		return 1;
    pZSection->ZSectionOffset+=sizeof(pZSection->ZHeader.dfBBmaxz);
	return 0;
}


int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF, MM_INTERNAL_FID nElements, 
                        struct MM_ZSection *pZSection)
{
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
MM_INTERNAL_FID nIndex=0;
MM_FILE_OFFSET nBlockSize;
struct MM_ZD *pZDescription=pZSection->pZDescription;

    nBlockSize=nElements*pZSection->nZDDiskSize;

    if(MMInitFlush(&FlushTMP, pF, nBlockSize, &pBuffer, 
                pZSection->ZSectionOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    if(MMReadFlush(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    for(nIndex=0; nIndex<nElements; nIndex++)
    {
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->dfBBminz);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->dfBBminz;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->dfBBmaxz;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->nZCount;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        if(hMiraMonLayer->LayerVersion==MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved=4;
            FlushTMP.pBlockToBeSaved=(void *)NULL;
            if(MM_ReadBlockFromBuffer(&FlushTMP))
            {
                if(pBuffer)free_function(pBuffer);
                return 1;
            }
        }

        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &(pZDescription+nIndex)->nOffsetZ))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    if(pBuffer)
        free_function(pBuffer);

	return 0;
}

int MMWriteZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF, MM_INTERNAL_FID nElements, 
                        struct MM_ZSection *pZSection)
{
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
MM_INTERNAL_FID nIndex=0;
MM_FILE_OFFSET nOffsetDiff;
struct MM_ZD *pZDescription=pZSection->pZDescription;

    nOffsetDiff=pZSection->ZSectionOffset+nElements*(
        sizeof(pZDescription->dfBBminz)+
        sizeof(pZDescription->dfBBmaxz)+
        sizeof(pZDescription->nZCount)+
        ((hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)?
        sizeof(nUL32):sizeof(pZDescription->nOffsetZ)));

    if(MMInitFlush(&FlushTMP, pF,
        hMiraMonLayer->nMemoryRatio?
        (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
        &pBuffer, 
                pZSection->ZSectionOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    for(nIndex=0; nIndex<nElements; nIndex++)
    {
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->dfBBminz);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->dfBBminz;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->dfBBmaxz);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->dfBBmaxz;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        FlushTMP.SizeOfBlockToBeSaved=
            sizeof((pZDescription+nIndex)->nZCount);
        FlushTMP.pBlockToBeSaved=
            (void *)&(pZDescription+nIndex)->nZCount;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        if(hMiraMonLayer->LayerVersion==MM_64BITS_VERSION)
        {
            FlushTMP.SizeOfBlockToBeSaved=4;
            FlushTMP.pBlockToBeSaved=(void *)NULL;
            if(MM_AppendBlockToBuffer(&FlushTMP))
            {
                if(pBuffer)free_function(pBuffer);
                return 1;
            }
        }

        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, (pZDescription+nIndex)->nOffsetZ+nOffsetDiff))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MM_AppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }
    pZSection->ZSectionOffset+=FlushTMP.TotalSavedBytes;

    if(pBuffer)
        free_function(pBuffer);

	return 0;
}

void MMDestroyZSectionDescription(struct MM_ZSection *pZSection)
{
    if(pZSection->pZL)
    {
        free_function(pZSection->pZL);
        pZSection->pZL=NULL;
    }

    if(pZSection->pZDescription)
    {
        free_function(pZSection->pZDescription);
        pZSection->pZDescription=NULL;
    }
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Initialization                                 */
/* -------------------------------------------------------------------- */
int MMInitZSectionDescription(struct MM_ZSection *pZSection)
{
    pZSection->pZDescription=(struct MM_ZD *)calloc_function(
        pZSection->nMaxZDescription*
            sizeof(*pZSection->pZDescription));
    if(!pZSection->pZDescription)
        return 1;
    return 0;
}

int MMInitZSectionLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF3d,
                        struct MM_ZSection *pZSection)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Zsection
    if(!hMiraMonLayer->TopHeader.bIs3d)
    {
        pZSection->pZDescription=NULL;
        return 0;
    }

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pZSection->ZHeader.dfBBminz = STATISTICAL_UNDEF_VALUE;
        pZSection->ZHeader.dfBBmaxz = -STATISTICAL_UNDEF_VALUE;
    }

    // ZH
    pZSection->ZHeader.nMyDiskSize=32;
    pZSection->ZSectionOffset=0;

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

    if (hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pZSection->nZDDiskSize=MM_SIZE_OF_ZD_32_BITS;
    else
        pZSection->nZDDiskSize=MM_SIZE_OF_ZD_64_BITS;

    pZSection->ZDOffset=0;

    // ZL
    if(hMiraMonLayer->ReadOrWrite==MM_WRITTING_MODE)
    {
        if(MMInitFlush(&pZSection->FlushZL, pF3d, 
                hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_250MB):MM_250MB,
                &pZSection->pZL, 
                0, sizeof(double)))
            return 1;
    }

    return 0;
}

// AA.pnt -> AAT.rel, for instance
void MMChangeMMRareExtension(char *pszName, const char *pszExt)
{
    if(strlen(pszExt)<=0)
        return;
    strcpy(pszName, reset_extension(pszName, pszExt));
    memcpy(pszName+ strlen(pszName)-strlen(pszExt)-1,
        pszName+ strlen(pszName)-strlen(pszExt), strlen(pszExt));
    pszName[strlen(pszName)-1]='\0';
}

int MMInitPointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, int bIs3d)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    hMiraMonLayer->bIsPoint=1;

    if(hMiraMonLayer->ReadOrWrite==MM_WRITTING_MODE)
    {
        // Geometrical part
        // Init header structure
        hMiraMonLayer->TopHeader.nElemCount=0;
        MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);
        
        hMiraMonLayer->TopHeader.bIs3d = 1; // Read description of bRealIs3d
        hMiraMonLayer->TopHeader.aFileType[0]='P';
        hMiraMonLayer->TopHeader.aFileType[1]='N';
        hMiraMonLayer->TopHeader.aFileType[2]='T';
    
        // Opening the binary file where sections TH, TL[...] and ZH-ZD[...]-ZL[...]
        // are going to be written.
        
        strcpy(hMiraMonLayer->MMPoint.pszLayerName, hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMPoint.pszLayerName, ".pnt");
    }
    if(NULL==(hMiraMonLayer->MMPoint.pF=fopen_function(
        hMiraMonLayer->MMPoint.pszLayerName, 
        hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(hMiraMonLayer->MMPoint.pF, 0, SEEK_SET);

    if(hMiraMonLayer->ReadOrWrite==MM_WRITTING_MODE)
    {
        // TL
        strcpy(hMiraMonLayer->MMPoint.pszTLName, hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMPoint.pszTLName, ".~TL");

        if(NULL==(hMiraMonLayer->MMPoint.pFTL = 
                fopen_function(hMiraMonLayer->MMPoint.pszTLName, 
                hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(hMiraMonLayer->MMPoint.pFTL, 0, SEEK_SET);

        if(MMInitFlush(&hMiraMonLayer->MMPoint.FlushTL, 
                hMiraMonLayer->MMPoint.pFTL, 
                hMiraMonLayer->nMemoryRatio?
                    (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_250MB):MM_250MB,
                &hMiraMonLayer->MMPoint.pTL, 
                0, MM_SIZE_OF_TL))
               return 1;
    
        // 3d part
        if(hMiraMonLayer->TopHeader.bIs3d)
        {
            strcpy(hMiraMonLayer->MMPoint.psz3DLayerName, hMiraMonLayer->pszSrcLayerName);
            strcat(hMiraMonLayer->MMPoint.psz3DLayerName, ".~z");

            if(NULL==(hMiraMonLayer->MMPoint.pF3d=fopen_function(
                hMiraMonLayer->MMPoint.psz3DLayerName, 
                hMiraMonLayer->pszFlags)))
                return 1;
            fseek_function(hMiraMonLayer->MMPoint.pF3d, 0, SEEK_SET);
        }
    }    
    // Zsection
    if (hMiraMonLayer->TopHeader.bIs3d)
    {
        if (MMInitZSectionLayer(hMiraMonLayer,
            hMiraMonLayer->MMPoint.pF3d,
            &hMiraMonLayer->MMPoint.pZSection))
            return 1;
    
        if(hMiraMonLayer->ReadOrWrite==MM_READING_MODE)
        {
            if(MMReadZSection(hMiraMonLayer, hMiraMonLayer->MMPoint.pF, 
                        &hMiraMonLayer->MMPoint.pZSection))
                return 1;

            if(MMReadZDescriptionHeaders(hMiraMonLayer, hMiraMonLayer->MMPoint.pF, 
                hMiraMonLayer->TopHeader.nElemCount, &hMiraMonLayer->MMPoint.pZSection))
                return 1;
        }
    }
    
    // MiraMon metadata
    strcpy(hMiraMonLayer->MMPoint.pszREL_LayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPoint.pszREL_LayerName, "T.rel");
    else
        MMChangeMMRareExtension(hMiraMonLayer->MMPoint.pszREL_LayerName, "T.rel");
    
    hMiraMonLayer->pszMainREL_LayerName=hMiraMonLayer->MMPoint.pszREL_LayerName;

    if(hMiraMonLayer->ReadOrWrite==MM_READING_MODE)
    {
       // This file has to exist and be the appropriate version.
        if(MM_Check_REL_FILE(hMiraMonLayer->MMPoint.pszREL_LayerName))
            return 1;
    }

    // MIRAMON DATA BASE
    // Creating the DBF file name
    strcpy(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName, "T.dbf");
    else
        MMChangeMMRareExtension(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName, "T.dbf");
    
    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    return 0;
}

int MMInitNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, int bIs3d)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Init header structure
        pMMArcLayer->TopNodeHeader.aFileType[0] = 'N';
        pMMArcLayer->TopNodeHeader.aFileType[1] = 'O';
        pMMArcLayer->TopNodeHeader.aFileType[2] = 'D';

        pMMArcLayer->TopNodeHeader.bIs3d = 1; // Read description of bRealIs3d
        MMInitBoundingBox(&pMMArcLayer->TopNodeHeader.hBB);
    }

    // Opening the binary file where sections TH, NH and NL[...]
    // are going to be written.
    strcpy(pMMArcLayer->MMNode.pszLayerName, pMMArcLayer->pszLayerName);
    strcpy(pMMArcLayer->MMNode.pszLayerName,
            reset_extension(pMMArcLayer->MMNode.pszLayerName, "nod"));

    if(NULL==(pMMArcLayer->MMNode.pF = 
            fopen_function(pMMArcLayer->MMNode.pszLayerName, 
            hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMArcLayer->MMNode.pF, 0, SEEK_SET);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Node Header
        pMMArcLayer->MMNode.nMaxNodeHeader = MM_FIRST_NUMBER_OF_NODES;
        if (NULL == (pMMArcLayer->MMNode.pNodeHeader = (struct MM_NH*)calloc_function(
            pMMArcLayer->MMNode.nMaxNodeHeader *
            sizeof(*pMMArcLayer->MMNode.pNodeHeader))))
            return 1;

        if (hMiraMonLayer->LayerVersion == MM_32BITS_VERSION)
            pMMArcLayer->MMNode.nSizeNodeHeader = MM_SIZE_OF_NH_32BITS;
        else
            pMMArcLayer->MMNode.nSizeNodeHeader = MM_SIZE_OF_NH_64BITS;

        // NL Section
        strcpy(pMMArcLayer->MMNode.pszNLName, pMMArcLayer->MMNode.pszLayerName);
        strcpy(pMMArcLayer->MMNode.pszNLName,
            reset_extension(pMMArcLayer->MMNode.pszNLName, "~NL"));
        
        if (NULL == (pMMArcLayer->MMNode.pFNL =
            fopen_function(pMMArcLayer->MMNode.pszNLName,
                hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(pMMArcLayer->MMNode.pFNL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->MMNode.FlushNL, pMMArcLayer->MMNode.pFNL,
            hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_250MB):MM_250MB,
            &pMMArcLayer->MMNode.pNL, 0, 0))
            return 1;

        // Creating the DBF file name
        strcpy(pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName, pMMArcLayer->MMNode.pszLayerName);
        MMChangeMMRareExtension(pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName, "N.dbf");
        
        // MiraMon metadata
        strcpy(pMMArcLayer->MMNode.pszREL_LayerName, pMMArcLayer->MMNode.pszLayerName);
        MMChangeMMRareExtension(pMMArcLayer->MMNode.pszREL_LayerName, "N.rel");
    }
    return 0;
}

int MMInitArcLayer(struct MiraMonVectLayerInfo* hMiraMonLayer, int bIs3d)
{
    struct MiraMonArcLayer* pMMArcLayer;
    struct MM_TH* pArcTopHeader;

    CheckMMVectorLayerVersion(hMiraMonLayer, 1)

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
        pArcTopHeader->bIs3d = 1; // Read description of bRealIs3d
        MMInitBoundingBox(&pArcTopHeader->hBB);

        pArcTopHeader->aFileType[0] = 'A';
        pArcTopHeader->aFileType[1] = 'R';
        pArcTopHeader->aFileType[2] = 'C';

        strcpy(pMMArcLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->bIsPolygon)
            strcat(pMMArcLayer->pszLayerName, "_bound.arc");
        else
            strcat(pMMArcLayer->pszLayerName, ".arc");
    }

    if (NULL == (pMMArcLayer->pF = fopen_function(pMMArcLayer->pszLayerName,
        hMiraMonLayer->pszFlags)))
        return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE &&
        hMiraMonLayer->bIsPolygon)
    {
        fseek_function(pMMArcLayer->pF, 0, SEEK_SET);
        MMReadHeader(pMMArcLayer->pF, &hMiraMonLayer->MMPolygon.TopArcHeader);
        // 3d information is in arcs file
        hMiraMonLayer->TopHeader.bIs3d=hMiraMonLayer->MMPolygon.TopArcHeader.bIs3d;
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
    
    if (NULL == (pMMArcLayer->pArcHeader = (struct MM_AH*)calloc_function(
            pMMArcLayer->nMaxArcHeader *
            sizeof(*pMMArcLayer->pArcHeader))))
            return 1;

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if(MMReadAHArcSection(hMiraMonLayer))
            return 1;
    }
    
    // AL
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        pMMArcLayer->nALElementSize = MM_SIZE_OF_AL;

        strcpy(pMMArcLayer->pszALName, hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->bIsPolygon)
            strcat(pMMArcLayer->pszALName, "_bound.~AL");
        else
            strcat(pMMArcLayer->pszALName, ".~AL");
        
        if (NULL == (pMMArcLayer->pFAL = fopen_function(pMMArcLayer->pszALName,
            hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(pMMArcLayer->pFAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMArcLayer->FlushAL, pMMArcLayer->pFAL,
            hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
            &pMMArcLayer->pAL, 0, 0))
            return 1;
    }

    // 3D
    if (pArcTopHeader->bIs3d)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        {
            strcpy(pMMArcLayer->psz3DLayerName, hMiraMonLayer->pszSrcLayerName);
            if(hMiraMonLayer->bIsPolygon)
                strcat(pMMArcLayer->psz3DLayerName, "_bound.~z");
            else
                strcat(pMMArcLayer->psz3DLayerName, ".~z");

            if (NULL == (pMMArcLayer->pF3d = fopen_function(pMMArcLayer->psz3DLayerName,
                hMiraMonLayer->pszFlags)))
                return 1;
            fseek_function(pMMArcLayer->pF3d, 0, SEEK_SET);
        }

        if(MMInitZSectionLayer(hMiraMonLayer, 
                        pMMArcLayer->pF3d, 
                        &pMMArcLayer->pZSection))
            return 1;

        if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            if(MMReadZSection(hMiraMonLayer, pMMArcLayer->pF, 
                       &pMMArcLayer->pZSection))
                return 1;

            if(MMReadZDescriptionHeaders(hMiraMonLayer, pMMArcLayer->pF, 
                pArcTopHeader->nElemCount, &pMMArcLayer->pZSection))
                return 1;
        }
    }

    // MiraMon metadata
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcpy(pMMArcLayer->pszREL_LayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszREL_LayerName, "_boundA.rel");
        else
        {
            strcpy(pMMArcLayer->pszREL_LayerName, pMMArcLayer->pszLayerName);
            MMChangeMMRareExtension(pMMArcLayer->pszREL_LayerName, "A.rel");
        }
    }
    else
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszREL_LayerName, "A.rel");
        else
            MMChangeMMRareExtension(pMMArcLayer->pszREL_LayerName, "A.rel");
    }

    if(hMiraMonLayer->ReadOrWrite==MM_READING_MODE)
    {
       // This file has to exist and be the appropriate version.
        if(MM_Check_REL_FILE(pMMArcLayer->pszREL_LayerName))
            return 1;
    }

    if(!hMiraMonLayer->bIsPolygon)
        hMiraMonLayer->pszMainREL_LayerName=pMMArcLayer->pszREL_LayerName;

    // MIRAMON DATA BASE
    // Creating the DBF file name
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcpy(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->bIsPolygon)
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, "_boundA.dbf");
        else
        {
            strcpy(pMMArcLayer->pszREL_LayerName, pMMArcLayer->pszLayerName);
            MMChangeMMRareExtension(pMMArcLayer->pszREL_LayerName, "A.dbf");
        }
    }
    else
    {
        if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, "A.dbf");
        else
            MMChangeMMRareExtension(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, "A.dbf");
    }
    
    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    // Node part
    if(MMInitNodeLayer(hMiraMonLayer, bIs3d))
        return 1;
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        MMSet1_1Version(&pMMArcLayer->TopNodeHeader);
    else
        MMSet2_0Version(&pMMArcLayer->TopNodeHeader);

    return 0;
}

int MMInitPolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, int bIs3d)
{
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Init header structure
    hMiraMonLayer->bIsPolygon=1;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d  = 1; // Read description of bRealIs3d
        MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);

        hMiraMonLayer->TopHeader.aFileType[0] = 'P';
        hMiraMonLayer->TopHeader.aFileType[1] = 'O';
        hMiraMonLayer->TopHeader.aFileType[2] = 'L';

        strcpy(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszLayerName, ".pol");
    }

    if(NULL==(pMMPolygonLayer->pF = 
            fopen_function(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszFlags)))
        return 1;

    // PS
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMPolygonLayer->nPSElementSize=MM_SIZE_OF_PS_32BITS;
    else
        pMMPolygonLayer->nPSElementSize=MM_SIZE_OF_PS_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        strcpy(pMMPolygonLayer->pszPSName , hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszPSName , ".~PS");

        if (NULL == (pMMPolygonLayer->pFPS = fopen_function(pMMPolygonLayer->pszPSName,
            hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(pMMPolygonLayer->pFPS, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPS, pMMPolygonLayer->pFPS,
            hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
            &pMMPolygonLayer->pPS, 0,
            pMMPolygonLayer->nPSElementSize))
            return 1;
    }

    // PH
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMPolygonLayer->nPHElementSize=MM_SIZE_OF_PH_32BITS;
    else
        pMMPolygonLayer->nPHElementSize=MM_SIZE_OF_PH_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        pMMPolygonLayer->nMaxPolHeader=MM_FIRST_NUMBER_OF_POLYGONS+1;
    else
        pMMPolygonLayer->nMaxPolHeader=hMiraMonLayer->TopHeader.nElemCount;

    if(NULL==(pMMPolygonLayer->pPolHeader=(struct MM_PH *)calloc_function(
            pMMPolygonLayer->nMaxPolHeader*
            sizeof(*pMMPolygonLayer->pPolHeader))))
        return 1;

    // PAL
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMPolygonLayer->nPALElementSize=MM_SIZE_OF_PAL_32BITS;
    else
        pMMPolygonLayer->nPALElementSize=MM_SIZE_OF_PAL_64BITS;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Universal polygon.
        memset(pMMPolygonLayer->pPolHeader, 0, sizeof(*pMMPolygonLayer->pPolHeader));
        hMiraMonLayer->TopHeader.nElemCount = 1;

        // PAL
        strcpy(pMMPolygonLayer->pszPALName , hMiraMonLayer->pszSrcLayerName);
        strcat(pMMPolygonLayer->pszPALName , ".~PL");

        if (NULL == (pMMPolygonLayer->pFPAL = fopen_function(pMMPolygonLayer->pszPALName,
            hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(pMMPolygonLayer->pFPAL, 0, SEEK_SET);

        if (MMInitFlush(&pMMPolygonLayer->FlushPAL, pMMPolygonLayer->pFPAL,
            hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
            &pMMPolygonLayer->pPAL, 0, 0))
            return 1;
    }

    // MiraMon metadata
    strcpy(hMiraMonLayer->MMPolygon.pszREL_LayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(hMiraMonLayer->MMPolygon.pszREL_LayerName, "P.rel");
    else
        MMChangeMMRareExtension(hMiraMonLayer->MMPolygon.pszREL_LayerName, "P.rel");

    if(hMiraMonLayer->ReadOrWrite==MM_READING_MODE)
    {
       // This file has to exist and be the appropriate version.
        if(MM_Check_REL_FILE(hMiraMonLayer->MMPolygon.pszREL_LayerName))
            return 1;
    }

    hMiraMonLayer->pszMainREL_LayerName=hMiraMonLayer->MMPolygon.pszREL_LayerName;

    // MIRAMON DATA BASE
    strcpy(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName, hMiraMonLayer->pszSrcLayerName);
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
        strcat(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName, "P.dbf");
    else
        MMChangeMMRareExtension(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName, "P.dbf");

    if (hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
    {
        if (MM_ReadExtendedDBFHeader(hMiraMonLayer))
            return 1;
    }

    return 0;
}

int MMInitLayerByType(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    int bIs3d=0;

    if(hMiraMonLayer->eLT==MM_LayerType_Point || 
        hMiraMonLayer->eLT==MM_LayerType_Point3d)
    {
        strcpy(hMiraMonLayer->MMPoint.pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(hMiraMonLayer->MMPoint.pszLayerName, ".pnt");
        if (hMiraMonLayer->MMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n", hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pnt\n", CPLGetBasename( hMiraMonLayer->pszSrcLayerName));
        }

        if(hMiraMonLayer->eLT==MM_LayerType_Point3d)
            bIs3d=1;

        if(MMInitPointLayer(hMiraMonLayer, bIs3d))
            return 1;
        return 0;
    }
    if(hMiraMonLayer->eLT==MM_LayerType_Arc || 
        hMiraMonLayer->eLT==MM_LayerType_Arc3d)
    {
        struct MiraMonArcLayer *pMMArcLayer=&hMiraMonLayer->MMArc;

        strcpy(pMMArcLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMArcLayer->pszLayerName, ".arc");

        if (hMiraMonLayer->MMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n", hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.arc\n", CPLGetBasename( hMiraMonLayer->pszSrcLayerName));
        }

        if(hMiraMonLayer->eLT==MM_LayerType_Arc3d)
            bIs3d=1;

        if(MMInitArcLayer(hMiraMonLayer, bIs3d))
            return 1;
        return 0;
    }
    if(hMiraMonLayer->eLT==MM_LayerType_Pol || 
        hMiraMonLayer->eLT==MM_LayerType_Pol3d)
    {
        struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

        strcpy(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
            strcat(pMMPolygonLayer->pszLayerName, ".pol");

        if (hMiraMonLayer->MMMap)
        {
            hMiraMonLayer->MMMap->nNumberOfLayers++;
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "[VECTOR_%d]\n", hMiraMonLayer->MMMap->nNumberOfLayers);
            VSIFPrintf(hMiraMonLayer->MMMap->fMMMap, "Fitxer=%s.pol\n", CPLGetBasename( hMiraMonLayer->pszSrcLayerName));
        }

        if(hMiraMonLayer->eLT==MM_LayerType_Pol3d)
            bIs3d=1;
        
        if(MMInitPolygonLayer(hMiraMonLayer, bIs3d))
            return 1;
        
        if(hMiraMonLayer->ReadOrWrite == MM_READING_MODE)
        {
            char *pszArcLayerName;
            const char *pszExt;
            // StringLine associated to the polygon
            pszArcLayerName = strdup_function(ReturnValueFromSectionINIFile( pMMPolygonLayer->pszREL_LayerName,
                SECTION_OVVW_ASPECTES_TECNICS, KEY_ArcSource));

            // If extension is not specified, then we'll use ".arc"
            pszExt=get_extension_function(pszArcLayerName);
            if(is_empty_string_function(pszExt))
            {
                char *pszArcLayerNameAux=calloc_function(strlen(pszArcLayerName)+5);
                if(!pszArcLayerNameAux)
                    return 1;
                strcpy(pszArcLayerNameAux, pszArcLayerName);
                strcat(pszArcLayerNameAux, ".arc");
                free_function(pszArcLayerName);
                pszArcLayerName=pszArcLayerNameAux;
            }
            
            strcpy(pMMPolygonLayer->MMArc.pszLayerName, 
                form_filename_function(
                    get_path_function(hMiraMonLayer->pszSrcLayerName),
                    pszArcLayerName));

            free_function(pszArcLayerName);

            if(NULL==(hMiraMonLayer->MMPolygon.MMArc.pF=fopen_function(
                pMMPolygonLayer->MMArc.pszLayerName, 
                hMiraMonLayer->pszFlags)))
                return 1;

            if(MMReadHeader(hMiraMonLayer->MMPolygon.MMArc.pF,
                &hMiraMonLayer->MMPolygon.TopArcHeader))
                return 1;

            if(MMReadPHPolygonSection(hMiraMonLayer))
                return 1;

            fclose_function(hMiraMonLayer->MMPolygon.MMArc.pF);
            hMiraMonLayer->MMPolygon.MMArc.pF=NULL;
        }
        else
        {
            // Creating the stringLine file associated to the polygon
            strcpy(pMMPolygonLayer->MMArc.pszLayerName, hMiraMonLayer->pszSrcLayerName);
            strcat(pMMPolygonLayer->MMArc.pszLayerName, ".arc");
        }
        
        if(MMInitArcLayer(hMiraMonLayer, bIs3d))
            return 1;

        // Polygon is 3D if Arc is 3D, by definition.
        hMiraMonLayer->TopHeader.bIs3d=
            hMiraMonLayer->MMPolygon.TopArcHeader.bIs3d;

        if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
            MMSet1_1Version(&pMMPolygonLayer->TopArcHeader);
        else
            MMSet2_0Version(&pMMPolygonLayer->TopArcHeader);
    }
    else if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        // Trying to get DBF information
        strcpy(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName, hMiraMonLayer->pszSrcLayerName);
        strcat(hMiraMonLayer->MMAdmDBWriting.pszExtDBFLayerName, ".dbf");
    }

    return 0;
}

int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, const char *pzFileName, 
                __int32 LayerVersion, double nMMMemoryRatio,
                struct MiraMonDataBase *pLayerDB,
                MM_BOOLEAN ReadOrWrite, struct MiraMonVectMapInfo *MMMap)
{
    MM_CPLDebug("MiraMon", "Initializing MiraMon layer...");

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));

    hMiraMonLayer->Version=MM_VECTOR_LAYER_LAST_VERSION;
    hMiraMonLayer->nMemoryRatio=nMMMemoryRatio;
    MM_CPLDebug("MiraMon", "Setting MemoryRatio to %f...", nMMMemoryRatio);

    hMiraMonLayer->ReadOrWrite=ReadOrWrite;
    hMiraMonLayer->MMMap=MMMap;

    // Don't free in destructor
    hMiraMonLayer->pLayerDB=pLayerDB;

    // Assigning the file name and the type
    strcpy(hMiraMonLayer->pszFlags, "wb+");
    
    hMiraMonLayer->bIsPolygon=0;

    if(LayerVersion==MM_UNKNOWN_VERSION)
        return 1;
    if(LayerVersion==MM_LAST_VERSION)
    {
        MMSet1_1Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_64_BITS;
        hMiraMonLayer->LayerVersion=MM_64BITS_VERSION;
    }
    else if(LayerVersion==MM_32BITS_VERSION)
    {
        MMSet1_1Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_32_BITS;
        hMiraMonLayer->LayerVersion=MM_32BITS_VERSION;
    }
    else
    {
        MMSet2_0Version(&hMiraMonLayer->TopHeader);
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_64_BITS;
        hMiraMonLayer->LayerVersion=MM_64BITS_VERSION;
    }

    hMiraMonLayer->pszSrcLayerName=strdup_function(pzFileName);
    hMiraMonLayer->szLayerTitle=strdup_function(get_filename_function(pzFileName));

    if(!hMiraMonLayer->bIsBeenInit && 
        hMiraMonLayer->eLT!=MM_LayerType_Unknown)
    {
        if(MMInitLayerByType(hMiraMonLayer))
            return 1;
        hMiraMonLayer->bIsBeenInit=1;
    }

    // If more nNumStringToOperate is needed, it'll be increased.
    hMiraMonLayer->nNumStringToOperate=0;
    if(MM_ResizeStringToOperateIfNeeded(hMiraMonLayer, 500))
        return 1;
    
    hMiraMonLayer->nCharSet=MM_JOC_CARAC_ANSI_DBASE;
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Closing                                        */
/* -------------------------------------------------------------------- */
int MMClose3DSectionLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        MM_INTERNAL_FID nElements,
                        FILE_TYPE *pF,
                        FILE_TYPE *pF3d,
                        const char *pszF3d,
                        struct MM_ZSection *pZSection,
                        MM_FILE_OFFSET FinalOffset)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Flushing if there is something to flush on the disk
    if(!pF || !pF3d || !pszF3d || !pZSection)
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
        if (MM_AppendBlockToBuffer(&pZSection->FlushZL))
            return 1;

        if (MMMoveFromFileToFile(pF3d, pF, &pZSection->ZSectionOffset))
            return 1;
    }

    if(pF3d)
        fclose_function(pF3d);
    if(pszF3d)
        remove_function(pszF3d);

    return 0;
}

int MMClosePointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d=hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->TopHeader))
            return 1;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // TL Section
        hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
            return 1;
        if (MMMoveFromFileToFile(hMiraMonLayer->MMPoint.pFTL,
            hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->OffsetCheck))
            return 1;

        fclose_function(hMiraMonLayer->MMPoint.pFTL);
        remove_function(hMiraMonLayer->MMPoint.pszTLName);

        if (MMClose3DSectionLayer(hMiraMonLayer,
            hMiraMonLayer->TopHeader.nElemCount,
            hMiraMonLayer->MMPoint.pF,
            hMiraMonLayer->MMPoint.pF3d,
            hMiraMonLayer->MMPoint.psz3DLayerName,
            &hMiraMonLayer->MMPoint.pZSection,
            hMiraMonLayer->OffsetCheck))
            return 1;
    }
    if(hMiraMonLayer->MMPoint.pF)
        fclose_function(hMiraMonLayer->MMPoint.pF);
    return 0;
}

int MMCloseNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if (hMiraMonLayer->bIsPolygon)
        pMMArcLayer = &hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer = &hMiraMonLayer->MMArc;

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->TopHeader.bIs3d=hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->MMNode.pF, &pMMArcLayer->TopNodeHeader))
            return 1;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // NH Section
        if (MMWriteNHNodeSection(hMiraMonLayer, hMiraMonLayer->nHeaderDiskSize))
            return 1;

        // NL Section
        pMMArcLayer->MMNode.FlushNL.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&pMMArcLayer->MMNode.FlushNL))
            return 1;
        if (MMMoveFromFileToFile(pMMArcLayer->MMNode.pFNL,
            pMMArcLayer->MMNode.pF, &hMiraMonLayer->OffsetCheck))
            return 1;

        if (pMMArcLayer->MMNode.pFNL)
            fclose_function(pMMArcLayer->MMNode.pFNL);
        if (pMMArcLayer->MMNode.pszNLName)
            remove_function(pMMArcLayer->MMNode.pszNLName);
    }

    if(pMMArcLayer->MMNode.pF)
        fclose_function(pMMArcLayer->MMNode.pF);
    
    return 0;
}

int MMCloseArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;
struct MM_TH *pArcTopHeader;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

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
        hMiraMonLayer->TopHeader.bIs3d=hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMArcLayer->pF, pArcTopHeader))
            return 1;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // AH Section
        if (MMWriteAHArcSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
            return 1;

        // AL Section
        pMMArcLayer->FlushAL.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&pMMArcLayer->FlushAL))
            return 1;
        if (MMMoveFromFileToFile(pMMArcLayer->pFAL, pMMArcLayer->pF,
            &hMiraMonLayer->OffsetCheck))
            return 1;
        if (pMMArcLayer->pFAL)
            fclose_function(pMMArcLayer->pFAL);
        if (pMMArcLayer->pszALName)
            remove_function(pMMArcLayer->pszALName);

        // 3D Section
        if (MMClose3DSectionLayer(hMiraMonLayer,
            pArcTopHeader->nElemCount,
            pMMArcLayer->pF,
            pMMArcLayer->pF3d,
            pMMArcLayer->psz3DLayerName,
            &pMMArcLayer->pZSection,
            hMiraMonLayer->OffsetCheck))
            return 1;
    }

    if(pMMArcLayer->pF)
        fclose_function(pMMArcLayer->pF);
    
    MMCloseNodeLayer(hMiraMonLayer);

    return 0;
}

int MMClosePolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;
    
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    MMCloseArcLayer(hMiraMonLayer);

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        hMiraMonLayer->nFinalElemCount = hMiraMonLayer->TopHeader.nElemCount;
        hMiraMonLayer->TopHeader.bIs3d=hMiraMonLayer->bIsReal3d;

        if (MMWriteHeader(pMMPolygonLayer->pF, &hMiraMonLayer->TopHeader))
            return 1;
        hMiraMonLayer->OffsetCheck = hMiraMonLayer->nHeaderDiskSize;

        // PS Section
        pMMPolygonLayer->FlushPS.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&pMMPolygonLayer->FlushPS))
            return 1;
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPS, pMMPolygonLayer->pF,
            &hMiraMonLayer->OffsetCheck))
            return 1;

        if (pMMPolygonLayer->pFPS)
            fclose_function(pMMPolygonLayer->pFPS);
        if (pMMPolygonLayer->pszPSName)
            remove_function(pMMPolygonLayer->pszPSName);

        // AH Section
        if (MMWritePHPolygonSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
            return 1;

        // PAL Section
        pMMPolygonLayer->FlushPAL.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&pMMPolygonLayer->FlushPAL))
            return 1;
        if (MMMoveFromFileToFile(pMMPolygonLayer->pFPAL, pMMPolygonLayer->pF,
            &hMiraMonLayer->OffsetCheck))
            return 1;
        if (pMMPolygonLayer->pFPAL)
            fclose_function(pMMPolygonLayer->pFPAL);
        if (pMMPolygonLayer->pszPALName)
            remove_function(pMMPolygonLayer->pszPALName);
    }

    if(pMMPolygonLayer->pF)
        fclose_function(pMMPolygonLayer->pF);
    
    return 0;
}

int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    MM_CPLDebug("MiraMon", "Closing MiraMon layer");
    if(!hMiraMonLayer)
        return 0;

    if(hMiraMonLayer->bIsPoint)
    {
        if(MMClosePointLayer(hMiraMonLayer))
            return 1;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseArcLayer(hMiraMonLayer))
            return 1;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
      if(MMClosePolygonLayer(hMiraMonLayer))
          return 1;
    }
    else
    {
        // If no geometry remove all created files
        if(hMiraMonLayer->pszSrcLayerName)
            remove_function(hMiraMonLayer->pszSrcLayerName);
        if(hMiraMonLayer->szLayerTitle)
            remove_function(hMiraMonLayer->szLayerTitle);
    }

    // MiraMon metadata files
    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        if (MMWriteVectorMetadata(hMiraMonLayer))
            return 1;
    }

    // MiraMon database files
    if(MMCloseMMBD_XP(hMiraMonLayer))
        return 1;

    MM_CPLDebug("MiraMon", "MiraMon layer closed");
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Destroying (allocated memory)                  */
/* -------------------------------------------------------------------- */
void MMDestroyMMAdmDB(struct MMAdmDatabase *pMMAdmDB)
{
    if(pMMAdmDB->pRecList)
    {
        free_function(pMMAdmDB->pRecList);
        pMMAdmDB->pRecList=NULL;
    }
    
    if(pMMAdmDB->szRecordOnCourse)
    {
        free_function(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse=NULL;
        pMMAdmDB->nNumRecordOnCourse=0;
    }
}
int MMDestroyPointLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->MMPoint.pTL)
    {
        free_function(hMiraMonLayer->MMPoint.pTL);
        hMiraMonLayer->MMPoint.pTL=NULL;
    }
    
    if(hMiraMonLayer->TopHeader.bIs3d)
        MMDestroyZSectionDescription(&hMiraMonLayer->MMPoint.pZSection);

    MMDestroyMMAdmDB(&hMiraMonLayer->MMPoint.MMAdmDB);

    return 0;
}

int MMDestroyNodeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    if(pMMArcLayer->MMNode.pNL)
    {
        free_function(pMMArcLayer->MMNode.pNL);
        pMMArcLayer->MMNode.pNL=NULL;
    }
        
    if(pMMArcLayer->MMNode.pNodeHeader)
    {
        free_function(pMMArcLayer->MMNode.pNodeHeader);
        pMMArcLayer->MMNode.pNodeHeader=NULL;
    }

    MMDestroyMMAdmDB(&hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    return 0;
}

int MMDestroyArcLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    if(pMMArcLayer->pAL)
    {
        free_function(pMMArcLayer->pAL);
        pMMArcLayer->pAL=NULL;
    }
    if(pMMArcLayer->pArcHeader)
    {
        free_function(pMMArcLayer->pArcHeader);
        pMMArcLayer->pArcHeader=NULL;
    }

    if(hMiraMonLayer->TopHeader.bIs3d)
        MMDestroyZSectionDescription(&pMMArcLayer->pZSection);

    MMDestroyMMAdmDB(&pMMArcLayer->MMAdmDB);

    MMDestroyNodeLayer(hMiraMonLayer);
    return 0;
}

int MMDestroyPolygonLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    MMDestroyArcLayer(hMiraMonLayer);

    if(pMMPolygonLayer->pPAL)
    {
        free_function(pMMPolygonLayer->pPAL);
        pMMPolygonLayer->pPAL=NULL;
    }
    
    if(pMMPolygonLayer->pPS)
    {
        free_function(pMMPolygonLayer->pPS);
        pMMPolygonLayer->pPS=NULL;
    }

    if(pMMPolygonLayer->pPolHeader)
    {
        free_function(pMMPolygonLayer->pPolHeader);
        pMMPolygonLayer->pPolHeader=NULL;
    }

    MMDestroyMMAdmDB(&pMMPolygonLayer->MMAdmDB);

    return 0;
}

int MMFreeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    MM_CPLDebug("MiraMon", "Destroying MiraMon layer memory");

    if(!hMiraMonLayer)
        return 0;

    if(hMiraMonLayer->bIsPoint)
        MMDestroyPointLayer(hMiraMonLayer);
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
        MMDestroyArcLayer(hMiraMonLayer);
    else if(hMiraMonLayer->bIsPolygon)
        MMDestroyPolygonLayer(hMiraMonLayer);
    
    if(hMiraMonLayer->pszSrcLayerName)
    {
        free_function(hMiraMonLayer->pszSrcLayerName);
        hMiraMonLayer->pszSrcLayerName=NULL;
    }
    if(hMiraMonLayer->szLayerTitle)
    {
        free_function(hMiraMonLayer->szLayerTitle);
        hMiraMonLayer->szLayerTitle=NULL;
    }
    if(hMiraMonLayer->pSRS)
    {
        free_function(hMiraMonLayer->pSRS);
        hMiraMonLayer->pSRS=NULL;
    }

    if(hMiraMonLayer->pMultRecordIndex)
    {
        free_function(hMiraMonLayer->pMultRecordIndex);
        hMiraMonLayer->pMultRecordIndex=NULL;
    }

    if(hMiraMonLayer->ReadedFeature.pNCoordRing)
    {
        free(hMiraMonLayer->ReadedFeature.pNCoordRing);
        hMiraMonLayer->ReadedFeature.pNCoordRing=NULL;
    }
    if(hMiraMonLayer->ReadedFeature.pCoord)
    {
        free(hMiraMonLayer->ReadedFeature.pCoord);
        hMiraMonLayer->ReadedFeature.pCoord=NULL;
    }
    if(hMiraMonLayer->ReadedFeature.pZCoord)
    {
        free(hMiraMonLayer->ReadedFeature.pZCoord);
        hMiraMonLayer->ReadedFeature.pZCoord=NULL;
    }
    if(hMiraMonLayer->ReadedFeature.pRecords)
    {
        free(hMiraMonLayer->ReadedFeature.pRecords);
        hMiraMonLayer->ReadedFeature.pRecords=NULL;
    }
    if(hMiraMonLayer->ReadedFeature.pbArcInfo)
    {
        free(hMiraMonLayer->ReadedFeature.pbArcInfo);
        hMiraMonLayer->ReadedFeature.pbArcInfo=NULL;
    }

    if(hMiraMonLayer->pArcs)
    {
        free_function(hMiraMonLayer->pArcs);
        hMiraMonLayer->pArcs=NULL;
    }
    
    if(hMiraMonLayer->szStringToOperate)
    {
        free_function(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate=NULL;
        hMiraMonLayer->nNumStringToOperate=0;
    }

    if (hMiraMonLayer->pLayerDB)
    {
        if(hMiraMonLayer->pLayerDB->pFields)
        {
            free_function(hMiraMonLayer->pLayerDB->pFields);
            hMiraMonLayer->pLayerDB->pFields=NULL;
        }
        free_function(hMiraMonLayer->pLayerDB);
        hMiraMonLayer->pLayerDB=NULL;
    }

    // Destroys all database objects
    MMDestroyMMDB(hMiraMonLayer);

    MM_CPLDebug("MiraMon", "MiraMon layer memory destroyed");

    return 0;
}

void MMDestroyLayer(struct MiraMonVectLayerInfo **hMiraMonLayer)
{
    if(!hMiraMonLayer)
        return;
    if(!(*hMiraMonLayer))
        return;
    if(!(*hMiraMonLayer))
        return;
    free_function(*hMiraMonLayer);
    *hMiraMonLayer=NULL;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Creating a layer                               */
/* -------------------------------------------------------------------- */
#ifndef GDAL_COMPILATION
struct MiraMonVectLayerInfo * MMCreateLayer(char *pzFileName, 
            __int32 LayerVersion, double nMMMemoryRatio,
            struct MiraMonDataBase *hLayerDB,
            MM_BOOLEAN ReadOrWrite)
{
struct MiraMonVectLayerInfo *hMiraMonLayer;

    // Creating of the handle to a MiraMon Layer
    hMiraMonLayer=(struct MiraMonVectLayerInfo *)calloc_function(
                    sizeof(*hMiraMonLayer));
    if(MMInitLayer(hMiraMonLayer, pzFileName, LayerVersion, nMMMemoryRatio,
                hLayerDB, ReadOrWrite, NULL /*Map*/))
        return NULL;

    // Return the handle to the layer
    return hMiraMonLayer;
}
#endif
/* -------------------------------------------------------------------- */
/*      Flush Layer Functions                                           */
/* -------------------------------------------------------------------- */
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, 
                unsigned __int64 nBlockSize, char **pBuffer, 
                MM_FILE_OFFSET DiskOffsetWhereToFlush, 
                __int32 nMyDiskSize)
{
    memset(pFlush, 0, sizeof(*pFlush));
    pFlush->nMyDiskSize=nMyDiskSize;
    pFlush->pF=pF;
    pFlush->nBlockSize=nBlockSize;
    pFlush->nNumBytes=0;
    if(NULL==(*pBuffer=(char *)calloc_function(nBlockSize)))
        return 1;
    pFlush->OffsetWhereToFlush=DiskOffsetWhereToFlush;
    pFlush->CurrentOffset=0;
    return 0;
}

int MMReadFlush(struct MM_FLUSH_INFO *pFlush)
{
    fseek_function(pFlush->pF, pFlush->OffsetWhereToFlush, SEEK_SET);
    if(pFlush->nBlockSize!=(fread_function(pFlush->pBlockWhereToSaveOrRead, 1, pFlush->nBlockSize, pFlush->pF)))
        return 1;
    return 0;
}

int MMFlushToDisk(struct MM_FLUSH_INFO *FlushInfo)
{
    if(!FlushInfo->nNumBytes)
        return 0;
    // Just flush to the disk at the correct place.
    fseek_function(FlushInfo->pF, FlushInfo->OffsetWhereToFlush, SEEK_SET);

    if(FlushInfo->nNumBytes!=fwrite_function(FlushInfo->pBlockWhereToSaveOrRead, 1, 
            FlushInfo->nNumBytes, FlushInfo->pF))
        return 1;
    FlushInfo->OffsetWhereToFlush+=FlushInfo->nNumBytes;
    FlushInfo->NTimesFlushed++;
    FlushInfo->TotalSavedBytes+=FlushInfo->nNumBytes;
    FlushInfo->nNumBytes=0;

    return 0;
}

int MM_ReadBlockFromBuffer(struct MM_FLUSH_INFO *FlushInfo)
{
    if(!FlushInfo->SizeOfBlockToBeSaved)
        return 0;
        
    if(FlushInfo->pBlockToBeSaved)
    {
        memcpy(FlushInfo->pBlockToBeSaved, 
                (void *)((char *)FlushInfo->pBlockWhereToSaveOrRead+
                        FlushInfo->CurrentOffset), 
                FlushInfo->SizeOfBlockToBeSaved);
    }
    FlushInfo->CurrentOffset+=FlushInfo->SizeOfBlockToBeSaved;

    return 0;
}

int MM_AppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo)
{
    if(FlushInfo->SizeOfBlockToBeSaved)
    {
        // If all the bloc itselfs doesn't fit to the buffer, 
        // then all the block is written directly to the disk
        if(FlushInfo->nNumBytes==0 && 
            FlushInfo->SizeOfBlockToBeSaved>=FlushInfo->nBlockSize)
        {
            if(MMFlushToDisk(FlushInfo))
                return 1;
            return 0;
        }

        // There is space in FlushInfo->pBlockWhereToSaveOrRead?
        if(FlushInfo->nNumBytes+FlushInfo->SizeOfBlockToBeSaved<=
            FlushInfo->nBlockSize)
        {
            if(FlushInfo->pBlockToBeSaved)
            {
                memcpy((void *)((char *)FlushInfo->pBlockWhereToSaveOrRead+
                    FlushInfo->nNumBytes), FlushInfo->pBlockToBeSaved, 
                    FlushInfo->SizeOfBlockToBeSaved);
            }
            else // Add zero caracters
            {
                char zero_caracters[8]="\0\0\0\0\0\0\0";
                memcpy((char *)FlushInfo->pBlockWhereToSaveOrRead+
                    FlushInfo->nNumBytes, zero_caracters, 
                    FlushInfo->SizeOfBlockToBeSaved);
            }

            FlushInfo->nNumBytes+=FlushInfo->SizeOfBlockToBeSaved;
        }
        else
        {
            // Empty the buffer
            if(MMFlushToDisk(FlushInfo))
                return 1;
            // Append the pendant bytes
            if(MM_AppendBlockToBuffer(FlushInfo))
                return 1;
        }
        return 0;
    }
    // Just flush to the disc.
    return MMFlushToDisk(FlushInfo);
}

int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile, 
                         MM_FILE_OFFSET *nOffset)
{
size_t bufferSize = 100 * 1024 * 1024; // 100 MB buffer
unsigned char* buffer;
size_t bytesRead, bytesWritten;

    if(!pSrcFile || !pDestFile || !nOffset)
        return 0;

    buffer = (unsigned char*)calloc_function(bufferSize);

    if (!buffer)
        return 1;
    
    //fflush_function(pSrcFile);
    fseek_function(pSrcFile, 0, SEEK_SET);
    while ((bytesRead = fread_function(buffer, sizeof(unsigned char), 
        bufferSize, pSrcFile)) > 0) 
    {
        bytesWritten = fwrite_function(buffer, sizeof(unsigned char), 
            bytesRead, pDestFile);
        if (bytesWritten != bytesRead)
            return 1;
        if(nOffset)
            (*nOffset)+=bytesWritten;
    }
    free_function(buffer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer: Writing sections of layers                               */
/* -------------------------------------------------------------------- */

void GetOffsetAlignedTo8(MM_FILE_OFFSET *Offset)
{
MM_FILE_OFFSET reajust;

    if ((*Offset) % 8L)
	{
		reajust=8-((*Offset)%8L);
		(*Offset)+=reajust;
	}
}

int MMReadIntegerDependingOnVersion(
                            struct MiraMonVectLayerInfo *hMiraMonLayer,
                            struct MM_FLUSH_INFO *FlushInfo, 
                            unsigned __int64 *nUI64)
{
unsigned long nUL32;

    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
    {
        FlushInfo->pBlockToBeSaved=(void *)&nUL32;
        FlushInfo->SizeOfBlockToBeSaved=sizeof(nUL32);
        if(MM_ReadBlockFromBuffer(FlushInfo))
            return 1;
        *nUI64=(unsigned __int64)nUL32;
    }
    else
    {
        FlushInfo->pBlockToBeSaved=(void *)nUI64;
        FlushInfo->SizeOfBlockToBeSaved=sizeof(&nUI64);
        if(MM_ReadBlockFromBuffer(FlushInfo))
            return 1;
    }
    return 0;
}

int MMAppendIntegerDependingOnVersion(
                            struct MiraMonVectLayerInfo *hMiraMonLayer,
                            struct MM_FLUSH_INFO *FlushInfo, 
                            unsigned long *nUL32, unsigned __int64 nUI64)
{
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
    {
        *nUL32=(unsigned long)nUI64;
        FlushInfo->SizeOfBlockToBeSaved=sizeof(*nUL32);
        hMiraMonLayer->OffsetCheck+=FlushInfo->SizeOfBlockToBeSaved;
        FlushInfo->pBlockToBeSaved=(void *)nUL32;
    }
    else
    {
        FlushInfo->SizeOfBlockToBeSaved=sizeof(nUI64);
        hMiraMonLayer->OffsetCheck+=FlushInfo->SizeOfBlockToBeSaved;
        FlushInfo->pBlockToBeSaved=(void *)&nUI64;
    }
    return MM_AppendBlockToBuffer(FlushInfo);
}

int MMReadAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
MM_INTERNAL_FID iElem, nElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
MM_FILE_OFFSET nBlockSize;
struct MiraMonArcLayer *pMMArcLayer;
unsigned __int64 nElementCount;

    if(hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
        nElem=hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount;
    }
    else
    {
        pMMArcLayer=&hMiraMonLayer->MMArc;
        nElem=hMiraMonLayer->TopHeader.nElemCount;
    }

    nBlockSize=nElem*(pMMArcLayer->nSizeArcHeader);

    if(MMInitFlush(&FlushTMP, pMMArcLayer->pF, 
                nBlockSize, &pBuffer, hMiraMonLayer->nHeaderDiskSize, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    if(MMReadFlush(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    for(iElem=0; iElem<nElem; iElem++)
    {
        // Bounding box
        FlushTMP.pBlockToBeSaved=
            (void *)&(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        nElementCount=pMMArcLayer->pArcHeader[iElem].nElemCount;
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nElementCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        pMMArcLayer->pArcHeader[iElem].nElemCount=(MM_N_VERTICES_TYPE)nElementCount;
        
        // Offset: offset of the first vertice of the arc
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMArcLayer->pArcHeader[iElem].nOffset))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Lenght of the arc
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLenght;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLenght);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    
    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMWriteAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        MM_FILE_OFFSET DiskOffset)
{
MM_INTERNAL_FID iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
MM_FILE_OFFSET nOffsetDiff;
struct MiraMonArcLayer *pMMArcLayer;

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    nOffsetDiff=hMiraMonLayer->nHeaderDiskSize+
        hMiraMonLayer->nFinalElemCount*(pMMArcLayer->nSizeArcHeader);


    if(MMInitFlush(&FlushTMP, pMMArcLayer->pF, 
        hMiraMonLayer->nMemoryRatio?
            (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
        &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    for(iElem=0; iElem<hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Element count: number of vertices of the arc
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMArcLayer->pArcHeader[iElem].nElemCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Offset: offset of the first vertice of the arc
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMArcLayer->pArcHeader[iElem].nOffset+nOffsetDiff))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // First node: first node of the arc
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMArcLayer->pArcHeader[iElem].nFirstIdNode))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Last node: first node of the arc
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMArcLayer->pArcHeader[iElem].nLastIdNode))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Lenght of the arc
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfLenght);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfLenght;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MM_AppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMReadNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
MM_INTERNAL_FID iElem, nElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
MM_FILE_OFFSET nBlockSize;
struct MiraMonArcLayer *pMMArcLayer;

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    nElem=pMMArcLayer->TopNodeHeader.nElemCount;

    nBlockSize=nElem*pMMArcLayer->MMNode.nSizeNodeHeader;

    if(MMInitFlush(&FlushTMP, pMMArcLayer->MMNode.pF, 
                nBlockSize, &pBuffer, hMiraMonLayer->nHeaderDiskSize, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    if(MMReadFlush(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    for(iElem=0; iElem<nElem; iElem++)
    {
        // Arcs count
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Node type
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved=1;
        FlushTMP.pBlockToBeSaved=(void *)NULL;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Offset: offset of the first arc to the node
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    
    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMWriteNHNodeSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                         MM_FILE_OFFSET DiskOffset)
{
MM_INTERNAL_FID iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
MM_FILE_OFFSET nOffsetDiff;
struct MiraMonArcLayer *pMMArcLayer;

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    nOffsetDiff=hMiraMonLayer->nHeaderDiskSize+
        (pMMArcLayer->TopNodeHeader.nElemCount*
        pMMArcLayer->MMNode.nSizeNodeHeader);

    if(MMInitFlush(&FlushTMP, pMMArcLayer->MMNode.pF, 
        hMiraMonLayer->nMemoryRatio?
        (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
        &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    for(iElem=0; iElem<pMMArcLayer->TopNodeHeader.nElemCount; iElem++)
    {
        // Arcs count
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        // Node type
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].cNodeType;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved=1;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=(void *)NULL;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Offset: offset of the first arc to the node
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, 
            pMMArcLayer->MMNode.pNodeHeader[iElem].nOffset+nOffsetDiff))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MM_AppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMReadPHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
MM_INTERNAL_FID iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
MM_FILE_OFFSET nBlockSize;
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    nBlockSize=hMiraMonLayer->TopHeader.nElemCount*
        (pMMPolygonLayer->nPHElementSize);

    if(MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, 
                nBlockSize, &pBuffer, 
                hMiraMonLayer->nHeaderDiskSize+
                (hMiraMonLayer->MMPolygon.TopArcHeader.nElemCount*
                hMiraMonLayer->MMPolygon.nPSElementSize), 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }
    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    if(MMReadFlush(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    for(iElem=0; iElem<hMiraMonLayer->TopHeader.nElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.pBlockToBeSaved=
            (void *)&(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY);
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Arcs count: number of the arcs of the polygon
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // External arcs count: number of the external arcs of the polygon
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Rings count: number of the rings of the polygon
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Offset: offset of the first vertice of the arc
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &pMMPolygonLayer->pPolHeader[iElem].nOffset))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Perimeter of the arc
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfPerimeter);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfPerimeter;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Area of the arc
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfArea);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfArea;
        if(MM_ReadBlockFromBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    
    if(pBuffer)
        free_function(pBuffer);
    return 0;
}


int MMWritePHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                            MM_FILE_OFFSET DiskOffset)
{
MM_INTERNAL_FID iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
MM_FILE_OFFSET nOffsetDiff;
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    if(!pMMPolygonLayer->pF)
        return 0;

    nOffsetDiff=DiskOffset+
        hMiraMonLayer->TopHeader.nElemCount*
        (pMMPolygonLayer->nPHElementSize);

    if(MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, 
        hMiraMonLayer->nMemoryRatio?
        (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_500MB):MM_500MB,
        &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSaveOrRead=(void *)pBuffer;
    for(iElem=0; iElem<hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Arcs count: number of the arcs of the polygon
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMPolygonLayer->pPolHeader[iElem].nArcsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // External arcs count: number of the external arcs of the polygon
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMPolygonLayer->pPolHeader[iElem].nExternalRingsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Rings count: number of the rings of the polygon
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMPolygonLayer->pPolHeader[iElem].nRingsCount))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Offset: offset of the first vertice of the arc
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, &FlushTMP,
            &nUL32, pMMPolygonLayer->pPolHeader[iElem].nOffset+nOffsetDiff))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        
        // Perimeter of the arc
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfPerimeter);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfPerimeter;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Area of the arc
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfArea);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfArea;
        if(MM_AppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MM_AppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    if(pBuffer)
        free_function(pBuffer);
    return 0;
}


/* -------------------------------------------------------------------- */
/*      Feature Functions                                               */
/* -------------------------------------------------------------------- */
int MMInitFeature(struct MiraMonFeature *hMMFeature)
{
    memset(hMMFeature, 0, sizeof(*hMMFeature));

    hMMFeature->nMaxMRecords=MM_INIT_NUMBER_OF_RECORDS;
    if((hMMFeature->pRecords=calloc_function(
            hMMFeature->nMaxMRecords*sizeof(*(hMMFeature->pRecords))))==NULL)
        return 1;

    hMMFeature->pRecords[0].nMaxField = MM_INIT_NUMBER_OF_FIELDS;
    hMMFeature->pRecords[0].nNumField = 0;
    if(NULL==(hMMFeature->pRecords[0].pField=calloc_function(
            hMMFeature->pRecords[0].nMaxField*
            sizeof(*(hMMFeature->pRecords[0].pField)))))
        return 1;
    
    return 0;
}

// Conserves all allocated memory but reset the information
void MMResetFeature(struct MiraMonFeature *hMMFeature)
{
    if(hMMFeature->pNCoordRing)
    {
        memset(hMMFeature->pNCoordRing, 0, 
            hMMFeature->nMaxpNCoordRing*
            sizeof(*(hMMFeature->pNCoordRing)));
    }
    if(hMMFeature->pCoord)
    {
	    memset(hMMFeature->pCoord, 
            0, hMMFeature->nMaxpCoord*
            sizeof(*(hMMFeature->pCoord)));
    }
    hMMFeature->nICoord=0;
	if (hMMFeature->pZCoord)
    {
		memset(hMMFeature->pZCoord, 0, 
            hMMFeature->nMaxpZCoord*
            sizeof(*(hMMFeature->pZCoord)));
    }
    hMMFeature->nNRings=0;
    hMMFeature->nIRing=0;
    
    if(hMMFeature->pbArcInfo)
    {
        memset(hMMFeature->pbArcInfo, 
            0, hMMFeature->nMaxpbArcInfo*
            sizeof(*(hMMFeature->pbArcInfo)));
    }

    if(hMMFeature->pRecords)
    {
        MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
        MM_EXT_DBF_N_FIELDS nIField;

        for(nIRecord=0; nIRecord<hMMFeature->nMaxMRecords; nIRecord++)
        {
            if(!hMMFeature->pRecords[nIRecord].pField)
                continue;
            for(nIField=0; nIField<hMMFeature->pRecords[nIRecord].nMaxField; nIField++)
            {
                if(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)
                    *(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)='\0';
                hMMFeature->pRecords[nIRecord].pField[nIField].bIsValid=0;
            }
        }
    }
}

// Conserves all allocated memroy but inicialize the counters to zero.
void MMDestroyFeature(struct MiraMonFeature *hMMFeature)
{
    if(hMMFeature->pCoord)
    {
        free_function(hMMFeature->pCoord);
        hMMFeature->pCoord=NULL;
    }
    if(hMMFeature->pZCoord)
    {
        free_function(hMMFeature->pZCoord);
        hMMFeature->pZCoord=NULL;
    }
    if(hMMFeature->pNCoordRing)
    {
        free_function(hMMFeature->pNCoordRing);
        hMMFeature->pNCoordRing=NULL;
    }
    
    if(hMMFeature->pbArcInfo)
    {
        free_function(hMMFeature->pbArcInfo);
        hMMFeature->pbArcInfo=NULL;
    }

    if(hMMFeature->pRecords)
    {
        MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
        MM_EXT_DBF_N_FIELDS nIField;

        for(nIRecord=0; nIRecord<hMMFeature->nMaxMRecords; nIRecord++)
        {
            if(!hMMFeature->pRecords[nIRecord].pField)
                continue;
            for(nIField=0; nIField<hMMFeature->pRecords[nIRecord].nMaxField; nIField++)
            {
                if(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)
                    free_function(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue);
            }
            free_function(hMMFeature->pRecords[nIRecord].pField);
        }
        free_function(hMMFeature->pRecords);
        hMMFeature->pRecords=NULL;
    }
	
    hMMFeature->nNRings=0;
    hMMFeature->nNumMRecords=0;
    hMMFeature->nMaxMRecords=0;
}

int MMCreateFeaturePolOrArc(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                struct MiraMonFeature *hMMFeature)
{
double *pZ=NULL;
struct MM_POINT_2D *pCoord;
unsigned __int64 nIPart, nIVertice;
double dtempx, dtempy;
MM_POLYGON_RINGS_COUNT nExternalRingsCount;
struct MM_PH *pCurrentPolHeader=NULL;
struct MM_AH *pCurrentArcHeader;
struct MM_NH *pCurrentNodeHeader, *pCurrentNodeHeaderPlus1=NULL;
unsigned long UnsignedLongNumber;
struct MiraMonArcLayer *pMMArc;
struct MiraMonNodeLayer *pMMNode;
struct MM_TH *pArcTopHeader;
struct MM_TH *pNodeTopHeader;
unsigned char VFG;
MM_FILE_OFFSET nOffsetTmp;
struct MM_ZD *pZDesc=NULL;
struct MM_FLUSH_INFO *pFlushAL, *pFlushNL, *pFlushZL, *pFlushPS, *pFlushPAL;
MM_N_VERTICES_TYPE nPolVertices=0;

    // Setting pointer to 3d structure (if exists).
    if(hMiraMonLayer->TopHeader.bIs3d)
		pZ=hMMFeature->pZCoord;

    // Setting pointers to arc/node structures.
    if(hMiraMonLayer->bIsPolygon)
    {
        pMMArc=&hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader=&hMiraMonLayer->MMPolygon.TopArcHeader;

        pMMNode=&hMiraMonLayer->MMPolygon.MMArc.MMNode;
        pNodeTopHeader=&hMiraMonLayer->MMPolygon.MMArc.TopNodeHeader;
    }
    else
    {
        pMMArc=&hMiraMonLayer->MMArc;
        pArcTopHeader=&hMiraMonLayer->TopHeader;

        pMMNode=&hMiraMonLayer->MMArc.MMNode;
        pNodeTopHeader=&hMiraMonLayer->MMArc.TopNodeHeader;
    }

    // Setting pointers to polygon structures
    if (hMiraMonLayer->bIsPolygon)
    {
        if(MMResizePolHeaderPointer(&hMiraMonLayer->MMPolygon.pPolHeader, 
                    &hMiraMonLayer->MMPolygon.nMaxPolHeader, 
                    pNodeTopHeader->nElemCount+2, 
                        MM_INCR_NUMBER_OF_POLYGONS,
                        0))
        {
            MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                "Memory error in MiraMon "
                "driver (MMResizePolHeaderPointer())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        pCurrentPolHeader=hMiraMonLayer->MMPolygon.pPolHeader+
            hMiraMonLayer->TopHeader.nElemCount;
        MMInitBoundingBox(&pCurrentPolHeader->dfBB);        
        
        pCurrentPolHeader->dfPerimeter=0;
        pCurrentPolHeader->dfArea=0L;
    }

    // Setting flushes to all sections described in 
    // format specifications document.
    pFlushAL=&pMMArc->FlushAL;
    pFlushNL=&pMMNode->FlushNL;
    pFlushZL=&pMMArc->pZSection.FlushZL;
    pFlushPS=&hMiraMonLayer->MMPolygon.FlushPS;
    pFlushPAL=&hMiraMonLayer->MMPolygon.FlushPAL;

    pFlushNL->pBlockWhereToSaveOrRead=(void *)pMMNode->pNL;
    pFlushAL->pBlockWhereToSaveOrRead=(void *)pMMArc->pAL;
    if (hMiraMonLayer->TopHeader.bIs3d)
        pFlushZL->pBlockWhereToSaveOrRead=(void *)pMMArc->pZSection.pZL;
    if (hMiraMonLayer->bIsPolygon)
    {
        pFlushPS->pBlockWhereToSaveOrRead=(void *)hMiraMonLayer->MMPolygon.pPS;
        pFlushPAL->pBlockWhereToSaveOrRead=
            (void *)hMiraMonLayer->MMPolygon.pPAL;
    }

    // Creation of the MiraMon extended database
    if(!hMiraMonLayer->bIsPolygon)
    {
        if(hMiraMonLayer->TopHeader.nElemCount==0)
        {
            MM_CPLDebug("MiraMon", "Creating MiraMon database");
            if(MMCreateMMDB(hMiraMonLayer))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            MM_CPLDebug("MiraMon", "MiraMon database created");
        }
    }
    else
    {   // Universal polygon has been created
        if(hMiraMonLayer->TopHeader.nElemCount==1)
        {
            MM_CPLDebug("MiraMon", "Creating MiraMon database");
            if(MMCreateMMDB(hMiraMonLayer))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            MM_CPLDebug("MiraMon", "MiraMon database created");
            
            // Universal polygon have a record with ID_GRAFIC=0 and blancs
            if(MMAddPolygonRecordToMMDB(hMiraMonLayer, NULL, 0, 0, NULL))
                return MM_FATAL_ERROR_WRITING_FEATURES;
        }
    }

    // Checking if its possible continue writing the file due
    // to version limitations.
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
    {
        MM_FILE_OFFSET nNodeOffset, nArcOffset, nPolOffset;
        MM_INTERNAL_FID nArcElemCount, nNodeElemCount;
        nNodeOffset=pFlushNL->TotalSavedBytes+pFlushNL->nNumBytes;
        nArcOffset=pMMArc->nOffsetArc;
        nPolOffset=pFlushPAL->TotalSavedBytes+pFlushPAL->nNumBytes;

        nArcElemCount=pArcTopHeader->nElemCount;
        nNodeElemCount=pNodeTopHeader->nElemCount;
        for (nIPart=0; nIPart<hMMFeature->nNRings; nIPart++, 
                       nArcElemCount++, 
                       nNodeElemCount+=(hMiraMonLayer->bIsPolygon?1:2))
	    {
            // There is space for the element that is going to be written?
            // Polygon or arc
            if(MMCheckVersionForFID(hMiraMonLayer, hMiraMonLayer->TopHeader.nElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMCheckVersionForFID() (1)");
                return MM_STOP_WRITING_FEATURES;
            }
                
            // Arc if there is no polygon
            if (MMCheckVersionForFID(hMiraMonLayer, nArcElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMCheckVersionForFID() (2)");
                return MM_STOP_WRITING_FEATURES;
            }

            // Nodes
            if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMCheckVersionForFID() (3)");
                return MM_STOP_WRITING_FEATURES;
            }

            // There is space for the last node(s) that is(are) going to be written?
            if(!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount + 1))
                {
                    MM_CPLDebug("MiraMon", "Error in MMCheckVersionForFID() (4)");
                    return MM_STOP_WRITING_FEATURES;
                }
            }

            // Checking offsets
            // AL: check the last point
            if (MMCheckVersionOffset(hMiraMonLayer, nArcOffset))
            {
                MM_CPLDebug("MiraMon", "Error in MMCheckVersionOffset() (0)");
                return MM_STOP_WRITING_FEATURES;
            }
            // Setting next offset
            nArcOffset+=(hMMFeature->pNCoordRing[nIPart])*pMMArc->nALElementSize;

            // NL: check the last node
            if(hMiraMonLayer->bIsPolygon)
                nNodeOffset+=(hMMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;
            else
                nNodeOffset+=(2*hMMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;

            if (MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
            {
                MM_CPLDebug("MiraMon", "Error in MMCheckVersionOffset() (1)");
                return MM_STOP_WRITING_FEATURES;
            }
            // Setting next offset
            nNodeOffset+=MM_SIZE_OF_NL_32BITS;

            if(!hMiraMonLayer->bIsPolygon)
            {
                if (MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
                {
                    MM_CPLDebug("MiraMon", "Error in MMCheckVersionOffset() (2)");
                    return MM_STOP_WRITING_FEATURES;
                }
                // Setting next offset
                nNodeOffset+=MM_SIZE_OF_NL_32BITS;
            }
            
            // PAL
            if (hMiraMonLayer->bIsPolygon)
            {
                nPolOffset+=hMMFeature->nNRings*
                        hMiraMonLayer->MMPolygon.nPSElementSize+
                    hMiraMonLayer->MMPolygon.nPHElementSize+
                    hMMFeature->nNRings*MM_SIZE_OF_PAL_32BITS;   
		    }

            // Where 3D part is going to start
            if (hMiraMonLayer->TopHeader.bIs3d)
            {
                nArcOffset+=hMMFeature->pNCoordRing[nIPart]*pMMArc->nALElementSize;
                if (MMCheckVersionFor3DOffset(hMiraMonLayer, nArcOffset,
                    hMiraMonLayer->TopHeader.nElemCount + hMMFeature->nNRings))
                {
                    MM_CPLDebug("MiraMon", "Error in MMCheckVersionFor3DOffset()");
                    return MM_STOP_WRITING_FEATURES;
                }
            }
        }
    }

    // Going through parts of the feature.
    nExternalRingsCount=0;
    pCoord=hMMFeature->pCoord;
    
    // Doing real job
    for (nIPart=0; nIPart<hMMFeature->nNRings; nIPart++, 
                   pArcTopHeader->nElemCount++, 
                   pNodeTopHeader->nElemCount+=(hMiraMonLayer->bIsPolygon?1:2))
	{
        // Resize structures if necessary
        if(MMResizeArcHeaderPointer(&pMMArc->pArcHeader, 
                        &pMMArc->nMaxArcHeader, 
                        pArcTopHeader->nElemCount+1,
                        MM_INCR_NUMBER_OF_ARCS,
                        0))
        {
            MM_CPLDebug("MiraMon", "Error in MMResizeArcHeaderPointer()");
            MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                "Memory error in MiraMon "
                "driver (MMCreateFeaturePolOrArc())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        if(MMResizeNodeHeaderPointer(&pMMNode->pNodeHeader, 
                        &pMMNode->nMaxNodeHeader, 
                        hMiraMonLayer->bIsPolygon?pNodeTopHeader->nElemCount+1:pNodeTopHeader->nElemCount+2,
                        MM_INCR_NUMBER_OF_NODES,
                        0))
        {
            MM_CPLDebug("MiraMon", "Error in MMResizeNodeHeaderPointer()");
            MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                "Memory error in MiraMon "
                "driver (MMCreateFeaturePolOrArc())");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        if (hMiraMonLayer->TopHeader.bIs3d)
		{
            if(MMResizeZSectionDescrPointer(
                    &pMMArc->pZSection.pZDescription, 
                    &pMMArc->pZSection.nMaxZDescription, 
                    pMMArc->nMaxArcHeader, 
                    MM_INCR_NUMBER_OF_ARCS,
                    0))
            {
                MM_CPLDebug("MiraMon", "Error in MMResizeZSectionDescrPointer()");
                MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                "Memory error in MiraMon "
                "driver (MMCreateFeaturePolOrArc())");
                return MM_FATAL_ERROR_WRITING_FEATURES;
			}
            pZDesc=pMMArc->pZSection.pZDescription;
        }

        // Setting pointers to current headers
        pCurrentArcHeader=pMMArc->pArcHeader+pArcTopHeader->nElemCount;
        MMInitBoundingBox(&pCurrentArcHeader->dfBB);

        pCurrentNodeHeader=pMMNode->pNodeHeader+
            pNodeTopHeader->nElemCount;
        if(!hMiraMonLayer->bIsPolygon)
            pCurrentNodeHeaderPlus1=pCurrentNodeHeader+1;
  
        // Initialiting feature information (section AH/PH)
        pCurrentArcHeader->nElemCount=hMMFeature->pNCoordRing[nIPart];
        pCurrentArcHeader->dfLenght=0.0;
        pCurrentArcHeader->nOffset=pFlushAL->TotalSavedBytes+
            pFlushAL->nNumBytes;

        // Dumping vertices and calculating stuff that 
        // MiraMon needs (longitude/perimeter, area)
        for (nIVertice=0; nIVertice<pCurrentArcHeader->nElemCount; 
            nIVertice++, pCoord++)
        {
			pFlushAL->SizeOfBlockToBeSaved=sizeof(pCoord->dfX);
            pFlushAL->pBlockToBeSaved=(void *)&pCoord->dfX;
            if (MM_AppendBlockToBuffer(pFlushAL))
            {
                MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer() (1)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            pFlushAL->pBlockToBeSaved=(void *)&pCoord->dfY;
            if (MM_AppendBlockToBuffer(pFlushAL))
            {
                MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer() (2)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

			MMUpdateBoundingBoxXY(&pCurrentArcHeader->dfBB, pCoord);
            if(nIVertice==0 || 
                        nIVertice==pCurrentArcHeader->nElemCount-1)
                MMUpdateBoundingBoxXY(&pNodeTopHeader->hBB, pCoord);
            if (nIVertice>0)
			{
                dtempx=pCoord->dfX - (pCoord-1)->dfX;
				dtempy=pCoord->dfY - (pCoord-1)->dfY;
				pCurrentArcHeader->dfLenght += 
                    sqrt(dtempx*dtempx + dtempy*dtempy);
	            if (hMiraMonLayer->bIsPolygon)
                {
					pCurrentPolHeader->dfArea += 
                        ( pCoord->dfX * (pCoord-1)->dfY - 
                        (pCoord-1)->dfX * pCoord->dfY);
                }
			}
		}
        nPolVertices+=pCurrentArcHeader->nElemCount;

        // Updating bounding boxes 
        MMUpdateBoundingBox(&pArcTopHeader->hBB, &pCurrentArcHeader->dfBB);
        if (hMiraMonLayer->bIsPolygon)
            MMUpdateBoundingBox(&hMiraMonLayer->TopHeader.hBB, 
            &pCurrentArcHeader->dfBB);

        pMMArc->nOffsetArc+=
            (pCurrentArcHeader->nElemCount)*pMMArc->nALElementSize;
        
        pCurrentArcHeader->nFirstIdNode=(2*pArcTopHeader->nElemCount);
        if(hMiraMonLayer->bIsPolygon)
        {
            pCurrentArcHeader->nFirstIdNode=pArcTopHeader->nElemCount;
            pCurrentArcHeader->nLastIdNode=pArcTopHeader->nElemCount;
        }
        else
        {
            pCurrentArcHeader->nFirstIdNode=(2*pArcTopHeader->nElemCount);
            pCurrentArcHeader->nLastIdNode=(2*pArcTopHeader->nElemCount+1);
        }
        if (MMAddArcRecordToMMDB(hMiraMonLayer, hMMFeature,
            pArcTopHeader->nElemCount, pCurrentArcHeader))
        {
            MM_CPLDebug("MiraMon", "Error in MMAddArcRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
                
        // Node Stuff: writting NL section
        pCurrentNodeHeader->nArcsCount=1;
        if(hMiraMonLayer->bIsPolygon)
            pCurrentNodeHeader->cNodeType=MM_RING_NODE;
        else
            pCurrentNodeHeader->cNodeType=MM_FINAL_NODE;
        
        pCurrentNodeHeader->nOffset=pFlushNL->TotalSavedBytes+
            pFlushNL->nNumBytes;
        if (MMAppendIntegerDependingOnVersion(hMiraMonLayer,
            pFlushNL,
            &UnsignedLongNumber, pArcTopHeader->nElemCount))
        {
            MM_CPLDebug("MiraMon", "Error in MMAppendIntegerDependingOnVersion()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        // 8bytes alignment
        nOffsetTmp=pFlushNL->TotalSavedBytes+
            pFlushNL->nNumBytes;
        GetOffsetAlignedTo8(&nOffsetTmp);
        if(nOffsetTmp!=pFlushNL->TotalSavedBytes+
            pFlushNL->nNumBytes)
        {
            pFlushNL->SizeOfBlockToBeSaved=
                nOffsetTmp-(pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes);
            pFlushNL->pBlockToBeSaved=(void *)NULL;
            if (MM_AppendBlockToBuffer(pFlushNL))
            {
                MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer() (3)");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        }
        if (MMAddNodeRecordToMMDB(hMiraMonLayer,
            pNodeTopHeader->nElemCount, pCurrentNodeHeader))
        {
            MM_CPLDebug("MiraMon", "Error in MMAddNodeRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        if(!hMiraMonLayer->bIsPolygon)
        {
            pCurrentNodeHeaderPlus1->nArcsCount=1;
            if(hMiraMonLayer->bIsPolygon)
                pCurrentNodeHeaderPlus1->cNodeType=MM_RING_NODE;
            else
                pCurrentNodeHeaderPlus1->cNodeType=MM_FINAL_NODE;
            
            pCurrentNodeHeaderPlus1->nOffset=pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes;

            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                pFlushNL,
                &UnsignedLongNumber, pArcTopHeader->nElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        
            // 8bytes alignment
            nOffsetTmp=pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes;
            GetOffsetAlignedTo8(&nOffsetTmp);
            if(nOffsetTmp!=pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes)
            {
                pFlushNL->SizeOfBlockToBeSaved=
                    nOffsetTmp-(pFlushNL->TotalSavedBytes+
                    pFlushNL->nNumBytes);
                pFlushNL->pBlockToBeSaved=(void *)NULL;
                if (MM_AppendBlockToBuffer(pFlushNL))
                {
                    MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }
            }
            if (MMAddNodeRecordToMMDB(hMiraMonLayer,
                pNodeTopHeader->nElemCount + 1, pCurrentNodeHeaderPlus1))
            {
                MM_CPLDebug("MiraMon", "Error in MMAddNodeRecordToMMDB()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
        }

        // 3D stuff
		if (hMiraMonLayer->TopHeader.bIs3d)
        {
            pZDesc[pArcTopHeader->nElemCount].dfBBminz=
                STATISTICAL_UNDEF_VALUE;
            pZDesc[pArcTopHeader->nElemCount].dfBBmaxz=
                -STATISTICAL_UNDEF_VALUE;
            for (nIVertice=0; nIVertice<pCurrentArcHeader->nElemCount; 
                nIVertice++, pZ++)
            {
            	pFlushZL->SizeOfBlockToBeSaved=sizeof(*pZ);
                pFlushZL->pBlockToBeSaved=(void *)pZ;
                if (MM_AppendBlockToBuffer(pFlushZL))
                {
                    MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer()");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
                }

                if (pZDesc[pArcTopHeader->nElemCount].dfBBminz>*pZ)
                	pZDesc[pArcTopHeader->nElemCount].dfBBminz=*pZ;
                if (pZDesc[pArcTopHeader->nElemCount].dfBBmaxz<*pZ)
                	pZDesc[pArcTopHeader->nElemCount].dfBBmaxz=*pZ;
			}
            pZDesc[pArcTopHeader->nElemCount].nZCount=1;
            if(hMiraMonLayer->TopHeader.nElemCount==0)
                pZDesc[hMiraMonLayer->TopHeader.nElemCount].nOffsetZ=0;
            else
                pZDesc[hMiraMonLayer->TopHeader.nElemCount].nOffsetZ=
                    pZDesc[hMiraMonLayer->TopHeader.nElemCount-1].nOffsetZ+sizeof(*pZ);
        }
        
        // Exclusive polygon stuff
        if (hMiraMonLayer->bIsPolygon)
        {
            // PS SECTION
            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                pFlushPS,
                &UnsignedLongNumber, 0))
            {
                MM_CPLDebug("MiraMon", "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
            
            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                pFlushPS,
                &UnsignedLongNumber, hMiraMonLayer->TopHeader.nElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }
            
            // PAL SECTION
			// Vertices of rings defining
			// holes in polygons are in a counterclockwise direction.
            // Hole are at the end of al external rings that contain the hole!!
            VFG=0L;
			VFG|=MM_END_ARC_IN_RING;
            if(hMMFeature->pbArcInfo[nIPart])
            {
				nExternalRingsCount++;
				VFG|=MM_EXTERIOR_ARC_SIDE;
			}

			pCurrentPolHeader->nArcsCount=(MM_POLYGON_ARCS_COUNT)hMMFeature->nNRings;
            pCurrentPolHeader->nExternalRingsCount=nExternalRingsCount;
            pCurrentPolHeader->nRingsCount=hMMFeature->nNRings;
            if(nIPart==0)
            {
                pCurrentPolHeader->nOffset=pFlushPAL->TotalSavedBytes+
                   pFlushPAL->nNumBytes;
            }

            if(nIPart==hMMFeature->nNRings-1)
                pCurrentPolHeader->dfArea/=2;

            pFlushPAL->SizeOfBlockToBeSaved=1;
            pFlushPAL->pBlockToBeSaved=(void *)&VFG;
            if (MM_AppendBlockToBuffer(pFlushPAL))
            {
                MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            if (MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPAL,
                &UnsignedLongNumber, pArcTopHeader->nElemCount))
            {
                MM_CPLDebug("MiraMon", "Error in MMAppendIntegerDependingOnVersion()");
                return MM_FATAL_ERROR_WRITING_FEATURES;
            }

            // 8bytes alignment
            if(nIPart==hMMFeature->nNRings-1)
            {
                nOffsetTmp=pFlushPAL->TotalSavedBytes+
                    pFlushPAL->nNumBytes;
                GetOffsetAlignedTo8(&nOffsetTmp);

                if(nOffsetTmp!=pFlushPAL->TotalSavedBytes+
                    pFlushPAL->nNumBytes)
                {
                    pFlushPAL->SizeOfBlockToBeSaved=
                        nOffsetTmp-(pFlushPAL->TotalSavedBytes+
                            pFlushPAL->nNumBytes);
                    pFlushPAL->pBlockToBeSaved=(void *)NULL;
                    if (MM_AppendBlockToBuffer(pFlushPAL))
                    {
                        MM_CPLDebug("MiraMon", "Error in MM_AppendBlockToBuffer()");
                        return MM_FATAL_ERROR_WRITING_FEATURES;
                    }
                }
            }

            MMUpdateBoundingBox(&pCurrentPolHeader->dfBB, &pCurrentArcHeader->dfBB);
            pCurrentPolHeader->dfPerimeter+=pCurrentArcHeader->dfLenght;
		}
    }

    // Updating element count and if the polygon is multipart.
    // MiraMon doesn't accept multipoints or multilines, only multipolygons.
    if(hMiraMonLayer->bIsPolygon)
    {
        if (MMAddPolygonRecordToMMDB(hMiraMonLayer, hMMFeature,
            hMiraMonLayer->TopHeader.nElemCount,
            nPolVertices, pCurrentPolHeader))
        {
            MM_CPLDebug("MiraMon", "Error in MMAddPolygonRecordToMMDB()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        hMiraMonLayer->TopHeader.nElemCount++;

	    if(nExternalRingsCount>1)
            hMiraMonLayer->TopHeader.bIsMultipolygon=TRUE;
    }

	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePolOrArc()

int MMCreateRecordDBF(struct MiraMonVectLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMMFeature)
{
int result;

    if(hMiraMonLayer->TopHeader.nElemCount==0)
    {
        if(MMCreateMMDB(hMiraMonLayer))
            return MM_FATAL_ERROR_WRITING_FEATURES;
    }

    result=MMAddDBFRecordToMMDB(hMiraMonLayer, hMMFeature);
    if(result==MM_FATAL_ERROR_WRITING_FEATURES ||
        result== MM_STOP_WRITING_FEATURES)
        return result;
    
    // Everything OK.
	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateRecordDBF()

int MMCreateFeaturePoint(struct MiraMonVectLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMMFeature)
{
double *pZ=NULL;
struct MM_POINT_2D *pCoord;
unsigned __int64 nIPart, nIVertice;
unsigned __int64 nCoord;
struct MM_ZD *pZDescription=NULL;
MM_INTERNAL_FID nElemCount;
int result;

    if(hMiraMonLayer->TopHeader.bIs3d)
		pZ=hMMFeature->pZCoord;

    nElemCount=hMiraMonLayer->TopHeader.nElemCount;
    for (nIPart=0, pCoord=hMMFeature->pCoord;
        nIPart<hMMFeature->nNRings; nIPart++, nElemCount++)
	{
        nCoord=hMMFeature->pNCoordRing[nIPart];
        
        // Checking if its possible continue writing the file due
        // to version limitations.
        if(MMCheckVersionForFID(hMiraMonLayer, 
                hMiraMonLayer->TopHeader.nElemCount+nCoord))
            return MM_STOP_WRITING_FEATURES;

        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if(nElemCount==0)
            {
                if(MMCheckVersionFor3DOffset(hMiraMonLayer,
                        0, nElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }
            else
            {
                pZDescription=hMiraMonLayer->MMPoint.pZSection.pZDescription;
                if(MMCheckVersionFor3DOffset(hMiraMonLayer,
                        pZDescription[nElemCount-1].nOffsetZ+
                        sizeof(*pZ), nElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }
        }

        // Doing real job
        // Memory issues
        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if(MMResizeZSectionDescrPointer(&hMiraMonLayer->MMPoint.pZSection.pZDescription, 
                    &hMiraMonLayer->MMPoint.pZSection.nMaxZDescription, 
                    nElemCount, 
                    MM_INCR_NUMBER_OF_POINTS,
                    0))
            {
                MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Memory error in MiraMon "
                    "driver (MMCreateFeaturePoint())");
                return MM_FATAL_ERROR_WRITING_FEATURES;
			}
            
            pZDescription=hMiraMonLayer->MMPoint.pZSection.pZDescription;

            pZDescription[nElemCount].dfBBminz=*pZ;
            pZDescription[nElemCount].dfBBmaxz=*pZ;
            pZDescription[nElemCount].nZCount=1;
            if(nElemCount==0)
                pZDescription[nElemCount].nOffsetZ=0;
            else
                pZDescription[nElemCount].nOffsetZ=
                    pZDescription[nElemCount-1].nOffsetZ+sizeof(*pZ);
        }
           
        // Flush settings
        hMiraMonLayer->MMPoint.FlushTL.pBlockWhereToSaveOrRead=(void *)hMiraMonLayer->MMPoint.pTL;
        if (hMiraMonLayer->TopHeader.bIs3d)
            hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockWhereToSaveOrRead=(void *)hMiraMonLayer->MMPoint.pZSection.pZL;

        // Dump point or points (MiraMon doesn't have multiple points)
        for (nIVertice=0; nIVertice<nCoord; nIVertice++, pCoord++, pZ++)
        {
            // Updating the bounding box of the layer
            MMUpdateBoundingBoxXY(&hMiraMonLayer->TopHeader.hBB, pCoord);

            // Adding the point at the memory block
            hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved=sizeof(pCoord->dfX);
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfX;
            if(MM_AppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfY;
            if(MM_AppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return MM_FATAL_ERROR_WRITING_FEATURES;

            // Adding the 3D part, if exists, at the memory block
	        if (hMiraMonLayer->TopHeader.bIs3d)
            {
        	    hMiraMonLayer->MMPoint.pZSection.FlushZL.SizeOfBlockToBeSaved=sizeof(*pZ);
                hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockToBeSaved=(void *)pZ;
                if(MM_AppendBlockToBuffer(&hMiraMonLayer->MMPoint.pZSection.FlushZL))
                    return MM_FATAL_ERROR_WRITING_FEATURES;

                if (pZDescription[nElemCount].dfBBminz>*pZ)
                	pZDescription[nElemCount].dfBBminz=*pZ;
                if (pZDescription[nElemCount].dfBBmaxz<*pZ)
                	pZDescription[nElemCount].dfBBmaxz=*pZ;

                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz>*pZ)
                	hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBminz=*pZ;
                if (hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz<*pZ)
                	hMiraMonLayer->MMPoint.pZSection.ZHeader.dfBBmaxz=*pZ;
			}
        }

        if(hMiraMonLayer->TopHeader.nElemCount==0)
        {
            if(MMCreateMMDB(hMiraMonLayer))
                return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        result=MMAddPointRecordToMMDB(hMiraMonLayer, hMMFeature, nElemCount);
        if(result==MM_FATAL_ERROR_WRITING_FEATURES ||
            result== MM_STOP_WRITING_FEATURES)
            return result;
    }
    // Updating nElemCount at the header of the layer
    hMiraMonLayer->TopHeader.nElemCount=nElemCount;

    // Everything OK.
	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePoint()

int MMCheckVersionForFID(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                         MM_INTERNAL_FID FID)
{
    if(hMiraMonLayer->LayerVersion!=MM_32BITS_VERSION)
        return 0;

    if(FID>=MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS)
            return 1;
    return 0;
}

int MMCheckVersionOffset(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                                MM_FILE_OFFSET OffsetToCheck)
{
    // Checking if the final version is 1.1 or 2.0
    if(hMiraMonLayer->LayerVersion!=MM_32BITS_VERSION)
        return 0;
    
    // User decided that if necessary, output version can be 2.0
    if(OffsetToCheck<MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;
    
    return 1;
}

int MMCheckVersionFor3DOffset(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                                MM_FILE_OFFSET nOffset, 
                                MM_INTERNAL_FID nElemCount)
{
MM_FILE_OFFSET LastOffset;

    // Checking if the final version is 1.1 or 2.0
    if(hMiraMonLayer->LayerVersion!=MM_32BITS_VERSION)
        return 0;
    
    // User decided that if necessary, output version can be 2.0
    LastOffset=nOffset+
        MM_HEADER_SIZE_32_BITS+
        nElemCount*MM_SIZE_OF_TL;

	LastOffset+=MM_SIZE_OF_ZH;
	LastOffset+=nElemCount*MM_SIZE_OF_ZD_32_BITS;	

    if(LastOffset<MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;
    
    return 1;
}

int AddMMFeature(struct MiraMonVectLayerInfo *hMiraMonLayer, 
        struct MiraMonFeature *hMiraMonFeature)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)
    
    if(!hMiraMonLayer->bIsBeenInit)
    {
        if (MMInitLayerByType(hMiraMonLayer))
        {
            MM_CPLDebug("MiraMon", "Error in MMInitLayerByType()");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        hMiraMonLayer->bIsBeenInit = 1;
    }

    if(hMiraMonLayer->bIsPoint)
        return MMCreateFeaturePoint(hMiraMonLayer, hMiraMonFeature);
    else if(hMiraMonLayer->bIsArc || hMiraMonLayer->bIsPolygon)
        return MMCreateFeaturePolOrArc(hMiraMonLayer, hMiraMonFeature);
    else
    {
        // Adding a record to DBF file
        return MMCreateRecordDBF(hMiraMonLayer, hMiraMonFeature);
    }
}


/* -------------------------------------------------------------------- */
/*      Tools that MiraMon uses                                         */
/* -------------------------------------------------------------------- */
unsigned long GetUnsignedLongFromINT64(unsigned __int64 Nnumber)
{
    unsigned long Nulnumber=(unsigned long) Nnumber;
    if(Nulnumber!=Nnumber)
        return (unsigned long )-1; // To detect out of range
    return Nulnumber;
}

void MMInitBoundingBox(struct MMBoundingBox *dfBB)
{
    dfBB->dfMinX=dfBB->dfMinX=STATISTICAL_UNDEF_VALUE;
    dfBB->dfMinX=dfBB->dfMaxX=-STATISTICAL_UNDEF_VALUE;
    dfBB->dfMinX=dfBB->dfMinY=STATISTICAL_UNDEF_VALUE;
    dfBB->dfMaxX=dfBB->dfMaxY=-STATISTICAL_UNDEF_VALUE;
}

void MMUpdateBoundingBox(struct MMBoundingBox *dfBBToBeAct, 
            struct MMBoundingBox *dfBBWithData)
{
    if(dfBBToBeAct->dfMinX>dfBBWithData->dfMinX)
        dfBBToBeAct->dfMinX=dfBBWithData->dfMinX;

    if(dfBBToBeAct->dfMinY>dfBBWithData->dfMinY)
        dfBBToBeAct->dfMinY=dfBBWithData->dfMinY;

    if(dfBBToBeAct->dfMaxX<dfBBWithData->dfMaxX)
        dfBBToBeAct->dfMaxX=dfBBWithData->dfMaxX;

    if(dfBBToBeAct->dfMaxY<dfBBWithData->dfMaxY)
        dfBBToBeAct->dfMaxY=dfBBWithData->dfMaxY;
}

void MMUpdateBoundingBoxXY(struct MMBoundingBox *dfBB, 
                        struct MM_POINT_2D *pCoord)
{
    if(pCoord->dfX<dfBB->dfMinX)
        dfBB->dfMinX=pCoord->dfX;

    if(pCoord->dfY<dfBB->dfMinY)
        dfBB->dfMinY=pCoord->dfY;

    if(pCoord->dfX>dfBB->dfMaxX)
        dfBB->dfMaxX=pCoord->dfX;

    if(pCoord->dfY>dfBB->dfMaxY)
        dfBB->dfMaxY=pCoord->dfY;
}

/* -------------------------------------------------------------------- */
/*      Resize reused structures if needed                              */
/* -------------------------------------------------------------------- */
int MMResizeMiraMonFieldValue(struct MiraMonFieldValue **pFieldValue, 
                        unsigned __int32 *nMax, 
                        unsigned __int32 nNum, 
                        unsigned __int32 nIncr,
                        unsigned __int32 nProposedMax)
{
unsigned __int32 nPrevMax;

    if(nNum<*nMax)
        return 0;

    nPrevMax=*nMax;
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pFieldValue)=realloc_function(*pFieldValue, 
        *nMax*sizeof(**pFieldValue)))==NULL)
		return 1;

    memset((*pFieldValue)+nPrevMax, 0, (*nMax-nPrevMax)*sizeof(**pFieldValue));
    return 0;
}

int MMResizeMiraMonPolygonArcs(struct MM_PAL_MEM **pFID, 
                        MM_POLYGON_ARCS_COUNT *nMax, 
                        MM_POLYGON_ARCS_COUNT nNum, 
                        MM_POLYGON_ARCS_COUNT nIncr,
                        MM_POLYGON_ARCS_COUNT nProposedMax)
{
MM_POLYGON_ARCS_COUNT nPrevMax;

    if(nNum<*nMax)
        return 0;

    nPrevMax=*nMax;
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pFID)=realloc_function(*pFID, 
        *nMax*sizeof(**pFID)))==NULL)
		return 1;

    memset((*pFID)+nPrevMax, 0, (*nMax-nPrevMax)*sizeof(**pFID));
    return 0;
}

int MMResizeMiraMonRecord(struct MiraMonRecord **pMiraMonRecord, 
                        MM_EXT_DBF_N_MULTIPLE_RECORDS *nMax, 
                        MM_EXT_DBF_N_MULTIPLE_RECORDS nNum, 
                        MM_EXT_DBF_N_MULTIPLE_RECORDS nIncr,
                        MM_EXT_DBF_N_MULTIPLE_RECORDS nProposedMax)
{
unsigned __int32 nPrevMax;

    if(nNum<*nMax)
        return 0;
    
    nPrevMax=*nMax;
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pMiraMonRecord)=realloc_function(*pMiraMonRecord, 
        *nMax*sizeof(**pMiraMonRecord)))==NULL)
		return 1;

    memset((*pMiraMonRecord)+nPrevMax, 0, (*nMax-nPrevMax)*sizeof(**pMiraMonRecord));
    return 0;
}

int MMResizeZSectionDescrPointer(struct MM_ZD **pZDescription, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pZDescription)=realloc_function(*pZDescription, 
        *nMax*sizeof(**pZDescription)))==NULL)
		return 1;
    return 0;
}

int MMResizeNodeHeaderPointer(struct MM_NH **pNodeHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pNodeHeader)=realloc_function(*pNodeHeader, 
        *nMax*sizeof(**pNodeHeader)))==NULL)
		return 1;
    return 0;
}

int MMResizeArcHeaderPointer(struct MM_AH **pArcHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pArcHeader)=realloc_function(*pArcHeader, 
        *nMax*sizeof(**pArcHeader)))==NULL)
		return 1;
    return 0;
}

int MMResizePolHeaderPointer(struct MM_PH **pPolHeader, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pPolHeader)=realloc_function(*pPolHeader, 
        *nMax*sizeof(**pPolHeader)))==NULL)
		return 1;
    return 0;
}

int MMResize_MM_N_VERTICES_TYPE_Pointer(MM_N_VERTICES_TYPE **pVrt, 
                        MM_POLYGON_RINGS_COUNT *nMax, 
                        MM_POLYGON_RINGS_COUNT nNum, 
                        MM_POLYGON_RINGS_COUNT nIncr,
                        MM_POLYGON_RINGS_COUNT nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pVrt)=realloc_function(*pVrt, 
        *nMax*sizeof(**pVrt)))==NULL)
		return 1;
    return 0;
}

int MMResizeIntPointer(int **pInt, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pInt)=realloc_function(*pInt, *nMax*sizeof(**pInt)))==NULL)
		return 1;
    return 0;
}

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D, 
                        MM_N_VERTICES_TYPE *nMax, 
                        MM_N_VERTICES_TYPE nNum, 
                        MM_N_VERTICES_TYPE nIncr,
                        MM_N_VERTICES_TYPE nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pPoint2D)=realloc_function(*pPoint2D, 
        *nMax*sizeof(**pPoint2D)))==NULL)
		return 1;
    return 0;
}

int MMResizeDoublePointer(double **pDouble, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pDouble)=realloc_function(*pDouble, 
        *nMax*sizeof(**pDouble)))==NULL)
		return 1;
    return 0;
}

int MM_ResizeStringToOperateIfNeeded(struct MiraMonVectLayerInfo *hMiraMonLayer,
        MM_EXT_DBF_N_FIELDS nNewSize)
{
    if(nNewSize>=hMiraMonLayer->nNumStringToOperate)
    {
        char *p;
		p=(char *)calloc_function(nNewSize);
        if(!p)
        {
            MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Memory error in MiraMon "
                    "driver (MM_ResizeStringToOperateIfNeeded())");
            return 1;
        }
        hMiraMonLayer->szStringToOperate=p;
        hMiraMonLayer->nNumStringToOperate=nNewSize;
    }
    return 0;
}

int IsEmptyString(const char *string)
{
char *ptr;

	for (ptr=(char*)string; *ptr; ptr++)
		if (*ptr!=' ' && *ptr!='\t')
			return 0;

	return 1;
}

const char * ReturnValueFromSectionINIFile(const char *filename, const char *section, const char *key)
{
FILE *file;
#define MAX_LINE_LENGTH 1024*40
char line[MAX_LINE_LENGTH];
int section_found = 0;
char *trimmed_line=NULL, *parsed_key=NULL, *parsed_value=NULL;

    if(!filename || !section)
        return NULL;

    if(NULL==(file = fopen(filename, "r")))
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "Cannot open INI file %s. ",
                             filename);
        return NULL;
    }

    while (fgets(line, sizeof(line), file))
    {
        // Remove blank spaces and new line characters from the end of the line
        trimmed_line  = strtok(line, "\r\n");
        
        if (trimmed_line == NULL)
            continue;
        
        if (section_found)
        {
            char *equal_sign;

            if(!key)
                return section;

            // Find the key in the section
            equal_sign = strchr(trimmed_line, '=');
            if (equal_sign != NULL)
            {
                *equal_sign = '\0';
                parsed_key = trimmed_line;
                parsed_value = equal_sign + 1;

                if(!parsed_value)
                    continue;

                // Remove whitespace around key and value
                while (*parsed_key == ' ' || *parsed_key == '\t')
                    parsed_key++;
                while (*parsed_value == ' ' || *parsed_value == '\t')
                    parsed_value++;

                // Check if the key matches
                if (strcmp(parsed_key, key) == 0)
                {
                    fclose(file);
                    return parsed_value;
                }
            }
        }
        else
        {
            // Find the section
            if (trimmed_line[0] == '[')
            {
                // Remove brackets
                char *section_name = trimmed_line + 1;
                char *closing_bracket = strchr(section_name, ']');
                if (closing_bracket != NULL)
                {
                    *closing_bracket = '\0';
                    // Check if section matches
                    if (strcmp(section_name, section) == 0)
                        section_found = 1;
                }
            }
        }
    }

    fclose(file);
    return NULL;
}

/* -------------------------------------------------------------------- */
/*      Metadata Functions                                              */
/* -------------------------------------------------------------------- */
int ReturnCodeFromMM_m_idofic(char* pMMSRS_or_pSRS, char * szResult, MM_BYTE direction)
{
    static char aEPSGCodeSRS[MM_MAX_ID_SNY];
    const char* aMMIDDBFFile = NULL; //m_idofic.dbf
    FILE* pfMMSRS;
    size_t nLong;
    int nLongBuffer = 5000;
    char* pszBuffer = calloc_function(nLongBuffer);
    char *id_geodes, *psidgeodes, *epsg;

    if (!pszBuffer)
    {
        printf("No memory.\n");
        return 1;
    }
    if (!pMMSRS_or_pSRS)
    {
        free_function(pszBuffer);
        return 1;
    }
    memset(aEPSGCodeSRS, '\0', sizeof(*aEPSGCodeSRS));

    #ifdef GDAL_COMPILATION
    aMMIDDBFFile=CPLFindFile("gdal", "MM_m_idofic.csv");
    #else
    aMMIDDBFFile=strdup("m_idofic.csv");
    #endif

    if(!aMMIDDBFFile)
    {
        free_function(pszBuffer);
        printf("Error opening data\\MM_m_idofic.csv.\n");
        return 1;
    }

    // Opening the file with SRS information
    if(NULL==(pfMMSRS=fopen(aMMIDDBFFile, "r")))
    {
        free_function(pszBuffer);
        printf("Error opening data\\m_idofic.csv.\n");
        return 1;
    }

    // Checking the header of the csv file
    memset(pszBuffer, 0, nLongBuffer);
    fgets(pszBuffer, nLongBuffer, pfMMSRS);
    id_geodes=strstr(pszBuffer, "ID_GEODES");
    if(!id_geodes)
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        printf("Wrong format in data\\m_idofic.csv.\n");
        return 1;
    }
    id_geodes[strlen("ID_GEODES")]='\0';
    psidgeodes=strstr(pszBuffer, "PSIDGEODES");
    if(!psidgeodes)
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        printf("Wrong format in data\\m_idofic.csv.\n");
        return 1;
    }
    psidgeodes[strlen("PSIDGEODES")]='\0';

    // Is PSIDGEODES in first place?
    if(strncmp(pszBuffer, psidgeodes, strlen("PSIDGEODES")))
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        printf("Wrong format in data\\m_idofic.csv.\n");
        return 1;
    }
    // Is ID_GEODES after PSIDGEODES?
    if(strncmp(pszBuffer+strlen("PSIDGEODES")+1, "ID_GEODES", strlen("ID_GEODES")))
    {
        free_function(pszBuffer);
        fclose(pfMMSRS);
        printf("Wrong format in data\\m_idofic.csv.\n");
        return 1;
    }

    // Looking for the information
    while(fgets(pszBuffer, nLongBuffer, pfMMSRS))
    {
        id_geodes=strstr(pszBuffer, ";");
        if(!id_geodes || (id_geodes+1)[0]=='\n')
        {
            free_function(pszBuffer);
            fclose(pfMMSRS);
            printf("Wrong format in data\\m_idofic.csv.\n");
            return 1;
        }

        psidgeodes=strstr(id_geodes+1, ";");
        if(!psidgeodes)
        {
            free_function(pszBuffer);
            fclose(pfMMSRS);
            printf("Wrong format in data\\m_idofic.csv.\n");
            return 1;
        }

        id_geodes[(ptrdiff_t)psidgeodes-(ptrdiff_t)id_geodes]='\0';
        psidgeodes=pszBuffer;
        psidgeodes[(ptrdiff_t)id_geodes-(ptrdiff_t)psidgeodes]='\0';
        id_geodes++;

        if(direction==EPSG_FROM_MMSRS)
        {
            // I have pMMSRS and I want pSRS
            if(strcmp(pMMSRS_or_pSRS, id_geodes))
                continue;

            epsg=strstr(psidgeodes, "EPSG:");
            nLong=strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg + nLong)
                {
                    strcpy(szResult, epsg + nLong);
                    free_function(pszBuffer);
                    fclose(pfMMSRS);
                    return 0; // found
                }
                else
                {
                    free_function(pszBuffer);
                    fclose(pfMMSRS);
                    *szResult='\0';
                    return 1; // not found
                }
            }
        }
        else
        {
            // I have pSRS and I want pMMSRS
            epsg=strstr(psidgeodes, "EPSG:");
            nLong=strlen("EPSG:");
            if (epsg && !strncmp(epsg, psidgeodes, nLong))
            {
                if (epsg + nLong)
                {
                    if(!strcmp(pMMSRS_or_pSRS, epsg + nLong))
                    {
                        strcpy(szResult, id_geodes);
                        free_function(pszBuffer);
                        fclose(pfMMSRS);
                        return 0; // found
                    }
                }
            }
        }
    }
        
    free_function(pszBuffer);
    fclose(pfMMSRS);
    return 1; // not found
}

char *GenerateFileIdentifierFromMetadataFileName(char *pMMFN)
{
    static char aCharRand[7], aCharset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i, len_charset;
    static char aFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER];

    aCharRand[0]='_';
    len_charset=(int)strlen(aCharset);
    for(i=1;i<7;i++)
        aCharRand[i]=aCharset[rand()%(len_charset-1)];
    MM_strnzcpy(aFileIdentifier, pMMFN, MM_MAX_LEN_LAYER_IDENTIFIER-7);
    strcat(aFileIdentifier,aCharRand);
    return aFileIdentifier;
}

/* -------------------------------------------------------------------- */
/*      MiraMon metadata functions                                      */
/* -------------------------------------------------------------------- */
int MMWriteMetadataFile(struct MiraMonVectorMetaData *hMMMD)
{
char aMessage[MM_MESSAGE_LENGHT], 
     aFileIdentifier[MM_MAX_LEN_LAYER_IDENTIFIER],
     aMMIDSRS[MM_MAX_ID_SNY];
MM_EXT_DBF_N_FIELDS nIField;
FILE_TYPE *pF;
time_t currentTime;
struct tm *pLocalTime;
char aTimeString[30];

    if(!hMMMD->aLayerName)
        return 0;

    if(NULL==(pF=fopen_function(hMMMD->aLayerName, "w+t")))
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file %s must exist.",
                             hMMMD->aLayerName);
        return 1;
    }
   
    // Writing MiraMon version section
    printf_function(pF, "[%s]\n", SECTION_VERSIO);
    
    printf_function(pF, "%s=%u\n", KEY_Vers, (unsigned)MM_VERS);
    printf_function(pF, "%s=%u\n", KEY_SubVers, (unsigned)MM_SUBVERS);

    printf_function(pF, "%s=%u\n", KEY_VersMetaDades, (unsigned)MM_VERS_METADADES);
    printf_function(pF, "%s=%u\n", KEY_SubVersMetaDades, (unsigned)MM_SUBVERS_METADADES);

    // Writing METADADES section
    printf_function(pF, "\n[%s]\n", SECTION_METADADES);
    strcpy(aMessage, hMMMD->aLayerName);
    strcpy(aFileIdentifier, GenerateFileIdentifierFromMetadataFileName(aMessage));
    printf_function(pF, "%s=%s\n", KEY_FileIdentifier, aFileIdentifier);
    printf_function(pF, "%s=%s\n", KEY_language, KEY_Value_eng);
    printf_function(pF, "%s=%s\n", KEY_MDIdiom, KEY_Value_eng);
    printf_function(pF, "%s=%s\n", KEY_characterSet, KEY_Value_characterSet);

    // Writing IDENTIFICATION section
    printf_function(pF, "\n[%s]\n", SECTION_IDENTIFICATION);
    printf_function(pF, "%s=%s\n", KEY_code, aFileIdentifier);
    printf_function(pF, "%s=\n", KEY_codeSpace);
    if(hMMMD->szLayerTitle && !IsEmptyString(hMMMD->szLayerTitle))
    {
        if(hMMMD->ePlainLT==MM_LayerType_Point)
            printf_function(pF, "%s=%s (pnt)\n", KEY_DatasetTitle, hMMMD->szLayerTitle);
        if(hMMMD->ePlainLT==MM_LayerType_Arc)
            printf_function(pF, "%s=%s (arc)\n", KEY_DatasetTitle, hMMMD->szLayerTitle);
        if(hMMMD->ePlainLT==MM_LayerType_Pol)
            printf_function(pF, "%s=%s (pol)\n", KEY_DatasetTitle, hMMMD->szLayerTitle);
    }
    printf_function(pF, "%s=%s\n", KEY_language, KEY_Value_eng);

    if(hMMMD->ePlainLT!=MM_LayerType_Node)
    {
        if(hMMMD->pSRS && hMMMD->ePlainLT!=MM_LayerType_Pol)
        {
            printf_function(pF, "\n[%s:%s]\n", SECTION_SPATIAL_REFERENCE_SYSTEM, SECTION_HORIZONTAL);
            ReturnMMIDSRSFromEPSGCodeSRS(hMMMD->pSRS,aMMIDSRS);
            if(!IsEmptyString(aMMIDSRS))
                printf_function(pF, "%s=%s\n", KEY_HorizontalSystemIdentifier, aMMIDSRS);
            else
            {
                MM_CPLWarning(CE_Warning, CPLE_NotSupported,
                            "The MiraMon driver cannot assign any HRS.");
    
                printf_function(pF, "%s=plane\n", KEY_HorizontalSystemIdentifier);
                printf_function(pF, "%s=local\n", KEY_HorizontalSystemDefinition);
                if(hMMMD->pXUnit)
                    printf_function(pF, "%s=%s\n", KEY_unitats, hMMMD->pXUnit);
                if(hMMMD->pYUnit)
                {
                    if(!hMMMD->pXUnit || stricmp(hMMMD->pXUnit, hMMMD->pYUnit))
                        printf_function(pF, "%s=%s\n", KEY_unitatsY, hMMMD->pYUnit);
                }
            }
        }
        else
        {
            printf_function(pF, "%s=plane\n", KEY_HorizontalSystemIdentifier);
            printf_function(pF, "%s=local\n", KEY_HorizontalSystemDefinition);
            if(hMMMD->pXUnit)
            {
                printf_function(pF, "%s=%s\n", KEY_unitats, hMMMD->pXUnit);
                if(hMMMD->pYUnit)
                {
                    if(!hMMMD->pXUnit || stricmp(hMMMD->pXUnit, hMMMD->pYUnit))
                        printf_function(pF, "%s=%s\n", KEY_unitatsY, hMMMD->pYUnit);
                }
            }
        }
    }
    
    // Writing OVERVIEW:ASPECTES_TECNICS in polygon metadata file. 
    // ArcSource=fitx_pol.arc
    if(hMMMD->ePlainLT==MM_LayerType_Pol)
    {
        printf_function(pF, "\n[%s]\n", SECTION_OVVW_ASPECTES_TECNICS);
        printf_function(pF, "%s=\"%s\"\n", KEY_ArcSource, hMMMD->aArcFile);
    }
    
    // Writing EXTENT section
    printf_function(pF, "\n[%s]\n", SECTION_EXTENT);
    printf_function(pF, "%s=0\n", KEY_toler_env);
    
    if(hMMMD->hBB.dfMinX!=MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxX!=-MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMinY!=MM_UNDEFINED_STATISTICAL_VALUE &&
        hMMMD->hBB.dfMaxY!=-MM_UNDEFINED_STATISTICAL_VALUE)
    {
        printf_function(pF, "%s=%lf\n", KEY_MinX, hMMMD->hBB.dfMinX);
        printf_function(pF, "%s=%lf\n", KEY_MaxX, hMMMD->hBB.dfMaxX);
        printf_function(pF, "%s=%lf\n", KEY_MinY, hMMMD->hBB.dfMinY);
        printf_function(pF, "%s=%lf\n", KEY_MaxY, hMMMD->hBB.dfMaxY);
    }
    
    // Writing OVERVIEW section
    printf_function(pF, "\n[%s]\n", SECTION_OVERVIEW);
        
    currentTime = time(NULL);
    pLocalTime = localtime(&currentTime);
    sprintf(aTimeString, "%04d%02d%02d %02d%02d%02d%02d+00:00",
        pLocalTime->tm_year + 1900, pLocalTime->tm_mon + 1, pLocalTime->tm_mday,
        pLocalTime->tm_hour, pLocalTime->tm_min, pLocalTime->tm_sec, 0);
    printf_function(pF, "%s=%s\n", KEY_CreationDate, aTimeString);

    // ·$· TEMPORAL MENTRE NO HO FEM BÉ:
    // A la documentació posa:
    // -preserve_fid
    // Use the FID of the source features instead of letting the output driver automatically 
    // assign a new one (for formats that require a FID). If not in append mode, 
    // this behavior is the default if the output driver has a FID layer creation 
    // option, in which case the name of the source FID column will be used and source 
    // feature IDs will be attempted to be preserved. This behavior can be disabled by setting -unsetFid.

    printf_function(pF, "\n");
    printf_function(pF, "[TAULA_PRINCIPAL]\n");
    printf_function(pF, "IdGrafic=ID_GRAFIC\n");
    printf_function(pF, "TipusRelacio=RELACIO_1_1_DICC\n");

    printf_function(pF, "\n");
    printf_function(pF, "[TAULA_PRINCIPAL:ID_GRAFIC]\n");
    printf_function(pF, "visible=1\n");
    printf_function(pF, "MostrarUnitats=0\n");
    printf_function(pF, "descriptor=Internal graphic identifier\n");
    
    if(hMMMD->ePlainLT==MM_LayerType_Arc)
    {
        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:N_VERTEXS]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Number of vertices\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:LONG_ARC]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Lenght of arc\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:NODE_INI]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Initial node\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:NODE_FI]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Final node\n");

        printf_function(pF, "[GEOMETRIA_I_TOPOLOGIA]\n");
        printf_function(pF, "NomCampNVertexs=N_VERTEXS\n");
        printf_function(pF, "NomCampLongitudArc=LONG_ARC\n");
        printf_function(pF, "NomCampNodeIni=NODE_INI\n");
        printf_function(pF, "NomCampNodeFi=NODE_FI\n");
    }
    else if(hMMMD->ePlainLT==MM_LayerType_Node)
    {
        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:ARCS_A_NOD]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Number of arcs to node\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:TIPUS_NODE]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Node type\n");
    }
    else if(hMMMD->ePlainLT==MM_LayerType_Pol)
    {
        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:N_VERTEXS]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Number of vertices\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:PERIMETRE]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Perimeter of the polygon\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:AREA]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Area of the polygon\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:N_ARCS]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Number of arcs\n");

        printf_function(pF, "\n");
        printf_function(pF, "[TAULA_PRINCIPAL:N_POLIG]\n");
        printf_function(pF, "visible=0\n");
        printf_function(pF, "simbolitzable=0\n");
        printf_function(pF, "MostrarUnitats=0\n");
        printf_function(pF, "descriptor=Number of elemental polygons\n");

        printf_function(pF, "[GEOMETRIA_I_TOPOLOGIA]\n");
        printf_function(pF, "NomCampNVertexs=N_VERTEXS\n");
        printf_function(pF, "NomCampPerimetre=PERIMETRE\n");
        printf_function(pF, "NomCampArea=AREA\n");
        printf_function(pF, "NomCampNArcs=N_ARCS\n");
        printf_function(pF, "NomCampNPoligons=N_POLIG\n");
    }

    // Writing TAULA_PRINCIPAL section
    if(hMMMD->pLayerDB && hMMMD->pLayerDB->nNFields>0)
    {
        // For each field of the databes
        for (nIField=0;nIField<hMMMD->pLayerDB->nNFields;nIField++)
        {
            if(!IsEmptyString(hMMMD->pLayerDB->pFields[nIField].pszFieldDescription))
            {
                printf_function(pF, "\n[%s:%s]\n", SECTION_TAULA_PRINCIPAL, hMMMD->pLayerDB->pFields[nIField].pszFieldName);
                printf_function(pF, "%s=%s\n", KEY_descriptor, hMMMD->pLayerDB->pFields[nIField].pszFieldDescription);
            }
        }
    }
    fclose_function(pF);
    return 0;
}

int MMWriteVectorMetadataFile(struct MiraMonVectLayerInfo *hMiraMonLayer, int layerPlainType, 
                            int layerMainPlainType)
{
struct MiraMonVectorMetaData hMMMD;

    // MiraMon writes a REL file of each .pnt, .arc, .nod or .pol
    memset(&hMMMD, 0, sizeof(hMMMD));
    hMMMD.ePlainLT=layerPlainType;
    hMMMD.pSRS=hMiraMonLayer->pSRS;

    hMMMD.szLayerTitle=hMiraMonLayer->szLayerTitle;
    if(layerPlainType==MM_LayerType_Point)
    {
        hMMMD.aLayerName=hMiraMonLayer->MMPoint.pszREL_LayerName;
        if(IsEmptyString(hMMMD.aLayerName))
            return 1;
        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
        return MMWriteMetadataFile(&hMMMD);
    }
    else if(layerPlainType==MM_LayerType_Arc)
    {
        // Arcs and not polygons
        if(layerMainPlainType==MM_LayerType_Arc)
        {
            hMMMD.aLayerName=hMiraMonLayer->MMArc.pszREL_LayerName;
            if(IsEmptyString(hMMMD.aLayerName))
                return 1;
            memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
            hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
        }
        // Arcs and polygons
        else
        {
            // Arc from polygon
            hMMMD.aLayerName=hMiraMonLayer->MMPolygon.MMArc.pszREL_LayerName;
            if(IsEmptyString(hMMMD.aLayerName))
                return 1;

            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMPolygon.TopArcHeader.hBB, sizeof(hMMMD.hBB));
            hMMMD.pLayerDB=NULL;
        }
        return MMWriteMetadataFile(&hMMMD);
    }
    else if(layerPlainType==MM_LayerType_Pol)
    {
        int nResult;

        hMMMD.aLayerName=hMiraMonLayer->MMPolygon.pszREL_LayerName;

        if(IsEmptyString(hMMMD.aLayerName))
            return 1;

        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
        hMMMD.aArcFile=strdup_function(get_filename_function(hMiraMonLayer->MMPolygon.MMArc.pszLayerName));
        nResult=MMWriteMetadataFile(&hMMMD);
        free_function(hMMMD.aArcFile);
        return nResult;
    }
    else if(layerPlainType==MM_LayerType_Node)
    {
        // Node from arc
        if(layerMainPlainType==MM_LayerType_Arc)
        {
            hMMMD.aLayerName=hMiraMonLayer->MMArc.MMNode.pszREL_LayerName;
            if(IsEmptyString(hMMMD.aLayerName))
                return 1;
            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMArc.TopNodeHeader.hBB, sizeof(hMMMD.hBB));
        }
        else // Node from polygon
        {
            hMMMD.aLayerName=hMiraMonLayer->MMPolygon.MMArc.MMNode.pszREL_LayerName;
            if(IsEmptyString(hMMMD.aLayerName))
                return 1;
            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMPolygon.MMArc.TopNodeHeader.hBB, sizeof(hMMMD.hBB));
        }
        hMMMD.pLayerDB=NULL;
        return MMWriteMetadataFile(&hMMMD);
    }
    return 0;
}

int MMWriteVectorMetadata(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if(hMiraMonLayer->bIsPoint)
        return MMWriteVectorMetadataFile(hMiraMonLayer, 
                MM_LayerType_Point, MM_LayerType_Point);
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMWriteVectorMetadataFile(hMiraMonLayer, 
                MM_LayerType_Node, MM_LayerType_Arc))
            return 1;
        return MMWriteVectorMetadataFile(hMiraMonLayer, 
                MM_LayerType_Arc, MM_LayerType_Arc);
    }
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MMWriteVectorMetadataFile(hMiraMonLayer, 
                    MM_LayerType_Node, MM_LayerType_Pol))
            return 1;
        if(MMWriteVectorMetadataFile(hMiraMonLayer, 
                    MM_LayerType_Arc, MM_LayerType_Pol))
            return 1;
        return MMWriteVectorMetadataFile(hMiraMonLayer, 
                    MM_LayerType_Pol, MM_LayerType_Pol);
    }
    return MMWriteVectorMetadataFile(hMiraMonLayer, 
        MM_LayerType_Unknown, MM_LayerType_Unknown);
}

int MM_Check_REL_FILE(const char *szREL_file)
{
const char *pszLine;
FILE_TYPE *pF;

    // Does the REL file exist?
    pF=fopen_function(szREL_file, "r");
    if(!pF)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file %s must exist.",
                             szREL_file);
        return 1;
    }

    // Does the REL file have VERSION?
    pszLine = ReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, NULL);
    if(!pszLine)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file \"%s\" must be REL4. "
                             "You can use ConvREL.exe from MiraMon Software "
                             "to convert this file to REL4.",
                             szREL_file);
        fclose_function(pF);
        return 1;
    }

    // Does the REL file have the correct VERSION?
    // Vers>=4?
    pszLine = ReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_Vers);
    if(*pszLine=='\0' || atoi(pszLine)<(int)MM_VERS)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file \"%s\" must have %s>=%d.",
                             szREL_file, KEY_Vers, MM_VERS);
        fclose_function(pF);
        return 1;
    }

    // SubVers>=3?
    pszLine = ReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_SubVers);
    if(*pszLine=='\0' || atoi(pszLine)<(int)MM_SUBVERS)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file \"%s\" must have %s>=%d.",
                             szREL_file, KEY_SubVers, MM_SUBVERS);
        fclose_function(pF);
        return 1;
    }

    // VersMetaDades>=5?
    pszLine = ReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_VersMetaDades);
    if(*pszLine=='\0' || atoi(pszLine)<(int)MM_VERS_METADADES)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file \"%s\" must have %s>=%d.",
                             szREL_file, KEY_VersMetaDades, MM_VERS_METADADES);
        fclose_function(pF);
        return 1;
    }

    // SubVersMetaDades>=0?
    pszLine = ReturnValueFromSectionINIFile(szREL_file, SECTION_VERSIO, KEY_SubVersMetaDades);
    if(*pszLine=='\0' || atoi(pszLine)<(int)MM_SUBVERS_METADADES)
    {
        MM_CPLError(CE_Failure, CPLE_OpenFailed,
                             "The file \"%s\" must have %s>=%d.",
                             szREL_file, KEY_SubVersMetaDades, MM_SUBVERS_METADADES);
        fclose_function(pF);
        return 1;
    }

    fclose_function(pF);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      MiraMon database functions                                      */
/* -------------------------------------------------------------------- */
int MMInitMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                struct MMAdmDatabase *pMMAdmDB)
{
    strcpy(pMMAdmDB->pMMBDXP->ModeLectura, "wb+");
    if (FALSE==MM_CreateDBFFile(pMMAdmDB->pMMBDXP, pMMAdmDB->pszExtDBFLayerName))
        return 1;

    // Opening the file
    if(NULL==(pMMAdmDB->pFExtDBF=fopen_function(
            pMMAdmDB->pszExtDBFLayerName, 
            "r+b"))) //hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMAdmDB->pFExtDBF, 
        pMMAdmDB->pMMBDXP->OffsetPrimeraFitxa, SEEK_SET);
    
    if(MMInitFlush(&pMMAdmDB->FlushRecList, 
            pMMAdmDB->pFExtDBF,
            hMiraMonLayer->nMemoryRatio?
                (unsigned __int64)(hMiraMonLayer->nMemoryRatio*MM_250MB):MM_250MB,
            &pMMAdmDB->pRecList, 
            pMMAdmDB->pMMBDXP->OffsetPrimeraFitxa, 0))
        return 1;

    pMMAdmDB->nNumRecordOnCourse=(unsigned __int64)pMMAdmDB->pMMBDXP->BytesPerFitxa+1;
    pMMAdmDB->szRecordOnCourse=calloc_function(pMMAdmDB->nNumRecordOnCourse);
    if(!pMMAdmDB->szRecordOnCourse)
    {
        MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Memory error in MiraMon "
                    "driver (MMInitMMDB())");
        return 1;
    }
    return 0;
}

int MMCreateMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL, *pBD_XP_Aux=NULL;
struct MM_CAMP MMField;
size_t nIFieldLayer;
MM_EXT_DBF_N_FIELDS nIField=0;
MM_EXT_DBF_N_FIELDS nNFields;

    if(hMiraMonLayer->bIsPoint)
    {
        if(hMiraMonLayer->pLayerDB)
            nNFields=MM_PRIVATE_POINT_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields=MM_PRIVATE_POINT_DB_FIELDS;
        hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields, 
            hMiraMonLayer->nCharSet);

        pBD_XP=hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;
	    if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstPointFieldsDB_XP(pBD_XP)))
	        return 1;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(hMiraMonLayer->pLayerDB)
            nNFields=MM_PRIVATE_ARC_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields=MM_PRIVATE_ARC_DB_FIELDS;

        pBD_XP=hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields,
            hMiraMonLayer->nCharSet);

        if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstArcFieldsDB_XP(pBD_XP, 0)))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMArc.MMNode.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(3,
            hMiraMonLayer->nCharSet);
        if (0==MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
	        return 1;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        if(hMiraMonLayer->pLayerDB)
            nNFields=MM_PRIVATE_POLYGON_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields=MM_PRIVATE_POLYGON_DB_FIELDS;

        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields,
            hMiraMonLayer->nCharSet);

        pBD_XP=hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;
	    if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstPolygonFieldsDB_XP(pBD_XP, 6)))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(5,
            hMiraMonLayer->nCharSet);
        if (0==MM_DefineFirstArcFieldsDB_XP(pBD_XP_Aux, 6))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(3,
            hMiraMonLayer->nCharSet);
        if (0==MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
	        return 1;
    }
    else
    {
        // Creating only a DBF 
        if(hMiraMonLayer->pLayerDB)
            nNFields=hMiraMonLayer->pLayerDB->nNFields;
        else
            nNFields=0;

        pBD_XP=hMiraMonLayer->MMAdmDBWriting.pMMBDXP=MM_CreateDBFHeader(nNFields, 
            hMiraMonLayer->nCharSet);

        if(!pBD_XP)
            return 1;
    }
    

    // After private MiraMon fields, other Fields are added.
    // If no compatible names, some changes are done.
    if(hMiraMonLayer->pLayerDB)
    {
	    for (nIFieldLayer=0; nIField<nNFields; nIField++, nIFieldLayer++)
	    {
            MM_InitializeField(&MMField);
            MM_strnzcpy(MMField.NomCamp, 
                hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].pszFieldName,
                MM_MAX_LON_FIELD_NAME_DBF);

            MM_strnzcpy(MMField.DescripcioCamp[0], 
                hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].pszFieldDescription,
                MM_MAX_BYTES_FIELD_DESC);

            MMField.BytesPerCamp=hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].nFieldSize;
            switch(hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].eFieldType)
            {
                case MM_Numeric:
                    MMField.TipusDeCamp='N';
                    if(hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].bIs64BitInteger)
                        MMField.Is64=1;
                    if(MMField.BytesPerCamp==0)
                        MMField.BytesPerCamp=MM_MAX_AMPLADA_CAMP_N_DBF;
                    break;
                case MM_Character:
                    MMField.TipusDeCamp='C';
                    if(MMField.BytesPerCamp==0)
                        MMField.BytesPerCamp=MM_MAX_AMPLADA_CAMP_C_DBF;
                    break;
                case MM_Data:
                    MMField.TipusDeCamp='D';
                    if(MMField.BytesPerCamp==0)
                        MMField.BytesPerCamp=MM_MAX_AMPLADA_CAMP_D_DBF;
                    break;
                case MM_Logic:
                    MMField.TipusDeCamp='L';
                    if(MMField.BytesPerCamp==0)
                        MMField.BytesPerCamp=1;
                    break;
                default:
                    MMField.TipusDeCamp='C';
                    if(MMField.BytesPerCamp==0)
                        MMField.BytesPerCamp=MM_MAX_AMPLADA_CAMP_C_DBF;
            };

           
            MMField.DecimalsSiEsFloat=(MM_BYTE)hMiraMonLayer->pLayerDB->pFields[nIFieldLayer].nNumberOfDecimals;
            
		    MM_DuplicateFieldDBXP(pBD_XP->Camp+nIField, &MMField);
		    MM_ModifyFieldNameAndDescriptorIfPresentBD_XP(pBD_XP->Camp+nIField, pBD_XP, FALSE, 0);
		    if (pBD_XP->Camp[nIField].mostrar_camp==MM_CAMP_NO_MOSTRABLE)
			    pBD_XP->Camp[nIField].mostrar_camp=MM_CAMP_MOSTRABLE;
		    if (pBD_XP->Camp[nIField].TipusDeCamp=='F')
			    pBD_XP->Camp[nIField].TipusDeCamp='N';
	    }
    }
	
	if(hMiraMonLayer->bIsPoint)
    {
        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPoint.MMAdmDB))
            return 1;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMArc.MMAdmDB))
            return 1;

        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMArc.MMNode.MMAdmDB))
            return 1;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;
        
        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;

        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB))
            return 1;
    }
    else
    {
        if(MMInitMMDB(hMiraMonLayer, &hMiraMonLayer->MMAdmDBWriting))
            return 1;
    }
    return 0;
}

int MMTestAndFixValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            struct MMAdmDatabase  *pMMAdmDB,
                            MM_EXT_DBF_N_FIELDS nIField, 
                            char *szValue)
{
    struct MM_CAMP *camp=pMMAdmDB->pMMBDXP->Camp+nIField;
    MM_TIPUS_BYTES_PER_CAMP_DBF nNewWidth;

    if(!szValue)
        return 0;

    nNewWidth=(MM_TIPUS_BYTES_PER_CAMP_DBF)strlen(szValue);
    if(MM_ResizeStringToOperateIfNeeded(hMiraMonLayer, nNewWidth+1))
        return 1;

    if(nNewWidth>camp->BytesPerCamp)
    {
        if(MM_WriteNRecordsMMBD_XPFile(hMiraMonLayer, pMMAdmDB))
            return 1;
        
        // Flushing all to be flushed
        pMMAdmDB->FlushRecList.SizeOfBlockToBeSaved=0;
        if(MM_AppendBlockToBuffer(&pMMAdmDB->FlushRecList))
            return 1;

        pMMAdmDB->pMMBDXP->pfBaseDades=pMMAdmDB->pFExtDBF;

        if(MM_ChangeDBFWidthField(pMMAdmDB->pMMBDXP, nIField, 
            nNewWidth, pMMAdmDB->pMMBDXP->Camp[nIField].DecimalsSiEsFloat,
            (MM_BYTE)MM_NOU_N_DECIMALS_NO_APLICA))
            return 1;

        // The record on course also has to change its size.
        if ((unsigned __int64)pMMAdmDB->pMMBDXP->BytesPerFitxa + 1 >= pMMAdmDB->nNumRecordOnCourse)
        {
            if (NULL == (pMMAdmDB->szRecordOnCourse =
                realloc_function(pMMAdmDB->szRecordOnCourse,
                    (unsigned __int64)pMMAdmDB->pMMBDXP->BytesPerFitxa + 1)))
            {
                MM_CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Memory error in MiraMon "
                    "driver (MMTestAndFixValueToRecordDBXP())");
                return 1;
            }
        }

        // File has changed it's size, so it has to be updated
        // at the Flush tool
        fseek_function(pMMAdmDB->pFExtDBF, 0, SEEK_END);
        pMMAdmDB->FlushRecList.OffsetWhereToFlush=
            ftell_function(pMMAdmDB->pFExtDBF);
    }
    return 0;
}

int MMWriteValueToRecordDBXP(struct MiraMonVectLayerInfo *hMiraMonLayer,
                            char *registre, 
                            const struct MM_CAMP *camp, 
                            const void *valor,
                            MM_BOOLEAN is_64)
{
    if(MM_ResizeStringToOperateIfNeeded(hMiraMonLayer, camp->BytesPerCamp+10))
        return 1;
    
    if (camp->TipusDeCamp=='N')
    {
        if(!is_64)
        {
    	    sprintf(hMiraMonLayer->szStringToOperate,
        		    "%*.*f",
                    camp->BytesPerCamp,
                    camp->DecimalsSiEsFloat,
                    *(const double *)valor);
        }
        else
        {
            sprintf(hMiraMonLayer->szStringToOperate,
        		    "%*lld",
                    camp->BytesPerCamp,
                    *(const __int64 *)valor);
        }
    }
    else
    {
    	sprintf(hMiraMonLayer->szStringToOperate,
        		"%-*s",
                camp->BytesPerCamp,
                (const char *)valor);
    }
    
    memcpy(registre+camp->BytesAcumulats, hMiraMonLayer->szStringToOperate, camp->BytesPerCamp);
    return 0;
}

// Gets the n-th value of the format (number_of_values:val1,val2,...,valN)
char *MMGetNFieldValue(const char *pszStringList, unsigned __int32 nIRecord)
{
char *p, *q;
int nNValues, nIValues;
char *pszAux;

    if(!pszStringList)
        return NULL;

    pszAux=strdup_function(pszStringList);
    p=strstr(pszAux, "(");
    if(!p)
        return NULL;
    p++;
    if(!p)
        return NULL;
    q=strstr(p, ":");
    p[(ptrdiff_t)q-(ptrdiff_t)p]='\0';	
    nNValues=atoi(p);
    if(nIRecord>(unsigned __int32)nNValues)
        return NULL;

    q++;
    nIValues=0;
    while((unsigned __int32)nIValues<=nIRecord)
    {
        if(!q)
            return NULL;
        p=strstr(q, ",");
        if(!p)
        {
            p=strstr(q, ")");
            if(!p)
                return NULL;
            q[(ptrdiff_t)p-(ptrdiff_t)q]='\0';	
            return q;
        }
        if ((unsigned __int32)nIValues == nIRecord)
        {
            p = strstr(q, ",");
            q[(ptrdiff_t)p - (ptrdiff_t)q] = '\0';

            return q;
        }
        q=p+1;
    }

    return q;
}

int MMAddFeatureRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer,
                             struct MiraMonFeature *hMMFeature,
                             struct MMAdmDatabase  *pMMAdmDB,
                             char *pszRecordOnCourse,
                             struct MM_FLUSH_INFO *pFlushRecList,
                             MM_EXT_DBF_N_RECORDS *nNumRecords,
                             MM_EXT_DBF_N_FIELDS nNumPrivateMMField)
{
MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord;
MM_EXT_DBF_N_FIELDS nIField;
struct MM_BASE_DADES_XP *pBD_XP=NULL;

    pBD_XP=pMMAdmDB->pMMBDXP;
    for(nIRecord=0; nIRecord<hMMFeature->nNumMRecords; nIRecord++)
    {
        for (nIField=0; nIField<hMMFeature->pRecords[nIRecord].nNumField; nIField++)
        {
            // A field with no valid value is written as blank
            if(!hMMFeature->pRecords[nIRecord].pField[nIField].bIsValid)
            {
                MM_TIPUS_BYTES_ACUMULATS_DBF i = 0;
                while(i<pBD_XP->Camp[nIField+nNumPrivateMMField].BytesPerCamp)
                {
                    memcpy(pszRecordOnCourse+
                        pBD_XP->Camp[nIField+nNumPrivateMMField].BytesAcumulats+i, 
                        " ",  1);
                    i++;
                }
                continue;
            }
            if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='C')
	        {
                if(MMWriteValueToRecordDBXP(hMiraMonLayer,
                           pszRecordOnCourse, 
                           pBD_XP->Camp+nIField+nNumPrivateMMField, 
                           hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue,
                           FALSE))
                    return 1;
	        }
            else if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='N')
	        {
                if(pBD_XP->Camp[nIField+nNumPrivateMMField].Is64)
                {
                    if(MMWriteValueToRecordDBXP(hMiraMonLayer,
                               pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].iValue,
                               TRUE))
                        return 1;
                }
                else
                {
                    if(MMWriteValueToRecordDBXP(hMiraMonLayer,
                               pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].dValue,
                               FALSE))
                       return 1;
                }
                
	        }
            else if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='D')
	        {
                if(MMWriteValueToRecordDBXP(hMiraMonLayer,
                               pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue,
                               FALSE))
                   return 1;
	        }
        }
        
        if(MM_AppendBlockToBuffer(pFlushRecList))
            return 1;

        (*nNumRecords)++;
    }
    return 0;
}

int MM_DetectAndFixDBFWidthChange(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                            struct MiraMonFeature *hMMFeature,
                            struct MMAdmDatabase *pMMAdmDB,
                            struct MM_FLUSH_INFO *pFlushRecList,
                            MM_EXT_DBF_N_FIELDS nNumPrivateMMField,
                            MM_EXT_DBF_N_MULTIPLE_RECORDS nIRecord,
                            MM_EXT_DBF_N_FIELDS nIField)
{
struct MM_BASE_DADES_XP *pBD_XP;

    if(!hMMFeature)
        return 0;

    pBD_XP=pMMAdmDB->pMMBDXP;
    if(nIRecord>=hMMFeature->nNumMRecords)
        return 0;
    
    if(nIField>=hMMFeature->pRecords[nIRecord].nNumField)
        return 0;

    if(MMTestAndFixValueToRecordDBXP(hMiraMonLayer, 
               pMMAdmDB, 
               nIField+nNumPrivateMMField, 
               hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue))
        return 1;
    
    // We analize next fields
    if(nIField==hMMFeature->pRecords[nIRecord].nNumField-1)
    {
        if(MM_DetectAndFixDBFWidthChange(hMiraMonLayer, 
                hMMFeature, pMMAdmDB, pFlushRecList, nNumPrivateMMField,
                nIRecord+1, 0))
            return 1;
    }
    else
    {
        if(MM_DetectAndFixDBFWidthChange(hMiraMonLayer, 
                hMMFeature, pMMAdmDB, pFlushRecList, nNumPrivateMMField,
                nIRecord, nIField+1))
            return 1;
    }
    return 0;
} // End of MM_DetectAndFixDBFWidthChange()

int MMAddDBFRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=0;
char *pszRecordOnCourse;
struct MM_FLUSH_INFO *pFlushRecList;

    // In V1.1 only _UI32_MAX records number is allowed
    if(MMCheckVersionForFID(hMiraMonLayer, 
            hMMFeature->nNumMRecords))
        return MM_STOP_WRITING_FEATURES;
    
    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&hMiraMonLayer->MMAdmDBWriting.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead=
        (void *)hMiraMonLayer->MMAdmDBWriting.pRecList;

    pszRecordOnCourse=hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    pBD_XP=hMiraMonLayer->MMAdmDBWriting.pMMBDXP;

    // Test lenght
    if(MM_DetectAndFixDBFWidthChange(hMiraMonLayer,
            hMMFeature, &hMiraMonLayer->MMAdmDBWriting,
            pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse=hMiraMonLayer->MMAdmDBWriting.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    if(MMAddFeatureRecordToMMDB(hMiraMonLayer, hMMFeature, 
            &hMiraMonLayer->MMAdmDBWriting,
            pszRecordOnCourse, pFlushRecList, 
            &hMiraMonLayer->MMAdmDBWriting.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

int MMAddPointRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=MM_PRIVATE_POINT_DB_FIELDS;
char *pszRecordOnCourse;
struct MM_FLUSH_INFO *pFlushRecList;

    // In V1.1 only _UI32_MAX records number is allowed
    if(MMCheckVersionForFID(hMiraMonLayer, 
            hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nRecords+hMMFeature->nNumMRecords))
        return MM_STOP_WRITING_FEATURES;
    
    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&hMiraMonLayer->MMPoint.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead=
        (void *)hMiraMonLayer->MMPoint.MMAdmDB.pRecList;

    pBD_XP=hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;
        
    // Test lenght
    if(MM_DetectAndFixDBFWidthChange(hMiraMonLayer,
            hMMFeature, &hMiraMonLayer->MMPoint.MMAdmDB,
            pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse=hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    if(MMAddFeatureRecordToMMDB(hMiraMonLayer, hMMFeature, 
            &hMiraMonLayer->MMPoint.MMAdmDB,
            pszRecordOnCourse, pFlushRecList, 
            &hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

int MMAddArcRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMMFeature,
                       MM_INTERNAL_FID nElemCount, struct MM_AH *pArcHeader)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
char *pszRecordOnCourse;
struct MiraMonArcLayer *pMMArcLayer;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=MM_PRIVATE_ARC_DB_FIELDS;
struct MM_FLUSH_INFO *pFlushRecList;

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    // In V1.1 only _UI32_MAX records number is allowed
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MMCheckVersionForFID(hMiraMonLayer, 
            pMMArcLayer->MMAdmDB.pMMBDXP->nRecords+1))
        return MM_STOP_WRITING_FEATURES;
    }
    else
    {
        if(MMCheckVersionForFID(hMiraMonLayer, 
            pMMArcLayer->MMAdmDB.pMMBDXP->nRecords+hMMFeature->nNumMRecords))
        return MM_STOP_WRITING_FEATURES;
    }

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&pMMArcLayer->MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead=
        (void *)pMMArcLayer->MMAdmDB.pRecList;

    pBD_XP=pMMArcLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse=pMMArcLayer->MMAdmDB.szRecordOnCourse;
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;
    
    // Test lenght
    if (!hMiraMonLayer->bIsPolygon)
    {
        if (MM_DetectAndFixDBFWidthChange(hMiraMonLayer,
            hMMFeature, &pMMArcLayer->MMAdmDB,
            pFlushRecList, nNumPrivateMMField, 0, 0))
            return MM_FATAL_ERROR_WRITING_FEATURES;

        // Reassign the point because the function can realloc it.
        pszRecordOnCourse=pMMArcLayer->MMAdmDB.szRecordOnCourse;
        pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;
    }
    
    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+1,
        &pArcHeader->nElemCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+2,
        &pArcHeader->dfLenght, FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+3,
        &pArcHeader->nFirstIdNode, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+4,
        &pArcHeader->nLastIdNode, TRUE);
    
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MM_AppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        pMMArcLayer->MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    if(MMAddFeatureRecordToMMDB(hMiraMonLayer, hMMFeature, 
            &pMMArcLayer->MMAdmDB,
            pszRecordOnCourse, pFlushRecList, 
            &pMMArcLayer->MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

int MMAddNodeRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                       MM_INTERNAL_FID nElemCount, struct MM_NH *pNodeHeader)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
char *pszRecordOnCourse;
struct MiraMonNodeLayer *pMMNodeLayer;
double nDoubleValue;

    if(hMiraMonLayer->bIsPolygon)
        pMMNodeLayer=&hMiraMonLayer->MMPolygon.MMArc.MMNode;
    else
        pMMNodeLayer=&hMiraMonLayer->MMArc.MMNode;

    // In V1.1 only _UI32_MAX records number is allowed
    if(MMCheckVersionForFID(hMiraMonLayer, 
            pMMNodeLayer->MMAdmDB.pMMBDXP->nRecords+1))
        return MM_STOP_WRITING_FEATURES;

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockWhereToSaveOrRead=
        (void *)pMMNodeLayer->MMAdmDB.pRecList;

    pBD_XP=pMMNodeLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse=pMMNodeLayer->MMAdmDB.szRecordOnCourse;
    
    pMMNodeLayer->MMAdmDB.FlushRecList.SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockToBeSaved=(void *)pszRecordOnCourse;
    
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    nDoubleValue=pNodeHeader->nArcsCount;
    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+1,
        &nDoubleValue, FALSE);

    nDoubleValue=pNodeHeader->cNodeType;
    MMWriteValueToRecordDBXP(hMiraMonLayer,
        pszRecordOnCourse, pBD_XP->Camp+2,
        &nDoubleValue, FALSE);

    if(MM_AppendBlockToBuffer(&pMMNodeLayer->MMAdmDB.FlushRecList))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    pMMNodeLayer->MMAdmDB.pMMBDXP->nRecords++;
    return MM_CONTINUE_WRITING_FEATURES;
}

int MMAddPolygonRecordToMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount, 
                        MM_N_VERTICES_TYPE nVerticesCount, 
                        struct MM_PH *pPolHeader)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
char *pszRecordOnCourse;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=MM_PRIVATE_POLYGON_DB_FIELDS;
struct MM_FLUSH_INFO *pFlushRecList;

    // In V1.1 only _UI32_MAX records number is allowed
    if(MMCheckVersionForFID(hMiraMonLayer, 
         hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords+hMMFeature?hMMFeature->nNumMRecords:0))
    return MM_STOP_WRITING_FEATURES;
    
    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&hMiraMonLayer->MMPolygon.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSaveOrRead=
        (void *)hMiraMonLayer->MMPolygon.MMAdmDB.pRecList;

    pBD_XP=hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;
    pszRecordOnCourse=hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    // Test lenght
    if(MM_DetectAndFixDBFWidthChange(hMiraMonLayer,
            hMMFeature, &hMiraMonLayer->MMPolygon.MMAdmDB,
            pFlushRecList, nNumPrivateMMField, 0, 0))
        return MM_FATAL_ERROR_WRITING_FEATURES;

    // Reassign the point because the function can realloc it.
    pszRecordOnCourse=hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    // Now lenght is sure, write
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    if(MMWriteValueToRecordDBXP(hMiraMonLayer,
            pszRecordOnCourse, pBD_XP->Camp, 
            &nElemCount, TRUE))
        return 1;

    if(!hMMFeature)
    {
        if(MM_AppendBlockToBuffer(pFlushRecList))
            return MM_FATAL_ERROR_WRITING_FEATURES;
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords++;
        return MM_CONTINUE_WRITING_FEATURES;
    }

    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp+1,
        &nVerticesCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp+2,
        &pPolHeader->dfPerimeter, FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp+3,
        &pPolHeader->dfArea, FALSE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp+4,
        &pPolHeader->nArcsCount, TRUE);

    MMWriteValueToRecordDBXP(hMiraMonLayer, 
        pszRecordOnCourse, pBD_XP->Camp+5,
        &pPolHeader->nRingsCount, TRUE);
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    if(MMAddFeatureRecordToMMDB(hMiraMonLayer, hMMFeature, 
            &hMiraMonLayer->MMPolygon.MMAdmDB,
            pszRecordOnCourse, pFlushRecList, 
            & hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nRecords,
            nNumPrivateMMField))
        return MM_FATAL_ERROR_WRITING_FEATURES;
    return MM_CONTINUE_WRITING_FEATURES;
}

int MM_WriteNRecordsMMBD_XPFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
            struct MMAdmDatabase *MMAdmDB)
{
    if(!MMAdmDB->pMMBDXP)
        return 0;

    // Updating number of features in database
    fseek_function(MMAdmDB->pFExtDBF, MM_FIRST_OFFSET_to_N_RECORDS, SEEK_SET);
    if (fwrite_function(&MMAdmDB->pMMBDXP->nRecords, 4, 1, 
            MMAdmDB->pFExtDBF) != 1)
	    return 1;
    if(hMiraMonLayer->LayerVersion==MM_64BITS_VERSION)
    {
        fseek_function(MMAdmDB->pFExtDBF, MM_SECOND_OFFSET_to_N_RECORDS, SEEK_SET);
        if (fwrite_function(((char*)&MMAdmDB->pMMBDXP->nRecords)+4, 4, 1, 
            MMAdmDB->pFExtDBF) != 1)
		return 1;
    }
        
    return 0;
}

int MMCloseMMBD_XPFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                    struct MMAdmDatabase *MMAdmDB)
{
    if(!MMAdmDB->pFExtDBF)
    {
        // In case of 0 elements created we have to 
        // create an empty DBF
        if(hMiraMonLayer->bIsPolygon)
        {
            if(hMiraMonLayer->TopHeader.nElemCount<=1)
            {
                if(MMCreateMMDB(hMiraMonLayer))
                    return 1;
            }
        }
        else if(hMiraMonLayer->bIsPoint || hMiraMonLayer->bIsArc)
        {
            if(hMiraMonLayer->TopHeader.nElemCount==0)
            {
                if(MMCreateMMDB(hMiraMonLayer))
                    return 1;
            }
        }
    }

    if (hMiraMonLayer->ReadOrWrite == MM_WRITTING_MODE)
    {
        if (MM_WriteNRecordsMMBD_XPFile(hMiraMonLayer, MMAdmDB))
            return 1;
        
        // Flushing all to be flushed
        MMAdmDB->FlushRecList.SizeOfBlockToBeSaved = 0;
        if (MM_AppendBlockToBuffer(&MMAdmDB->FlushRecList))
            return 1;
    }

    // Closing database files
    if (MMAdmDB->pFExtDBF)
    {
        if (fclose_function(MMAdmDB->pFExtDBF))
            return 1;
        MMAdmDB->pFExtDBF=NULL;
    }

    return 0;
}

int MMCloseMMBD_XP(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if (hMiraMonLayer->pMMBDXP && hMiraMonLayer->pMMBDXP->pfBaseDades)
    {
        fclose_function(hMiraMonLayer->pMMBDXP->pfBaseDades);
        hMiraMonLayer->pMMBDXP->pfBaseDades = NULL;
    }

    if(hMiraMonLayer->bIsPoint)
        return MMCloseMMBD_XPFile(hMiraMonLayer, 
                        &hMiraMonLayer->MMPoint.MMAdmDB);
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseMMBD_XPFile(hMiraMonLayer, 
                &hMiraMonLayer->MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(hMiraMonLayer, 
            &hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    }
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseMMBD_XPFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;
        if(MMCloseMMBD_XPFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(hMiraMonLayer, 
            &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
    return MMCloseMMBD_XPFile(hMiraMonLayer, 
                    &hMiraMonLayer->MMAdmDBWriting);
}

void MMDestroyMMDBFile(struct MiraMonVectLayerInfo *hMiraMonLayer,
                struct MMAdmDatabase *pMMAdmDB)
{
	if (pMMAdmDB->szRecordOnCourse)
    {
        free_function(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse=NULL;
    }
    if (hMiraMonLayer->szStringToOperate)
    {
        free_function(hMiraMonLayer->szStringToOperate);
        hMiraMonLayer->szStringToOperate=NULL;
        hMiraMonLayer->nNumStringToOperate=0;
    }
    
    MM_ReleaseDBFHeader(pMMAdmDB->pMMBDXP);
    hMiraMonLayer->pMMBDXP=pMMAdmDB->pMMBDXP=NULL;
}

void MMDestroyMMDB(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
    if(hMiraMonLayer->bIsPoint)
    {
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPoint.MMAdmDB);
    }
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMArc.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    }
    if(hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPolygon.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPolygon.MMArc.MMAdmDB);
        MMDestroyMMDBFile(hMiraMonLayer, 
                &hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
}

CPL_C_END // Necessary for compiling in GDAL project
