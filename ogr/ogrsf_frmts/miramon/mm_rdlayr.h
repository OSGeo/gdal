#ifndef __MMRDLAYR_H
#define __MMRDLAYR_H

#include "mm_gdal_driver_structs.h"
CPL_C_START  // Necessary for compiling in GDAL project

    int
    MMInitLayerToRead(struct MiraMonVectLayerInfo *hMiraMonLayer,
                      VSILFILE *m_fp, const char *pszFilename);

int MMGetGeoFeatureFromVector(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              MM_INTERNAL_FID i_elem);
int MM_ReadExtendedDBFHeader(struct MiraMonVectLayerInfo *hMiraMonLayer);

CPL_C_END  // Necessary for compiling in GDAL project
#endif     //__MMRDLAYR_H
