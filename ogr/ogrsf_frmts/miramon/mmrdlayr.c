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
int MMInitLayerToRead(struct MiraMonLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, const char *pszFilename)
{
char *aRELLayerName=NULL;

    memset(hMiraMonLayer, 0, sizeof(*hMiraMonLayer));
    MMReadHeader(m_fp, &hMiraMonLayer->TopHeader);
    hMiraMonLayer->ReadOrWrite=MM_READING_MODE;
    strcpy(hMiraMonLayer->pszFlags, "rb");
    
    hMiraMonLayer->pszSrcLayerName=strdup_function(pszFilename);
    aRELLayerName=calloc_function(strlen(hMiraMonLayer->pszSrcLayerName)+6);
    
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
        if(MMResetExtensionAndLastLetter(aRELLayerName, 
                    hMiraMonLayer->pszSrcLayerName, "T.rel"))
            return 1;
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
        if(MMResetExtensionAndLastLetter(aRELLayerName, 
                    hMiraMonLayer->pszSrcLayerName, "A.rel"))
            return 1;
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

        // MULTIPOLYGON Â·$Â·
        hMiraMonLayer->bIsPolygon=TRUE;
        if(MMResetExtensionAndLastLetter(aRELLayerName, 
                    hMiraMonLayer->pszSrcLayerName, "P.rel"))
            return 1;
    }
    
    hMiraMonLayer->Version=MM_VECTOR_LAYER_LAST_VERSION;
    
    // Don't free in destructor
    hMiraMonLayer->pLayerDB=NULL; //·$·

    hMiraMonLayer->pSRS=strdup(ReturnEPSGCodeSRSFromMMIDSRS(
        ReturnValueFromSectionINIFile(aRELLayerName, 
        "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier")));

    if(MMInitLayerByType(hMiraMonLayer))
        return 1;
    hMiraMonLayer->bIsBeenInit=1;
    
    // If more nNumStringToOperate is needed, it'll be increased.
    hMiraMonLayer->nNumStringToOperate=500; 
    hMiraMonLayer->szStringToOperate=
            calloc_function(hMiraMonLayer->nNumStringToOperate);
    if(!hMiraMonLayer->szStringToOperate)
    {
        error_message_function("Not enough memory");
        return 1;
    }

    // ·$· hMiraMonLayer->nCharSet=MM_JOC_CARAC_ANSI_DBASE;
              
    if(hMiraMonLayer->bIsPolygon)
    {
        #ifdef JA_NO_FALTA
        DonaNomArcDelPol(shlayer->nom_fitxer_arc, shlayer->nom_fitxer);
        if ((shlayer->pfarc=fopen_function(shlayer->nom_fitxer_arc, "rb"))==0 || 
            lect_capcalera_topo(shlayer->pfarc, ExtArcs, &(shlayer->cap_top_arc)))
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
        #endif
    }
    else if(hMiraMonLayer->bIsArc)
    {
        #ifdef JA_NO_FALTA
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
        #endif
    }
    #ifdef JA_NO_FALTA
    else if(hMiraMonLayer->tipus_fitxer==MM32DLL_NOD)
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
    #endif
    
    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->bIsArc)
    {
        #ifdef JA_NO_FALTA
	    if(NULL==(shlayer->cap_arc_3d=Llegeix_cap_arcZ(shlayer->pf, shlayer->an.cap_arc, shlayer->an.n_arc, NULL, NULL)))
        {
        	lastErrorMM=UltimErrorStb0;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    #endif
    }

    #ifdef JA_NO_FALTA
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
    else
    #endif
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

#ifdef IS_COMING
MM_TIPUS_I_ELEM_CAPA_VECTOR MMRecuperaNElemCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.n_elem;
}


MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

  	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VX_MIN];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VX_MAX];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VY_MIN];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VY_MAX];
}

MM_POLYGON_RINGS_COUNT MMRecuperMaxNAnellsCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
unsigned long int i;
MM_POLYGON_RINGS_COUNT max=0;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    if(shlayer->tipus_fitxer!=MM32DLL_POL)
        return 1;

	for (i=0;i<shlayer->an.n_pol;i++)
    {
    	if(max<shlayer->an.cap_pol[i].npol_per_polip)
			max=shlayer->an.cap_pol[i].npol_per_polip;
    }
    return max;
}

MM_N_VERTICES_TYPE MMRecuperaMaxNCoordCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
MM_N_VERTICES_TYPE maxncoord;
MM_N_VERTICES_TYPE *n_vrt_ring;
TIPUS_N_POLIGONS n_mini_pol;
TIPUS_NUMERADOR_OBJECTE i_elem;
MM_N_VERTICES_TYPE n_ring;
long int j;
struct VARIABLES_LLEGEIX_POLS var_pols;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;

    if(shlayer->tipus_fitxer==MM32DLL_PNT || shlayer->tipus_fitxer==MM32DLL_NOD)
    	return 1;

    maxncoord=0;
    if(shlayer->tipus_fitxer==MM32DLL_ARC)
    {
    	maxncoord=0;
        for(i_elem=0; i_elem<shlayer->an.n_arc;i_elem++)
        {
            if(maxncoord<shlayer->an.cap_arc[i_elem].nomb_vertex)
                maxncoord=shlayer->an.cap_arc[i_elem].nomb_vertex;
        }
        return maxncoord;
    }

    if(shlayer->tipus_fitxer==MM32DLL_POL)
    {
        maxncoord=0;
        n_vrt_ring=NULL;
        for(i_elem=0; i_elem<shlayer->an.n_pol;i_elem++)
        {
            LlegeixPoliPoligon3D_POL(NULL, &n_vrt_ring, &n_mini_pol, NULL, NULL, NULL, NULL,
               NULL, NULL, shlayer->pf, shlayer->pfarc,
               shlayer->an.cap_arc, NULL, shlayer->an.cap_pol, i_elem, FALSE, 0L, &var_pols);

            n_ring=0;
            for(j=0;j<(int)n_mini_pol;j++)
                (n_ring)+=n_vrt_ring[j];

            if(maxncoord<n_ring)
                maxncoord=n_ring;
        }
		AlliberaIAnullaPoliPoligon3D_POL(NULL, &n_vrt_ring, &n_mini_pol, NULL, NULL, NULL,
               NULL, NULL, &var_pols);

        return maxncoord;
    }
	return maxncoord;
}
#endif

int MMGetFeatureFromVector(struct MiraMonLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem)
{
FILE_TYPE *pF, *pFArc;
struct MM_ZD *pZDescription;
unsigned long int flag_z;
size_t k, i_coord_acumulat;
size_t i_ring;
struct MM_POINT_2D *coord=NULL;
//struct MM_POINT_2D un_punt;
double *z=NULL;
int num;
double cz;
struct MM_POINT_2D *punts;
MM_N_VERTICES_TYPE *n_vrt_ring_interna=NULL;
// DESPRES struct VARIABLES_LLEGEIX_POLS var_pols;
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
        if(MMResizeMM_POINT2DPointer(&hMiraMonLayer->nCoordXY, 
            &hMiraMonLayer->nMaxCoordXY, 
            hMiraMonLayer->nNumCoordXY, 1, 1))
            return 1;

        if (1!=fread_function(hMiraMonLayer->nCoordXY, sizeof(MM_COORD_TYPE)*2, 1, pF))
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
                hMiraMonLayer->nCoordZ[0]=MM_NODATA_COORD_Z;
            else
            {
                if(MMResizeDoublePointer(&hMiraMonLayer->nCoordZ, 
                    &hMiraMonLayer->nMaxCoordZ, 
                    hMiraMonLayer->nNumCoordZ, 1, 1))
                    return 1;

                fseek_function(pF, pZDescription->nOffsetZ ,SEEK_SET);
                if((size_t)num!=fread_function(hMiraMonLayer->nCoordZ,sizeof(*hMiraMonLayer->nCoordZ),num,pF))
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
                    cz=hMiraMonLayer->nCoordZ[0];

                // ·$· Which value is nodata por Z en GDAL?
                /*if(!DOUBLES_DIFERENTS_DJ(cz, MM_NODATA_COORD_Z))
                    hMiraMonLayer->nCoordZ[0]=-DBL_MAX;
                else */hMiraMonLayer->nCoordZ[0]=cz; // Només ens quedem el primer.
            }
        }
        hMiraMonLayer->nNRing=1;
        if(hMiraMonLayer->nNVrtRing)hMiraMonLayer->nNVrtRing[0]=1;
        return 0;
    }

    #ifdef TODO
    if(hMiraMonLayer->bIsNode)
    {
    	if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->nCoordZ)
        {
            DonaPosicioNode(&un_punt, &cz, i_elem,
                shlayer->an.cap_nod, shlayer->cap_top.n_elem,
                shlayer->pf, shlayer->an.cap_arc, shlayer->cap_arc_3d,
                shlayer->cap_top_arc.n_elem, shlayer->pfarc, flag_z);
            hMiraMonLayer->nCoordZ[0]=cz;
        }
        else
        {
            DonaPosicioNode(&un_punt, NULL, i_elem,
                shlayer->an.cap_nod, shlayer->cap_top.n_elem,
                shlayer->pf, shlayer->an.cap_arc, NULL,
                shlayer->cap_top_arc.n_elem, shlayer->pfarc, flag_z);
        }
        hMiraMonLayer->nCoordX[0]=un_punt.x;
        hMiraMonLayer->nCoordY[0]=un_punt.y;

        hMiraMonLayer->nNRing=1;
        if(hMiraMonLayer->nNVrtRing)hMiraMonLayer->nNVrtRing[0]=1;
        return 0;
    }
    

    if(hMiraMonLayer->bIsArc && !hMiraMonLayer->bIsPolygon)
    {
        pF=hMiraMonLayer->MMArc.pF;
        pArcHeader=hMiraMonLayer->MMArc.pArcHeader;
                
        fseek_function(pF, pArcHeader[i_elem].nOffset, SEEK_SET);
        // LLegim els vertexs de l'arc
        punts=calloc_function(pArcHeader[i_elem].nElemCount*sizeof(*punts));
        if(pArcHeader[i_elem].nElemCount!=
            fread_function(punts,sizeof(*punts),pArcHeader[i_elem].nElemCount, pF))
        {
            #ifndef GDAL_COMPILATION
            lastErrorMM=Mida_incoherent_Corromput;
            #endif
            return 1;
        }

        for(k=0;k<pArcHeader[i_elem].nElemCount;k++)
        {
        	hMiraMonLayer->nCoordX[k]=punts[k].dfX;
	        hMiraMonLayer->nCoordY[k]=punts[k].dfY;
        }
        free_function(punts);

        if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->nCoordZ)
        {
            pZDescription=hMiraMonLayer->MMArc.pZSection.pZDescription;

            punts_z=calloc(pArcHeader[i_elem].nElemCount, sizeof(double));
            DonaAlcadesDArc(punts_z, pF, pArcHeader[i_elem].nElemCount,
                pZDescription+i_elem, flag_z);

            for(k=0; k<(pArcHeader[i_elem].nElemCount); k++)
            {
                // ·$· Which value is nodata por Z en GDAL?
                /*if(!DOUBLES_DIFERENTS_DJ(punts_z[k], MM_NODATA_COORD_Z))
                    cz=-DBL_MAX;
                else */cz=punts_z[k]; // Només ens quedem el primer.

                hMiraMonLayer->nCoordZ[k]=cz;
            }
            free_function(punts_z);
        }
        hMiraMonLayer->nNRing=1;
        hMiraMonLayer->nNVrtRing[0]=pArcHeader[i_elem].nElemCount;
        return 0;
    }
    #endif // TODO
#ifdef QUAN_OBRI_POLIGONS
    pFArc=hMiraMonLayer->MMPolygon.MMArc.pF;
    pF=hMiraMonLayer->MMPolygon.pF;
    pPolHeader=hMiraMonLayer->MMPolygon.pPolHeader;
    pArcHeader=hMiraMonLayer->MMPolygon.MMArc.pArcHeader;

    // Ara he llegir la capçalera de l'arc i llavors l'offset del primer vèrtex, llegirlos i afegirlos
    // a la llista que anirà al final.
    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->nCoordZ)
    {
        pZDescription=hMiraMonLayer->MMPolygon.MMArc.pZSection.pZDescription;

        LlegeixPoliPoligon3D_POL(&coord, (MM_N_VERTICES_TYPE **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)&hMiraMonLayer->nNRing, NULL, &z, NULL, NULL,
            NULL, NULL, pF, pFArc,
            pArcHeader, pZDescription, pPolHeader, i_elem, FALSE, flag_z, &var_pols);
    }
    else
    {
        LlegeixPoliPoligon3D_POL(&coord, (MM_N_VERTICES_TYPE **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)&hMiraMonLayer->nNRing, NULL,	NULL, NULL, NULL,
            NULL, NULL, pF, pFArc,
            pArcHeader, NULL, pPolHeader, i_elem, FALSE, flag_z, &var_pols);
    }

    for(i_ring=0, i_coord_acumulat=0; i_ring<hMiraMonLayer->nNRing; i_ring++)
    {
        for(k=0; k<hMiraMonLayer->nNVrtRing[i_ring]; k++, i_coord_acumulat++)
        {
            hMiraMonLayer->nCoordX[i_coord_acumulat]=coord[i_coord_acumulat].dfX;
            hMiraMonLayer->nCoordY[i_coord_acumulat]=coord[i_coord_acumulat].dfY;
            if (hMiraMonLayer->nCoordZ)
            {
	            if(hMiraMonLayer->TopHeader.bIs3d && z)
    	        	hMiraMonLayer->nCoordZ[i_coord_acumulat]=z[i_coord_acumulat];
        	    else
            		hMiraMonLayer->nCoordZ[i_coord_acumulat]=MM_NODATA_COORD_Z;
            }
        }
        hMiraMonLayer->nNVrtRing[i_ring]=n_vrt_ring_interna[i_ring];
    }

    if(hMiraMonLayer->TopHeader.bIs3d && hMiraMonLayer->nCoordZ)
    {
        AlliberaIAnullaPoliPoligon3D_POL(&coord, (MM_N_VERTICES_TYPE **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)&hMiraMonLayer->nNRing,	&z, NULL, NULL,
            NULL, NULL, &var_pols);
    }
    else
    {
        AlliberaIAnullaPoliPoligon3D_POL(&coord, (MM_N_VERTICES_TYPE **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)&hMiraMonLayer->nNRing,	NULL, NULL, NULL,
            NULL, NULL, &var_pols);
    }
#endif //QUAN_OBRI_POLIGONS
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
