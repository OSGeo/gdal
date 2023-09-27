/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 2023,  MiraMon
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
#include "CmptCmp.h" // Compatibility between compilators
#include "mm_gdal_driver_structs.h"    // SECCIO_VERSIO
#include <string.h>
#include <stdlib.h>
#include <stddef.h>     // For size_t
#include "mm_gdal\mm_wrlayr.h" 
#include "mm_gdal\mm_gdal_functions.h"
#else
#include "mm_wrlayr.h" 
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#endif

#include "gdal.h"			// For GDALDatasetH
#include "ogr_srs_api.h"	// For OSRGetAuthorityCode
CPL_C_START  // Necessary for compiling in GDAL project


/* -------------------------------------------------------------------- */
/*      Header Functions                                                */
/* -------------------------------------------------------------------- */
int MMAppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo);
void MMInitBoundingBox(struct MMBoundingBox *dfBB);
int MMWriteAHArcSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMWriteNHNodeSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMWritePHPolygonSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            MM_FILE_OFFSET DiskOffset);
int MMAppendIntegerDependingOnVersion(
            struct MiraMonLayerInfo *hMiraMonLayer,
            struct MM_FLUSH_INFO *FlushInfo, 
            unsigned long *nUL32, 
            unsigned __int64 nUI64);
int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile, 
            MM_FILE_OFFSET *nOffset);
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, 
            unsigned __int64 nBlockSize, char **pBuffer, 
            MM_FILE_OFFSET DiskOffsetWhereToFlush, 
            __int32 nMyDiskSize);
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
void MMUpdateBoundingBoxXY(struct MMBoundingBox *dfBB, 
            struct MM_POINT_2D *pCoord);
void MMUpdateBoundingBox(struct MMBoundingBox *dfBBToBeAct, 
            struct MMBoundingBox *dfBBWithData);
int MMCheckVersionFor3DOffset(struct MiraMonLayerInfo *hMiraMonLayer, 
                                MM_FILE_OFFSET nOffset, 
                                MM_INTERNAL_FID nElemCount);
int MMCheckVersionOffset(struct MiraMonLayerInfo *hMiraMonLayer, 
                                MM_FILE_OFFSET OffsetToCheck);
int MMCheckVersionForFID(struct MiraMonLayerInfo *hMiraMonLayer, 
                         MM_INTERNAL_FID FID);

int MMResetExtensionAndLastLetter(char *pzNewLayerName, 
                                const char *pzOldLayerName, 
                                const char *MDExt);

// Extended DBF functions
#ifdef NO_HO_VULL_PERO_HO_GUARDO
int MMInitMMPointPrivateFields(struct MiraMonLayerInfo *hMiraMonLayer);
#endif
int MMCreateMMDB(struct MiraMonLayerInfo *hMiraMonLayer);
int MMAddPointRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
            struct MiraMonFeature *hMMFeature,MM_INTERNAL_FID nElemCount);
int MMAddArcRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount, 
                        struct MM_AH *pArcHeader);
int MMAddNodeRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
                        MM_INTERNAL_FID nElemCount, 
                        struct MM_NH *pNodeHeader);
int MMAddPolygonRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount, 
                        MM_TIPUS_N_VERTEXS nVerticesCount, 
                        struct MM_PH *pPolHeader);
int MMCloseMMBD_XP(struct MiraMonLayerInfo *hMiraMonLayer);
void MMDestroyMMDB(struct MiraMonLayerInfo *hMiraMonLayer);
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
	return 0;
}


int MMWriteHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader)
{
char dot='.';
unsigned long NCount;
long reservat4=0L;
MM_INTERNAL_FID nNumber1=1, nNumber0=0;

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
    pMMHeader.hBB.dfMinX=MM_STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMaxX=-MM_STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMinY=MM_STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMaxY=-MM_STATISTICAL_UNDEFINED_VALUE;

	return MMWriteHeader(pF, &pMMHeader);
}

int MMWrite3DHeader(FILE_TYPE *pF, struct MM_ZSection *pZSection)
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

int MMWrite3DDescriptionHeaders(struct MiraMonLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF, MM_INTERNAL_FID nElements, 
                        struct MM_ZSection *pZSection)
{
long reservat4=0L;
unsigned long Offset2G;
MM_INTERNAL_FID nIndex;
MM_FILE_OFFSET nOffsetDiff;
struct MM_ZD *pZDescription=pZSection->pZDescription;

    nOffsetDiff=pZSection->ZSectionOffset+nElements*(
        sizeof(pZDescription->dfBBminz)+
        sizeof(pZDescription->dfBBmaxz)+
        sizeof(pZDescription->nZCount)+
        ((hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)?
        sizeof(Offset2G):sizeof((pZDescription+nIndex)->nOffsetZ)));

    for(nIndex=0; nIndex<nElements; nIndex++)
    {
        if (fwrite_function(&(pZDescription+nIndex)->dfBBminz, 
                            sizeof(pZDescription->dfBBminz), 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=sizeof(pZDescription->dfBBminz);

        if (fwrite_function(&(pZDescription+nIndex)->dfBBmaxz, 
                            sizeof(pZDescription->dfBBmaxz), 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=sizeof(pZDescription->dfBBmaxz);

        if (fwrite_function(&(pZDescription+nIndex)->nZCount, 
                            sizeof(pZDescription->nZCount), 1, pF)!=1)
		    return 1;
        pZSection->ZSectionOffset+=sizeof(pZDescription->nZCount);

        (pZDescription+nIndex)->nOffsetZ+=nOffsetDiff;
        if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        {
            Offset2G=(unsigned long)(pZDescription+nIndex)->nOffsetZ;

            if (fwrite_function(&Offset2G, sizeof(Offset2G), 1, pF)!=1)
		        return 1;
            pZSection->ZSectionOffset+=sizeof(Offset2G);
        }
        else
        {
            if (fwrite_function(&reservat4, 4, 1, pF)!=1)
		        return 1;
            pZSection->ZSectionOffset+=4;

            if (fwrite_function(&(pZDescription+nIndex)->nOffsetZ, 
                            sizeof((pZDescription+nIndex)->nOffsetZ), 1, pF)!=1)
		        return 1;
            pZSection->ZSectionOffset+=sizeof((pZDescription+nIndex)->nOffsetZ);
        }
    }
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

int MMInitZSectionLayer(struct MiraMonLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF3d,
                        struct MM_ZSection *pZSection,
                        MM_FILE_OFFSET ZSectionOffset)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Zsection
    if(!hMiraMonLayer->TopHeader.bIs3d)
    {
        pZSection->pZDescription=NULL;
        return 0;
    }

    pZSection->ZHeader.dfBBminz=STATISTICAL_UNDEF_VALUE;
    pZSection->ZHeader.dfBBmaxz=-STATISTICAL_UNDEF_VALUE;

    // ZH
    pZSection->ZHeader.nMyDiskSize=32;
    pZSection->ZSectionOffset=ZSectionOffset;

    // ZD
    pZSection->nMaxZDescription=hMiraMonLayer->nSuposedElemCount;
    if(MMInitZSectionDescription(pZSection))
        return 1;

    if (hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pZSection->nZDDiskSize=MM_SIZE_OF_ZD_32_BITS;
    else
        pZSection->nZDDiskSize=MM_SIZE_OF_ZD_64_BITS;

    pZSection->ZDOffset=pZSection->ZSectionOffset+
            pZSection->ZHeader.nMyDiskSize;

    // ZL
    if(MMInitFlush(&pZSection->FlushZL, pF3d, 
            sizeof(double)*hMiraMonLayer->nSuposedElemCount, &pZSection->pZL, 
            0, sizeof(double)))
        return 1;

    return 0;
}

int MMInitPointLayer(struct MiraMonLayerInfo *hMiraMonLayer, int bIs3d)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    hMiraMonLayer->bIsPoint=1;

    // Geometrical part
    // Init header structure
    hMiraMonLayer->TopHeader.nElemCount=0;
    MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);
    
    hMiraMonLayer->TopHeader.bIs3d=bIs3d;
    hMiraMonLayer->TopHeader.aFileType[0]='P';
    hMiraMonLayer->TopHeader.aFileType[1]='N';
    hMiraMonLayer->TopHeader.aFileType[2]='T';

    // Opening the binary file where sections TH, TL[...] and ZH-ZD[...]-ZL[...]
    // are going to be written.
    if(NULL==(hMiraMonLayer->MMPoint.pF=fopen_function(
        hMiraMonLayer->MMPoint.pszLayerName, 
        hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(hMiraMonLayer->MMPoint.pF, 0, SEEK_SET);

    // TL
    hMiraMonLayer->MMPoint.pszTLName=strdup_function(
                hMiraMonLayer->MMPoint.pszLayerName);
    strcpy(hMiraMonLayer->MMPoint.pszTLName, 
            reset_extension(hMiraMonLayer->MMPoint.pszTLName, "~TL"));

    if(NULL==(hMiraMonLayer->MMPoint.pFTL = 
            fopen_function(hMiraMonLayer->MMPoint.pszTLName, 
            hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(hMiraMonLayer->MMPoint.pFTL, 0, SEEK_SET);

    if(MMInitFlush(&hMiraMonLayer->MMPoint.FlushTL, 
            hMiraMonLayer->MMPoint.pFTL, 
            2*sizeof(double)*hMiraMonLayer->nSuposedElemCount, 
            &hMiraMonLayer->MMPoint.pTL, 
            0, MM_SIZE_OF_TL))
           return 1;

    // 3d part
    if(hMiraMonLayer->TopHeader.bIs3d)
    {
        hMiraMonLayer->MMPoint.psz3DLayerName=strdup_function(
            hMiraMonLayer->MMPoint.pszLayerName);
        strcpy(hMiraMonLayer->MMPoint.psz3DLayerName, 
            reset_extension(hMiraMonLayer->MMPoint.psz3DLayerName, "~z"));
        
        if(NULL==(hMiraMonLayer->MMPoint.pF3d=fopen_function(
            hMiraMonLayer->MMPoint.psz3DLayerName, 
            hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(hMiraMonLayer->MMPoint.pF3d, 0, SEEK_SET);
    }
    
    // Zsection
    if(MMInitZSectionLayer(hMiraMonLayer, 
                    hMiraMonLayer->MMPoint.pF3d, 
                    &hMiraMonLayer->MMPoint.pZSection,
                    hMiraMonLayer->nHeaderDiskSize+
                    hMiraMonLayer->nSuposedElemCount*
                    hMiraMonLayer->MMPoint.FlushTL.nMyDiskSize))
        return 1;

    // MIRAMON DATA BASE
    // Creating the DBF file name
    hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName=strdup_function(
                hMiraMonLayer->MMPoint.pszLayerName);
    if(MMResetExtensionAndLastLetter(hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName, 
                hMiraMonLayer->MMPoint.pszLayerName, "T.dbf"))
            return 1;

    return 0;
}

int MMInitNodeLayer(struct MiraMonLayerInfo *hMiraMonLayer, int bIs3d)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    // Init header structure
    pMMArcLayer->TopNodeHeader.aFileType[0]='N';
    pMMArcLayer->TopNodeHeader.aFileType[1]='O';
    pMMArcLayer->TopNodeHeader.aFileType[2]='D';

    pMMArcLayer->TopNodeHeader.bIs3d=bIs3d;
    MMInitBoundingBox(&pMMArcLayer->TopNodeHeader.hBB);

    // Opening the binary file where sections TH, NH and NL[...]
    // are going to be written.
    pMMArcLayer->MMNode.pszLayerName=strdup_function(pMMArcLayer->pszLayerName);
    strcpy(pMMArcLayer->MMNode.pszLayerName, 
                reset_extension(pMMArcLayer->MMNode.pszLayerName, "nod"));
    if(NULL==(pMMArcLayer->MMNode.pF = 
            fopen_function(pMMArcLayer->MMNode.pszLayerName, 
            hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMArcLayer->MMNode.pF, 0, SEEK_SET);


    // Node Header
    pMMArcLayer->MMNode.nMaxNodeHeader=2*hMiraMonLayer->nSuposedElemCount;
    if(NULL==(pMMArcLayer->MMNode.pNodeHeader=(struct MM_NH *)calloc_function(
                            pMMArcLayer->MMNode.nMaxNodeHeader*
                            sizeof(*pMMArcLayer->MMNode.pNodeHeader))))
        return 1;

    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMArcLayer->MMNode.nSizeNodeHeader=MM_SIZE_OF_NH_32BITS;
    else
        pMMArcLayer->MMNode.nSizeNodeHeader=MM_SIZE_OF_NH_64BITS;

    // NL Section
    pMMArcLayer->MMNode.pszNLName=strdup_function(pMMArcLayer->pszLayerName);
            strcpy(pMMArcLayer->MMNode.pszNLName, 
            reset_extension(pMMArcLayer->MMNode.pszNLName, "~NL"));

    if(NULL==(pMMArcLayer->MMNode.pFNL = 
            fopen_function(pMMArcLayer->MMNode.pszNLName, 
            hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMArcLayer->MMNode.pFNL, 0, SEEK_SET);

    if(MMInitFlush(&pMMArcLayer->MMNode.FlushNL, pMMArcLayer->MMNode.pFNL, 
            2*hMiraMonLayer->nSuposedElemCount, &pMMArcLayer->MMNode.pNL, 0, 0))
           return 1;

    // Creating the DBF file name
    pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName=strdup_function(
                pMMArcLayer->MMNode.pszLayerName);
    if(MMResetExtensionAndLastLetter(pMMArcLayer->MMNode.MMAdmDB.pszExtDBFLayerName, 
                pMMArcLayer->MMNode.pszLayerName, "N.dbf"))
            return 1;

    return 0;
}

int MMInitArcLayer(struct MiraMonLayerInfo *hMiraMonLayer, int bIs3d)
{
struct MiraMonArcLayer *pMMArcLayer;
struct MM_TH *pArcTopHeader;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader=&hMiraMonLayer->MMPolygon.TopArcHeader;
    }
    else
    {
        pMMArcLayer=&hMiraMonLayer->MMArc;
        pArcTopHeader=&hMiraMonLayer->TopHeader;
    }

    // Init header structure
    hMiraMonLayer->bIsArc=1;
    pArcTopHeader->bIs3d=bIs3d;
    MMInitBoundingBox(&pArcTopHeader->hBB);
    
    pArcTopHeader->aFileType[0]='A';
    pArcTopHeader->aFileType[1]='R';
    pArcTopHeader->aFileType[2]='C';

    if(NULL==(pMMArcLayer->pF = fopen_function(pMMArcLayer->pszLayerName, 
        hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMArcLayer->pF, 0, SEEK_SET);

    // AH
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMArcLayer->nSizeArcHeader=MM_SIZE_OF_AH_32BITS;
    else
        pMMArcLayer->nSizeArcHeader=MM_SIZE_OF_AH_64BITS;

    pMMArcLayer->nMaxArcHeader=hMiraMonLayer->nSuposedElemCount;
    if(NULL==(pMMArcLayer->pArcHeader=(struct MM_AH *)calloc_function(
                            pMMArcLayer->nMaxArcHeader*
                            sizeof(*pMMArcLayer->pArcHeader))))
        return 1;

    // AL
    pMMArcLayer->nALElementSize=MM_SIZE_OF_AL;
    pMMArcLayer->pszALName=strdup_function(pMMArcLayer->pszLayerName);
    strcpy(pMMArcLayer->pszALName, 
        reset_extension(pMMArcLayer->pszALName, "~AL"));
        
    if(NULL==(pMMArcLayer->pFAL = fopen_function(pMMArcLayer->pszALName, 
        hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMArcLayer->pFAL, 0, SEEK_SET);

    // ·$· Aproximate properly (N vertices in a layer)
    if(MMInitFlush(&pMMArcLayer->FlushAL, pMMArcLayer->pFAL, 
            2*hMiraMonLayer->nSuposedElemCount, &pMMArcLayer->pAL, 0, 0))
           return 1;

    // 3D
    if(pArcTopHeader->bIs3d)
    {
        pMMArcLayer->psz3DLayerName=strdup_function(pMMArcLayer->pszLayerName);
            strcpy(pMMArcLayer->psz3DLayerName, 
                reset_extension(pMMArcLayer->psz3DLayerName, "~z"));
    
        if(NULL==(pMMArcLayer->pF3d=fopen_function(pMMArcLayer->psz3DLayerName, 
            hMiraMonLayer->pszFlags)))
            return 1;
        fseek_function(pMMArcLayer->pF3d, 0, SEEK_SET);

        if(MMInitZSectionLayer(hMiraMonLayer, 
                        pMMArcLayer->pF3d, 
                        &pMMArcLayer->pZSection,
                        hMiraMonLayer->nHeaderDiskSize+
                        hMiraMonLayer->nSuposedElemCount*pMMArcLayer->nSizeArcHeader+
                        hMiraMonLayer->nSuposedElemCount*pMMArcLayer->FlushAL.nMyDiskSize))
            return 1;
    }

    // Node part
    if(MMInitNodeLayer(hMiraMonLayer, bIs3d))
    {
        MMFreeLayer(hMiraMonLayer);
        return 1;
    }
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        MMSet1_1Version(&pMMArcLayer->TopNodeHeader);
    else
        MMSet2_0Version(&pMMArcLayer->TopNodeHeader);

    // MIRAMON DATA BASE
    // Creating the DBF file name
    pMMArcLayer->MMAdmDB.pszExtDBFLayerName=strdup_function(
                pMMArcLayer->pszLayerName);
    if(MMResetExtensionAndLastLetter(pMMArcLayer->MMAdmDB.pszExtDBFLayerName, 
                pMMArcLayer->pszLayerName, "A.dbf"))
            return 1;

    return 0;
}

int MMInitPolygonLayer(struct MiraMonLayerInfo *hMiraMonLayer, int bIs3d)
{
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Init header structure
    hMiraMonLayer->bIsPolygon=1;
    hMiraMonLayer->TopHeader.bIs3d=bIs3d;
    MMInitBoundingBox(&hMiraMonLayer->TopHeader.hBB);

    hMiraMonLayer->TopHeader.aFileType[0]='P';
    hMiraMonLayer->TopHeader.aFileType[1]='O';
    hMiraMonLayer->TopHeader.aFileType[2]='L';

    if(NULL==(pMMPolygonLayer->pF = 
            fopen_function(pMMPolygonLayer->pszLayerName, hMiraMonLayer->pszFlags)))
        return 1;

    // PS
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMPolygonLayer->nPSElementSize=MM_SIZE_OF_PS_32BITS;
    else
        pMMPolygonLayer->nPSElementSize=MM_SIZE_OF_PS_64BITS;

    pMMPolygonLayer->pszPSName=strdup_function(pMMPolygonLayer->pszLayerName);
    strcpy(pMMPolygonLayer->pszPSName, 
        reset_extension(pMMPolygonLayer->pszPSName, "~PS"));

    if(NULL==(pMMPolygonLayer->pFPS = fopen_function(pMMPolygonLayer->pszPSName, 
            hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMPolygonLayer->pFPS, 0, SEEK_SET);

    if(MMInitFlush(&pMMPolygonLayer->FlushPS, pMMPolygonLayer->pFPS, 
            hMiraMonLayer->nSuposedElemCount, &pMMPolygonLayer->pPS, 0, 
            pMMPolygonLayer->nPSElementSize))
           return 1;

    // PH
    if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        pMMPolygonLayer->nPHElementSize=MM_SIZE_OF_PH_32BITS;
    else
        pMMPolygonLayer->nPHElementSize=MM_SIZE_OF_PH_64BITS;

    pMMPolygonLayer->nMaxPolHeader=hMiraMonLayer->nSuposedElemCount+1;  
    if(NULL==(pMMPolygonLayer->pPolHeader=(struct MM_PH *)calloc_function(
            pMMPolygonLayer->nMaxPolHeader*
            sizeof(*pMMPolygonLayer->pPolHeader))))
        return 1;

    // Universal polygon.
    memset(pMMPolygonLayer->pPolHeader, 0, sizeof(*pMMPolygonLayer->pPolHeader));
    hMiraMonLayer->TopHeader.nElemCount=1;

    // PAL
    pMMPolygonLayer->pszPALName=strdup_function(pMMPolygonLayer->pszLayerName);
    strcpy(pMMPolygonLayer->pszPALName, 
        reset_extension(pMMPolygonLayer->pszPALName, "~PL"));

    if(NULL==(pMMPolygonLayer->pFPAL = fopen_function(pMMPolygonLayer->pszPALName, 
        hMiraMonLayer->pszFlags)))
        return 1;
    fseek_function(pMMPolygonLayer->pFPAL, 0, SEEK_SET);

    if(MMInitFlush(&pMMPolygonLayer->FlushPAL, pMMPolygonLayer->pFPAL, 
            hMiraMonLayer->nSuposedElemCount, &pMMPolygonLayer->pPAL, 0, 0))
           return 1;

    // Creating the DBF file name
    pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName=strdup_function(
                pMMPolygonLayer->pszLayerName);
    if(MMResetExtensionAndLastLetter(pMMPolygonLayer->MMAdmDB.pszExtDBFLayerName, 
                pMMPolygonLayer->pszLayerName, "P.dbf"))
            return 1;

    return 0;
}

int MMInitLayer(struct MiraMonLayerInfo *hMiraMonLayer, const char *pzFileName, 
                __int32 LayerVersion, int eLT,MM_INTERNAL_FID nElemCount, 
                struct MiraMonDataBase *pLayerDB)
{
    int bIs3d=0;

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));
    hMiraMonLayer->Version=MM_VECTOR_LAYER_LAST_VERSION;
    
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

    if(eLT==MM_LayerType_Point || eLT==MM_LayerType_Point3d)
    {
        // ·$· TREURE 
        nElemCount=0;

        if(nElemCount)
            hMiraMonLayer->nSuposedElemCount=nElemCount;
        else
            hMiraMonLayer->nSuposedElemCount=MM_FIRST_NUMBER_OF_POINTS;
        
        hMiraMonLayer->MMPoint.pszLayerName=strdup_function(pzFileName);
        if(eLT==MM_LayerType_Point3d)
            bIs3d=1;

        if(MMInitPointLayer(hMiraMonLayer, bIs3d))
        {
            MMFreeLayer(hMiraMonLayer);
            return 1;
        }
    }
    else if(eLT==MM_LayerType_Arc || eLT==MM_LayerType_Arc3d)
    {
        struct MiraMonArcLayer *pMMArcLayer=&hMiraMonLayer->MMArc;

        // ·$· TREURE 
        nElemCount=0;

        if(nElemCount)
            hMiraMonLayer->nSuposedElemCount=nElemCount;
        else
            hMiraMonLayer->nSuposedElemCount=MM_FIRST_NUMBER_OF_ARCS;

        pMMArcLayer->pszLayerName=strdup_function(pzFileName);
        if(eLT==MM_LayerType_Arc3d)
            bIs3d=1;

        if(MMInitArcLayer(hMiraMonLayer, bIs3d))
        {
            MMFreeLayer(hMiraMonLayer);
            return 1;
        }
    }
    else if(eLT==MM_LayerType_Pol || eLT==MM_LayerType_Pol3d)
    {
        struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

         // ·$· TREURE 
        nElemCount=0;

        if(nElemCount)
            hMiraMonLayer->nSuposedElemCount=nElemCount;
        else
            hMiraMonLayer->nSuposedElemCount=MM_FIRST_NUMBER_OF_POLYGONS;

        if(eLT==MM_LayerType_Pol3d)
            bIs3d=1;
        
        pMMPolygonLayer->pszLayerName=strdup_function(pzFileName);
        if(MMInitPolygonLayer(hMiraMonLayer, bIs3d))
        {
            MMFreeLayer(hMiraMonLayer);
            return 1;
        }

        pMMPolygonLayer->MMArc.pszLayerName=strdup_function(pzFileName);
        strcpy(pMMPolygonLayer->MMArc.pszLayerName, 
            reset_extension(pMMPolygonLayer->MMArc.pszLayerName, "arc"));
        if(MMInitArcLayer(hMiraMonLayer, bIs3d))
        {
            MMFreeLayer(hMiraMonLayer);
            return 1;
        }

        if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
            MMSet1_1Version(&pMMPolygonLayer->TopArcHeader);
        else
            MMSet2_0Version(&pMMPolygonLayer->TopArcHeader);
    }

    // Return the handle to the layer
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Closing                                        */
/* -------------------------------------------------------------------- */
int MMClose3DSectionLayer(struct MiraMonLayerInfo *hMiraMonLayer, 
                        MM_INTERNAL_FID nElements,
                        FILE_TYPE *pF,
                        FILE_TYPE *pF3d,
                        const char *pszF3d,
                        struct MM_ZSection *pZSection,
                        MM_FILE_OFFSET FinalOffset)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    // Flushing if there is something to flush on the disk
    if(!hMiraMonLayer->TopHeader.bIs3d)
        return 0;
    
    pZSection->ZSectionOffset=FinalOffset;
    if(MMWrite3DHeader(pF, pZSection))
        return 1;
    
    // Header 3D. Writes it after header
    if(MMWrite3DDescriptionHeaders(hMiraMonLayer, pF, nElements, pZSection))
        return 1;
    
    // ZL section
    pZSection->FlushZL.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&pZSection->FlushZL))
        return 1;
    
    if(MMMoveFromFileToFile(pF3d, pF, NULL))
        return 1;

    fclose_function(pF3d);
    remove_function(pszF3d);

    return 0;
}

int MMClosePointLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    hMiraMonLayer->nFinalElemCount=hMiraMonLayer->TopHeader.nElemCount;

    if(MMWriteHeader(hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->TopHeader))
        return 1;
    hMiraMonLayer->OffsetCheck=hMiraMonLayer->nHeaderDiskSize;

    // TL Section
    hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
        return 1;
    if(MMMoveFromFileToFile(hMiraMonLayer->MMPoint.pFTL, 
            hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->OffsetCheck))
        return 1;
    fclose_function(hMiraMonLayer->MMPoint.pFTL);
    remove_function(hMiraMonLayer->MMPoint.pszTLName);

    if(MMClose3DSectionLayer(hMiraMonLayer, 
        hMiraMonLayer->TopHeader.nElemCount,
        hMiraMonLayer->MMPoint.pF, 
        hMiraMonLayer->MMPoint.pF3d, 
        hMiraMonLayer->MMPoint.psz3DLayerName,
        &hMiraMonLayer->MMPoint.pZSection,
        hMiraMonLayer->OffsetCheck))
        return 1;

    fclose_function(hMiraMonLayer->MMPoint.pF);
    return 0;
}

int MMCloseNodeLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArcLayer=&hMiraMonLayer->MMArc;

    if(MMWriteHeader(pMMArcLayer->MMNode.pF, &pMMArcLayer->TopNodeHeader))
        return 1;
    hMiraMonLayer->OffsetCheck=hMiraMonLayer->nHeaderDiskSize;

    // NH Section
    if(MMWriteNHNodeSection(hMiraMonLayer, hMiraMonLayer->nHeaderDiskSize))
        return 1;

    // NL Section
    pMMArcLayer->MMNode.FlushNL.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&pMMArcLayer->MMNode.FlushNL))
        return 1;
    if(MMMoveFromFileToFile(pMMArcLayer->MMNode.pFNL, 
            pMMArcLayer->MMNode.pF, &hMiraMonLayer->OffsetCheck))
        return 1;
    fclose_function(pMMArcLayer->MMNode.pFNL);
    remove_function(pMMArcLayer->MMNode.pszNLName);

    fclose_function(pMMArcLayer->MMNode.pF);
    
    return 0;
}

int MMCloseArcLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
struct MiraMonArcLayer *pMMArcLayer;
struct MM_TH *pArcTopHeader;

    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->bIsPolygon)
    {
        pMMArcLayer=&hMiraMonLayer->MMPolygon.MMArc;
        pArcTopHeader=&hMiraMonLayer->MMPolygon.TopArcHeader;
    }
    else
    {
        pMMArcLayer=&hMiraMonLayer->MMArc;
        pArcTopHeader=&hMiraMonLayer->TopHeader;
    }

    
    hMiraMonLayer->nFinalElemCount=pArcTopHeader->nElemCount;
    
    if(MMWriteHeader(pMMArcLayer->pF, pArcTopHeader))
        return 1;
    hMiraMonLayer->OffsetCheck=hMiraMonLayer->nHeaderDiskSize;
    
    // AH Section
    if(MMWriteAHArcSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        return 1;

    // AL Section
    pMMArcLayer->FlushAL.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&pMMArcLayer->FlushAL))
        return 1;
    if(MMMoveFromFileToFile(pMMArcLayer->pFAL, pMMArcLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;
    fclose_function(pMMArcLayer->pFAL);
    remove_function(pMMArcLayer->pszALName);

    // 3D Section
    if(MMClose3DSectionLayer(hMiraMonLayer, 
            pArcTopHeader->nElemCount,
            pMMArcLayer->pF, 
            pMMArcLayer->pF3d, 
            pMMArcLayer->psz3DLayerName,
            &pMMArcLayer->pZSection,
            hMiraMonLayer->OffsetCheck))
            return 1;

    fclose_function(pMMArcLayer->pF);
    
    MMCloseNodeLayer(hMiraMonLayer);

    return 0;
}

int MMClosePolygonLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;
    
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    MMCloseArcLayer(hMiraMonLayer);

    hMiraMonLayer->nFinalElemCount=hMiraMonLayer->TopHeader.nElemCount;
    if(MMWriteHeader(pMMPolygonLayer->pF, &hMiraMonLayer->TopHeader))
        return 1;
    hMiraMonLayer->OffsetCheck=hMiraMonLayer->nHeaderDiskSize;

    // PS Section
    pMMPolygonLayer->FlushPS.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPS))
        return 1;
    if(MMMoveFromFileToFile(pMMPolygonLayer->pFPS, pMMPolygonLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;
    fclose_function(pMMPolygonLayer->pFPS);
    remove_function(pMMPolygonLayer->pszPSName);

    // AH Section
    if(MMWritePHPolygonSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        return 1;

    // PAL Section
    pMMPolygonLayer->FlushPAL.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPAL))
        return 1;
    if(MMMoveFromFileToFile(pMMPolygonLayer->pFPAL, pMMPolygonLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;
    fclose_function(pMMPolygonLayer->pFPAL);
    remove_function(pMMPolygonLayer->pszPALName);

    fclose_function(pMMPolygonLayer->pF);
    
    return 0;
}

int MMCloseLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(!hMiraMonLayer)
        return 0;

    if(hMiraMonLayer->bIsPoint)
    {
        if(MMClosePointLayer(hMiraMonLayer))
            return 1;
    }
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseArcLayer(hMiraMonLayer))
            return 1;
    }
    if(hMiraMonLayer->bIsPolygon)
    {
      if(MMClosePolygonLayer(hMiraMonLayer))
          return 1;
    }

    // MiraMon metadata files
    if(MMWriteVectorMetadata(hMiraMonLayer))
        return 1;

    // MiraMon database files
    if(MMCloseMMBD_XP(hMiraMonLayer))
        return 1;

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
}
int MMDestroyPointLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(hMiraMonLayer->MMPoint.pTL)
    {
        free_function(hMiraMonLayer->MMPoint.pTL);
        hMiraMonLayer->MMPoint.pTL=NULL;
    }

    if(hMiraMonLayer->TopHeader.bIs3d)
        MMDestroyZSectionDescription(&hMiraMonLayer->MMPoint.pZSection);

    if(hMiraMonLayer->MMPoint.pszLayerName)
    {
        free_function(hMiraMonLayer->MMPoint.pszLayerName);
        hMiraMonLayer->MMPoint.pszLayerName=NULL;
        if(hMiraMonLayer->TopHeader.bIs3d)
        {
            free_function(hMiraMonLayer->MMPoint.psz3DLayerName);
            hMiraMonLayer->MMPoint.psz3DLayerName=NULL;
        }
    }

    MMDestroyMMAdmDB(&hMiraMonLayer->MMPoint.MMAdmDB);

    return 0;
}

int MMDestroyNodeLayer(struct MiraMonLayerInfo *hMiraMonLayer)
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
    if(pMMArcLayer->MMNode.pszLayerName)
    {
        free_function(pMMArcLayer->MMNode.pszLayerName);
        pMMArcLayer->MMNode.pszLayerName=NULL;
    }

    MMDestroyMMAdmDB(&hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    return 0;
}

int MMDestroyArcLayer(struct MiraMonLayerInfo *hMiraMonLayer)
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

    if(pMMArcLayer->pszLayerName)
    {
        free_function(pMMArcLayer->pszLayerName);
        pMMArcLayer->pszLayerName=NULL;
    }

    MMDestroyMMAdmDB(&pMMArcLayer->MMAdmDB);

    MMDestroyNodeLayer(hMiraMonLayer);
    return 0;
}

int MMDestroyPolygonLayer(struct MiraMonLayerInfo *hMiraMonLayer)
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

    if(pMMPolygonLayer->pszLayerName)
    {
        free_function(pMMPolygonLayer->pszLayerName);
        pMMPolygonLayer->pszLayerName=NULL;
    }

    if(pMMPolygonLayer->pszPSName)
    {
        free_function(pMMPolygonLayer->pszPSName);
        pMMPolygonLayer->pszPSName=NULL;
    }

    if(pMMPolygonLayer->pszPALName)
    {
        free_function(pMMPolygonLayer->pszPALName);
        pMMPolygonLayer->pszPALName=NULL;
    }

    MMDestroyMMAdmDB(&pMMPolygonLayer->MMAdmDB);

    return 0;
}

int MMFreeLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(!hMiraMonLayer)
        return 0;

    if(hMiraMonLayer->bIsPoint)
        MMDestroyPointLayer(hMiraMonLayer);
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
        MMDestroyArcLayer(hMiraMonLayer);
    else if(hMiraMonLayer->bIsPolygon)
        MMDestroyPolygonLayer(hMiraMonLayer);

    // Destroys all database objects
    MMDestroyMMDB(hMiraMonLayer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Creating a layer                               */
/* -------------------------------------------------------------------- */
struct MiraMonLayerInfo * MMCreateLayer(char *pzFileName, 
            __int32 LayerVersion, int eLT, MM_INTERNAL_FID nElemCount, 
            struct MiraMonDataBase *hLayerDB)
{
struct MiraMonLayerInfo *hMiraMonLayer;

    // Creating of the handle to a MiraMon Layer
    hMiraMonLayer=(struct MiraMonLayerInfo *)calloc_function(
                    sizeof(*hMiraMonLayer));
    if(MMInitLayer(hMiraMonLayer, pzFileName, LayerVersion, eLT, 
                nElemCount, hLayerDB))
        return NULL;

    // Return the handle to the layer
    return hMiraMonLayer;
}

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
    return 0;
}

int MMFlushToDisk(struct MM_FLUSH_INFO *FlushInfo)
{
    if(!FlushInfo->nNumBytes)
        return 0;
    // Just flush to the disk at the correct place.
    fseek_function(FlushInfo->pF, FlushInfo->OffsetWhereToFlush, SEEK_SET);

    if(FlushInfo->nNumBytes!=fwrite_function(FlushInfo->pBlockWhereToSave, 1, 
            FlushInfo->nNumBytes, FlushInfo->pF))
        return 1;
    FlushInfo->OffsetWhereToFlush+=FlushInfo->nNumBytes;
    FlushInfo->NTimesFlushed++;
    FlushInfo->TotalSavedBytes+=FlushInfo->nNumBytes;
    FlushInfo->nNumBytes=0;

    return 0;
}

int MMAppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo)
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

        // There is space in FlushInfo->pBlockWhereToSave?
        if(FlushInfo->nNumBytes+FlushInfo->SizeOfBlockToBeSaved<=
            FlushInfo->nBlockSize)
        {
            if(FlushInfo->pBlockToBeSaved)
            {
                memcpy((void *)((char *)FlushInfo->pBlockWhereToSave+
                    FlushInfo->nNumBytes), FlushInfo->pBlockToBeSaved, 
                    FlushInfo->SizeOfBlockToBeSaved);
            }
            else // Add zero caracters
            {
                char zero_caracters[8]="\0\0\0\0\0\0\0";
                memcpy((char *)FlushInfo->pBlockWhereToSave+
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
            if(MMAppendBlockToBuffer(FlushInfo))
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
unsigned char* buffer = (unsigned char*)calloc_function(bufferSize);
size_t bytesRead, bytesWritten;

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
size_t reajust;

    if ((*Offset) % 8L)
	{
		reajust=(size_t)8-((*Offset)%8L);
		(*Offset)+=reajust;
	}
}


int MMAppendIntegerDependingOnVersion(
                            struct MiraMonLayerInfo *hMiraMonLayer,
                            struct MM_FLUSH_INFO *FlushInfo, 
                            unsigned long *nUL32, unsigned __int64 nUI64)
{
    // Offset: offset of the first vertice of the arc
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
    return MMAppendBlockToBuffer(FlushInfo);
}

int MMWriteAHArcSection(struct MiraMonLayerInfo *hMiraMonLayer, 
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
                MM_500MB, &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSave=(void *)pBuffer;
    for(iElem=0; iElem<hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->pArcHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
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
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMWriteNHNodeSection(struct MiraMonLayerInfo *hMiraMonLayer, 
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
                MM_500MB, &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSave=(void *)pBuffer;
    for(iElem=0; iElem<pMMArcLayer->TopNodeHeader.nElemCount; iElem++)
    {
        // Arcs count
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount);
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMArcLayer->MMNode.pNodeHeader[iElem].nArcsCount;
        if(MMAppendBlockToBuffer(&FlushTMP))
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
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.SizeOfBlockToBeSaved=1;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        FlushTMP.pBlockToBeSaved=(void *)NULL;
        if(MMAppendBlockToBuffer(&FlushTMP))
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
    if(MMAppendBlockToBuffer(&FlushTMP))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    if(pBuffer)
        free_function(pBuffer);
    return 0;
}

int MMWritePHPolygonSection(struct MiraMonLayerInfo *hMiraMonLayer, 
                            MM_FILE_OFFSET DiskOffset)
{
MM_INTERNAL_FID iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
MM_FILE_OFFSET nOffsetDiff;
struct MiraMonPolygonLayer *pMMPolygonLayer=&hMiraMonLayer->MMPolygon;

    nOffsetDiff=DiskOffset+
        hMiraMonLayer->TopHeader.nElemCount*
        (pMMPolygonLayer->nPHElementSize);

    if(MMInitFlush(&FlushTMP, pMMPolygonLayer->pF, 
                MM_500MB, &pBuffer, DiskOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    FlushTMP.pBlockWhereToSave=(void *)pBuffer;
    for(iElem=0; iElem<hMiraMonLayer->nFinalElemCount; iElem++)
    {
        // Bounding box
        FlushTMP.SizeOfBlockToBeSaved=
            sizeof(pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX);
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxX;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMinY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
        FlushTMP.pBlockToBeSaved=
            (void *)&pMMPolygonLayer->pPolHeader[iElem].dfBB.dfMaxY;
        hMiraMonLayer->OffsetCheck+=FlushTMP.SizeOfBlockToBeSaved;
        if(MMAppendBlockToBuffer(&FlushTMP))
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
        if(MMAppendBlockToBuffer(&FlushTMP))
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
        if(MMAppendBlockToBuffer(&FlushTMP))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }
    }
    FlushTMP.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&FlushTMP))
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

    hMMFeature->nMaxRecords=1;
    if((hMMFeature->pRecords=calloc_function(
            hMMFeature->nMaxRecords*sizeof(*(hMMFeature->pRecords))))==NULL)
        return 1;

    hMMFeature->pRecords[0].nMaxField = MM_INIT_NUMBER_OF_FIELDS;
    hMMFeature->pRecords[0].nNumField = 0;
    if(NULL==(hMMFeature->pRecords[0].pField=calloc_function(
            hMMFeature->pRecords[0].nMaxField*
            sizeof(*(hMMFeature->pRecords[0].pField)))))
        return 1;
    
    return 0;
}

// Conserves all allocated memroy but inicialize the counters to zero.
void MMResetFeature(struct MiraMonFeature *hMMFeature)
{
    hMMFeature->nNRings=0;
    hMMFeature->nIRing=0;
    hMMFeature->nICoord=0;
    hMMFeature->nNumRecords=0;
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
    if(hMMFeature->pNCoord)
    {
        free_function(hMMFeature->pNCoord);
        hMMFeature->pNCoord=NULL;
    }
    if(hMMFeature->pRecords)
    {
        free_function(hMMFeature->pRecords);
        hMMFeature->pRecords=NULL;
    }
    hMMFeature->nNRings=0;
    hMMFeature->nNumRecords=0;
    hMMFeature->nMaxRecords=0;
}

int MMCreateFeaturePolOrArc(struct MiraMonLayerInfo *hMiraMonLayer, 
                struct MiraMonFeature *hMMFeature)
{
double *pZ=NULL;
struct MM_POINT_2D *pCoord;
unsigned __int64 nIPart, nIVertice;
double dtempx, dtempy;
unsigned __int64 nExternalRingsCount;
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
MM_TIPUS_N_VERTEXS nPolVertices=0;

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

    pFlushNL->pBlockWhereToSave=(void *)pMMNode->pNL;
    pFlushAL->pBlockWhereToSave=(void *)pMMArc->pAL;
    if (hMiraMonLayer->TopHeader.bIs3d)
        pFlushZL->pBlockWhereToSave=(void *)pMMArc->pZSection.pZL;
    if (hMiraMonLayer->bIsPolygon)
    {
        pFlushPS->pBlockWhereToSave=(void *)hMiraMonLayer->MMPolygon.pPS;
        pFlushPAL->pBlockWhereToSave=
            (void *)hMiraMonLayer->MMPolygon.pPAL;
    }

    // Creation of the MiraMon extended database
    if(!hMiraMonLayer->bIsPolygon)
    {
        if(hMiraMonLayer->TopHeader.nElemCount==0)
        {
            if(MMCreateMMDB(hMiraMonLayer))
                return 1;
        }
    }
    else
    {   // Universal polygon has been created
        if(hMiraMonLayer->TopHeader.nElemCount==1)
        {
            if(MMCreateMMDB(hMiraMonLayer))
                return 1;
            
            // Universal polygon have a record with ID_GRAFIC=0 and blancs
            if(MMAddPolygonRecordToMMDB(hMiraMonLayer, NULL, 0, 0, NULL))
                return 1;
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
                return MM_STOP_WRITING_FEATURES;
            
            // Arc if there is no polygon
            if(MMCheckVersionForFID(hMiraMonLayer, nArcElemCount))
                return MM_STOP_WRITING_FEATURES;

            // Nodes
            if(MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount))
                return MM_STOP_WRITING_FEATURES;

            // There is space for the last node(s) that is(are) going to be written?
            if(!hMiraMonLayer->bIsPolygon)
            {
                if(MMCheckVersionForFID(hMiraMonLayer, nNodeElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }

            // Checking offsets
            // AL: check the last point
            if(MMCheckVersionOffset(hMiraMonLayer, nArcOffset))
                return MM_STOP_WRITING_FEATURES;
            // Setting next offset
            nArcOffset+=(hMMFeature->pNCoord[nIPart])*pMMArc->nALElementSize;

            // NL: check the last node
            if(hMiraMonLayer->bIsPolygon)
                nNodeOffset+=(hMMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;
            else
                nNodeOffset+=(2*hMMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;

            if(MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
                return MM_STOP_WRITING_FEATURES;
            // Setting next offset
            nNodeOffset+=MM_SIZE_OF_NL_32BITS;

            if(!hMiraMonLayer->bIsPolygon)
            {
                if(MMCheckVersionOffset(hMiraMonLayer, nNodeOffset))
                   return MM_STOP_WRITING_FEATURES;
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
                nArcOffset+=hMMFeature->pNCoord[nIPart]*pMMArc->nALElementSize;
                if(MMCheckVersionFor3DOffset(hMiraMonLayer, nArcOffset, 
                    hMiraMonLayer->TopHeader.nElemCount+hMMFeature->nNRings))
                    return MM_STOP_WRITING_FEATURES;
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
                        pArcTopHeader->nElemCount,
                        MM_INCR_NUMBER_OF_ARCS,
                        hMMFeature->nNRings>hMiraMonLayer->nSuposedElemCount?
                        hMMFeature->nNRings:hMiraMonLayer->nSuposedElemCount))
        {
            error_message_function("Memory error\n");
            return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        if(MMResizeNodeHeaderPointer(&pMMNode->pNodeHeader, 
                        &pMMNode->nMaxNodeHeader, 
                        pNodeTopHeader->nElemCount+1, 
                        MM_INCR_NUMBER_OF_NODES,
                        0))
        {
            error_message_function("Memory error\n");
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
                error_message_function("Memory error\n");
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
        pCurrentArcHeader->nElemCount=hMMFeature->pNCoord[nIPart];
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
            if(MMAppendBlockToBuffer(pFlushAL))
                return 1;
            pFlushAL->pBlockToBeSaved=(void *)&pCoord->dfY;
            if(MMAppendBlockToBuffer(pFlushAL))
                return 1;

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
        if(MMAddArcRecordToMMDB(hMiraMonLayer, hMMFeature, 
                pArcTopHeader->nElemCount, pCurrentArcHeader))
            return 1;
                
        // Node Stuff: writting NL section
        pCurrentNodeHeader->nArcsCount=1;
        if(hMiraMonLayer->bIsPolygon)
            pCurrentNodeHeader->cNodeType=MM_RING_NODE;
        else
            pCurrentNodeHeader->cNodeType=MM_FINAL_NODE;
        
        pCurrentNodeHeader->nOffset=pFlushNL->TotalSavedBytes+
            pFlushNL->nNumBytes;
        if(MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                       pFlushNL, 
                       &UnsignedLongNumber, pArcTopHeader->nElemCount))
            return MM_FATAL_ERROR_WRITING_FEATURES;

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
            if(MMAppendBlockToBuffer(pFlushNL))
                return 1;
        }
        if(MMAddNodeRecordToMMDB(hMiraMonLayer,
                pNodeTopHeader->nElemCount, pCurrentNodeHeader))
            return 1;

        if(!hMiraMonLayer->bIsPolygon)
        {
            pCurrentNodeHeaderPlus1->nArcsCount=1;
            if(hMiraMonLayer->bIsPolygon)
                pCurrentNodeHeaderPlus1->cNodeType=MM_RING_NODE;
            else
                pCurrentNodeHeaderPlus1->cNodeType=MM_FINAL_NODE;
            
            pCurrentNodeHeaderPlus1->nOffset=pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes;

            if(MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                           pFlushNL, 
                           &UnsignedLongNumber, pArcTopHeader->nElemCount))
                return MM_FATAL_ERROR_WRITING_FEATURES;
        
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
                if(MMAppendBlockToBuffer(pFlushNL))
                    return 1;
            }
            if(MMAddNodeRecordToMMDB(hMiraMonLayer,
                pNodeTopHeader->nElemCount+1, pCurrentNodeHeaderPlus1))
                return 1;
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
                if(MMAppendBlockToBuffer(pFlushZL))
                    return 1;

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
            if(MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                           pFlushPS, 
                           &UnsignedLongNumber, 0))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            
            if(MMAppendIntegerDependingOnVersion(hMiraMonLayer,
                           pFlushPS, 
                           &UnsignedLongNumber, hMiraMonLayer->TopHeader.nElemCount))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            
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

			pCurrentPolHeader->nArcsCount=hMMFeature->nNRings;
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
            if(MMAppendBlockToBuffer(pFlushPAL))
                return 1;

            if(MMAppendIntegerDependingOnVersion(hMiraMonLayer, pFlushPAL, 
                    &UnsignedLongNumber, pArcTopHeader->nElemCount))
                return MM_FATAL_ERROR_WRITING_FEATURES;

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
                    if(MMAppendBlockToBuffer(pFlushPAL))
                        return 1;
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
        if(MMAddPolygonRecordToMMDB(hMiraMonLayer, hMMFeature, 
            hMiraMonLayer->TopHeader.nElemCount, 
            nPolVertices, pCurrentPolHeader))
            return 1;
        hMiraMonLayer->TopHeader.nElemCount++;

	    if(nExternalRingsCount>1)
            hMiraMonLayer->TopHeader.bIsMultipolygon=TRUE;
    }

	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePolOrArc()

int MMCreateFeaturePoint(struct MiraMonLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMMFeature)
{
double *pZ=NULL;
struct MM_POINT_2D *pCoord;
unsigned __int64 nIPart, nIVertice;
unsigned __int64 nCoord;
struct MM_ZD *pZDescription=NULL;
MM_INTERNAL_FID nElemCount;

    if(hMiraMonLayer->TopHeader.bIs3d)
		pZ=hMMFeature->pZCoord;

    nElemCount=hMiraMonLayer->TopHeader.nElemCount;
    for (nIPart=0, pCoord=hMMFeature->pCoord;
        nIPart<hMMFeature->nNRings; nIPart++, nElemCount++)
	{
        nCoord=hMMFeature->pNCoord[nIPart];
        
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
            if (hMiraMonLayer->TopHeader.bIs3d)
			{
                if(MMResizeZSectionDescrPointer(&hMiraMonLayer->MMPoint.pZSection.pZDescription, 
                        &hMiraMonLayer->MMPoint.pZSection.nMaxZDescription, 
                        nElemCount, 
                        MM_INCR_NUMBER_OF_POINTS,
                        0))
                {
                    error_message_function("Memory error\n");
                    return MM_FATAL_ERROR_WRITING_FEATURES;
				}
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
        hMiraMonLayer->MMPoint.FlushTL.pBlockWhereToSave=(void *)hMiraMonLayer->MMPoint.pTL;
        if (hMiraMonLayer->TopHeader.bIs3d)
            hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockWhereToSave=(void *)hMiraMonLayer->MMPoint.pZSection.pZL;

        // Dump point or points (MiraMon doesn't have multiple points)
        for (nIVertice=0; nIVertice<nCoord; nIVertice++, pCoord++, pZ++)
        {
            // Updating the bounding box of the layer
            MMUpdateBoundingBoxXY(&hMiraMonLayer->TopHeader.hBB, pCoord);

            // Adding the point at the memory block
            hMiraMonLayer->MMPoint.FlushTL.SizeOfBlockToBeSaved=sizeof(pCoord->dfX);
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfX;
            if(MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return 1;
            hMiraMonLayer->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfY;
            if(MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL))
                return 1;

            // Adding the 3D part, if exists, at the memory block
	        if (hMiraMonLayer->TopHeader.bIs3d)
            {
        	    hMiraMonLayer->MMPoint.pZSection.FlushZL.SizeOfBlockToBeSaved=sizeof(*pZ);
                hMiraMonLayer->MMPoint.pZSection.FlushZL.pBlockToBeSaved=(void *)pZ;
                if(MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.pZSection.FlushZL))
                    return 1;

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
                return 1;
        }

        if(MMAddPointRecordToMMDB(hMiraMonLayer, hMMFeature, nElemCount))
            return 1;
    }
    // Updating nElemCount at the header of the layer
    hMiraMonLayer->TopHeader.nElemCount=nElemCount;

    // Everything OK.
	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePoint()

int MMCheckVersionForFID(struct MiraMonLayerInfo *hMiraMonLayer, 
                         MM_INTERNAL_FID FID)
{
    if(hMiraMonLayer->LayerVersion!=MM_32BITS_VERSION)
        return 0;

    if(FID>=MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS)
            return 1;
    return 0;
}

int MMCheckVersionOffset(struct MiraMonLayerInfo *hMiraMonLayer, 
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

int MMCheckVersionFor3DOffset(struct MiraMonLayerInfo *hMiraMonLayer, 
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

int AddMMFeature(struct MiraMonLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMiraMonFeature)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)
    
    if(hMiraMonLayer->bIsPoint)
        return MMCreateFeaturePoint(hMiraMonLayer, hMiraMonFeature);
    return MMCreateFeaturePolOrArc(hMiraMonLayer, hMiraMonFeature);
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
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pFieldValue)=realloc_function(*pFieldValue, 
        *nMax*sizeof(**pFieldValue)))==NULL)
		return 1;

    return 0;
}

int MMResizeMiraMonRecord(struct MiraMonRecord **pMiraMonRecord, 
                        unsigned __int32 *nMax, 
                        unsigned __int32 nNum, 
                        unsigned __int32 nIncr,
                        unsigned __int32 nProposedMax)
{
unsigned __int32 nIRecord;

    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pMiraMonRecord)=realloc_function(*pMiraMonRecord, 
        *nMax*sizeof(**pMiraMonRecord)))==NULL)
		return 1;

    for(nIRecord=nNum; nIRecord<*nMax; nIRecord++)
    {
        memset((*pMiraMonRecord)[nIRecord].pField, 0, 
                sizeof(*((*pMiraMonRecord)[nIRecord].pField)));
        if(MMResizeMiraMonFieldValue(&((*pMiraMonRecord)[nIRecord].pField), 
                &((*pMiraMonRecord)[nIRecord].nMaxField),
                (*pMiraMonRecord)[nIRecord].nNumField, 
                MM_INIT_NUMBER_OF_FIELDS,
                0))
        return 1;
    }

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

int MMResizeUI64Pointer(unsigned __int64 **pUI64, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
{
    if(nNum<*nMax)
        return 0;
    
    *nMax=max_function(nNum+nIncr, nProposedMax);
	if(((*pUI64)=realloc_function(*pUI64, 
        *nMax*sizeof(**pUI64)))==NULL)
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
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax)
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

int IsEmptyString(const char *string)
{
char *ptr;

	for (ptr=(char*)string; *ptr; ptr++)
		if (*ptr!=' ' && *ptr!='\t')
			return 0;

	return 1;
}

/* -------------------------------------------------------------------- */
/*      Metadata Functions                                              */
/* -------------------------------------------------------------------- */
char *ReturnMMIDSRSFromEPSGCodeSRS (char *pSRS)
{
static char aMMIDSRS[MM_MAX_ID_SNY], aTempIDSRS[MM_MAX_ID_SNY];
char aMMIDDBFFile[MM_MAX_PATH]; //m_idofic.dbf
const char* filepath = __FILE__;
size_t i, len;
MM_BOOLEAN bIDFounded;

    memset(aMMIDSRS, '\0', sizeof(*aMMIDSRS));
    // Getting the current directory to build the address of the m_idofic.dbf file. 
    len = strlen(filepath);
    for (i = len; i > 0; i--)
    {
    #if defined(_WIN32)
        if (filepath[i - 1] == '\\' || filepath[i - 1] == '/') {
    #elif defined(__linux__) || defined(__APPLE__)
        if (filepath[i - 1] == '/') {
    #else
        #error "Unsupported platform"
    #endif
            if (i >= MM_MAX_PATH)
            {
                // Buffer is too small to store the folder path
                return aMMIDSRS;
            }
            MM_strnzcpy(aMMIDDBFFile, filepath, i);
            aMMIDDBFFile[i] = '\0'; // Null-terminate to get the folder path
            break;
        }
    }
    strcat(aMMIDDBFFile, "m_idofic.dbf"); // ·$· REVISEM SI HA D'ESTAR AL MATEIX DIRECTORI QUE mmwrlayr.c



    {
        GDALDatasetH hIDOfic;
        OGRLayerH hLayer;
        OGRFeatureH hFeature;
        OGRFeatureDefnH hFeatureDefn;
        OGRFieldDefnH hFieldDefn;
        int numFields=0, niField, j;
        char pszFieldName[MM_MAX_LON_FIELD_NAME_DBF];

        // Opening DBF file
        hIDOfic = GDALOpenEx(aMMIDDBFFile, GDAL_OF_VECTOR, NULL, NULL, NULL);
        if (hIDOfic == NULL) {
            printf("Error opening the DBF file.\n");
            return aMMIDSRS;
        }

        // Get the layer from the dataset (assuming there is only one layer)
        hLayer = GDALDatasetGetLayer(hIDOfic, 0);
        OGR_L_ResetReading(hLayer);
        bIDFounded=0;
        while ((hFeature = OGR_L_GetNextFeature(hLayer)) != NULL)
        {
            hFeatureDefn = OGR_L_GetLayerDefn(hLayer);
            numFields = OGR_FD_GetFieldCount(hFeatureDefn);
            for (niField=0; niField<numFields; niField++)
            {
                hFieldDefn = OGR_FD_GetFieldDefn(hFeatureDefn, niField);
                MM_strnzcpy(pszFieldName,OGR_Fld_GetNameRef(hFieldDefn), MM_MAX_LON_FIELD_NAME_DBF);
                if(0==stricmp(pszFieldName, "PSIDGEODES") && 0==stricmp(pSRS, OGR_F_GetFieldAsString(hFeature, niField)))
                {
                    bIDFounded=1;
                    for (j=niField+1; j<numFields; j++)
                    {
                        hFieldDefn = OGR_FD_GetFieldDefn(hFeatureDefn, j);
                        MM_strnzcpy(pszFieldName,OGR_Fld_GetNameRef(hFieldDefn), MM_MAX_LON_FIELD_NAME_DBF);
                        if(0==stricmp(pszFieldName, "ID_GEODES"))
                        {
                            MM_strnzcpy(aMMIDSRS, OGR_F_GetFieldAsString(hFeature, j),MM_MAX_ID_SNY);
                            OGR_F_Destroy(hFeature);
                            GDALClose(hIDOfic);
                            return aMMIDSRS;
                        }
                    }
                    break;
                }
            }
            OGR_F_Destroy(hFeature);
            if(bIDFounded)
                break;
        }
        GDALClose(hIDOfic);
    }
    return aMMIDSRS;
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

    if(NULL==(pF=fopen_function(hMMMD->aLayerName, "w+t")))
    {
        info_message_function(aMessage);
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
    printf_function(pF, "%s=%s\n", KEY_DatasetTitle, hMMMD->aLayerName);

    if(hMMMD->ePlainLT!=MM_LayerType_Node)
    {
        if(hMMMD->pSRS && hMMMD->ePlainLT!=MM_LayerType_Pol)
        {
            printf_function(pF, "\n[%s:%s]\n", SECTION_SPATIAL_REFERENCE_SYSTEM, SECTION_HORIZONTAL);
            strcpy(aMMIDSRS,ReturnMMIDSRSFromEPSGCodeSRS(hMMMD->pSRS));
            if(!IsEmptyString(aMMIDSRS))
                printf_function(pF, "%s=%s\n", KEY_HorizontalSystemIdentifier, aMMIDSRS);
            else
            {
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
    printf_function(pF, "%s=%lf\n", KEY_MinX, hMMMD->hBB.dfMinX);
    printf_function(pF, "%s=%lf\n", KEY_MaxX, hMMMD->hBB.dfMaxX);
    printf_function(pF, "%s=%lf\n", KEY_MinY, hMMMD->hBB.dfMinY);
    printf_function(pF, "%s=%lf\n", KEY_MaxY, hMMMD->hBB.dfMaxY);
    
    // Writing OVERVIEW section
    printf_function(pF, "\n[%s]\n", SECTION_OVERVIEW);
    {
        time_t currentTime;
        struct tm *pLocalTime;
        char aTimeString[30];

        currentTime = time(NULL);
        pLocalTime = localtime(&currentTime);
        sprintf(aTimeString, "%04d%02d%02d %02d%02d%02d%02d+00:00",
            pLocalTime->tm_year + 1900, pLocalTime->tm_mon + 1, pLocalTime->tm_mday,
            pLocalTime->tm_hour, pLocalTime->tm_min, pLocalTime->tm_sec, 0);
        printf_function(pF, "%s=%s\n", KEY_CreationDate, aTimeString);

    }

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

int MMResetExtensionAndLastLetter(char *pzNewLayerName, 
                                const char *pzOldLayerName, 
                                const char *MDExt)
{
    char *pszAuxName=strdup_function(pzOldLayerName);
    strcpy(pzNewLayerName,
         reset_extension(pszAuxName, "k"));
    if(strlen(pzNewLayerName)<3)
    {
        free_function(pszAuxName);
        return 1;
    }
    pzNewLayerName[strlen(pzNewLayerName)-2]='\0';
    strcat(pzNewLayerName, MDExt);
    free_function(pszAuxName);
    return 0;
}

int MMWriteVectorMetadataFile(struct MiraMonLayerInfo *hMiraMonLayer, int layerPlainType, 
                            int layerMainPlainType)
{
struct MiraMonVectorMetaData hMMMD;

    // MiraMon writes a REL file of each .pnt, .arc, .nod or .pol
    memset(&hMMMD, 0, sizeof(hMMMD));
    hMMMD.ePlainLT=layerPlainType;
    hMMMD.pSRS=hMiraMonLayer->pSRS;

    if(layerPlainType==MM_LayerType_Point)
    {
        if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                hMiraMonLayer->MMPoint.pszLayerName, "T.rel"))
            return 1;

        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
    }
    else if(layerPlainType==MM_LayerType_Arc)
    {
        // Arcs and not polygons
        if(layerMainPlainType==MM_LayerType_Arc)
        {
            if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                    hMiraMonLayer->MMArc.pszLayerName, "A.rel"))
                return 1;

            memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
            hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
        }
        // Arcs and polygons
        else
        {
            // Arc from polygon
            if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                    hMiraMonLayer->MMPolygon.MMArc.pszLayerName, "A.rel"))
                return 1;

            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMPolygon.TopArcHeader.hBB, sizeof(hMMMD.hBB));
            hMMMD.pLayerDB=NULL;
        }
    }
    else if(layerPlainType==MM_LayerType_Pol)
    {
        if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                hMiraMonLayer->MMPolygon.pszLayerName, "P.rel"))
            return 1;

        memcpy(&hMMMD.hBB, &hMiraMonLayer->TopHeader.hBB, sizeof(hMMMD.hBB));
        hMMMD.pLayerDB=hMiraMonLayer->pLayerDB;
        strcpy(hMMMD.aArcFile, 
                get_filename_function(hMiraMonLayer->MMPolygon.MMArc.pszLayerName));
    }
    else if(layerPlainType==MM_LayerType_Node)
    {
        // Node from arc
        if(layerMainPlainType==MM_LayerType_Arc)
        {
            if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                    hMiraMonLayer->MMArc.pszLayerName, "N.rel"))
                return 1;
            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMArc.TopNodeHeader.hBB, sizeof(hMMMD.hBB));
        }
        else // Node from polygon
        {
            if(MMResetExtensionAndLastLetter(hMMMD.aLayerName, 
                    hMiraMonLayer->MMPolygon.MMArc.pszLayerName, "N.rel"))
                return 1;
            memcpy(&hMMMD.hBB, &hMiraMonLayer->MMPolygon.MMArc.TopNodeHeader.hBB, sizeof(hMMMD.hBB));
        }
        hMMMD.pLayerDB=NULL;
    }
    else
        return 1;

    return MMWriteMetadataFile(&hMMMD);
}

int MMWriteVectorMetadata(struct MiraMonLayerInfo *hMiraMonLayer)
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
    error_message_function("Failed to create metadata file.");
    return 1;
}


/* -------------------------------------------------------------------- */
/*      MiraMon database functions                                      */
/* -------------------------------------------------------------------- */
int MMInitMMDB(struct MMAdmDatabase *pMMAdmDB)
{

    strcpy(pMMAdmDB->pMMBDXP->ModeLectura, "wb");
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
            pMMAdmDB->pFExtDBF, MM_250MB, &pMMAdmDB->pRecList, 
            pMMAdmDB->pMMBDXP->OffsetPrimeraFitxa, 0))
        return 1;

    pMMAdmDB->szRecordOnCourse=calloc_function(pMMAdmDB->pMMBDXP->BytesPerFitxa);
    if(!pMMAdmDB->szRecordOnCourse)
    {
        error_message_function("Not enough memory");
        return 1;
    }
    return 0;
}

int MMCreateMMDB(struct MiraMonLayerInfo *hMiraMonLayer)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL, *pBD_XP_Aux=NULL;
struct MM_CAMP MMField;
size_t nIFieldLayer;
MM_EXT_DBF_N_FIELDS nIField;
MM_EXT_DBF_N_FIELDS nNFields;

    if(hMiraMonLayer->bIsPoint)
    {
        nNFields=MM_PRIVATE_POINT_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields);

        pBD_XP=hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;
	    if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstPointFieldsDB_XP(pBD_XP)))
	        return 1;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        nNFields=MM_PRIVATE_ARC_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        pBD_XP=hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields);

        if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstArcFieldsDB_XP(pBD_XP, 0)))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMArc.MMNode.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(3);
        if (0==MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
	        return 1;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        nNFields=MM_PRIVATE_POLYGON_DB_FIELDS+hMiraMonLayer->pLayerDB->nNFields;
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(nNFields);

        pBD_XP=hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;
	    if (0==(nIField=(MM_EXT_DBF_N_FIELDS)
                    MM_DefineFirstPolygonFieldsDB_XP(pBD_XP, 0)))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMPolygon.MMArc.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(5);
        if (0==MM_DefineFirstArcFieldsDB_XP(pBD_XP_Aux, 0))
	        return 1;

        pBD_XP_Aux=hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB.pMMBDXP=MM_CreateDBFHeader(3);
        if (0==MM_DefineFirstNodeFieldsDB_XP(pBD_XP_Aux))
	        return 1;
    }
    else
        return 1;
    

    // After private MiraMon fields, other Fields are added.
    // If no compatible names, some changes are done.
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
	
	if(hMiraMonLayer->bIsPoint)
    {
        if(MMInitMMDB(&hMiraMonLayer->MMPoint.MMAdmDB))
            return 1;
    }
    else if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMInitMMDB(&hMiraMonLayer->MMArc.MMAdmDB))
            return 1;

        if(MMInitMMDB(&hMiraMonLayer->MMArc.MMNode.MMAdmDB))
            return 1;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        if(MMInitMMDB(&hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;
        
        if(MMInitMMDB(&hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;

        if(MMInitMMDB(&hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB))
            return 1;
    }
    return 0;
}

int MMAddFeatureRecordToMMDB(struct MiraMonFeature *hMMFeature,
                             struct MM_BASE_DADES_XP *pBD_XP,
                             char *pszRecordOnCourse,
                             struct MM_FLUSH_INFO *pFlushRecList,
                             MM_NUMERATOR_RECORD *nNumRecords,
                             MM_EXT_DBF_N_FIELDS nNumPrivateMMField)
{
unsigned __int32 nIRecord;
MM_EXT_DBF_N_FIELDS nIField;

    for(nIRecord=0; nIRecord<hMMFeature->nNumRecords; nIRecord++)
    {
        for (nIField=0; nIField<hMMFeature->pRecords[nIRecord].nNumField; nIField++)
        {
            if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='C')
	        {
                if(hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue)
                {
                    MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               hMMFeature->pRecords[nIRecord].pField[nIField].pDinValue,
                               FALSE);
                }
                else
                {
                    MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               hMMFeature->pRecords[nIRecord].pField[nIField].pStaticValue,
                               FALSE);
                }
	        }
            else if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='N')
	        {
                if(pBD_XP->Camp[nIField+nNumPrivateMMField].Is64)
                {
                    MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].iValue,
                               TRUE);
                }
                else
                {
                    MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].dValue,
                               FALSE);
                }
                
	        }
            else if (pBD_XP->Camp[nIField+nNumPrivateMMField].TipusDeCamp=='D')
	        {
                MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].pStaticValue,
                               FALSE);
	        }
            else
	        {
                MM_WriteValueToRecordDBXP(pszRecordOnCourse, 
                               pBD_XP->Camp+nIField+nNumPrivateMMField, 
                               &hMMFeature->pRecords[nIRecord].pField[nIField].bValue,
                               FALSE);
	        }
        }
        
        if(MMAppendBlockToBuffer(pFlushRecList))
            return 1;

        (*nNumRecords)++;
    }
    return 0;
}

int MMAddPointRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=MM_PRIVATE_POINT_DB_FIELDS;
char *pszRecordOnCourse;
struct MM_FLUSH_INFO *pFlushRecList;

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&hMiraMonLayer->MMPoint.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSave=
        (void *)hMiraMonLayer->MMPoint.MMAdmDB.pRecList;

    pBD_XP=hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP;
    pszRecordOnCourse=hMiraMonLayer->MMPoint.MMAdmDB.szRecordOnCourse;
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);
    
    if(MMAddFeatureRecordToMMDB(hMMFeature, pBD_XP,
            pszRecordOnCourse, pFlushRecList, 
            &hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP->nfitxes,
            nNumPrivateMMField))
        return 1;
    return 0;
}

int MMAddArcRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMMFeature,
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

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&pMMArcLayer->MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSave=
        (void *)pMMArcLayer->MMAdmDB.pRecList;

    pBD_XP=pMMArcLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse=pMMArcLayer->MMAdmDB.szRecordOnCourse;
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;
    
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &pArcHeader->nElemCount, TRUE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+2,
        &pArcHeader->dfLenght, FALSE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+3,
        &pArcHeader->nFirstIdNode, TRUE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+3,
        &pArcHeader->nLastIdNode, TRUE);
    
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MMAppendBlockToBuffer(pFlushRecList))
            return 1;
        pMMArcLayer->MMAdmDB.pMMBDXP->nfitxes++;
        return 0;
    }

    if(MMAddFeatureRecordToMMDB(hMMFeature, pBD_XP,
            pszRecordOnCourse, pFlushRecList, 
            &pMMArcLayer->MMAdmDB.pMMBDXP->nfitxes,
            nNumPrivateMMField))
        return 1;
    return 0;
}

int MMAddNodeRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
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

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockWhereToSave=
        (void *)pMMNodeLayer->MMAdmDB.pRecList;

    pBD_XP=pMMNodeLayer->MMAdmDB.pMMBDXP;
    pszRecordOnCourse=pMMNodeLayer->MMAdmDB.szRecordOnCourse;
    
    pMMNodeLayer->MMAdmDB.FlushRecList.SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pMMNodeLayer->MMAdmDB.FlushRecList.pBlockToBeSaved=(void *)pszRecordOnCourse;
    
    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    nDoubleValue=pNodeHeader->nArcsCount;
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &nDoubleValue, FALSE);

    nDoubleValue=pNodeHeader->cNodeType;
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+2,
        &nDoubleValue, FALSE);

    if(MMAppendBlockToBuffer(&pMMNodeLayer->MMAdmDB.FlushRecList))
        return 1;
    pMMNodeLayer->MMAdmDB.pMMBDXP->nfitxes++;
    return 0;
}

int MMAddPolygonRecordToMMDB(struct MiraMonLayerInfo *hMiraMonLayer, 
                        struct MiraMonFeature *hMMFeature,
                        MM_INTERNAL_FID nElemCount, 
                        MM_TIPUS_N_VERTEXS nVerticesCount, 
                        struct MM_PH *pPolHeader)
{
struct MM_BASE_DADES_XP *pBD_XP=NULL;
char *pszRecordOnCourse;
MM_EXT_DBF_N_FIELDS nNumPrivateMMField=MM_PRIVATE_POLYGON_DB_FIELDS;
struct MM_FLUSH_INFO *pFlushRecList;

    // Adding record to the MiraMon database (extended DBF)
    // Flush settings
    pFlushRecList=&hMiraMonLayer->MMPolygon.MMAdmDB.FlushRecList;
    pFlushRecList->pBlockWhereToSave=
        (void *)hMiraMonLayer->MMPolygon.MMAdmDB.pRecList;

    pBD_XP=hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP;
    pszRecordOnCourse=hMiraMonLayer->MMPolygon.MMAdmDB.szRecordOnCourse;
    
    pFlushRecList->SizeOfBlockToBeSaved=pBD_XP->BytesPerFitxa;
    pFlushRecList->pBlockToBeSaved=(void *)pszRecordOnCourse;

    memset(pszRecordOnCourse, 0, pBD_XP->BytesPerFitxa);
    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp, 
        &nElemCount, TRUE);

    if(!hMMFeature)
    {
        if(MMAppendBlockToBuffer(pFlushRecList))
            return 1;
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nfitxes++;
        return 0;
    }

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &nVerticesCount, TRUE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &pPolHeader->dfPerimeter, FALSE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &pPolHeader->dfArea, FALSE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &pPolHeader->nArcsCount, TRUE);

    MM_WriteValueToRecordDBXP(pszRecordOnCourse, pBD_XP->Camp+1,
        &pPolHeader->nRingsCount, TRUE);
    
    if(MMAddFeatureRecordToMMDB(hMMFeature, pBD_XP,
            pszRecordOnCourse, pFlushRecList, 
            & hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP->nfitxes,
            nNumPrivateMMField))
        return 1;
    return 0;
}

int MMCloseMMBD_XPFile(struct MMAdmDatabase *MMAdmDB)
{
    if(!MMAdmDB->pFExtDBF)
        return 0;

    // Updating number of features in database
    fseek_function(MMAdmDB->pFExtDBF, 4, SEEK_SET);
    // ·$· V2.0 !!!
    if (fwrite_function(&MMAdmDB->pMMBDXP->nfitxes, 4, 1, 
            MMAdmDB->pFExtDBF) != 1)
		return 1;
    
    // Flushing all to be flushed
    MMAdmDB->FlushRecList.SizeOfBlockToBeSaved=0;
    if(MMAppendBlockToBuffer(&MMAdmDB->FlushRecList))
        return 1;

    // Closing database files
    if(fclose_function(MMAdmDB->pFExtDBF))
        return 1;

    return 0;
}

int MMCloseMMBD_XP(struct MiraMonLayerInfo *hMiraMonLayer)
{
    if(hMiraMonLayer->bIsPoint)
        return MMCloseMMBD_XPFile(&hMiraMonLayer->MMPoint.MMAdmDB);
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseMMBD_XPFile(&hMiraMonLayer->MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(&hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    }
    if(hMiraMonLayer->bIsPolygon)
    {
        if(MMCloseMMBD_XPFile(&hMiraMonLayer->MMPolygon.MMAdmDB))
            return 1;
        if(MMCloseMMBD_XPFile(&hMiraMonLayer->MMPolygon.MMArc.MMAdmDB))
            return 1;
        return MMCloseMMBD_XPFile(&hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
    error_message_function("Failed to create database files.");
    return 1;
}

void MMDestroyMMDBFile(struct MMAdmDatabase *pMMAdmDB)
{
	if (pMMAdmDB->szRecordOnCourse)
    {
        free_function(pMMAdmDB->szRecordOnCourse);
        pMMAdmDB->szRecordOnCourse=NULL;
    }
    MM_ReleaseDBFHeader(pMMAdmDB->pMMBDXP);
}

void MMDestroyMMDB(struct MiraMonLayerInfo *hMiraMonLayer)
{
    if(hMiraMonLayer->bIsPoint)
        MMDestroyMMDBFile(&hMiraMonLayer->MMPoint.MMAdmDB);
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(&hMiraMonLayer->MMArc.MMAdmDB);
        MMDestroyMMDBFile(&hMiraMonLayer->MMArc.MMNode.MMAdmDB);
    }
    if(hMiraMonLayer->bIsPolygon)
    {
        MMDestroyMMDBFile(&hMiraMonLayer->MMPolygon.MMAdmDB);
        MMDestroyMMDBFile(&hMiraMonLayer->MMPolygon.MMArc.MMAdmDB);
        MMDestroyMMDBFile(&hMiraMonLayer->MMPolygon.MMArc.MMNode.MMAdmDB);
    }
}

CPL_C_END // Necessary for compiling in GDAL project
