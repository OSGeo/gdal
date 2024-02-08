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
#include "mm_gdal\mm_wrlayr.h"            // For MMReadHeader()
#include "mm_gdal\mm_gdal_functions.h"  // For int MM_GetArcHeights()
#include "mm_gdal\mm_gdal_constants.h"
#else
#include "ogr_api.h"    // For CPL_C_START
#include "mm_wrlayr.h" 
#include "mm_wrlayr.h" // For MMReadHeader()
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#endif

#include "mmrdlayr.h"

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling in GDAL project
#endif

int MMInitLayerToRead(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, const char *pszFilename)
{
char szResult[MM_MAX_ID_SNY+10];

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));
    if(MMReadHeader(m_fp, &hMiraMonLayer->TopHeader))
    {
        MM_CPLError(CE_Failure, CPLE_NoWriteAccess,
                "Error reading header of the file %s",
                pszFilename);
        return 1;
    }
    hMiraMonLayer->nMemoryRatio=1.0;
    hMiraMonLayer->ReadOrWrite=MM_READING_MODE;
    strcpy(hMiraMonLayer->pszFlags, "rb");
    
    hMiraMonLayer->pszSrcLayerName=strdup_function(pszFilename);
        
    hMiraMonLayer->LayerVersion = (char)MMGetVectorVersion(&hMiraMonLayer->TopHeader);
    if (hMiraMonLayer->LayerVersion == MM_UNKNOWN_VERSION)
    {
        MM_CPLError(CE_Failure, CPLE_NotSupported,
                "MiraMon version file unknown.");
        return 1;
    }
    if(hMiraMonLayer->LayerVersion==MM_LAST_VERSION)
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_64_BITS;
    else if(hMiraMonLayer->LayerVersion==MM_32BITS_VERSION)
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_32_BITS;
    else
        hMiraMonLayer->nHeaderDiskSize=MM_HEADER_SIZE_64_BITS;
    
    if (hMiraMonLayer->TopHeader.aFileType[0] == 'P' &&
        hMiraMonLayer->TopHeader.aFileType[1] == 'N' &&
        hMiraMonLayer->TopHeader.aFileType[2] == 'T')
    {
        if (hMiraMonLayer->TopHeader.Flag & MM_LAYER_3D_INFO)
        {
            hMiraMonLayer->TopHeader.bIs3d = 1;
            hMiraMonLayer->eLT = MM_LayerType_Point3d;
        }
        else
            hMiraMonLayer->eLT = MM_LayerType_Point;

        hMiraMonLayer->bIsPoint=TRUE;
    }
    else if (hMiraMonLayer->TopHeader.aFileType[0] == 'A' &&
        hMiraMonLayer->TopHeader.aFileType[1] == 'R' &&
        hMiraMonLayer->TopHeader.aFileType[2] == 'C')
    {
        if (hMiraMonLayer->TopHeader.Flag & MM_LAYER_3D_INFO)
        {
            hMiraMonLayer->TopHeader.bIs3d = 1;
            hMiraMonLayer->eLT = MM_LayerType_Arc3d;
        }
        else
            hMiraMonLayer->eLT = MM_LayerType_Arc;

        hMiraMonLayer->bIsArc=TRUE;
    }
    else if (hMiraMonLayer->TopHeader.aFileType[0] == 'P' &&
        hMiraMonLayer->TopHeader.aFileType[1] == 'O' &&
        hMiraMonLayer->TopHeader.aFileType[2] == 'L')
    {
        // 3D
        if (hMiraMonLayer->TopHeader.Flag & MM_LAYER_3D_INFO)
        {
            hMiraMonLayer->TopHeader.bIs3d = 1;
            hMiraMonLayer->eLT = MM_LayerType_Pol3d;
        }
        else
            hMiraMonLayer->eLT = MM_LayerType_Pol;

        hMiraMonLayer->bIsPolygon=TRUE;

        if (hMiraMonLayer->TopHeader.Flag & MM_LAYER_MULTIPOLYGON)
            hMiraMonLayer->TopHeader.bIsMultipolygon = 1;
    }
    
    hMiraMonLayer->Version=MM_VECTOR_LAYER_LAST_VERSION;
    
    if(MMInitLayerByType(hMiraMonLayer))
        return 1;
    hMiraMonLayer->bIsBeenInit=1;

    // Get the basic metadata
    hMiraMonLayer->pSRS=strdup(ReturnValueFromSectionINIFile(hMiraMonLayer->pszMainREL_LayerName, 
        "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier"));

    if (!hMiraMonLayer->pSRS && hMiraMonLayer->bIsPolygon)
    {
        hMiraMonLayer->pSRS=strdup(ReturnValueFromSectionINIFile(hMiraMonLayer->MMPolygon.MMArc.pszREL_LayerName, 
            "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier"));
    }
    if (!ReturnEPSGCodeSRSFromMMIDSRS(hMiraMonLayer->pSRS, szResult))
    {
        if (IsEmptyString(szResult))
            hMiraMonLayer->nSRS_EPSG = 0;
        else
            hMiraMonLayer->nSRS_EPSG = atoi(szResult);
    }
    else
        hMiraMonLayer->nSRS_EPSG = 0;

    if(hMiraMonLayer->nSRS_EPSG == 0)
    {
        if (hMiraMonLayer->pSRS && strcmp(hMiraMonLayer->pSRS, "plane"))
        {
            MM_CPLWarning(CE_Warning, CPLE_NotSupported,
                "The MiraMon layer HRS has no equivalent "
                "in EPSG code");
        }
    }

    // If more nNumStringToOperate is needed, it'll be increased.
    hMiraMonLayer->nNumStringToOperate=0;
    if(MM_ResizeStringToOperateIfNeeded(hMiraMonLayer, 5000))
        return 1;
    
    return 0;
}

int MMAddStringLineCoordinates(struct MiraMonVectLayerInfo *hMiraMonLayer,
                MM_INTERNAL_FID i_elem,
				IN unsigned long int flag_z,
                MM_N_VERTICES_TYPE nStartVertice,
                MM_BOOLEAN bAvoidFirst,
                unsigned char VFG)
{
FILE_TYPE *pF;
struct MM_AH *pArcHeader;
struct MiraMonArcLayer *pMMArc;
struct MM_ZD *pZDescription=NULL;

    if (hMiraMonLayer->bIsPolygon)
        pMMArc=&hMiraMonLayer->MMPolygon.MMArc;
    else
        pMMArc=&hMiraMonLayer->MMArc;

    pF=pMMArc->pF;
    pArcHeader=pMMArc->pArcHeader;
    if(hMiraMonLayer->TopHeader.bIs3d)
        pZDescription=pMMArc->pZSection.pZDescription;    
                
    fseek_function(pF, pArcHeader[i_elem].nOffset, SEEK_SET);

    if (hMiraMonLayer->bIsPolygon && (VFG & MM_POL_REVERSE_ARC))
    {
        MM_N_VERTICES_TYPE nIVertice;

        // Reading arcs vertices in an inverse order
        if (MMResizeMM_POINT2DPointer(&hMiraMonLayer->ReadedFeature.pCoord,
            &hMiraMonLayer->ReadedFeature.nMaxpCoord, nStartVertice + pArcHeader[i_elem].nElemCount*2, // ask for twice memory to reverse
            0, 0))
            return 1;

        // Get the vertices far away from their place
        if (pArcHeader[i_elem].nElemCount !=
            fread_function(hMiraMonLayer->ReadedFeature.pCoord + nStartVertice + pArcHeader[i_elem].nElemCount,
                sizeof(*hMiraMonLayer->ReadedFeature.pCoord), pArcHeader[i_elem].nElemCount, pF))
        {
            return 1;
        }

        if (hMiraMonLayer->TopHeader.bIs3d)
        {
            if (MMResizeDoublePointer(&hMiraMonLayer->ReadedFeature.pZCoord,
                &hMiraMonLayer->ReadedFeature.nMaxpZCoord,
                nStartVertice + pArcHeader[i_elem].nElemCount*2,
                0, 0))
                return 1;

            // +nStartVertice
            MM_GetArcHeights(hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice + pArcHeader[i_elem].nElemCount,
                pF, pArcHeader[i_elem].nElemCount, pZDescription + i_elem, flag_z);

            // If there is a value for Z-nodata in GDAL this lines can be uncomented
            // and MM_GDAL_NODATA_COORD_Z can be defined
            /*if(!DOUBLES_DIFERENTS_DJ(punts_z[k], MM_NODATA_COORD_Z))
            {
                MM_N_VERTICES_TYPE nIVertice;
                for(nIVertice=0; nIVertice<pArcHeader[i_elem].nElemCount; nIVertice++)
                    hMiraMonLayer->ReadedFeature.pZCoord[nIVertice]=MM_GDAL_NODATA_COORD_Z;
            }
            */
        }

        // Reverse the vertices
        for (nIVertice = 0; nIVertice < pArcHeader[i_elem].nElemCount; nIVertice++)
        {
            memcpy(hMiraMonLayer->ReadedFeature.pCoord + nStartVertice - (bAvoidFirst ? 1 : 0) + nIVertice,
                hMiraMonLayer->ReadedFeature.pCoord + nStartVertice + 2*pArcHeader[i_elem].nElemCount-nIVertice-1,
                sizeof(*hMiraMonLayer->ReadedFeature.pCoord));

            if (hMiraMonLayer->TopHeader.bIs3d)
            {
                memcpy(hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice - (bAvoidFirst ? 1 : 0) + nIVertice,
                    hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice + 2*pArcHeader[i_elem].nElemCount-nIVertice-1,
                    sizeof(*hMiraMonLayer->ReadedFeature.pZCoord));
            }
        }
    }
    else
    {
        // Reading arcs vertices
        if (MMResizeMM_POINT2DPointer(&hMiraMonLayer->ReadedFeature.pCoord,
            &hMiraMonLayer->ReadedFeature.nMaxpCoord, nStartVertice + pArcHeader[i_elem].nElemCount,
            0, 0))
            return 1;

        if (pArcHeader[i_elem].nElemCount !=
            fread_function(hMiraMonLayer->ReadedFeature.pCoord + nStartVertice - (bAvoidFirst ? 1 : 0), sizeof(*hMiraMonLayer->ReadedFeature.pCoord), pArcHeader[i_elem].nElemCount, pF))
        {
            return 1;
        }

        if(hMiraMonLayer->TopHeader.bIs3d)
        {
            if(MMResizeDoublePointer(&hMiraMonLayer->ReadedFeature.pZCoord, 
                    &hMiraMonLayer->ReadedFeature.nMaxpZCoord, 
                    nStartVertice+pArcHeader[i_elem].nElemCount,
                    0, 0))
                return 1;

            // +nStartVertice
            MM_GetArcHeights(hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice - (bAvoidFirst ? 1 : 0), pF, pArcHeader[i_elem].nElemCount,
                pZDescription+i_elem, flag_z);

            // If there is a value for Z-nodata in GDAL this lines can be uncomented
            // and MM_GDAL_NODATA_COORD_Z can be defined
            /*if(!DOUBLES_DIFERENTS_DJ(punts_z[k], MM_NODATA_COORD_Z))
            {
                MM_N_VERTICES_TYPE nIVertice;
                for(nIVertice=0; nIVertice<pArcHeader[i_elem].nElemCount; nIVertice++)
                    hMiraMonLayer->ReadedFeature.pZCoord[nIVertice]=MM_GDAL_NODATA_COORD_Z;
            }
            */
        }
    }
    hMiraMonLayer->ReadedFeature.nNumpCoord=pArcHeader[i_elem].nElemCount-(bAvoidFirst?1:0);

    return 0;
}

int MMGetMultiPolygonCoordinates(struct MiraMonVectLayerInfo *hMiraMonLayer,
                IN size_t i_pol,
				IN unsigned long int flag_z)
{
struct MM_PH *pPolHeader;
struct MM_AH *pArcHeader;
char *pBuffer;
MM_POLYGON_ARCS_COUNT nIndex;
MM_BOOLEAN bAvoidFirst;
MM_N_VERTICES_TYPE nNAcumulVertices=0;

    MMResetFeatureGeometry(&hMiraMonLayer->ReadedFeature);
    MMResetFeatureRecord(&hMiraMonLayer->ReadedFeature);
    pPolHeader=hMiraMonLayer->MMPolygon.pPolHeader+i_pol;
    
    if(MMResizeMiraMonPolygonArcs(&hMiraMonLayer->pArcs, 
                        &hMiraMonLayer->nMaxArcs, 
                        pPolHeader->nArcsCount, 0, 0))
        return 1;

    
    if(MMInitFlush(&hMiraMonLayer->FlushPAL, hMiraMonLayer->MMPolygon.pF, 
        hMiraMonLayer->MMPolygon.nPALElementSize*pPolHeader->nArcsCount, &pBuffer, 
                pPolHeader->nOffset, 0))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    hMiraMonLayer->FlushPAL.pBlockWhereToSaveOrRead=(void *)pBuffer;
    if(MMReadFlush(&hMiraMonLayer->FlushPAL))
    {
        if(pBuffer)free_function(pBuffer);
        return 1;
    }

    hMiraMonLayer->ReadedFeature.nNRings=0;
    hMiraMonLayer->ReadedFeature.nNumpCoord=0;
    if(MMResize_MM_N_VERTICES_TYPE_Pointer(&hMiraMonLayer->ReadedFeature.pNCoordRing, 
            &hMiraMonLayer->ReadedFeature.nMaxpNCoordRing, 
            hMiraMonLayer->ReadedFeature.nNRings+1, 10, 10))
        return 1;

    if(MMResizeVFGPointer(&hMiraMonLayer->ReadedFeature.flag_VFG, 
            &hMiraMonLayer->ReadedFeature.nMaxVFG,
            pPolHeader->nArcsCount, 0, 0)) // Perhaps more memory than needed
        return 1;

    
    // Preparing memory for all coordinates
    hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]=0;
    for(nIndex=0; nIndex<pPolHeader->nArcsCount; nIndex++)
    {
        hMiraMonLayer->FlushPAL.SizeOfBlockToBeSaved=
            sizeof((hMiraMonLayer->pArcs+nIndex)->VFG);
        hMiraMonLayer->FlushPAL.pBlockToBeSaved=
            (void *)&(hMiraMonLayer->pArcs+nIndex)->VFG;
        if(MM_ReadBlockFromBuffer(&hMiraMonLayer->FlushPAL))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Arc index
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer,
            &hMiraMonLayer->FlushPAL,
            &((hMiraMonLayer->pArcs+nIndex)->nIArc)))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader+(hMiraMonLayer->pArcs+nIndex)->nIArc;
        hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]+=
            pArcHeader->nElemCount;
    }
    if (MMResizeMM_POINT2DPointer(&hMiraMonLayer->ReadedFeature.pCoord,
            &hMiraMonLayer->ReadedFeature.nMaxpCoord, 
            hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings],
            0, 0))
        return 1;

    hMiraMonLayer->FlushPAL.CurrentOffset=0;

    // Real work
    hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]=0;
    for(nIndex=0; nIndex<pPolHeader->nArcsCount; nIndex++)
    {
        hMiraMonLayer->FlushPAL.SizeOfBlockToBeSaved=
            sizeof((hMiraMonLayer->pArcs+nIndex)->VFG);
        hMiraMonLayer->FlushPAL.pBlockToBeSaved=
            (void *)&(hMiraMonLayer->pArcs+nIndex)->VFG;
        if(MM_ReadBlockFromBuffer(&hMiraMonLayer->FlushPAL))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        // Arc index
        if(MMReadIntegerDependingOnVersion(hMiraMonLayer,
            &hMiraMonLayer->FlushPAL,
            &((hMiraMonLayer->pArcs+nIndex)->nIArc)))
        {
            if(pBuffer)free_function(pBuffer);
            return 1;
        }

        pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader+(hMiraMonLayer->pArcs+nIndex)->nIArc;

        bAvoidFirst=FALSE;
        if(hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]!=0)
            bAvoidFirst=TRUE;

        // Add coordinates to hMiraMonLayer->ReadedFeature.pCoord
        if(MMAddStringLineCoordinates(hMiraMonLayer, (hMiraMonLayer->pArcs+nIndex)->nIArc, flag_z,
                nNAcumulVertices,
                bAvoidFirst, (hMiraMonLayer->pArcs+nIndex)->VFG))
            return 1;

        if(MMResize_MM_N_VERTICES_TYPE_Pointer(&hMiraMonLayer->ReadedFeature.pNCoordRing, 
                        &hMiraMonLayer->ReadedFeature.nMaxpNCoordRing, 
                        hMiraMonLayer->ReadedFeature.nNRings+1, 10, 10))
            return 1;

        hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]+=hMiraMonLayer->ReadedFeature.nNumpCoord;
        nNAcumulVertices+=hMiraMonLayer->ReadedFeature.nNumpCoord;
        if ((hMiraMonLayer->pArcs + nIndex)->VFG & MM_POL_END_RING)
        {
            hMiraMonLayer->ReadedFeature.flag_VFG[hMiraMonLayer->ReadedFeature.nNRings]=(hMiraMonLayer->pArcs + nIndex)->VFG;
            hMiraMonLayer->ReadedFeature.nNRings++;
            hMiraMonLayer->ReadedFeature.pNCoordRing[hMiraMonLayer->ReadedFeature.nNRings]=0;
        }
    }
    hMiraMonLayer->nNumArcs=pPolHeader->nArcsCount;
    if(pBuffer)
        free_function(pBuffer);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    
	return 0;
}

int MMGetFeatureFromVector(struct MiraMonVectLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem)
{
FILE_TYPE *pF;
struct MM_ZD *pZDescription;
unsigned long int flag_z;
int num;
double cz;
struct MM_AH *pArcHeader;
struct MM_PH *pPolHeader;

    if (hMiraMonLayer->nSelectCoordz==MM_SELECT_HIGHEST_COORDZ)
    	flag_z=MM_STRING_HIGHEST_ALTITUDE;
	else if (hMiraMonLayer->nSelectCoordz==MM_SELECT_LOWEST_COORDZ)
        flag_z=MM_STRING_LOWEST_ALTITUDE;
    else
        flag_z=0L;

    if(hMiraMonLayer->bIsPoint)
    {
        pF=hMiraMonLayer->MMPoint.pF;
        
        // Getting to the i-th element offset
        fseek_function(pF, hMiraMonLayer->nHeaderDiskSize+sizeof(MM_COORD_TYPE)*2*i_elem, SEEK_SET);

        // Reading the point
        if(MMResizeMM_POINT2DPointer(&hMiraMonLayer->ReadedFeature.pCoord, 
            &hMiraMonLayer->ReadedFeature.nMaxpCoord, 
            hMiraMonLayer->ReadedFeature.nNumpCoord, 1, 1))
            return 1;

        if (1!=fread_function(hMiraMonLayer->ReadedFeature.pCoord, sizeof(MM_COORD_TYPE)*2, 1, pF))
        {
            return 1;
        }

        hMiraMonLayer->ReadedFeature.nNRings=1;
        
        if(MMResize_MM_N_VERTICES_TYPE_Pointer(&hMiraMonLayer->ReadedFeature.pNCoordRing, 
                        &hMiraMonLayer->ReadedFeature.nMaxpNCoordRing, 
                        1, 0, 1))
            return 1;

        hMiraMonLayer->ReadedFeature.pNCoordRing[0]=1;

        if(hMiraMonLayer->TopHeader.bIs3d)
        {
            pZDescription=hMiraMonLayer->MMPoint.pZSection.pZDescription+i_elem;
            num=MM_ARC_N_TOTAL_ALCADES_DISC(pZDescription->nZCount, 1);
            if(num==0)
                hMiraMonLayer->ReadedFeature.pZCoord[0]=MM_NODATA_COORD_Z;
            else
            {
                if (MMResizeDoublePointer(&hMiraMonLayer->ReadedFeature.pZCoord,
                    &hMiraMonLayer->ReadedFeature.nMaxpZCoord,
                    1, 1, 1))
                    return 1;

                if (flag_z==MM_STRING_HIGHEST_ALTITUDE) // Max z
                    cz=pZDescription->dfBBmaxz;
                else if (flag_z==MM_STRING_LOWEST_ALTITUDE) // Min z
                    cz=pZDescription->dfBBminz;
                else
                {
                    // Reading the first z coordinate
                    fseek_function(pF, pZDescription->nOffsetZ, SEEK_SET);
                    if ((size_t)1 != fread_function(&cz, sizeof(*hMiraMonLayer->ReadedFeature.pZCoord), 1, pF))
                    {
                        return 1;
                    }
                }
                // If there is a value for Z-nodata in GDAL this lines can be uncomented
                // and MM_GDAL_NODATA_COORD_Z can be defined
                /*if(!DOUBLES_DIFERENTS_DJ(cz, MM_NODATA_COORD_Z))
                    hMiraMonLayer->ReadedFeature.pZCoord[0]=MM_GDAL_NODATA_COORD_Z;
                else */hMiraMonLayer->ReadedFeature.pZCoord[0]=cz;
            }
        }
        
        return 0;
    }

    // Stringlines
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMAddStringLineCoordinates(hMiraMonLayer, i_elem, flag_z, 0, FALSE, 0))
            return 1;

        if(MMResize_MM_N_VERTICES_TYPE_Pointer(&hMiraMonLayer->ReadedFeature.pNCoordRing, 
                        &hMiraMonLayer->ReadedFeature.nMaxpNCoordRing, 
                        1, 0, 1))
            return 1;

        hMiraMonLayer->ReadedFeature.pNCoordRing[0]=hMiraMonLayer->ReadedFeature.nNumpCoord;

        return 0;
    }

    // Polygons or multipolygons
    pF=hMiraMonLayer->MMPolygon.pF;
    pPolHeader=hMiraMonLayer->MMPolygon.pPolHeader;
    pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader;

    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->ReadedFeature.pZCoord)
    {
        pZDescription=hMiraMonLayer->MMPolygon.MMArc.pZSection.pZDescription;
        if(MMGetMultiPolygonCoordinates(hMiraMonLayer, i_elem, flag_z))
            return 1;
    }
    else
        if(MMGetMultiPolygonCoordinates(hMiraMonLayer, i_elem, flag_z))
            return 1;

	return 0;
}


int MM_ReadExtendedDBFHeader(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
const char * pszRelFile=NULL;
struct MM_BASE_DADES_XP *pMMBDXP;
const char *szDBFFileName=NULL;

    // If readed don't read again. It happens when Polygon reads
    // the database and then in initArc() it's readed again.
    if(hMiraMonLayer->pMMBDXP)
        return 0;

     pMMBDXP=hMiraMonLayer->pMMBDXP=calloc_function(sizeof(*pMMBDXP));

    if (hMiraMonLayer->bIsPoint)
    {
        hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP = pMMBDXP;
        szDBFFileName=hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName;
        pszRelFile=hMiraMonLayer->MMPoint.pszREL_LayerName;
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP = pMMBDXP;
        szDBFFileName=hMiraMonLayer->MMArc.MMAdmDB.pszExtDBFLayerName;
        pszRelFile=hMiraMonLayer->MMArc.pszREL_LayerName;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP=pMMBDXP;
        szDBFFileName=hMiraMonLayer->MMPolygon.MMAdmDB.pszExtDBFLayerName;
        pszRelFile=hMiraMonLayer->MMPolygon.pszREL_LayerName;
    }        

    if(MM_ReadExtendedDBFHeaderFromFile(szDBFFileName, pMMBDXP, pszRelFile))
    {
        MM_CPLError(CE_Failure, CPLE_NotSupported,
            "Erorr reading the format in the DBF file %s.",
            szDBFFileName);
        return 1;
    }

    fclose_function(pMMBDXP->pfBaseDades);
	pMMBDXP->pfBaseDades=NULL;

    return 0;
} 


#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
