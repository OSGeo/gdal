/* -------------------------------------------------------------------- */
/*      Necessary functions to write a MiraMon Vector File              */
/* -------------------------------------------------------------------- */
#ifdef APLICACIO_MM32
#include "CmptCmp.h" // Compatibility between compilators
#include "Vers_MM.h" // Versions stuff
#include "str_rel.h" // For CLAU_VersMetaDades
#endif

#include <string.h>
#include <stdlib.h>
#include <stddef.h>     // For size_t

#include "gdal.h"			// Per a GDALDatasetH
#include "ogr_srs_api.h"	// Per a OSRGetAuthorityCode

#ifdef MM_TODO_IF_NECESSARY
#include "f_snyder.h"   // For PreparaCoordDistGeodGeodesia()
#endif

#include "mmwrlayr.h" 

CPL_C_START  // Necessary for compiling in GDAL project

/* -------------------------------------------------------------------- */
/*      Header Functions                                                */
/* -------------------------------------------------------------------- */
int MMAppendBlockToBuffer(struct MM_FLUSH_INFO *FlushInfo);
void MMInitBoundingBox(struct MMBoundingBox *dfBB);
int MMWriteAHArcSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            unsigned __int64 DiskOffset);
int MMWriteNHNodeSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            unsigned __int64 DiskOffset);
int MMWritePHPolygonSection(struct MiraMonLayerInfo *hMiraMonLayer, 
            unsigned __int64 DiskOffset);
int MMAppendIntegerDependingOnVersion(
            struct MiraMonLayerInfo *hMiraMonLayer,
            struct MM_FLUSH_INFO *FlushInfo, 
            unsigned long *nUL32, 
            unsigned __int64 nUI64);
int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile, 
            unsigned __int64 *nOffset);
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, 
            unsigned __int64 nBlockSize, char **pBuffer, 
            unsigned __int64 DiskOffsetWhereToFlush, 
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
int MMCheckVersionFor3DOffset(struct MiraMonLayerInfo *MMLayerInfo, 
                                unsigned __int64 nOffset, 
                                unsigned __int64 nElemCount);
int MMCheckVersionOffset(struct MiraMonLayerInfo *MMLayerInfo, 
                                unsigned __int64 OffsetToCheck);
int MMCheckVersionForFID(struct MiraMonLayerInfo *MMLayerInfo, 
                         unsigned __int64 FID);

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

        pMMHeader->nElemCount=(unsigned __int64)NCount;
    
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
unsigned __int64 nNumber1=1, nNumber0=0;

    pMMHeader->Flag=MM_LAYER_GENERATED_USING_MM; // Created from MiraMon
    if(pMMHeader->bIs3d)
        pMMHeader->Flag|=MM_LAYER_3D_INFO; // 3D

    if(pMMHeader->bIsMultipolygon)
        pMMHeader->Flag|=MM_LAYER_MULTIPOLYGON; // Multipolygon.

    if(pMMHeader->aFileType[0]=='P' &&
            pMMHeader->aFileType[1]=='O' && 
            pMMHeader->aFileType[2]=='L')
        pMMHeader->Flag|=MM_LAYER_EXPLICITAL_POLYGONS;

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
    pMMHeader.hBB.dfMinX=STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMaxX=-STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMinY=STATISTICAL_UNDEFINED_VALUE;
    pMMHeader.hBB.dfMaxY=-STATISTICAL_UNDEFINED_VALUE;

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
                        FILE_TYPE *pF, unsigned __int64 nElements, 
                        struct MM_ZSection *pZSection)
{
long reservat4=0L;
unsigned long Offset2G;
unsigned __int64 nIndex;
unsigned __int64 nOffsetDiff;
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
                        unsigned __int64 ZSectionOffset)
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

    return 0;
}

int MMInitLayer(struct MiraMonLayerInfo *hMiraMonLayer, const char *pzFileName, 
                __int32 LayerVersion, int eLT,unsigned __int64 nElemCount, 
                struct MiraMonDataBase *attributes)
{
    int bIs3d=0;

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));
    hMiraMonLayer->Version=MM_VECTOR_LAYER_LAST_VERSION;
    
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

    // Don't free in destructor
    hMiraMonLayer->attributes=attributes;

    // Return the handle to the layer
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Closing                                        */
/* -------------------------------------------------------------------- */
int MMClose3DSectionLayer(struct MiraMonLayerInfo *hMiraMonLayer, 
                        unsigned __int64 nElements,
                        FILE_TYPE *pF,
                        FILE_TYPE *pF3d,
                        struct MM_ZSection *pZSection,
                        unsigned __int64 FinalOffset)
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
    MMAppendBlockToBuffer(&pZSection->FlushZL);
    
    if(MMMoveFromFileToFile(pF3d, pF, NULL))
        return 1;
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
    MMAppendBlockToBuffer(&hMiraMonLayer->MMPoint.FlushTL);
    if(MMMoveFromFileToFile(hMiraMonLayer->MMPoint.pFTL, 
            hMiraMonLayer->MMPoint.pF, &hMiraMonLayer->OffsetCheck))
        return 1;
    
    if(MMClose3DSectionLayer(hMiraMonLayer, 
        hMiraMonLayer->TopHeader.nElemCount,
        hMiraMonLayer->MMPoint.pF, 
        hMiraMonLayer->MMPoint.pF3d, 
        &hMiraMonLayer->MMPoint.pZSection,
        hMiraMonLayer->OffsetCheck))
        return 1;

    fclose_function(hMiraMonLayer->MMPoint.pF);
    if(hMiraMonLayer->TopHeader.bIs3d)
    {
        fclose_function(hMiraMonLayer->MMPoint.pF3d);
        remove_function(hMiraMonLayer->MMPoint.psz3DLayerName);
    }
    fclose_function(hMiraMonLayer->MMPoint.pFTL);
    remove_function(hMiraMonLayer->MMPoint.pszTLName);
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
    MMAppendBlockToBuffer(&pMMArcLayer->MMNode.FlushNL);
    if(MMMoveFromFileToFile(pMMArcLayer->MMNode.pFNL, 
            pMMArcLayer->MMNode.pF, &hMiraMonLayer->OffsetCheck))
        return 1;
    
    fclose_function(pMMArcLayer->MMNode.pF);
    fclose_function(pMMArcLayer->MMNode.pFNL);
    remove_function(pMMArcLayer->MMNode.pszNLName);
    
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
    MMAppendBlockToBuffer(&pMMArcLayer->FlushAL);
    if(MMMoveFromFileToFile(pMMArcLayer->pFAL, pMMArcLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;

    // 3D Section
    if(MMClose3DSectionLayer(hMiraMonLayer, 
            pArcTopHeader->nElemCount,
            pMMArcLayer->pF, 
            pMMArcLayer->pF3d, 
            &pMMArcLayer->pZSection,
            hMiraMonLayer->OffsetCheck))
            return 1;

    if(pArcTopHeader->bIs3d)
    {
        fclose_function(pMMArcLayer->pF3d);
        remove_function(pMMArcLayer->psz3DLayerName);
    }
    
    fclose_function(pMMArcLayer->pF);
    fclose_function(pMMArcLayer->pFAL);
    remove_function(pMMArcLayer->pszALName);
    
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
    MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPS);
    if(MMMoveFromFileToFile(pMMPolygonLayer->pFPS, pMMPolygonLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;

    // AH Section
    if(MMWritePHPolygonSection(hMiraMonLayer, hMiraMonLayer->OffsetCheck))
        return 1;

    // PAL Section
    pMMPolygonLayer->FlushPAL.SizeOfBlockToBeSaved=0;
    MMAppendBlockToBuffer(&pMMPolygonLayer->FlushPAL);
    if(MMMoveFromFileToFile(pMMPolygonLayer->pFPAL, pMMPolygonLayer->pF, 
        &hMiraMonLayer->OffsetCheck))
        return 1;

    fclose_function(pMMPolygonLayer->pF);
    fclose_function(pMMPolygonLayer->pFPS);
    remove_function(pMMPolygonLayer->pszPSName);
    fclose_function(pMMPolygonLayer->pFPAL);
    remove_function(pMMPolygonLayer->pszPALName);
    
    return 0;
}

int MMCloseLayer(struct MiraMonLayerInfo *hMiraMonLayer)
{
    CheckMMVectorLayerVersion(hMiraMonLayer,1)

    if(!hMiraMonLayer)
        return 0;

    if(hMiraMonLayer->bIsPoint)
        return MMClosePointLayer(hMiraMonLayer);
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
        return MMCloseArcLayer(hMiraMonLayer);
    if(hMiraMonLayer->bIsPolygon)
      return MMClosePolygonLayer(hMiraMonLayer);
    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Destroying (allocated memory)                  */
/* -------------------------------------------------------------------- */
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

    return 0;
}

/* -------------------------------------------------------------------- */
/*      Layer Functions: Creating a layer                               */
/* -------------------------------------------------------------------- */
struct MiraMonLayerInfo * MMCreateLayer(char *pzFileName, 
            __int32 LayerVersion, int eLT, unsigned __int64 nElemCount, 
            struct MiraMonDataBase *attributes)
{
struct MiraMonLayerInfo *hMiraMonLayer;

    // Creating of the handle to a MiraMon Layer
    hMiraMonLayer=(struct MiraMonLayerInfo *)calloc_function(
                    sizeof(*hMiraMonLayer));
    if(MMInitLayer(hMiraMonLayer, pzFileName, LayerVersion, eLT, 
                nElemCount, attributes))
        return NULL;

    // Return the handle to the layer
    return hMiraMonLayer;
}

/* -------------------------------------------------------------------- */
/*      Flush Layer Functions                                           */
/* -------------------------------------------------------------------- */
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, 
                unsigned __int64 nBlockSize, char **pBuffer, 
                unsigned __int64 DiskOffsetWhereToFlush, 
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

    fwrite_function(FlushInfo->pBlockWhereToSave, 1, FlushInfo->nNumBytes, 
        FlushInfo->pF);
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
            else // Add blancs
            {
                char blanc[8]="\0\0\0\0\0\0\0";
                memcpy((char *)FlushInfo->pBlockWhereToSave+
                    FlushInfo->nNumBytes, blanc, 
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
            MMAppendBlockToBuffer(FlushInfo);
        }
        return 0;
    }
    // Just flush to the disc.
    return MMFlushToDisk(FlushInfo);
}

int MMMoveFromFileToFile(FILE_TYPE *pSrcFile, FILE_TYPE *pDestFile, 
                         unsigned __int64 *nOffset)
{
size_t bufferSize = 100 * 1024 * 1024; // 100 MB buffer
unsigned char* buffer = (unsigned char*)calloc_function(bufferSize);
size_t bytesRead, bytesWritten;

    if (!buffer)
        return 1;
    
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

void GetOffsetAlignedTo8(unsigned __int64 *Offset)
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
                        unsigned __int64 DiskOffset)
{
unsigned __int64 iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
unsigned __int64 nOffsetDiff;
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
                         unsigned __int64 DiskOffset)
{
unsigned __int64 iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
unsigned __int64 nOffsetDiff;
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
                            unsigned __int64 DiskOffset)
{
unsigned __int64 iElem;
struct MM_FLUSH_INFO FlushTMP;
char *pBuffer=NULL;
unsigned long nUL32;
unsigned __int64 nOffsetDiff;
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
void MMInitFeature(struct MiraMonFeature *MMFeature)
{
    memset(MMFeature, 0, sizeof(*MMFeature));
}

// Conserves all allocated memroy but inicialize the counters to zero.
void MMResetFeature(struct MiraMonFeature *MMFeature)
{
    MMFeature->nNRings=0;
    MMFeature->nIRing=0;
    MMFeature->nICoord=0;
    MMFeature->nRecords=0;
}

// Conserves all allocated memroy but inicialize the counters to zero.
void MMDestroyFeature(struct MiraMonFeature *MMFeature)
{
    if(MMFeature->pCoord)
    {
        free_function(MMFeature->pCoord);
        MMFeature->pCoord=NULL;
    }
    if(MMFeature->pZCoord)
    {
        free_function(MMFeature->pZCoord);
        MMFeature->pZCoord=NULL;
    }
    if(MMFeature->pNCoord)
    {
        free_function(MMFeature->pNCoord);
        MMFeature->pNCoord=NULL;
    }
    if(MMFeature->pRecords)
    {
        free_function(MMFeature->pRecords);
        MMFeature->pRecords=NULL;
    }
    MMFeature->nNRings=0;
    MMFeature->nRecords=0;
}

int MMCreateFeaturePolOrArc(struct MiraMonLayerInfo *MMLayerInfo, 
                struct MiraMonFeature *MMFeature)
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
unsigned __int64 nOffsetTmp;
struct MM_ZD *pZDesc=NULL;
struct MM_FLUSH_INFO *pFlushAL, *pFlushNL, *pFlushZL, *pFlushPS, *pFlushPAL;

    // Test. Eliminate
    //if(MMLayerInfo->TopHeader.nElemCount==3)
    //    return MM_STOP_WRITING_FEATURES;
       
    // Setting pointer to 3d structure (if exists).
    if(MMLayerInfo->TopHeader.bIs3d)
		pZ=MMFeature->pZCoord;

    // Setting pointers to arc/node structures.
    if(MMLayerInfo->bIsPolygon)
    {
        pMMArc=&MMLayerInfo->MMPolygon.MMArc;
        pArcTopHeader=&MMLayerInfo->MMPolygon.TopArcHeader;

        pMMNode=&MMLayerInfo->MMPolygon.MMArc.MMNode;
        pNodeTopHeader=&MMLayerInfo->MMPolygon.MMArc.TopNodeHeader;
    }
    else
    {
        pMMArc=&MMLayerInfo->MMArc;
        pArcTopHeader=&MMLayerInfo->TopHeader;

        pMMNode=&MMLayerInfo->MMArc.MMNode;
        pNodeTopHeader=&MMLayerInfo->MMArc.TopNodeHeader;
    }

    // Setting pointers to polygon structures
    if (MMLayerInfo->bIsPolygon)
    {
        pCurrentPolHeader=MMLayerInfo->MMPolygon.pPolHeader+
            MMLayerInfo->TopHeader.nElemCount;
        MMInitBoundingBox(&pCurrentPolHeader->dfBB);        
        
        pCurrentPolHeader->dfPerimeter=0;
        pCurrentPolHeader->dfArea=0L;
        //pCurrentPolHeader->GeoTopoPol.n_vertex_pol=0L;
    }

    // Setting flushes to all sections described in 
    // format specifications document.
    pFlushAL=&pMMArc->FlushAL;
    pFlushNL=&pMMNode->FlushNL;
    pFlushZL=&pMMArc->pZSection.FlushZL;
    pFlushPS=&MMLayerInfo->MMPolygon.FlushPS;
    pFlushPAL=&MMLayerInfo->MMPolygon.FlushPAL;

    pFlushNL->pBlockWhereToSave=(void *)pMMNode->pNL;
    pFlushAL->pBlockWhereToSave=(void *)pMMArc->pAL;
    if (MMLayerInfo->TopHeader.bIs3d)
        pFlushZL->pBlockWhereToSave=(void *)pMMArc->pZSection.pZL;
    if (MMLayerInfo->bIsPolygon)
    {
        pFlushPS->pBlockWhereToSave=(void *)MMLayerInfo->MMPolygon.pPS;
        pFlushPAL->pBlockWhereToSave=
            (void *)MMLayerInfo->MMPolygon.pPAL;
    }

    // Going through parts of the feature.
    nExternalRingsCount=0;
    pCoord=MMFeature->pCoord;

    // Checking if its possible continue writing the file due
    // to version limitations.
    if(MMLayerInfo->LayerVersion==MM_32BITS_VERSION)
    {
        unsigned __int64 nNodeOffset, nArcOffset, nPolOffset;
        nNodeOffset=pFlushNL->TotalSavedBytes+pFlushNL->nNumBytes;
        nArcOffset=pMMArc->nOffsetArc;
        nPolOffset=pFlushPAL->TotalSavedBytes+pFlushPAL->nNumBytes;

        for (nIPart=0; nIPart<MMFeature->nNRings; nIPart++, 
                       pArcTopHeader->nElemCount++, 
                       pNodeTopHeader->nElemCount+=(MMLayerInfo->bIsPolygon?1:2))
	    {
            // There is space for the element that is going to be written?
            if(MMCheckVersionForFID(MMLayerInfo, MMLayerInfo->TopHeader.nElemCount))
                return MM_STOP_WRITING_FEATURES;

            // There is space for the last node(s) that is(are) going to be written?
            if(MMCheckVersionForFID(MMLayerInfo, 2*MMLayerInfo->TopHeader.nElemCount))
                return MM_STOP_WRITING_FEATURES;
            if(!MMLayerInfo->bIsPolygon)
            {
                if(MMCheckVersionForFID(MMLayerInfo, 2*MMLayerInfo->TopHeader.nElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }

            // Checking offsets
            // AL: check the last point
            if(MMCheckVersionOffset(MMLayerInfo, nArcOffset))
                return MM_STOP_WRITING_FEATURES;
            // Setting next offset
            nArcOffset+=(MMFeature->pNCoord[nIPart])*pMMArc->nALElementSize;

            // NL: check the last node
            if(MMLayerInfo->bIsPolygon)
                nNodeOffset+=(MMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;
            else
                nNodeOffset+=(2*MMFeature->nNRings)*MM_SIZE_OF_NL_32BITS;

            if(MMCheckVersionOffset(MMLayerInfo, nNodeOffset))
                return MM_STOP_WRITING_FEATURES;
            // Setting next offset
            nNodeOffset+=MM_SIZE_OF_NL_32BITS;

            if(!MMLayerInfo->bIsPolygon)
            {
                if(MMCheckVersionOffset(MMLayerInfo, nNodeOffset))
                   return MM_STOP_WRITING_FEATURES;
                // Setting next offset
                nNodeOffset+=MM_SIZE_OF_NL_32BITS;
            }
            
            // PAL
            if (MMLayerInfo->bIsPolygon)
            {
                nPolOffset+=MMFeature->nNRings*
                        MMLayerInfo->MMPolygon.nPSElementSize+
                    MMLayerInfo->MMPolygon.nPHElementSize+
                    MMFeature->nNRings*MM_SIZE_OF_PAL_32BITS;   
		    }

            // Where 3D part is going to start
            if (MMLayerInfo->TopHeader.bIs3d)
            {
                nArcOffset+=MMFeature->pNCoord[nIPart]*pMMArc->nALElementSize;
                if(MMCheckVersionFor3DOffset(MMLayerInfo, nArcOffset, 
                    MMLayerInfo->TopHeader.nElemCount+MMFeature->nNRings))
                    return MM_STOP_WRITING_FEATURES;
            }
        }
    }
    
    // Doing real job
    for (nIPart=0; nIPart<MMFeature->nNRings; nIPart++, 
                   pArcTopHeader->nElemCount++, 
                   pNodeTopHeader->nElemCount+=(MMLayerInfo->bIsPolygon?1:2))
	{
        // Resize structures if necessary
        if(MMResizeArcHeaderPointer(&pMMArc->pArcHeader, 
                        &pMMArc->nMaxArcHeader, 
                        pArcTopHeader->nElemCount,
                        MM_INCR_NUMBER_OF_ARCS,
                        MMFeature->nNRings>MMLayerInfo->nSuposedElemCount?
                        MMFeature->nNRings:MMLayerInfo->nSuposedElemCount))
        {
            #ifdef APLICACIO_MM32
            error_message_function("Memory error\n");
            #endif
          	return MM_FATAL_ERROR_WRITING_FEATURES;
        }
        if(MMResizeNodeHeaderPointer(&pMMNode->pNodeHeader, 
                        &pMMNode->nMaxNodeHeader, 
                        pNodeTopHeader->nElemCount+1, 
                        MM_INCR_NUMBER_OF_NODES,
                        0))
        {
            #ifdef APLICACIO_MM32
            error_message_function("Memory error\n");
            #endif
          	return MM_FATAL_ERROR_WRITING_FEATURES;
        }

        if (MMLayerInfo->TopHeader.bIs3d)
		{
            if(MMResizeZSectionDescrPointer(
                    &pMMArc->pZSection.pZDescription, 
                    &pMMArc->pZSection.nMaxZDescription, 
                    pMMArc->nMaxArcHeader, 
                    MM_INCR_NUMBER_OF_ARCS,
                    0))
            {
                #ifdef APLICACIO_MM32
                error_message_function("Memory error\n");
                #endif
	          	return MM_FATAL_ERROR_WRITING_FEATURES;
			}
            pZDesc=pMMArc->pZSection.pZDescription;
        }

        // Setting pointers to current headers
        pCurrentArcHeader=pMMArc->pArcHeader+pArcTopHeader->nElemCount;
        MMInitBoundingBox(&pCurrentArcHeader->dfBB);

        pCurrentNodeHeader=pMMNode->pNodeHeader+
            pNodeTopHeader->nElemCount;
        if(!MMLayerInfo->bIsPolygon)
            pCurrentNodeHeaderPlus1=pCurrentNodeHeader+1;
  
        // Initialiting feature information (section AH/PH)
        pCurrentArcHeader->nElemCount=MMFeature->pNCoord[nIPart];
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
            MMAppendBlockToBuffer(pFlushAL);
            pFlushAL->pBlockToBeSaved=(void *)&pCoord->dfY;
            MMAppendBlockToBuffer(pFlushAL);

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
	            if (MMLayerInfo->bIsPolygon)
                {
					pCurrentPolHeader->dfArea += 
                        ( pCoord->dfX * (pCoord-1)->dfY - 
                        (pCoord-1)->dfX * pCoord->dfY);
                }
			}
		}

        #ifdef MM_TODO_IF_NECESSARY
        if(pMMArc->pEllipLong && pMMArc->GeodesiaTransform)
		{
			unsigned __int64 nNCoords;
			nNCoords=pCurrentArcHeader->nElemCount-
                nRepeatedVertices;
			PreparaCoordDistGeodGeodesia(pMMArc->GeodesiaTransform, 
                pCoord, &nNCoords, 1);

			// Per arcs
            pMMArc->pEllipLong[MMLayerInfo->TopHeader.nElemCount]=
                CalculaDistGeod(pCoord, nNCoords);

			if(MMLayerInfo->bIsPolygon)
			{
				long_ellipsoidal=CalculaDistGeod(pCoord, nNCoords);
				area_ellipsoidal=CalculaAreaGeod(pCoord, &nNCoords, 1);
			}
		}
        #endif //#ifdef MM_TODO_IF_NECESSARY
        
        // Updating bounding boxes 
        MMUpdateBoundingBox(&pArcTopHeader->hBB, &pCurrentArcHeader->dfBB);
        if (MMLayerInfo->bIsPolygon)
            MMUpdateBoundingBox(&MMLayerInfo->TopHeader.hBB, 
            &pCurrentArcHeader->dfBB);

        pMMArc->nOffsetArc+=
            (pCurrentArcHeader->nElemCount)*pMMArc->nALElementSize;
        
        pCurrentArcHeader->nFirstIdNode=(2*pArcTopHeader->nElemCount);
        if(MMLayerInfo->bIsPolygon)
        {
            pCurrentArcHeader->nFirstIdNode=pArcTopHeader->nElemCount;
            pCurrentArcHeader->nLastIdNode=pArcTopHeader->nElemCount;
        }
        else
        {
            pCurrentArcHeader->nFirstIdNode=(2*pArcTopHeader->nElemCount);
            pCurrentArcHeader->nLastIdNode=(2*pArcTopHeader->nElemCount+1);
        }
        
        // Node Stuff: writting NL section
        pCurrentNodeHeader->nArcsCount=1;
        if(MMLayerInfo->bIsPolygon)
            pCurrentNodeHeader->cNodeType=MM_RING_NODE;
        else
            pCurrentNodeHeader->cNodeType=MM_FINAL_NODE;
        
        pCurrentNodeHeader->nOffset=pFlushNL->TotalSavedBytes+
            pFlushNL->nNumBytes;
        if(MMAppendIntegerDependingOnVersion(MMLayerInfo,
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
            MMAppendBlockToBuffer(pFlushNL);
        }

        if(!MMLayerInfo->bIsPolygon)
        {
            pCurrentNodeHeaderPlus1->nArcsCount=1;
            if(MMLayerInfo->bIsPolygon)
                pCurrentNodeHeaderPlus1->cNodeType=MM_RING_NODE;
            else
                pCurrentNodeHeaderPlus1->cNodeType=MM_FINAL_NODE;
            
            pCurrentNodeHeaderPlus1->nOffset=pFlushNL->TotalSavedBytes+
                pFlushNL->nNumBytes;

            if(MMAppendIntegerDependingOnVersion(MMLayerInfo,
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
                MMAppendBlockToBuffer(pFlushNL);
            }
        }

        // 3D stuff
		if (MMLayerInfo->TopHeader.bIs3d)
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
                MMAppendBlockToBuffer(pFlushZL);

                if (pZDesc[pArcTopHeader->nElemCount].dfBBminz>*pZ)
                	pZDesc[pArcTopHeader->nElemCount].dfBBminz=*pZ;
                if (pZDesc[pArcTopHeader->nElemCount].dfBBmaxz<*pZ)
                	pZDesc[pArcTopHeader->nElemCount].dfBBmaxz=*pZ;
			}
            pZDesc[pArcTopHeader->nElemCount].nZCount=1;
            if(MMLayerInfo->TopHeader.nElemCount==0)
                pZDesc[MMLayerInfo->TopHeader.nElemCount].nOffsetZ=0;
            else
                pZDesc[MMLayerInfo->TopHeader.nElemCount].nOffsetZ=
                    pZDesc[MMLayerInfo->TopHeader.nElemCount-1].nOffsetZ+sizeof(*pZ);
        }
        
        // Exclusive polygon stuff
        if (MMLayerInfo->bIsPolygon)
        {
            // PS SECTION
            if(MMAppendIntegerDependingOnVersion(MMLayerInfo,
                           pFlushPS, 
                           &UnsignedLongNumber, 0))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            
            if(MMAppendIntegerDependingOnVersion(MMLayerInfo,
                           pFlushPS, 
                           &UnsignedLongNumber, MMLayerInfo->TopHeader.nElemCount))
                return MM_FATAL_ERROR_WRITING_FEATURES;
            
            // PAL SECTION
			// Vertices of rings defining
			// holes in polygons are in a counterclockwise direction.
            // Hole are at the end of al external rings that contain the hole!!
            VFG=0L;
			VFG|=MM_END_ARC_IN_RING;
            if(MMFeature->pbArcInfo[nIPart])
            {
				nExternalRingsCount++;
				VFG|=MM_EXTERIOR_ARC_SIDE;
			}

			pCurrentPolHeader->nArcsCount=MMFeature->nNRings;
            pCurrentPolHeader->nExternalRingsCount=nExternalRingsCount;
            pCurrentPolHeader->nRingsCount=MMFeature->nNRings;
            if(nIPart==0)
            {
                pCurrentPolHeader->nOffset=pFlushPAL->TotalSavedBytes+
                   pFlushPAL->nNumBytes;
            }

            if(nIPart==MMFeature->nNRings-1)
                pCurrentPolHeader->dfArea/=2;

            pFlushPAL->SizeOfBlockToBeSaved=1;
            pFlushPAL->pBlockToBeSaved=(void *)&VFG;
            MMAppendBlockToBuffer(pFlushPAL);

            if(MMAppendIntegerDependingOnVersion(MMLayerInfo, pFlushPAL, 
                    &UnsignedLongNumber, pArcTopHeader->nElemCount))
                return MM_FATAL_ERROR_WRITING_FEATURES;

            // 8bytes alignment
            if(nIPart==MMFeature->nNRings-1)
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
                    MMAppendBlockToBuffer(pFlushPAL);
                }
            }

            MMUpdateBoundingBox(&pCurrentPolHeader->dfBB, &pCurrentArcHeader->dfBB);
            pCurrentPolHeader->dfPerimeter+=pCurrentArcHeader->dfLenght;

            #ifdef MM_TODO_IF_NECESSARY
			if(pMMArc->pEllipLong)
			{
				(*pgeo_topo_pol)->perimetre_ellipsoidal+=long_ellipsoidal;
				(*pgeo_topo_pol)->area_ellipsoidal+=area_ellipsoidal;
			}
            (*pgeo_topo_pol)->n_vertex_pol+=pCurrentArcHeader->nElemCount-nRepeatedVertices;
            #endif
		}
    }

    // Updating element count and if the polygon is multipart.
    // MiraMon doesn't accept multipoints or multilines, only multipolygons.
    if(MMLayerInfo->bIsPolygon)
    {
        MMLayerInfo->TopHeader.nElemCount++;

	    if(nExternalRingsCount>1)
            MMLayerInfo->TopHeader.bIsMultipolygon=TRUE;
    }

	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePolOrArc()


int MMCreateFeaturePoint(struct MiraMonLayerInfo *MMLayerInfo, struct MiraMonFeature *MMFeature)
{
double *pZ=NULL;
struct MM_POINT_2D *pCoord;
unsigned __int64 nIPart, nIVertice;
unsigned __int64 nCoord;
struct MM_ZD *pZDescription=NULL;
unsigned __int64 nElemCount;

    if(MMLayerInfo->TopHeader.bIs3d)
		pZ=MMFeature->pZCoord;

    nElemCount=MMLayerInfo->TopHeader.nElemCount;
    for (nIPart=0, pCoord=MMFeature->pCoord;
        nIPart<MMFeature->nNRings; nIPart++, nElemCount++)
	{
        nCoord=MMFeature->pNCoord[nIPart];
        
        // Checking if its possible continue writing the file due
        // to version limitations.
        if(MMCheckVersionForFID(MMLayerInfo, 
                MMLayerInfo->TopHeader.nElemCount+nCoord))
            return MM_STOP_WRITING_FEATURES;

        if (MMLayerInfo->TopHeader.bIs3d)
        {
            if(nElemCount==0)
            {
                if(MMCheckVersionFor3DOffset(MMLayerInfo,
                        0, nElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }
            else
            {
                pZDescription=MMLayerInfo->MMPoint.pZSection.pZDescription;
                if(MMCheckVersionFor3DOffset(MMLayerInfo,
                        pZDescription[nElemCount-1].nOffsetZ+
                        sizeof(*pZ), nElemCount+1))
                    return MM_STOP_WRITING_FEATURES;
            }
        }

        // Doing real job
        if (MMLayerInfo->TopHeader.bIs3d)
        {
            if (MMLayerInfo->TopHeader.bIs3d)
			{
                if(MMResizeZSectionDescrPointer(&MMLayerInfo->MMPoint.pZSection.pZDescription, 
                        &MMLayerInfo->MMPoint.pZSection.nMaxZDescription, 
                        nElemCount, 
                        MM_INCR_NUMBER_OF_POINTS,
                        0))
                {
                    #ifdef APLICACIO_MM32
                    error_message_function("Memory error\n");
                    #endif
		          	return MM_FATAL_ERROR_WRITING_FEATURES;
				}
            }
            pZDescription=MMLayerInfo->MMPoint.pZSection.pZDescription;

            pZDescription[nElemCount].dfBBminz=STATISTICAL_UNDEF_VALUE;
            pZDescription[nElemCount].dfBBmaxz=-STATISTICAL_UNDEF_VALUE;
        }
                
        MMLayerInfo->MMPoint.FlushTL.pBlockWhereToSave=(void *)MMLayerInfo->MMPoint.pTL;
        if (MMLayerInfo->TopHeader.bIs3d)
            MMLayerInfo->MMPoint.pZSection.FlushZL.pBlockWhereToSave=(void *)MMLayerInfo->MMPoint.pZSection.pZL;

        for (nIVertice=0; nIVertice<nCoord; nIVertice++, pCoord++, pZ++)
        {
            MMUpdateBoundingBoxXY(&MMLayerInfo->TopHeader.hBB, pCoord);

            MMLayerInfo->MMPoint.FlushTL.SizeOfBlockToBeSaved=sizeof(pCoord->dfX);
            
            MMLayerInfo->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfX;
            MMAppendBlockToBuffer(&MMLayerInfo->MMPoint.FlushTL);
            
            MMLayerInfo->MMPoint.FlushTL.pBlockToBeSaved=(void *)&pCoord->dfY;
            MMAppendBlockToBuffer(&MMLayerInfo->MMPoint.FlushTL);

	        if (MMLayerInfo->TopHeader.bIs3d)
            {
        	    MMLayerInfo->MMPoint.pZSection.FlushZL.SizeOfBlockToBeSaved=sizeof(*pZ);
                MMLayerInfo->MMPoint.pZSection.FlushZL.pBlockToBeSaved=(void *)pZ;
                MMAppendBlockToBuffer(&MMLayerInfo->MMPoint.pZSection.FlushZL);

                if (pZDescription[nElemCount].dfBBminz>*pZ)
                	pZDescription[nElemCount].dfBBminz=*pZ;
                if (pZDescription[nElemCount].dfBBmaxz<*pZ)
                	pZDescription[nElemCount].dfBBmaxz=*pZ;

                if (MMLayerInfo->MMPoint.pZSection.ZHeader.dfBBminz>*pZ)
                	MMLayerInfo->MMPoint.pZSection.ZHeader.dfBBminz=*pZ;
                if (MMLayerInfo->MMPoint.pZSection.ZHeader.dfBBmaxz<*pZ)
                	MMLayerInfo->MMPoint.pZSection.ZHeader.dfBBmaxz=*pZ;
			}
        }
        if (MMLayerInfo->TopHeader.bIs3d)
        {
            pZDescription[nElemCount].nZCount=1;
            if(nElemCount==0)
                pZDescription[nElemCount].nOffsetZ=0;
            else
                pZDescription[nElemCount].nOffsetZ=
                    pZDescription[nElemCount-1].nOffsetZ+sizeof(*pZ);
        }
    }
    MMLayerInfo->TopHeader.nElemCount=nElemCount;

	return MM_CONTINUE_WRITING_FEATURES;
}// End of de MMCreateFeaturePoint()

int MMCheckVersionForFID(struct MiraMonLayerInfo *MMLayerInfo, 
                         unsigned __int64 FID)
{
    if(MMLayerInfo->LayerVersion!=MM_32BITS_VERSION)
        return 0;

    if(FID>=MAXIMUM_OBJECT_INDEX_IN_2GB_VECTORS)
            return 1;
    return 0;
}

int MMCheckVersionOffset(struct MiraMonLayerInfo *MMLayerInfo, 
                                unsigned __int64 OffsetToCheck)
{
    // Checking if the final version is 1.1 or 2.0
    if(MMLayerInfo->LayerVersion!=MM_32BITS_VERSION)
        return 0;
    
    // User decided that if necessary, output version can be 2.0
    if(OffsetToCheck<MAXIMUM_OFFSET_IN_2GB_VECTORS)
        return 0;
    
    return 1;
}

int MMCheckVersionFor3DOffset(struct MiraMonLayerInfo *MMLayerInfo, 
                                unsigned __int64 nOffset, 
                                unsigned __int64 nElemCount)
{
unsigned __int64 LastOffset;

    // Checking if the final version is 1.1 or 2.0
    if(MMLayerInfo->LayerVersion!=MM_32BITS_VERSION)
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

/*
int IsNoDataZValue(double Z)
{
    return FALSE; // TO DO IF NECESSARY
}
*/

unsigned long GetUnsignedLongFromINT64(unsigned __int64 number)
{
    unsigned long ulnumber=(unsigned long) number;
    if(ulnumber!=number)
        return (unsigned long )-1; // To detect out of range
    return ulnumber;
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

void MMUpdateBoundingBoxXY(struct MMBoundingBox *dfBB, struct MM_POINT_2D *pCoord)
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
BOOL_CHAR bIDFounded;

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
            strncpy(aMMIDDBFFile, filepath, i);
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
        char pszFieldName[MM_MAX_LON_NAME_DBF_FIELD];

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
                strncpy(pszFieldName,OGR_Fld_GetNameRef(hFieldDefn), MM_MAX_LON_NAME_DBF_FIELD);
                if(0==stricmp(pszFieldName, "PSIDGEODES") && 0==stricmp(pSRS, OGR_F_GetFieldAsString(hFeature, niField)))
                {
                    bIDFounded=1;
                    for (j=niField+1; j<numFields; j++)
                    {
                        hFieldDefn = OGR_FD_GetFieldDefn(hFeatureDefn, j);
                        strncpy(pszFieldName,OGR_Fld_GetNameRef(hFieldDefn), MM_MAX_LON_NAME_DBF_FIELD);
                        if(0==stricmp(pszFieldName, "ID_GEODES"))
                        {
                            strncpy(aMMIDSRS, OGR_F_GetFieldAsString(hFeature, j),MM_MAX_ID_SNY);
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
    static char aFileIdentifier[MM_MAX_LEN_LAYER_NAME];

    aCharRand[0]='_';
    len_charset=(int)strlen(aCharset);
    for(i=1;i<7;i++)
        aCharRand[i]=aCharset[rand()%(len_charset-1)];
    strncpy(aFileIdentifier, pMMFN, MM_MAX_LEN_LAYER_NAME-7);
    strcat(aFileIdentifier,aCharRand);
    return aFileIdentifier;
}

int MMReadVectorMetadataFromLayer(struct MiraMonMetaData *hMMMD, OGRLayerH layer, 
                    struct MiraMonLayerInfo *hMiraMonLayer)
{
    OGREnvelope extent;
    OGRSpatialReferenceH oSRS;
    const char *authorityCode=NULL, *authorityName=NULL;
    char idSRS[MM_MAX_ID_SNY];


    hMMMD->nBands=1;

    if(!layer || !hMiraMonLayer)
    {
        info_message_function("Failed to read metadata from the input file\n");
        return 1;
    }
    
    // Reading the bounding box
    hMMMD->hBB=(struct MMBoundingBox *)calloc_function(
            hMMMD->nBands*sizeof(*hMMMD->hBB));
    if(hMiraMonLayer)
    {
        hMMMD->hBB->dfMinX = hMiraMonLayer->TopHeader.hBB.dfMinX;
        hMMMD->hBB->dfMaxX = hMiraMonLayer->TopHeader.hBB.dfMaxX;
        hMMMD->hBB->dfMinY = hMiraMonLayer->TopHeader.hBB.dfMinY;
        hMMMD->hBB->dfMaxY = hMiraMonLayer->TopHeader.hBB.dfMaxY;
    }
    else
    {
        OGR_L_GetExtent(layer, &extent, 1);
        
        hMMMD->hBB->dfMinX = extent.MinX;
        hMMMD->hBB->dfMaxX = extent.MaxX;
        hMMMD->hBB->dfMinY = extent.MinY;
        hMMMD->hBB->dfMaxY = extent.MaxY;
    }

    //if(hMiraMonLayer)
    //{
    //}
    //else
    {
        // Reading the Spatial reference
        oSRS  = OGR_L_GetSpatialRef(layer);
        if(oSRS)
        {
            authorityCode = OSRGetAuthorityCode(oSRS, NULL);
            authorityName = OSRGetAuthorityName(oSRS, NULL);
        }
	    if(authorityName && authorityCode)
	    {
		    if(0==stricmp(authorityName, "EPSG"))
			    sprintf(idSRS, "%s:%s", authorityName, authorityCode);
		    else
			    sprintf(idSRS, "%s%s", authorityName, authorityCode);
            hMMMD->pSRS=strdup(idSRS);
	    }
        else
            hMMMD->pSRS=NULL;
    }

    return 0;
}

// Write minimal metadata for the layer depending on the layerType
int MMWriteMetadataFile(const char *szMDFileName, struct MiraMonMetaData *hMMMD, struct MiraMonDataBase *hMMDB)
{
char aMDFile[MM_MAX_PATH], *p, aMessage[MM_MESSAGE_LENGHT], 
     aFileIdentifier[MM_MAX_LEN_LAYER_NAME],
     aArcFile[MM_MAX_PATH], aMMIDSRS[MM_MAX_ID_SNY];
unsigned __int32 i, i_id;
FILE_TYPE *pF;

    // Build the Metadata file name and create it (if it exists, its contents are deleted).
    strcpy(aMDFile, szMDFileName);
    p=strrchr(aMDFile, '.');
    if(p!=NULL)
        *p='\0';
    if(hMMMD->eLT==MM_LayerType_Point || hMMMD->eLT==MM_LayerType_Point3d)
        strcat(aMDFile, "T.rel");
    else if(hMMMD->eLT==MM_LayerType_Arc || hMMMD->eLT==MM_LayerType_Arc3d)
        strcat(aMDFile, "A.rel");
    else if(hMMMD->eLT==MM_LayerType_Pol || hMMMD->eLT==MM_LayerType_Pol3d)
        strcat(aMDFile, "P.rel");
    else if(hMMMD->eLT==MM_LayerType_Raster)
        strcat(aMDFile, "I.rel");
    else
    {
        info_message_function("Failed to create metadata file.");
        return 1;
    }
    if(NULL==(pF=fopen_function(aMDFile, "w+t")))
    {
        sprintf(aMessage, "Failed to write the file: %s\n", aMDFile);
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

    // Writing IDENTIFICATION section
    printf_function(pF, "\n[%s]\n", SECTION_IDENTIFICATION);
    printf_function(pF, "%s=%s\n", KEY_code, aFileIdentifier);
    printf_function(pF, "%s=\n", KEY_codeSpace);
    printf_function(pF, "%s=%s\n", KEY_DatasetTitle, hMMMD->aLayerName);

    if(hMMMD->pSRS && !(hMMMD->eLT==MM_LayerType_Pol || hMMMD->eLT==MM_LayerType_Pol3d))
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

    // Writing OVERVIEW:ASPECTES_TECNICS in polygon metadata file. 
    // ArcSource=fitx_pol.arc
    if(hMMMD->eLT==MM_LayerType_Pol || hMMMD->eLT==MM_LayerType_Pol3d)
    {
        strcpy(aArcFile, szMDFileName);
        p=strrchr(aArcFile, '.');
        if(p!=NULL)
            *p='\0';
        strcat(aArcFile, ".arc");
        printf_function(pF, "[%s]\n", SECTION_OVVW_ASPECTES_TECNICS);
        printf_function(pF, "%s=\"%s\"\n", KEY_ArcSource, aArcFile);
    }

    // Writing EXTENT section
    printf_function(pF, "\n[%s]\n", SECTION_EXTENT);
    printf_function(pF, "%s=0\n", KEY_toler_env);
    if(hMMMD->hBB)
    {
        printf_function(pF, "%s=%lf\n", KEY_MinX, hMMMD->hBB->dfMinX);
        printf_function(pF, "%s=%lf\n", KEY_MaxX, hMMMD->hBB->dfMaxX);
        printf_function(pF, "%s=%lf\n", KEY_MinY, hMMMD->hBB->dfMinY);
        printf_function(pF, "%s=%lf\n", KEY_MaxY, hMMMD->hBB->dfMaxY);
    }

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
    
    // Writing TAULA_PRINCIPAL section (for vector files)
    if(hMMDB && hMMDB->nNFields>0)
    {
        // Looking for the Identify Graph field
        for (i_id=0;i_id<hMMDB->nNFields;i_id++)
        {
            if(hMMDB->pFields[i_id].bIsIdGraph)
                break;
        }
        if(i_id<hMMDB->nNFields)
        {
            printf_function(pF, "\n[%s]\n", SECTION_TAULA_PRINCIPAL);
            printf_function(pF, "%s=%s\n", KEY_IdGrafic, hMMDB->pFields[i_id].pszFieldName);
            printf_function(pF, "%s=RELACIO_1_N_DICC\n", KEY_TipusRelacio);
        }

        // For each field of the databes
        for (i=0;i<hMMDB->nNFields;i++)
        {
            if(!IsEmptyString(hMMDB->pFields[i].pszFieldDescription))
            {
                printf_function(pF, "\n[%s:%s]\n", SECTION_TAULA_PRINCIPAL, hMMDB->pFields[i].pszFieldName);
                printf_function(pF, "%s=%s\n", KEY_descriptor, hMMDB->pFields[i].pszFieldDescription);
            }
        }
    }
    // ·$· falta pel cas ràster!!!!!!!!!!!!!!!!!!
    //·$· XC
    fclose_function(pF);

    return 0;
}

void MMFreeMetaData(struct MiraMonMetaData *hMMMD)
{
    if(hMMMD->hBB)
    {
        free_function(hMMMD->hBB);
        hMMMD->hBB=NULL;
    }
    return;
}

CPL_C_END // Necessary for compiling in GDAL project
