#ifndef __MMWRLAYR_H
#define __MMWRLAYR_H

/* -------------------------------------------------------------------- */
/*      Necessary functions to read/write a MiraMon Vector File         */
/* -------------------------------------------------------------------- */

#include "ogr_api.h"    // For OGRLayerH
#include "mmStruct.h"

CPL_C_START // Necessary for compiling in GDAL project
#ifdef APLICACIO_MM32
    #include "memo.h"
    #include "F64_str.h"	//For FILE_64
    #include "FILE_64.h"	// Per a fseek_64(),...
    #include "bd_xp.h"	    //For MAX_LON_CAMP_DBF
    #include "deftoler.h"   // For QUASI_IGUAL
    //#include "LbTopStr.h"	// For struct GEOMETRIC_I_TOPOLOGIC_POL
    //#include "str_snyd.h"   // For struct SNY_TRANSFORMADOR_GEODESIA
    #include "nomsfitx.h"   // Per a CanviaExtensio()
    #include "fitxers.h"    // Per a removeAO()
    #define calloc_function(a) MM_calloc(a)
    #define realloc_function MM_realloc
    #define free_function MM_free
    #define fopen_function(f,a) fopen_64(f,a)
    #define fclose_function(f) fclose_64(f)
    #define ftell_function(f) ftell_64(f)
    #define fwrite_function(p,s,r,f) fwrite_64(p,s,r,f)
    #define fread_function(p,s,r,f) fread_64(p,s,r,f)
    #define fseek_function(f,s,g) fseek_64(f,s,g)
    #define strdup_function(p)  strdup(p)
    #define error_message_function puts
    #define info_message_function puts
    #define printf_function fprintf_64
    #define max_function max
    #define ALMOST_THE_SAME(x1,x2,tol) QUASI_IGUAL((x1), (x2), (tol))
    #define DOUBLES_GIVE_INFINITESIMAL(a,b) DOUBLES_DONA_INFINITESIM((a),(b))
    #define reset_extension(a,b)    CanviaExtensio((a),(b))
    #define remove_function(a)  removeAO((a))

#else
    #define calloc_function(a) CPLCalloc(1,a)
    #define realloc_function CPLRealloc
    #define free_function CPLFree
    #define fopen_function(f,a) VSIFOpenL(f,a)
    #define fclose_function(f) VSIFCloseL(f)
    #define ftell_function(f) VSIFTellL(f)
    #define fwrite_function(p,s,r,f) VSIFWriteL(p,s,r,f)
    #define fread_function(p,s,r,f) VSIFReadL(p,s,r,f)
    #define fseek_function(f,s,g) VSIFSeekL(f,s,g)
    #define strdup_function(p)  CPLStrdup(p)
    #define error_message_function puts //CPLError
    #define info_message_function puts
    #define printf_function VSIFPrintfL
    #define max_function MAX
    #define ALMOST_THE_SAME(x1, x2, tol) ( ( ((x1)-(x2))<=(tol) ) && ( (-(tol))<=((x1)-(x2)) ) )
    #define DOUBLES_GIVE_INFINITESIMAL(a,b) (max_function(fabs(a), fabs(b))*DBL_EPSILON*TOLERANCE_DIFFERENT_DOUBLES)
    #define reset_extension(a,b)    CPLResetExtension((a),(b))
    #define remove_function(a)  VSIUnlink((a))
#endif

/* -------------------------------------------------------------------- */
/*      Functions                                                       */
/* -------------------------------------------------------------------- */
// Layer functions
struct MiraMonLayerInfo * MMCreateLayer(char *pzFileName, __int32 LayerVersion, 
            int eLT, unsigned __int64 nElemCount, struct MiraMonDataBase *attributes);
int MMInitLayer(struct MiraMonLayerInfo *hMiraMonLayer, const char *pzFileName, __int32 LayerVersion, int eLT,
                unsigned __int64 nElemCount, struct MiraMonDataBase *attributes);
int MMFreeLayer(struct MiraMonLayerInfo *hMiraMonLayer);
int MMCloseLayer(struct MiraMonLayerInfo *hMiraMonLayer);
int MMReadHeader(FILE_TYPE *pF, struct MM_TH *pMMHeader);
int MMWriteEmptyHeader(FILE_TYPE *pF, int layerType, int nVersion);

// Feature functions
void MMInitFeature(struct MiraMonFeature *MMFeature);
void MMResetFeature(struct MiraMonFeature *MMFeature);
void MMDestroyFeature(struct MiraMonFeature *MMFeature);
int AddMMFeature(struct MiraMonLayerInfo *hMiraMonLayer, struct MiraMonFeature *hMiraMonFeature);
int MMGetVectorVersion(struct MM_TH *pTopHeader);

// Tools functions

// In order to be efficient we reserve space to reuse it every
// time a feature is added.
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
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int MMResizeDoublePointer(double **pDouble, 
                        unsigned __int64 *nMax, 
                        unsigned __int64 nNum, 
                        unsigned __int64 nIncr,
                        unsigned __int64 nProposedMax);

int IsEmptyString(const char *string);

// Metadata functions
int MMReadVectorMetadataFromLayer(struct MiraMonMetaData *hMMMD, OGRLayerH layer, struct MiraMonLayerInfo *hMiraMonLayer);
int MMWriteMetadataFile(const char *szMDFileName, struct MiraMonMetaData *hMMMD, struct MiraMonDataBase *hMMDB);
void MMFreeMetaData(struct MiraMonMetaData *hMMMD);

CPL_C_END // Necessary for compiling in GDAL project
#endif //__MMWRLAYR_H
