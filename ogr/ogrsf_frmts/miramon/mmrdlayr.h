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

int MMInitLayerToRead(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, 
                      const char *pszFilename);


// Les de sota, fora?
MM_TIPUS_ERROR MMRecuperaUltimError(void);

#define MM32DLL_PNT 	0
#define MM32DLL_NOD 	1
#define MM32DLL_ARC 	2
#define MM32DLL_POL 	3

int MMGetFeatureFromVector(struct MiraMonVectLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem);
int MM_ReadExtendedDBFHeaderFromFile(struct MM_BASE_DADES_XP *pMMBDXP, char * pszRelFile);
int MM_ReadExtendedDBFHeader(struct MiraMonVectLayerInfo *hMiraMonLayer);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MMRDLAYR_H
