#ifndef __MMRDLAYR_H
#define __MMRDLAYR_H

#include <windows.h>
#ifndef GDAL_COMPILATION
#include "mm_gdal\mm_wrlayr.h"
#include "mm_gdal\mm_gdal_driver_structs.h"
#else
#include "ogr_api.h"    // For CPL_C_START
#include "mm_wrlayr.h"
#include "mm_gdal_driver_structs.h"
CPL_C_START // Necessary for compiling in GDAL project
#endif

typedef void * MM_HANDLE_CAPA_VECTOR;
typedef MM_INTERNAL_FID MM_TIPUS_I_ELEM_CAPA_VECTOR;
typedef unsigned long int MM_TIPUS_I_ANELL_CAPA_VECTOR;
typedef double MM_TIPUS_ENV_CAPA_VECTOR;
typedef int MM_TIPUS_ERROR;
typedef int MM_TIPUS_TIPUS_FITXER;
typedef MM_BOOLEAN MM_TIPUS_BOLEA;

int MMInitLayerToRead(struct MiraMonLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, 
                      const char *pszFilename);


// Les de sota, fora?
MM_TIPUS_ERROR MMRecuperaUltimError(void);
MM_HANDLE_CAPA_VECTOR MMIniciaCapaVector(const char *nom_fitxer);

MM_TIPUS_I_ELEM_CAPA_VECTOR MMRecuperaNElemCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_I_ANELL_CAPA_VECTOR MMRecuperMaxNAnellsCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_N_VERTICES_TYPE MMRecuperaMaxNCoordCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);

#define MM32DLL_PNT 	0
#define MM32DLL_NOD 	1
#define MM32DLL_ARC 	2
#define MM32DLL_POL 	3
MM_TIPUS_TIPUS_FITXER MMTipusFitxerCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_BOLEA MMEs3DCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);


int MMGetFeatureFromVector(struct MiraMonLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem);

void MMFinalitzaCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MMRDLAYR_H