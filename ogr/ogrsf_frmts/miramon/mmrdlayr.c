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
//#include "mm_gdal\mm_gdal_functions.h"
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

/*struct SMM_HANDLE_CAPA_VECTOR
{
	int tipus_fitxer;
	struct TREBALL_ARC_NOD an;
    char nom_fitxer[MM_MAX_PATH];
    char nom_fitxer_arc[MM_MAX_PATH];
	struct Capcalera_Top cap_top;
    struct Capcalera_Top cap_top_arc;
    MM_BOOLEAN es_3d;
    struct CAPCALERA_VECTOR_3D *cap_arc_3d;  // pel cas en que necessitem llegir les capçaleres 3d amb el "pic i la pala".
    FILE_TYPE *pf, *pfarc;
};*/
#ifdef TO_BE_REVISED
MM_HANDLE_CAPA_VECTOR MMIniciaCapaVector(const char *nom_fitxer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
char ext[MAX_MIDA_EXTENSIO_FITXER];

//Aquests defines fan que no es miri el tema de la llicència.
#define ANY_CADUCITAT 2008
#define MES_CADUCITAT 8
#define DIA_CADUCITAT 1

#if defined ANY_CADUCITAT && defined MES_CADUCITAT && defined DIA_CADUCITAT
time_t t;
   struct tm *tm_local;
#endif

#if defined ANY_CADUCITAT && defined MES_CADUCITAT && defined DIA_CADUCITAT
   //   La llicència temporal. En aquest cas no es valida la llicència basada
   //   en paràmetres.

   t = time(NULL);
   tm_local = localtime(&t);

   if (tm_local->tm_year+1900>ANY_CADUCITAT ||
       (tm_local->tm_year+1900==ANY_CADUCITAT && tm_local->tm_mon+1>MES_CADUCITAT) ||
       (tm_local->tm_year+1900==ANY_CADUCITAT && tm_local->tm_mon+1==MES_CADUCITAT && tm_local->tm_mday>=DIA_CADUCITAT))
   {
    	lastErrorMM=MalInstallat_o_CopiaIllegal;
       	return NULL;
   }
#endif


	if(!nom_fitxer || VerificaSiPucLlegirFitxerXerraire(nom_fitxer, FALSE))
    {
	    lastErrorMM=No_puc_obrir_demanat;
        return NULL;
    }

	if(NULL==(shlayer=calloc(1,sizeof(*shlayer))))
    {
	    lastErrorMM=No_mem;
        return NULL;
    }

    OmpleExtensio(ext,MAX_MIDA_EXTENSIO_FITXER, nom_fitxer);
    if(!stricmp(ext, ExtPoligons+1))
    {
        shlayer->bIsPolygon=TRUE;
    }
    else if(!stricmp(ext, ExtArcs+1))
    {
        shlayer->bIsArc=TRUE;
    }
    else if(!stricmp(ext, ExtNodes+1))
    {
    	shlayer->bIsNode=TRUE;
    }
    else if(!stricmp(ext, ExtPunts+1))
    {
        shlayer->bIsPoint=TRUE;
    }
    else
    {
	    lastErrorMM=Ext_incorrecta_No_puc_obrir;
        free(shlayer);
        return NULL;
    }

    shlayer->pszSrcLayerName=strdup_function(nom_fitxer);

    if(shlayer->tipus_fitxer==MM32DLL_POL)
    {
        DonaNomArcDelPol(shlayer->nom_fitxer_arc, shlayer->nom_fitxer);
        if ((shlayer->pfarc=fopen_function(shlayer->nom_fitxer_arc, "rb"))==0 || lect_capcalera_topo(shlayer->pfarc, ExtArcs, &(shlayer->cap_top_arc)))
        {
            lastErrorMM=No_puc_obrir_origen;
            free(shlayer);
            return NULL;
        }

        LlegeixStructTreballArcNod(shlayer->nom_fitxer, shlayer->nom_fitxer_arc, &(shlayer->an));

        if ((shlayer->pf=fopen_function(shlayer->nom_fitxer, "rb"))==0 || lect_capcalera_topo(shlayer->pf, ExtPoligons, &(shlayer->cap_top)))
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        shlayer->cap_arc_3d=NULL;
        hMiraMonLayer->TopHeader.bIs3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
    }
    else if(shlayer->tipus_fitxer==MM32DLL_ARC)
    {
    	*(shlayer->nom_fitxer_arc)='\0';
        shlayer->pfarc=NULL;

        LlegeixStructTreballArcNod(NULL, shlayer->nom_fitxer, &(shlayer->an));
        if ((shlayer->pf=fopen_function(shlayer->nom_fitxer, "rb"))==0 || lect_capcalera_topo(shlayer->pf, ExtArcs, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        hMiraMonLayer->TopHeader.bIs3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
    }
    else if(shlayer->tipus_fitxer==MM32DLL_NOD)
    {
        sprintf(shlayer->nom_fitxer_arc, "%s%s", TreuExtensio(nom_fitxer), ExtArcs);

        LlegeixStructTreballArcNod(NULL, shlayer->nom_fitxer_arc, &(shlayer->an));

        if ((shlayer->pfarc=fopen_function(shlayer->nom_fitxer_arc, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pfarc, ExtArcs, &(shlayer->cap_top_arc)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if ((shlayer->pf=fopen_function(shlayer->nom_fitxer, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pf, ExtNodes, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
		if(shlayer->an.arcZ)
    	    hMiraMonLayer->TopHeader.bIs3d=TRUE;
        else
	        hMiraMonLayer->TopHeader.bIs3d=FALSE;
	}
    else if(shlayer->tipus_fitxer==MM32DLL_PNT)
    {
    	*(shlayer->nom_fitxer_arc)='\0';
        shlayer->pfarc=NULL;

        if ((shlayer->pf=fopen_function(shlayer->nom_fitxer, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pf, ExtPunts, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        hMiraMonLayer->TopHeader.bIs3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
	}

    if(hMiraMonLayer->TopHeader.bIs3d && shlayer->tipus_fitxer==MM32DLL_ARC)
    {
	    if(NULL==(shlayer->cap_arc_3d=Llegeix_cap_arcZ(shlayer->pf, shlayer->an.cap_arc, shlayer->an.n_arc, NULL, NULL)))
        {
        	lastErrorMM=UltimErrorStb0;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    if(hMiraMonLayer->TopHeader.bIs3d && shlayer->tipus_fitxer==MM32DLL_NOD)
    {
    	if(NULL==(shlayer->cap_arc_3d=Llegeix_cap_arcZ(shlayer->pfarc, shlayer->an.cap_arc, shlayer->an.n_arc, NULL, NULL)))
  		{
        	lastErrorMM=UltimErrorStb0;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    else if(hMiraMonLayer->TopHeader.bIs3d && shlayer->tipus_fitxer==MM32DLL_PNT)
    {
	    if (NULL==(shlayer->cap_arc_3d=calloc(shlayer->cap_top.n_elem, sizeof(*(shlayer->cap_arc_3d)))))
        {
        	lastErrorMM=No_mem;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
        // Saltem la part de punts i llegim les capçaleres 3d
        fseek_function(shlayer->pf, MIDA_CAPCALERA_TOP+shlayer->cap_top.n_elem*sizeof(double)*2+16+sizeof(double)*2,SEEK_SET);
        // LLegim les capçaleres
        if (fread_function(shlayer->cap_arc_3d, sizeof(struct CAPCALERA_VECTOR_3D), shlayer->cap_top.n_elem, shlayer->pf)!=(size_t)shlayer->cap_top.n_elem)
        {
            lastErrorMM=Mida_incoherent_Corromput;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    return (MM_HANDLE_CAPA_VECTOR)shlayer;
}
#else
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
    
    // Don't free in destructor
    hMiraMonLayer->pLayerDB=NULL; //·$·

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
    hMiraMonLayer->nNumStringToOperate=500; 
    hMiraMonLayer->szStringToOperate=
            calloc_function(hMiraMonLayer->nNumStringToOperate);
    if(!hMiraMonLayer->szStringToOperate)
    {
        error_message_function("Not enough memory");
        return 1;
    }

    // S'ha de llegir de la DBF
    // ·$· hMiraMonLayer->nCharSet=MM_JOC_CARAC_ANSI_DBASE;
    
    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->bIsPoint)
    {
        if(MMReadZSection(hMiraMonLayer, hMiraMonLayer->MMPoint.pF, 
                   &hMiraMonLayer->MMPoint.pZSection))
            return 1;

        if(MMReadZDescriptionHeaders(hMiraMonLayer, hMiraMonLayer->MMPoint.pF, 
            hMiraMonLayer->TopHeader.nElemCount, &hMiraMonLayer->MMPoint.pZSection))
            return 1;
    }
    return 0;
}
#endif // TO_BE_REVISED

int MMAddStringLineCoordinates(struct MiraMonVectLayerInfo *hMiraMonLayer,
                MM_INTERNAL_FID i_elem,
				IN unsigned long int flag_z,
                MM_N_VERTICES_TYPE nStartVertice,
                MM_BOOLEAN bAvoidFirst,
                unsigned char VFG)
{
FILE_TYPE *pF;
struct MM_AH *pArcHeader;

    if (hMiraMonLayer->bIsPolygon)
    {
        pF=hMiraMonLayer->MMPolygon.MMArc.pF;
        pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader;
    }
    else
    {
        pF=hMiraMonLayer->MMArc.pF;
        pArcHeader=hMiraMonLayer->MMArc.pArcHeader;
    }
        
                
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

        // Reverse the vertices
        for (nIVertice = 0; nIVertice < pArcHeader[i_elem].nElemCount; nIVertice++)
        {
            memcpy(hMiraMonLayer->ReadedFeature.pCoord + nStartVertice - (bAvoidFirst ? 1 : 0) + nIVertice,
                hMiraMonLayer->ReadedFeature.pCoord + nStartVertice + 2*pArcHeader[i_elem].nElemCount-nIVertice-1,
                sizeof(*hMiraMonLayer->ReadedFeature.pCoord));
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
    }
    hMiraMonLayer->ReadedFeature.nNumpCoord=pArcHeader[i_elem].nElemCount-(bAvoidFirst?1:0);

    #ifdef CAL_FER_3D_ARCS
    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->ReadedFeature.pZCoord)
    {
        pZDescription=hMiraMonLayer->MMArc.pZSection.pZDescription;

        if(MMResizeDoublePointer(&hMiraMonLayer->ReadedFeature.pZCoord, 
                &hMiraMonLayer->ReadedFeature.nMaxpZCoord, 
                nStartVertice+pArcHeader[i_elem].nElemCount,
                0, 0))
            return 1;

        // +nStartVertice
        DonaAlcadesDArc(punts_z, pF, pArcHeader[i_elem].nElemCount,
            pZDescription+i_elem, flag_z);

        for(k=0; k<(pArcHeader[i_elem].nElemCount); k++)
        {
            // ·$· Which value is nodata por Z en GDAL?
            /*if(!DOUBLES_DIFERENTS_DJ(punts_z[k], MM_NODATA_COORD_Z))
                cz=-DBL_MAX;
            else */cz=punts_z[k]; // Només ens quedem el primer.

            hMiraMonLayer->ReadedFeature.pZCoord[k]=cz;
        }
    }
    #endif //CAL_FER_3D_ARCS
    return 0;
}

int MMGetMultiPolygonCoordinates(struct MiraMonVectLayerInfo *hMiraMonLayer,
                OUT double **coord_z, 
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
    
    #ifdef ANEM_FENT
    

    if (coord_z && *coord_z==NULL)
    {
		var_pols->Nomb_Max_Coord_Z=var_pols->Nomb_Max_Coord;
		if (NULL==(*coord_z = MM_calloc(var_pols->Nomb_Max_Coord*sizeof(**coord_z))))
        {
    	    return 1;
        }
	}

    ncoord=0;
	if(hMiraMonLayer->ReadedFeature.nNumpCoord)
	{
		hMiraMonLayer->nNRing=0;
		if(n_mini_pol_ext)
			*n_mini_pol_ext=0;
	}
	else
	{
		hMiraMonLayer->nNRing=0;
		if(n_mini_pol_ext)
			*n_mini_pol_ext=0;
	}

    for( nIArcAux=0, nvertex_per_minipol=0;nIArcAux<pPolHeader->nArcsCount; nIArcAux++)
    {
		if (coord_z)
        {
            if ((ncoord+vertex_arc)>var_pols->Nomb_Max_Coord_Z)
            {
                if (NULL==(*coord_z = MM_recalloc(*coord_z, (ncoord+vertex_arc)*sizeof(**coord_z), var_pols->Nomb_Max_Coord_Z*sizeof(**coord_z))))
                {
					return 1;
                }
                var_pols->Nomb_Max_Coord_Z=ncoord+vertex_arc;
            }
	        DonaAlcadesDArc(*coord_z+(size_t)ncoord, hMiraMonLayer->MMPolygon.MMArc.pF, vertex_arc, cap_arcZ+(size_t)un_arc_del_pol.i_arc, flag_z);
            if (un_arc_del_pol.vora_fi_gir&TOPO_ARC_GIRAR)
            {
                for ( reves=0; reves<vertex_arc/2; reves++)
                {
                    un_double=*(*coord_z+(size_t)ncoord+reves);
                    *(*coord_z+(size_t)ncoord+reves)=*(*coord_z+((size_t)ncoord+vertex_arc-1-reves));
                    *(*coord_z+(size_t)ncoord+vertex_arc-1-reves)=un_double;
                }
            }
        }
	}

	
    #endif //#ifdef ANEM_FENT
	

	return 0;
}

int MMGetFeatureFromVector(struct MiraMonVectLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem)
{
FILE_TYPE *pF;
struct MM_ZD *pZDescription;
unsigned long int flag_z;
double *z=NULL;
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

        if(hMiraMonLayer->TopHeader.bIs3d)
        {
            pZDescription=hMiraMonLayer->MMPoint.pZSection.pZDescription+i_elem;
            num=MM_ARC_N_TOTAL_ALCADES_DISC(pZDescription->nZCount, 1);
            if(num==0)
                hMiraMonLayer->ReadedFeature.pZCoord[0]=MM_NODATA_COORD_Z;
            else
            {
                if(MMResizeDoublePointer(&hMiraMonLayer->ReadedFeature.pZCoord, 
                    &hMiraMonLayer->ReadedFeature.nMaxpZCoord, 
                    hMiraMonLayer->ReadedFeature.nNumpZCoord, 1, 1))
                    return 1;

                fseek_function(pF, pZDescription->nOffsetZ ,SEEK_SET);
                if((size_t)num!=fread_function(hMiraMonLayer->ReadedFeature.pZCoord,sizeof(*hMiraMonLayer->ReadedFeature.pZCoord),num,pF))
                {
                    #ifndef GDAL_COMPILATION
                    lastErrorMM=Error_lectura_Corromput;
                    #endif
                    return 1;
                }

                // ·$· De moment agafarem el primer
                /*if (flag_z==MM_STRING_HIGHEST_ALTITUDE)
                    cz=pZDescription->dfBBmaxz;
                else if (flag_z==MM_STRING_LOWEST_ALTITUDE)
                    cz=pZDescription->dfBBminz;
                else*/
                    cz=hMiraMonLayer->ReadedFeature.pZCoord[0];

                // ·$· Which value is nodata por Z en GDAL?
                /*if(!DOUBLES_DIFERENTS_DJ(cz, MM_NODATA_COORD_Z))
                    hMiraMonLayer->ReadedFeature.pZCoord[0]=-DBL_MAX;
                else */hMiraMonLayer->ReadedFeature.pZCoord[0]=cz; // NomÃ©s ens quedem el primer.
            }
        }
        hMiraMonLayer->ReadedFeature.nNRings=1;
        
        if(MMResize_MM_N_VERTICES_TYPE_Pointer(&hMiraMonLayer->ReadedFeature.pNCoordRing, 
                        &hMiraMonLayer->ReadedFeature.nMaxpNCoordRing, 
                        1, 0, 1))
            return 1;

        hMiraMonLayer->ReadedFeature.pNCoordRing[0]=1;
        return 0;
    }

    // Stringlines
    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        if(MMAddStringLineCoordinates(hMiraMonLayer, i_elem, flag_z, 0, FALSE, 0))
            return 1;

        return 0;
    }

    // Polygons or multipolygons
    pF=hMiraMonLayer->MMPolygon.pF;
    pPolHeader=hMiraMonLayer->MMPolygon.pPolHeader;
    pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader;

    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->ReadedFeature.pZCoord)
    {
        pZDescription=hMiraMonLayer->MMPolygon.MMArc.pZSection.pZDescription;
        if(MMGetMultiPolygonCoordinates(hMiraMonLayer, &z, i_elem, flag_z))
            return 1;
    }
    else
        if(MMGetMultiPolygonCoordinates(hMiraMonLayer, NULL, i_elem, flag_z))
            return 1;

	return 0;
}

#ifdef IS_COMING
MM_TIPUS_BOLEA MMEs3DCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
 	return (MM_TIPUS_BOLEA)hMiraMonLayer->TopHeader.bIs3d;
}

MM_TIPUS_TIPUS_FITXER MMTipusFitxerCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
 	return (MM_TIPUS_TIPUS_FITXER)(shlayer->tipus_fitxer);
}

void MMFinalitzaCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
	AlliberaStructTreballArcNod(&(shlayer->an));
    if(shlayer->pf)fclose_64(shlayer->pf);
    if(shlayer->pfarc)fclose_64(shlayer->pfarc);
    if(shlayer->cap_arc_3d)free(shlayer->cap_arc_3d);
}
#endif //#ifdef IS_COMING

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif