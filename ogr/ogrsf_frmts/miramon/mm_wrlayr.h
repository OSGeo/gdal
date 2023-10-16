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
    #define calloc_function(a) MM_calloc((a))
    #define realloc_function MM_realloc
    #define free_function MM_free
    #define fopen_function(f,a) fopen_64((f),(a))
    #define fflush_function fflush_64
    #define fclose_function(f) fclose_64((f))
    #define ftell_function(f) ftell_64((f))
    #define fwrite_function(p,s,r,f) fwrite_64((p),(s),(r),(f))
    #define fread_function(p,s,r,f) fread_64((p),(s),(r),(f))
    #define fseek_function(f,s,g) fseek_64((f),(s),(g))
    #define TruncateFile_function(a,b) TruncaFitxer_64((a),(b))
    #define strdup_function(p)  strdup((p))
    #define get_filename_function TreuAdreca
    #define error_message_function puts
    #define info_message_function puts
    #define printf_function fprintf_64
    #define max_function max
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
#else
    #define calloc_function(a) CPLCalloc(1,(a))
    #define realloc_function CPLRealloc
    #define free_function CPLFree
    #define fopen_function(f,a) VSIFOpenL((f),(a))
    #define fflush_function VSIFFlushL
    #define fclose_function(f) VSIFCloseL((f))
    #define ftell_function(f) VSIFTellL((f))
    #define fwrite_function(p,s,r,f) VSIFWriteL((p),(s),(r),(f))
    #define fread_function(p,s,r,f) VSIFReadL((p),(s),(r),(f))
    #define fseek_function(f,s,g) VSIFSeekL((f),(s),(g))
    #define TruncateFile_function(a,b) VSIFTruncateL((a),(b))
    #define strdup_function(p)  CPLStrdup((p))
    #define get_filename_function CPLGetFilename
    #define error_message_function puts //CPLError
    #define info_message_function puts
    #define printf_function VSIFPrintfL
    #define max_function MAX
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
#endif

/* -------------------------------------------------------------------- */
/*      Functions                                                       */
/* -------------------------------------------------------------------- */
// Layer functions
struct MiraMonLayerInfo * MMCreateLayer(char *pzFileName, 
            __int32 LayerVersion, 
            struct MiraMonDataBase *attributes);
int MMInitLayer(struct MiraMonLayerInfo *hMiraMonLayer, 
                const char *pzFileName, 
                __int32 LayerVersion, 
                struct MiraMonDataBase *pLayerDB);
int MMInitLayerByType(struct MiraMonLayerInfo *hMiraMonLayer);
int MMFreeLayer(struct MiraMonLayerInfo *hMiraMonLayer);
int MMCloseLayer(struct MiraMonLayerInfo *hMiraMonLayer);
int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader);
int MMWriteEmptyHeader(FILE_TYPE *pF, int layerType, int nVersion);

// Feature functions
int MMInitFeature(struct MiraMonFeature *MMFeature);
void MMResetFeature(struct MiraMonFeature *MMFeature);
void MMDestroyFeature(struct MiraMonFeature *MMFeature);
int AddMMFeature(struct MiraMonLayerInfo *hMiraMonLayer, 
            struct MiraMonFeature *hMiraMonFeature);
int MMGetVectorVersion(struct MM_TH *pTopHeader);

// Tool functions

// In order to be efficient we reserve space to reuse it every
// time a feature is added.
int MMResizeMiraMonFieldValue(struct MiraMonFieldValue **pFieldValue, 
                        unsigned __int32 *nMax, 
                        unsigned __int32 nNum, 
                        unsigned __int32 nIncr,
                        unsigned __int32 nProposedMax);

int MMResizeMiraMonRecord(struct MiraMonRecord **pMiraMonRecord, 
                        unsigned __int32 *nMax, 
                        unsigned __int32 nNum, 
                        unsigned __int32 nIncr,
                        unsigned __int32 nProposedMax);

int MMResizeUI64Pointer(unsigned __int64 **pUI64, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int MMResizeIntPointer(int **pInt, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int MMResizeMM_POINT2DPointer(struct MM_POINT_2D **pPoint2D, 
                        MM_TIPUS_N_VERTEXS *nMax, 
                        MM_TIPUS_N_VERTEXS nNum, 
                        MM_TIPUS_N_VERTEXS nIncr,
                        MM_TIPUS_N_VERTEXS nProposedMax);

int MMResizeDoublePointer(double **pDouble, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int IsEmptyString(const char *string);
char *MMGetNFieldValue(const char *pszStringList, unsigned __int32 nIRecord);
// Metadata functions
int MMWriteVectorMetadata(struct MiraMonLayerInfo *hMiraMonLayer);

#ifdef GDAL_COMPILATION
CPL_C_END // Necessary for compiling in GDAL project
#endif
#endif //__MM_WRLAYR_H
