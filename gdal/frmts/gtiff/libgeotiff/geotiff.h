/**********************************************************************
 *
 *  geotiff.h - Public interface for Geotiff tag parsing.
 *
 *   Written By: Niles D. Ritter
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 **********************************************************************/

#ifndef LIBGEOTIFF_GEOTIFF_H_
#define LIBGEOTIFF_GEOTIFF_H_

/**
 * \file geotiff.h
 *
 * Primary libgeotiff include file.
 *
 * This is the de facto registry for valid GeoTIFF GeoKeys
 * and their associated symbolic values. This is also the only file
 * of the GeoTIFF library which needs to be included in client source
 * code.
 */

/* This Version code should only change if a drastic
 * alteration is made to the GeoTIFF key structure. Readers
 * encountering a larger value should give up gracefully.
 */
#define GvCurrentVersion   1

#define LIBGEOTIFF_VERSION 1430

#include "geo_config.h"
#include "geokeys.h"

/**********************************************************************
 * Do we want to build as a DLL on windows?
 **********************************************************************/
#if !defined(GTIF_DLL)
#  if defined(_WIN32) && defined(BUILD_AS_DLL)
#    define GTIF_DLL     __declspec(dllexport)
#  else
#    define GTIF_DLL
#  endif
#endif

/**********************************************************************
 *
 *                 Public Structures & Definitions
 *
 **********************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct gtiff GTIF;   /* struct gtiff is private */
typedef struct _TIFFMethod TIFFMethod;
typedef unsigned short tifftag_t;
typedef unsigned short geocode_t;
typedef int (*GTIFPrintMethod)(char *string, void *aux);
typedef int (*GTIFReadMethod)(char *string, void *aux); // string 1024+ in size

typedef enum {
   TYPE_BYTE=1,
   TYPE_SHORT=2,
   TYPE_LONG=3,
   TYPE_RATIONAL=4,
   TYPE_ASCII=5,
   TYPE_FLOAT=6,
   TYPE_DOUBLE=7,
   TYPE_SBYTE=8,
   TYPE_SSHORT=9,
   TYPE_SLONG=10,
   TYPE_UNKNOWN=11
} tagtype_t;

#define LIBGEOTIFF_WARNING 0
#define LIBGEOTIFF_ERROR   1

#ifndef GTIF_PRINT_FUNC_FORMAT
#if defined(__GNUC__) && __GNUC__ >= 3
#define GTIF_PRINT_FUNC_FORMAT( format_idx, arg_idx )  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
#define GTIF_PRINT_FUNC_FORMAT( format_idx, arg_idx )
#endif
#endif

#ifndef GTERRORCALLBACK_DEFINED
#define GTERRORCALLBACK_DEFINED
/* Defined in both geotiff.h and geo_kep.h */
typedef void (*GTErrorCallback) (struct gtiff*,
                                 int level,
                                 const char* msg, ...) GTIF_PRINT_FUNC_FORMAT(3,4);
#endif

/**********************************************************************
 *
 *                 Public Function Declarations
 *
 **********************************************************************/

/* TIFF-level interface */
GTIF GTIF_DLL *GTIFNew(void *tif);
GTIF GTIF_DLL *GTIFNewEx(void *tif,
                         GTErrorCallback error_callback, void* user_data);
GTIF GTIF_DLL *GTIFNewSimpleTags(void *tif);
GTIF GTIF_DLL *GTIFNewWithMethods(void *tif, TIFFMethod*);
GTIF GTIF_DLL *GTIFNewWithMethodsEx(void *tif, TIFFMethod* methods,
                                    GTErrorCallback error_callback,
                                    void* user_data);
void GTIF_DLL  GTIFFree(GTIF *gtif);
int  GTIF_DLL  GTIFWriteKeys(GTIF *gtif);
void GTIF_DLL  GTIFDirectoryInfo(GTIF *gtif, int *versions, int *keycount);
void GTIF_DLL *GTIFGetUserData(GTIF *gtif);

/* GeoKey Access */
int  GTIF_DLL  GTIFKeyInfo(GTIF *gtif, geokey_t key, int *size, tagtype_t* type);
int  GTIF_DLL  GTIFKeyGet(GTIF *gtif, geokey_t key, void *val, int index,
                         int count);
int  GTIF_DLL  GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type,
                         int count,...);

/* Metadata Import-Export utilities */
void  GTIF_DLL  GTIFPrint(GTIF *gtif, GTIFPrintMethod print, void *aux);
int   GTIF_DLL  GTIFImport(GTIF *gtif, GTIFReadMethod scan, void *aux);
char  GTIF_DLL *GTIFKeyName(geokey_t key);
char  GTIF_DLL *GTIFValueName(geokey_t key,int value);
char  GTIF_DLL *GTIFTypeName(tagtype_t type);
char  GTIF_DLL *GTIFTagName(int tag);
int   GTIF_DLL  GTIFKeyCode(char * key);
int   GTIF_DLL  GTIFValueCode(geokey_t key,char *value);
int   GTIF_DLL  GTIFTypeCode(char *type);
int   GTIF_DLL  GTIFTagCode(char *tag);

/* Translation between image/PCS space */

int GTIF_DLL    GTIFImageToPCS( GTIF *gtif, double *x, double *y );
int GTIF_DLL    GTIFPCSToImage( GTIF *gtif, double *x, double *y );

#if defined(__cplusplus)
}
#endif

#endif /* LIBGEOTIFF_GEOTIFF_H_ */
