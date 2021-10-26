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

/* Versions of GeoTIFF specification and corresponding
 * (version, key_revision, minor_revision) triplets.
 * At that time, only 2 versions are known: */

/* GEOTIFF_SPEC_1_0 corresponds to the original GeoTIFF specification,
 * "Revision 1.0, specification version 1.8.2 in November 1995 (N. Ritter & Ruth, 1995)"
 * available at http://geotiff.maptools.org/spec/geotiffhome.html */
#define GEOTIFF_SPEC_1_0_VERSION        1
#define GEOTIFF_SPEC_1_0_KEY_REVISION   1
#define GEOTIFF_SPEC_1_0_MINOR_REVISION 0

/* GEOTIFF_SPEC_1_1 corresponds to the OGC GeoTIFF standard 19-008 */
#define GEOTIFF_SPEC_1_1_VERSION        1
#define GEOTIFF_SPEC_1_1_KEY_REVISION   1
#define GEOTIFF_SPEC_1_1_MINOR_REVISION 1

/* Library version */
#define LIBGEOTIFF_VERSION 1700

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
/* versions must be an array of 3 int */
void GTIF_DLL  GTIFDirectoryInfo(GTIF *gtif, int *versions, int *keycount);
void GTIF_DLL *GTIFGetUserData(GTIF *gtif);
int  GTIF_DLL  GTIFSetVersionNumbers(GTIF* gtif,
                                     unsigned short version,
                                     unsigned short key_revision,
                                     unsigned short minor_revision);

/* GeoKey Access */
int  GTIF_DLL  GTIFKeyInfo(GTIF *gtif, geokey_t key, int *size, tagtype_t* type);
int  GTIF_DLL  GTIFKeyGet(GTIF *gtif, geokey_t key, void *val, int index,
                         int count);
int  GTIF_DLL  GTIFKeyGetASCII(GTIF *gtif, geokey_t key, char* szStr,
                               int szStrMaxLen);
int  GTIF_DLL  GTIFKeyGetSHORT(GTIF *gtif, geokey_t key, unsigned short *val, int index,
                               int count);
int  GTIF_DLL  GTIFKeyGetDOUBLE(GTIF *gtif, geokey_t key, double *val, int index,
                                int count);
int  GTIF_DLL  GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type,
                          int count,...);

/* Metadata Import-Export utilities */
void  GTIF_DLL  GTIFPrint(GTIF *gtif, GTIFPrintMethod print, void *aux);
int   GTIF_DLL  GTIFImport(GTIF *gtif, GTIFReadMethod scan, void *aux);
char  GTIF_DLL *GTIFKeyName(geokey_t key);
const char GTIF_DLL *GTIFKeyNameEx(GTIF* gtif, geokey_t key);
char  GTIF_DLL *GTIFValueName(geokey_t key,int value);
const char GTIF_DLL *GTIFValueNameEx(GTIF* gtif, geokey_t key,int value);
char  GTIF_DLL *GTIFTypeName(tagtype_t type);
char  GTIF_DLL *GTIFTagName(int tag);
int   GTIF_DLL  GTIFKeyCode(const char * key);
int   GTIF_DLL  GTIFValueCode(geokey_t key,const char *value);
int   GTIF_DLL  GTIFTypeCode(const char *type);
int   GTIF_DLL  GTIFTagCode(const char *tag);

/* Translation between image/PCS space */

int GTIF_DLL    GTIFImageToPCS( GTIF *gtif, double *x, double *y );
int GTIF_DLL    GTIFPCSToImage( GTIF *gtif, double *x, double *y );

#if defined(__cplusplus)
}
#endif

#endif /* LIBGEOTIFF_GEOTIFF_H_ */
