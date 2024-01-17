#ifndef __MM_WRLAYR_H
#define __MM_WRLAYR_H

/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */

#include "ogr_api.h"    // For OGRLayerH
#include "mm_gdal_driver_structs.h"
#ifndef GDAL_COMPILATION
#include "gdalmm.h"     // For PTR_MM_OGR_F_GetFieldAsString()
#else
CPL_C_START // Necessary for compiling in GDAL project
#endif

#ifndef GDAL_COMPILATION
    #include "memo.h"
    #include "F64_str.h"	//For FILE_64
    #include "FILE_64.h"	// Per a fseek_64(),...
    #include "bd_xp.h"	    //For MAX_LON_CAMP_DBF
    #include "deftoler.h"   // For QUASI_IGUAL
    //#include "LbTopStr.h"	// For struct GEOMETRIC_I_TOPOLOGIC_POL
    //#include "str_snyd.h"   // For struct SNY_TRANSFORMADOR_GEODESIA
    #include "nomsfitx.h"   // Per a CanviaExtensio()
    #include "fitxers.h"    // Per a removeAO()
    #include "cadenes.h"	// Per a EsCadenaDeBlancs()
    #define calloc_function(a) MM_calloc((a))
    #define realloc_function MM_realloc
    #define free_function(a) MM_free((a))
    #define fopen_function(f,a) fopenAO_64((f),(a))
    #define fgets_function(a,b,c)   fgets_64((a),(b),(c))
    #define fflush_function fflush_64
    #define fclose_function(f) fclose_64((f))
    #define ftell_function(f) ftell_64((f))
    #define fwrite_function(p,s,r,f) fwrite_64((p),(s),(r),(f))
    #define fread_function(p,s,r,f) fread_64((p),(s),(r),(f))
    #define fseek_function(f,s,g) fseek_64((f),(s),(g))
    #define TruncateFile_function(a,b) TruncaFitxer_64((a),(b))
    #define strdup_function(p)  strdup((p))
    #define get_filename_function TreuAdreca
    #define get_path_function DonaAdreca
    #define printf_function fprintf_64
    #define max_function(a,b) max((a),(b))
    #define get_extension_function(a) extensio(a)
    #define is_empty_string_function(a) EsCadenaDeBlancs(a)
    #define reset_extension(a,b)    CanviaExtensio((a),(b))
    #define remove_function(a)  removeAO((a))
    #define OGR_F_GetFieldAsString_function(a,b) ptr_MM_OGR_F_GetFieldAsString((a),(b))
    #define OGR_F_Destroy_function(a)           ptr_MM_OGR_F_Destroy((a))
    #define GDALClose_function(a)               ptr_MM_GDALClose((a))
    #define OGR_Fld_GetNameRef_function(a)      ptr_MM_OGR_Fld_GetNameRef((a))
    #define OGR_FD_GetFieldDefn_function(a,b)   ptr_MM_OGR_FD_GetFieldDefn((a),(b))
    #define GDALOpenEx_function(a,b,c,d,e)      ptr_MM_GDALOpenEx((a),(b),(c),(d),(e))
    #define OGR_FD_GetFieldCount_function(a)    ptr_MM_OGR_FD_GetFieldCount((a))
    #define OGR_L_GetLayerDefn_function(a)      ptr_MM_OGR_L_GetLayerDefn((a))
    #define OGR_L_GetNextFeature_function(a)    ptr_MM_OGR_L_GetNextFeature((a))
    #define OGR_L_ResetReading_function(a)      ptr_MM_OGR_L_ResetReading((a))
    #define GDALDatasetGetLayer_function(a,b)   ptr_MM_GDALDatasetGetLayer((a),(b))
    #define form_filename_function(a,b)         MuntaPath((a),(b),TRUE)
#else
    #define calloc_function(a) CPLCalloc(1,(a))
    #define realloc_function CPLRealloc
    #define free_function(a) CPLFree((a))
    #define fopen_function(f,a) VSIFOpenL((f),(a))
    #define fgets_function(a,b,c)   VSIFGets((a),(b),(c))
    #define fflush_function VSIFFlushL
    #define fclose_function(f) VSIFCloseL((f))
    #define ftell_function(f) VSIFTellL((f))
    #define fwrite_function(p,s,r,f) VSIFWriteL((p),(s),(r),(f))
    #define fread_function(p,s,r,f) VSIFReadL((p),(s),(r),(f))
    #define fseek_function(f,s,g) VSIFSeekL((f),(s),(g))
    #define TruncateFile_function(a,b) VSIFTruncateL((a),(b))
    #define strdup_function(p)  CPLStrdup((p))
    #define get_filename_function CPLGetFilename
    #define get_path_function CPLGetPath
    #define printf_function VSIFPrintfL
    #define max_function(a,b) MAX((a),(b))
    #define get_extension_function(a) CPLGetExtension((a))
    #define is_empty_string_function(a) IsEmptyString((a))
    #define reset_extension(a,b)    CPLResetExtension((a),(b))
    #define remove_function(a)  VSIUnlink((a))
    #define OGR_F_GetFieldAsString_function(a,b) OGR_F_GetFieldAsString((a),(b))
    #define OGR_F_Destroy_function(a)  OGR_F_Destroy((a))
    #define GDALClose_function(a)      GDALClose((a))
    #define OGR_Fld_GetNameRef_function(a)  OGR_Fld_GetNameRef((a))
    #define OGR_FD_GetFieldDefn_function(a,b) OGR_FD_GetFieldDefn((a),(b))
    #define GDALOpenEx_function(a,b,c,d,e)     GDALOpenEx((a),(b),(c),(d),(e))
    #define OGR_FD_GetFieldCount_function(a)    OGR_FD_GetFieldCount((a))
    #define OGR_L_GetLayerDefn_function(a)      OGR_L_GetLayerDefn((a))
    #define OGR_L_GetNextFeature_function(a)    OGR_L_GetNextFeature((a))
    #define OGR_L_ResetReading_function(a)      OGR_L_ResetReading((a))
    #define GDALDatasetGetLayer_function(a,b)   GDALDatasetGetLayer((a),(b))
    #define form_filename_function(a,b)       CPLFormFilename((a),(b),NULL)
#endif

/* -------------------------------------------------------------------- */
/*      Functions                                                       */
/* -------------------------------------------------------------------- */
// MM-GDAL functions
void MM_CPLError(
    int level, int code,
    const char* format, ...);

void MM_CPLWarning(
    int level, int code,
    const char* format, ...);

// Layer functions
#ifndef GDAL_COMPILATION
struct MiraMonVectLayerInfo * MMCreateLayer(char *pzFileName, 
                __int32 LayerVersion, double nMMMemoryRatio,
                struct MiraMonDataBase *attributes,
                MM_BOOLEAN ReadOrWrite);
#endif
int MMInitLayer(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                const char *pzFileName, 
                __int32 LayerVersion, double nMMMemoryRatio,
                struct MiraMonDataBase *pLayerDB,
                MM_BOOLEAN ReadOrWrite);
int MMInitLayerByType(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMFreeLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMCloseLayer(struct MiraMonVectLayerInfo *hMiraMonLayer);
void MMDestroyLayer(struct MiraMonVectLayerInfo **hMiraMonLayer);
int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader);
int MMWriteEmptyHeader(FILE_TYPE *pF, int layerType, int nVersion);
int MMReadAHArcSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadPHPolygonSection(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MMReadZDescriptionHeaders(struct MiraMonVectLayerInfo *hMiraMonLayer, 
                        FILE_TYPE *pF, MM_INTERNAL_FID nElements, 
                        struct MM_ZSection *pZSection);
int MMReadZSection(struct MiraMonVectLayerInfo *hMiraMonLayer,
                   FILE_TYPE *pF, 
                   struct MM_ZSection *pZSection);

// Feature functions
int MMInitFeature(struct MiraMonFeature *MMFeature);
void MMResetFeature(struct MiraMonFeature *MMFeature);
void MMDestroyFeature(struct MiraMonFeature *MMFeature);
int AddMMFeature(struct MiraMonVectLayerInfo *hMiraMonLayer, 
            struct MiraMonFeature *hMiraMonFeature);
int MMGetVectorVersion(struct MM_TH *pTopHeader);
int MMInitFlush(struct MM_FLUSH_INFO *pFlush, FILE_TYPE *pF, 
            unsigned __int64 nBlockSize, char **pBuffer, 
            MM_FILE_OFFSET DiskOffsetWhereToFlush, 
            __int32 nMyDiskSize);
int MMReadFlush(struct MM_FLUSH_INFO *pFlush);
int MM_ReadBlockFromBuffer(struct MM_FLUSH_INFO *FlushInfo);
int MMReadIntegerDependingOnVersion(
                            struct MiraMonVectLayerInfo *hMiraMonLayer,
                            struct MM_FLUSH_INFO *FlushInfo, 
                                unsigned __int64 *nUI64);


// Tool functions
int MMResetExtensionAndLastLetter(char *pzNewLayerName, 
                                const char *pzOldLayerName, 
                                const char *MDExt);
const char * ReturnValueFromSectionINIFile(const char *filename, 
                                const char *section, const char *key);

// In order to be efficient we reserve space to reuse it every
// time a feature is added.
int MMResizeMiraMonFieldValue(struct MiraMonFieldValue **pFieldValue, 
                        unsigned __int32 *nMax, 
                        unsigned __int32 nNum, 
                        unsigned __int32 nIncr,
                        unsigned __int32 nProposedMax);

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
                        MM_POLYGON_RINGS_COUNT *nMax, 
                        MM_POLYGON_RINGS_COUNT nNum, 
                        MM_POLYGON_RINGS_COUNT nIncr,
                        MM_POLYGON_RINGS_COUNT nProposedMax);

int MMResizeIntPointer(int **pInt, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D, 
                        MM_N_VERTICES_TYPE *nMax, 
                        MM_N_VERTICES_TYPE nNum, 
                        MM_N_VERTICES_TYPE nIncr,
                        MM_N_VERTICES_TYPE nProposedMax);

int MMResizeDoublePointer(double **pDouble, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);
int MM_ResizeStringToOperateIfNeeded(struct MiraMonVectLayerInfo *hMiraMonLayer,
                        MM_EXT_DBF_N_FIELDS nNewSize);
int IsEmptyString(const char *string);
char *MMGetNFieldValue(const char *pszStringList, unsigned __int32 nIRecord);
// Metadata functions
int ReturnCodeFromMM_m_idofic(char* pMMSRS_or_pSRS, char * result, MM_BYTE direction);

#define EPSG_FROM_MMSRS 0
#define MMSRS_FROM_EPSG 1
#define ReturnEPSGCodeSRSFromMMIDSRS(pMMSRS,szResult) ReturnCodeFromMM_m_idofic((pMMSRS),(szResult),EPSG_FROM_MMSRS)
#define ReturnMMIDSRSFromEPSGCodeSRS(pSRS,szResult) ReturnCodeFromMM_m_idofic((pSRS),(szResult),MMSRS_FROM_EPSG)

int MMWriteVectorMetadata(struct MiraMonVectLayerInfo *hMiraMonLayer);
int MM_Check_REL_FILE(const char *szREL_file);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_WRLAYR_H
