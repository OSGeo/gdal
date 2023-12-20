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
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "giraarc.h"
#include "env.h"
#include "DefTopMM.h"
#include "libtop.h"
#include "metadata.h"
#include "NomsFitx.h" // Per a OmpleExtensio()
#include "ExtensMM.h"	//Per a ExtPoligons,...

//#include "mm_gdal\mm_gdal_driver_structs.h"    // SECCIO_VERSIO
//#include "mm_gdal\mm_wrlayr.h" 
#include "mm_gdal\mm_gdal_functions.h" // Per a int MM_DonaAlcadesDArc()
#include "mm_gdal\mm_gdal_constants.h"
#else
#include "ogr_api.h"    // For CPL_C_START
#include "mm_wrlayr.h" 
#include "mm_gdal_functions.h"
#include "mm_gdal_constants.h"
#endif

#include "mmrdlayr.h"

#ifdef GDAL_COMPILATION
CPL_C_START  // Necessary for compiling in GDAL project
#endif

static MM_TIPUS_ERROR lastErrorMM;

MM_TIPUS_ERROR MMRecuperaUltimError(void)
{
	return lastErrorMM;
}


int MMInitLayerToRead(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, const char *pszFilename)
{
    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));
    MMReadHeader(m_fp, &hMiraMonLayer->TopHeader);
    hMiraMonLayer->ReadOrWrite=MM_READING_MODE;
    strcpy(hMiraMonLayer->pszFlags, "rb");
    
    hMiraMonLayer->pszSrcLayerName=strdup_function(pszFilename);
        
    hMiraMonLayer->LayerVersion = (char)MMGetVectorVersion(&hMiraMonLayer->TopHeader);
    if (hMiraMonLayer->LayerVersion == MM_UNKNOWN_VERSION)
        return 1;
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

    hMiraMonLayer->nSRS_EPSG=ReturnEPSGCodeSRSFromMMIDSRS(hMiraMonLayer->pSRS);
    
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
            #ifndef GDAL_COMPILATION
            lastErrorMM = Mida_incoherent_Corromput;
            #endif
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
            MM_DonaAlcadesDArc(hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice + pArcHeader[i_elem].nElemCount,
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
            #ifndef GDAL_COMPILATION
            lastErrorMM = Mida_incoherent_Corromput;
            #endif
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
            MM_DonaAlcadesDArc(hMiraMonLayer->ReadedFeature.pZCoord + nStartVertice - (bAvoidFirst ? 1 : 0), pF, pArcHeader[i_elem].nElemCount,
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

    MMResetFeature(&hMiraMonLayer->ReadedFeature);
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

    if(MMResizeIntPointer(&hMiraMonLayer->ReadedFeature.pbArcInfo, 
            &hMiraMonLayer->ReadedFeature.nMaxpbArcInfo,
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
            if((hMiraMonLayer->pArcs+nIndex)->VFG&MM_POL_EXTERIOR_SIDE)
                hMiraMonLayer->ReadedFeature.pbArcInfo[hMiraMonLayer->ReadedFeature.nNRings]=TRUE;
            else
                hMiraMonLayer->ReadedFeature.pbArcInfo[hMiraMonLayer->ReadedFeature.nNRings]=FALSE;

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
            #ifndef GDAL_COMPILATION
            lastErrorMM=Error_lectura_Corromput;
            #endif
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
                        #ifndef GDAL_COMPILATION
                        lastErrorMM = Error_lectura_Corromput;
                        #endif
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


// READING THE HEADER OF AN EXTENDED DBF
int MM_ReadExtendedDBFHeader(struct MiraMonVectLayerInfo *hMiraMonLayer)
{
MM_BYTE  variable_byte;
FILE_TYPE *pf;
unsigned short int ushort;
MM_EXT_DBF_N_FIELDS nIField, j;
MM_FIRST_RECORD_OFFSET_TYPE offset_primera_fitxa;
MM_FIRST_RECORD_OFFSET_TYPE offset_fals=0;
MM_BOOLEAN grandaria_registre_incoherent=FALSE;
char local_file_name[MM_MAX_PATH];
MM_BYTE un_byte;
MM_TIPUS_BYTES_PER_CAMP_DBF bytes_per_camp;
MM_BYTE tretze_bytes[13];
MM_FIRST_RECORD_OFFSET_TYPE offset_possible;
MM_BYTE n_queixes_estructura_incorrecta=0;
MM_FILE_OFFSET offset_reintent=0;
struct MM_BASE_DADES_XP *pMMBDXP;
char cpg_file[MM_MAX_PATH];
char * pszRelFile=NULL, *pszDesc;
char section[MM_MAX_LON_FIELD_NAME_DBF+25]; // TAULA_PRINCIPAL:field_name

     pMMBDXP=hMiraMonLayer->pMMBDXP=calloc_function(sizeof(*pMMBDXP));

    if (hMiraMonLayer->bIsPoint)
    {
        hMiraMonLayer->MMPoint.MMAdmDB.pMMBDXP = pMMBDXP;
        strcpy(pMMBDXP->szNomFitxer, hMiraMonLayer->MMPoint.MMAdmDB.pszExtDBFLayerName);
        pszRelFile=hMiraMonLayer->MMPoint.pszREL_LayerName;
    }
    else if (hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        hMiraMonLayer->MMArc.MMAdmDB.pMMBDXP = pMMBDXP;
        strcpy(pMMBDXP->szNomFitxer, hMiraMonLayer->MMArc.MMAdmDB.pszExtDBFLayerName);
        pszRelFile=hMiraMonLayer->MMArc.pszREL_LayerName;
    }
    else if(hMiraMonLayer->bIsPolygon)
    {
        hMiraMonLayer->MMPolygon.MMAdmDB.pMMBDXP=pMMBDXP;
        strcpy(pMMBDXP->szNomFitxer, hMiraMonLayer->MMPolygon.MMAdmDB.pszExtDBFLayerName);
        pszRelFile=hMiraMonLayer->MMPolygon.pszREL_LayerName;
    }        

    strcpy(local_file_name, pMMBDXP->szNomFitxer);
    strcpy(pMMBDXP->ModeLectura, "rb");

	if ((pMMBDXP->pfBaseDades=fopen_function(pMMBDXP->szNomFitxer,
    	 pMMBDXP->ModeLectura))==NULL)
	      return 1;

    pf=pMMBDXP->pfBaseDades;
    
	/* ====== Header reading (32 bytes) =================== */
	offset_primera_fitxa=0;

	if (1!=fread_function(&(pMMBDXP->versio_dbf), 1, 1, pf) ||
		1!=fread_function(&variable_byte, 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->mes), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->dia), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->nfitxes), 4, 1, pf) ||
		1!=fread_function(&offset_primera_fitxa, 2, 1, pf))
	{
		fclose_function(pMMBDXP->pfBaseDades);
        return 1;
	}

	pMMBDXP->any = (short)(1900+variable_byte);
    reintenta_lectura_per_si_error_CreaCampBD_XP:
    
    if (n_queixes_estructura_incorrecta>0)
    {
	    if (!MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
    	{
    		offset_fals = offset_primera_fitxa;
	        if ((offset_primera_fitxa-1)%32)
    	    {
            	for (offset_fals=(offset_primera_fitxa-1); !((offset_fals-1)%32); offset_fals--)
                	;
	        }
		}
    }
    else
		offset_reintent=ftell_function(pf);

	if (1!=fread_function(&ushort, 2, 1, pf) ||
		1!=fread_function(&(pMMBDXP->reservat_1), 2, 1, pf) ||
		1!=fread_function(&(pMMBDXP->transaction_flag), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->encryption_flag), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->dbf_on_a_LAN), 12, 1, pf) ||
		1!=fread_function(&(pMMBDXP->MDX_flag), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->JocCaracters), 1, 1, pf) ||
		1!=fread_function(&(pMMBDXP->reservat_2), 2, 1, pf))
	{
		fclose_function(pMMBDXP->pfBaseDades);
        return 1;
	}

	// Checking for a cpg file
	if (pMMBDXP->JocCaracters==0)
	{
        strcpy(cpg_file, pMMBDXP->szNomFitxer);
        reset_extension(cpg_file, ".cpg");
        FILE *f_cpg=fopen(cpg_file, "rt");
        if(f_cpg)
        {
            char local_message[11];
			fseek(f_cpg, 0L, SEEK_SET);
			if(NULL!=fgets(local_message, 10, f_cpg))
			{
                char *p=strstr(local_message, "UTF-8");
				if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_UTF8_DBF;
                p=strstr(local_message, "UTF8");
				if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_UTF8_DBF;
                p=strstr(local_message, "ISO-8859-1");
				if(p)
                    pMMBDXP->JocCaracters=MM_JOC_CARAC_ANSI_DBASE;
			}
			fclose(f_cpg);
		}
	}
    if (MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
    {
    	memcpy(&pMMBDXP->OffsetPrimeraFitxa,&offset_primera_fitxa,2);
    	memcpy(((char*)&pMMBDXP->OffsetPrimeraFitxa)+2,&pMMBDXP->reservat_2,2);

	    if (n_queixes_estructura_incorrecta>0)
    		offset_fals=pMMBDXP->OffsetPrimeraFitxa;
        
    	memcpy(&pMMBDXP->BytesPerFitxa, &ushort, 2);
    	memcpy(((char*)&pMMBDXP->BytesPerFitxa)+2, &pMMBDXP->reservat_1, 2);
    }
    else
    {
		pMMBDXP->OffsetPrimeraFitxa=offset_primera_fitxa;
    	pMMBDXP->BytesPerFitxa=ushort;
    }

	/* ====== Record structure ========================= */

    if (n_queixes_estructura_incorrecta>0)
        pMMBDXP->ncamps = (MM_EXT_DBF_N_FIELDS)(((offset_fals-1)-32)/32);   // Cas de DBF clàssica
    else
    {
    	MM_TIPUS_BYTES_ACUMULATS_DBF bytes_acumulats=1;

    	pMMBDXP->ncamps=0;

        fseek_function(pf, 0, SEEK_END);
        if(32<(ftell_function(pf)-1))
        {
			fseek_function(pf, 32, SEEK_SET);
			do
			{
				bytes_per_camp=0;
				fseek_function(pf, 32+(MM_FILE_OFFSET)pMMBDXP->ncamps*32+(MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF+1+4), SEEK_SET);
				if (1!=fread_function(&bytes_per_camp, 1, 1, pf) ||
					1!=fread_function(&un_byte, 1, 1, pf) ||
					1!=fread_function(&tretze_bytes, 3+sizeof(bytes_per_camp), 1, pf))
				{
					free(pMMBDXP->Camp);
					fclose_function(pMMBDXP->pfBaseDades);
					return 1;
				}
				if (bytes_per_camp==0)
					memcpy(&bytes_per_camp, (char*)(&tretze_bytes)+3, sizeof(bytes_per_camp));

				bytes_acumulats+=bytes_per_camp;
				pMMBDXP->ncamps++;
			}while(bytes_acumulats<pMMBDXP->BytesPerFitxa);
		}
    }

    if(pMMBDXP->ncamps != 0)
    {
        pMMBDXP->Camp = MM_CreateAllFields(pMMBDXP->ncamps);
        if (!pMMBDXP->Camp)
        {
			fclose_function(pMMBDXP->pfBaseDades);
            return 1;
        }
    }
    else
    	pMMBDXP->Camp = NULL;

    fseek_function(pf, 32, SEEK_SET);
	for (nIField=0; nIField<pMMBDXP->ncamps; nIField++)
	{
    	if (1!=fread_function(pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].TipusDeCamp), 1, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].reservat_1), 4, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].BytesPerCamp), 1, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].DecimalsSiEsFloat), 1, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].reservat_2), 13, 1, pf) ||
			1!=fread_function(&(pMMBDXP->Camp[nIField].MDX_camp_flag), 1, 1, pf))
		{
			free(pMMBDXP->Camp);
            fclose_function(pMMBDXP->pfBaseDades);
			return 1;
		}

        #ifdef CODIFICATION_NEED_TO_BE_FINISHED
		MM_CanviaJocCaracLlegitDeDBF_CONSOLE(pMMBDXP->Camp[nIField].NomCamp, MM_JocCaracDBFaMM(pMMBDXP->JocCaracters, 850));
        #endif

		if(pMMBDXP->Camp[nIField].TipusDeCamp=='F')
			pMMBDXP->Camp[nIField].TipusDeCamp='N';

        pMMBDXP->Camp[nIField].NomCamp[MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF-1]='\0';
        if (EQUAL(pMMBDXP->Camp[nIField].NomCamp, "ID_GRAFIC"))
            pMMBDXP->CampIdGrafic=nIField;
        
        if (pMMBDXP->Camp[nIField].BytesPerCamp==0)
        {
			if (!MM_ES_DBF_ESTESA(pMMBDXP->versio_dbf))
            {
                free(pMMBDXP->Camp);
                fclose_function(pMMBDXP->pfBaseDades);
                return 1;
            }
			if (pMMBDXP->Camp[nIField].TipusDeCamp!='C')
            {
                free(pMMBDXP->Camp);
                fclose_function(pMMBDXP->pfBaseDades);
                return 1;
            }

            memcpy(&pMMBDXP->Camp[nIField].BytesPerCamp, (char*)(&pMMBDXP->Camp[nIField].reservat_2)+3,
            			sizeof(MM_TIPUS_BYTES_PER_CAMP_DBF));
        }

		if (nIField)
			pMMBDXP->Camp[nIField].BytesAcumulats=
				(pMMBDXP->Camp[nIField-1].BytesAcumulats+
					pMMBDXP->Camp[nIField-1].BytesPerCamp);
		else
			pMMBDXP->Camp[nIField].BytesAcumulats=1;

	    for (j=0; j<MM_NUM_IDIOMES_MD_MULTIDIOMA; j++)
		{
        	pMMBDXP->Camp[nIField].separador[j]=NULL;

            
            sprintf(section, "TAULA_PRINCIPAL:%s", pMMBDXP->Camp[nIField].NomCamp);
            pszDesc=ReturnValueFromSectionINIFile(pszRelFile, section, "descriptor_eng");
            if(pszDesc)
                MM_strnzcpy(pMMBDXP->Camp[nIField].DescripcioCamp[j], pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
            else
            {
                sprintf(section, "TAULA_PRINCIPAL:%s", pMMBDXP->Camp[nIField].NomCamp);
                pszDesc = ReturnValueFromSectionINIFile(pszRelFile, section, "descriptor");
                if (pszDesc)
                    MM_strnzcpy(pMMBDXP->Camp[nIField].DescripcioCamp[j], pszDesc, MM_MAX_LON_DESCRIPCIO_CAMP_DBF);
                pMMBDXP->Camp[nIField].DescripcioCamp[j][0] = 0;
            }
		}
		pMMBDXP->Camp[nIField].mostrar_camp=MM_CAMP_MOSTRABLE;
        pMMBDXP->Camp[nIField].simbolitzable=MM_CAMP_SIMBOLITZABLE;
        pMMBDXP->Camp[nIField].TractamentVariable=MM_DBFFieldTypeToVariableProcessing(pMMBDXP->Camp[nIField].TipusDeCamp);
	}

    if (!pMMBDXP->ncamps)
    {
    	if (pMMBDXP->BytesPerFitxa)
        	grandaria_registre_incoherent=TRUE;
    }
    else if(pMMBDXP->Camp[pMMBDXP->ncamps-1].BytesPerCamp+
			pMMBDXP->Camp[pMMBDXP->ncamps-1].BytesAcumulats
				>pMMBDXP->BytesPerFitxa)
    	grandaria_registre_incoherent=TRUE;
    if (grandaria_registre_incoherent)
    {
    	if (n_queixes_estructura_incorrecta==0)
        {
            grandaria_registre_incoherent=FALSE;
            fseek_function(pf, offset_reintent, SEEK_SET);
            n_queixes_estructura_incorrecta++;
            goto reintenta_lectura_per_si_error_CreaCampBD_XP;
        }
    }

    offset_possible=32+32*(pMMBDXP->ncamps)+1;

    if (!grandaria_registre_incoherent &&
    	offset_possible!=pMMBDXP->OffsetPrimeraFitxa)
    {	// Extended names
    	MM_FIRST_RECORD_OFFSET_TYPE offset_nom_camp;
        int mida_nom;

    	for(nIField=0; nIField<pMMBDXP->ncamps; nIField++)
        {
            offset_nom_camp=MM_GiveOffsetExtendedFieldName(pMMBDXP->Camp+nIField);
			mida_nom=MM_DonaBytesNomEstesCamp(pMMBDXP->Camp+nIField);
            if(mida_nom>0 && mida_nom<MM_MAX_LON_FIELD_NAME_DBF &&
            	offset_nom_camp>=offset_possible &&
            	offset_nom_camp<pMMBDXP->OffsetPrimeraFitxa)
            {
            	MM_strnzcpy(pMMBDXP->Camp[nIField].NomCampDBFClassica, pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
            	fseek_function(pf, offset_nom_camp, SEEK_SET);
                if (1!=fread_function(pMMBDXP->Camp[nIField].NomCamp, mida_nom, 1, pf))
                {
                    free(pMMBDXP->Camp);
                    fclose_function(pMMBDXP->pfBaseDades);
                    return 1;
                }
                pMMBDXP->Camp[nIField].NomCamp[mida_nom]='\0';
                #ifdef CODIFICATION_NEED_TO_BE_FINISHED
                CanviaJocCaracLlegitDeDBF(pMMBDXP->Camp[nIField].NomCamp, JocCaracDBFaMM(pMMBDXP->JocCaracters, 850));
                #endif          
            }
            else
            {
				MM_strnzcpy(pMMBDXP->Camp[nIField].NomCampDBFClassica, pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
				MM_PassaAMajuscules(pMMBDXP->Camp[nIField].NomCampDBFClassica);
            }
        }
    }
    else 
    {
    	for(nIField=0; nIField<pMMBDXP->ncamps; nIField++)
		{
	    	MM_strnzcpy(pMMBDXP->Camp[nIField].NomCampDBFClassica, pMMBDXP->Camp[nIField].NomCamp, MM_MAX_LON_CLASSICAL_FIELD_NAME_DBF);
			MM_PassaAMajuscules(pMMBDXP->Camp[nIField].NomCampDBFClassica);
		}
    }

    fclose_function(pf);
	pMMBDXP->pfBaseDades=NULL;

    pMMBDXP->CampIdEntitat=MM_MAX_EXT_DBF_N_FIELDS_TYPE;

    
    return 0;
} 



#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
