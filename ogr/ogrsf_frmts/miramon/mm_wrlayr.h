#ifndef __MM_WRLAYR_H
#define __MM_WRLAYR_H

/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */

#include "mm_gdal_driver_structs.h"
#ifndef GDAL_COMPILATION
#include "gdalmmf.h"  // For PTR_MM_CPLRecode, ptr_MM_CPLRecode(), ...
#else
#include "ogr_api.h"  // For OGRLayerH
CPL_C_START  // Necessary for compiling in GDAL project
#endif

#ifndef GDAL_COMPILATION
#include "memo.h"
#include "F64_str.h"   //For FILE_64
#include "FILE_64.h"   // Per a fseek_64(),...
#include "bd_xp.h"     //For MAX_LON_CAMP_DBF
#include "deftoler.h"  // For QUASI_IGUAL
//#include "LbTopStr.h" // For struct GEOMETRIC_I_TOPOLOGIC_POL
//#include "str_snyd.h" // For struct SNY_TRANSFORMADOR_GEODESIA
#include "nomsfitx.h"  // Per a CanviaExtensio()
#include "fitxers.h"   // Per a removeAO()
#include "cadenes.h"   // Per a EsCadenaDeBlancs()
#define calloc_function(a) MM_calloc((a))
#define realloc_function MM_realloc
#define free_function(a) MM_free((a))
#define fopen_function(f, a) fopenAO_64((f), (a))
#define fflush_function fflush_64
#define fclose_function(f) fclose_64((f))
#define ftell_function(f) ftell_64((f))
#define fwrite_function(p, s, r, f) fwrite_64((p), (s), (r), (f))
#define fread_function(p, s, r, f) fread_64((p), (s), (r), (f))
#define fseek_function(f, s, g) fseek_64((f), (s), (g))
#define fgets_function(f, s, g) fgets_64((f), (s), (g))
#define TruncateFile_function(a, b) TruncaFitxer_64((a), (b))
#define strdup_function(p) strdup((p))
#define get_filename_function TreuAdreca
#define get_path_function DonaAdreca
#define fprintf_function fprintf_64
#define max_function(a, b) max((a), (b))
#define get_extension_function(a) extensio(a)
#define reset_extension(a, b) CanviaExtensio((a), (b))
#define remove_function(a) removeAO((a))
#define OGR_F_GetFieldAsString_function(a, b)                                  \
    ptr_MM_OGR_F_GetFieldAsString((a), (b))
#define OGR_F_Destroy_function(a) ptr_MM_OGR_F_Destroy((a))
#define GDALClose_function(a) ptr_MM_GDALClose((a))
#define OGR_Fld_GetNameRef_function(a) ptr_MM_OGR_Fld_GetNameRef((a))
#define OGR_FD_GetFieldDefn_function(a, b) ptr_MM_OGR_FD_GetFieldDefn((a), (b))
#define GDALOpenEx_function(a, b, c, d, e)                                     \
    ptr_MM_GDALOpenEx((a), (b), (c), (d), (e))
#define OGR_FD_GetFieldCount_function(a) ptr_MM_OGR_FD_GetFieldCount((a))
#define OGR_L_GetLayerDefn_function(a) ptr_MM_OGR_L_GetLayerDefn((a))
#define OGR_L_GetNextFeature_function(a) ptr_MM_OGR_L_GetNextFeature((a))
#define OGR_L_ResetReading_function(a) ptr_MM_OGR_L_ResetReading((a))
#define GDALDatasetGetLayer_function(a, b) ptr_MM_GDALDatasetGetLayer((a), (b))
#define CPLRecode_function(a, b, c) ptr_MM_CPLRecode((a), (b), (c))
#define CPLFree_function(a) ptr_MM_CPLFree((a))
#define form_filename_function(a, b) MuntaPath((a), (b), TRUE)
#define MM_CPLGetBasename(a) TreuAdreca((a))
#define MM_IsNANDouble(x) EsNANDouble((x))
#define MM_IsDoubleInfinite(x) EsDoubleInfinit((x))
#else
#define calloc_function(a) VSICalloc(1, (a))
#define realloc_function VSIRealloc
#define free_function(a) VSIFree((a))
#define fopen_function(f, a) VSIFOpenL((f), (a))
#define fflush_function VSIFFlushL
#define fclose_function(f) VSIFCloseL((f))
#define ftell_function(f) VSIFTellL((f))
#define fwrite_function(p, s, r, f) VSIFWriteL((p), (s), (r), (f))
#define fread_function(p, s, r, f) VSIFReadL((p), (s), (r), (f))
#define fseek_function(f, s, g) VSIFSeekL((f), (s), (g))
#define TruncateFile_function(a, b) VSIFTruncateL((a), (b))
#define strdup_function(p) CPLStrdup((p))
#define get_filename_function CPLGetFilename
#define get_path_function CPLGetPath
#define fprintf_function VSIFPrintfL
#define max_function(a, b) MAX((a), (b))
#define get_extension_function(a) CPLGetExtension((a))
#define reset_extension(a, b) CPLResetExtension((a), (b))
#define remove_function(a) VSIUnlink((a))
#define OGR_F_GetFieldAsString_function(a, b) OGR_F_GetFieldAsString((a), (b))
#define OGR_F_Destroy_function(a) OGR_F_Destroy((a))
#define GDALClose_function(a) GDALClose((a))
#define OGR_Fld_GetNameRef_function(a) OGR_Fld_GetNameRef((a))
#define OGR_FD_GetFieldDefn_function(a, b) OGR_FD_GetFieldDefn((a), (b))
#define GDALOpenEx_function(a, b, c, d, e) GDALOpenEx((a), (b), (c), (d), (e))
#define OGR_FD_GetFieldCount_function(a) OGR_FD_GetFieldCount((a))
#define OGR_L_GetLayerDefn_function(a) OGR_L_GetLayerDefn((a))
#define OGR_L_GetNextFeature_function(a) OGR_L_GetNextFeature((a))
#define OGR_L_ResetReading_function(a) OGR_L_ResetReading((a))
#define GDALDatasetGetLayer_function(a, b) GDALDatasetGetLayer((a), (b))
#define CPLRecode_function(a, b, c) CPLRecode((a), (b), (c))
#define CPLFree_function(a) CPLFree((a))
#define form_filename_function(a, b) CPLFormFilename((a), (b), "")
#define MM_CPLGetBasename(a) CPLGetBasename((a))
#define MM_IsNANDouble(x) CPLIsNan((x))
#define MM_IsDoubleInfinite(x) CPLIsInf((x))
#endif

/* -------------------------------------------------------------------- */
/*      Functions                                                       */
/* -------------------------------------------------------------------- */
// MM-GDAL functions
#ifdef GDAL_COMPILATION
#define MMCPLError CPLError
#define MMCPLWarning CPLError
#define MMCPLDebug CPLDebugOnly
#else
    void
    MMCPLError(int code, const char *fmt, ...);
void MMCPLWarning(int code, const char *fmt, ...);
void MMCPLDebug(int code, const char *fmt, ...);
#endif

// Layer functions
int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer,
                const char *pzFileName, int LayerVersion, char nMMRecode,
                char nMMLanguage, struct MiraMonDataBase *pLayerDB,
                MM_BOOLEAN ReadOrWrite, struct MiraMonVectMapInfo *MMMap);
int MMInitLayerByType(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMDestroyLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader);
int MMReadAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadPHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer,
                              FILE_TYPE *pF, MM_INTERNAL_FID nElements,
                              struct MM_ZSection *pZSection);
int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer, FILE_TYPE *pF,
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
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, GUInt64 nBlockSize,
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
char *MMReturnValueFromSectionINIFile(const char *filename, const char *section,
                                      const char *key);

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
int MMReturnCodeFromMM_m_idofic(char *pMMSRS_or_pSRS, char *result,
                                MM_BYTE direction);

#define EPSG_FROM_MMSRS 0
#define MMSRS_FROM_EPSG 1
#define ReturnEPSGCodeSRSFromMMIDSRS(pMMSRS, szResult)                         \
    MMReturnCodeFromMM_m_idofic((pMMSRS), (szResult), EPSG_FROM_MMSRS)
#define ReturnMMIDSRSFromEPSGCodeSRS(pSRS, szResult)                           \
    MMReturnCodeFromMM_m_idofic((pSRS), (szResult), MMSRS_FROM_EPSG)

int MMWriteVectorMetadata(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMCheck_REL_FILE(const char *szREL_file);

#ifdef GDAL_COMPILATION
CPL_C_END  // Necessary for compiling in GDAL project
#endif
#endif  //__MM_WRLAYR_H
