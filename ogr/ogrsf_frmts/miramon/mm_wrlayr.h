#ifndef __MM_WRLAYR_H
#define __MM_WRLAYR_H

/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */

#include "mm_gdal_driver_structs.h"
CPL_C_START  // Necessary for compiling in GDAL project

    bool
    MM_IsNANDouble(double x);
bool MM_IsDoubleInfinite(double x);

/* -------------------------------------------------------------------- */
/*      Functions                                                       */
/* -------------------------------------------------------------------- */

// Layer functions
int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                const char *pzFileName, int LayerVersion, char nMMRecode,
                char nMMLanguage, struct MiraMonDataBase *pLayerDB,
                MM_BOOLEAN ReadOrWrite, struct MiraMonVectMapInfo *MMMap);
int MMInitLayerByType(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMDestroyLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadHeader(VSILFILE *pF, struct MM_TH *pMMHeader);
int MMReadAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadPHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              VSILFILE *pF, MM_INTERNAL_FID nElements,
                              struct MM_ZSection *pZSection);
int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer, VSILFILE *pF,
                   struct MM_ZSection *pZSection);

// Feature functions
int MMInitFeature(struct MiraMonFeature *MMFeature);
void MMResetFeatureGeometry(struct MiraMonFeature *MMFeature);
void MMResetFeatureRecord(struct MiraMonFeature *hMMFeature);
void MMDestroyFeature(struct MiraMonFeature *MMFeature);
int MMAddFeature(struct MiraMonVectLayerInfo *hMiraMonLayer,
                 struct MiraMonFeature *hMiraMonFeature);
int MMCheckSize_t(GUInt64 nCount, GUInt64 nSize);
int MMGetVectorVersion(struct MM_TH *pTopHeader);
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, VSILFILE *pF, GUInt64 nBlockSize,
                char **pBuffer, MM_FILE_OFFSET DiskOffsetWhereToFlush,
                GInt32 nMyDiskSize);
int MMReadFlush(struct MM_FLUSH_INFO *pFlush);
int MMReadBlockFromBuffer(struct MM_FLUSH_INFO *FlushInfo);
int MMReadGUInt64DependingOnVersion(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                    struct MM_FLUSH_INFO *FlushInfo,
                                    GUInt64 *pnUI64);
int MMReadOffsetDependingOnVersion(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                   struct MM_FLUSH_INFO *FlushInfo,
                                   MM_FILE_OFFSET *nUI64);

// Tool functions

// In order to be efficient we reserve space to reuse it every
// time a feature is added.
int MMResizeMiraMonFieldValue(struct MiraMonFieldValue **pFieldValue,
                              MM_EXT_DBF_N_FIELDS *nMax,
                              MM_EXT_DBF_N_FIELDS nNum,
                              MM_EXT_DBF_N_FIELDS nIncr,
                              MM_EXT_DBF_N_FIELDS nProposedMax);

int MMResizeMiraMonPolygonArcs(struct MM_PAL_MEM **pFID,
                               MM_POLYGON_ARCS_COUNT *nMax,
                               MM_POLYGON_ARCS_COUNT nNum,
                               MM_POLYGON_ARCS_COUNT nIncr,
                               MM_POLYGON_ARCS_COUNT nProposedMax);

int MMResizeMiraMonRecord(struct MiraMonRecord **pMiraMonRecord,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS *nMax,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nNum,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nIncr,
                          MM_EXT_DBF_N_MULTIPLE_RECORDS nProposedMax);

int MMResize_MM_N_VERTICES_TYPE_Pointer(MM_N_VERTICES_TYPE **pUI64,
                                        MM_N_VERTICES_TYPE *nMax,
                                        MM_N_VERTICES_TYPE nNum,
                                        MM_N_VERTICES_TYPE nIncr,
                                        MM_N_VERTICES_TYPE nProposedMax);

int MMResizeVFGPointer(char **pInt, MM_INTERNAL_FID *nMax, MM_INTERNAL_FID nNum,
                       MM_INTERNAL_FID nIncr, MM_INTERNAL_FID nProposedMax);

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D,
                              MM_N_VERTICES_TYPE *nMax, MM_N_VERTICES_TYPE nNum,
                              MM_N_VERTICES_TYPE nIncr,
                              MM_N_VERTICES_TYPE nProposedMax);

int MMResizeDoublePointer(MM_COORD_TYPE **pDouble, MM_N_VERTICES_TYPE *nMax,
                          MM_N_VERTICES_TYPE nNum, MM_N_VERTICES_TYPE nIncr,
                          MM_N_VERTICES_TYPE nProposedMax);
int MMResizeStringToOperateIfNeeded(struct MiraMonVectLayerInfo *hMiraMonLayer,
                                    MM_EXT_DBF_N_FIELDS nNewSize);
int MMIsEmptyString(const char *string);
// Metadata functions

int MMWriteVectorMetadata(struct MiraMonVectLayerInfo *hMiraMonLayer);

CPL_C_END  // Necessary for compiling in GDAL project
#endif     //__MM_WRLAYR_H
