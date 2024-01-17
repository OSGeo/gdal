#ifndef __MMRDLAYR_H
#define __MMRDLAYR_H

#include <windows.h>
#ifndef GDAL_COMPILATION
#include "mm_gdal\mm_gdal_driver_structs.h"
#else
//#include "ogr_api.h"    // For CPL_C_START
#include "mm_gdal_driver_structs.h"
CPL_C_START // Necessary for compiling in GDAL project
#endif

int MMInitLayerToRead(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *m_fp, 
                      const char *pszFilename);


int MMGetFeatureFromVector(struct MiraMonVectLayerInfo *hMiraMonLayer, MM_INTERNAL_FID i_elem);
int MM_ReadExtendedDBFHeaderFromFile(const char * szFileName, struct MM_BASE_DADES_XP *pMMBDXP, const char * pszRelFile);
int MM_ReadExtendedDBFHeader(struct MiraMonVectLayerInfo *hMiraMonLayer);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MMRDLAYR_H
