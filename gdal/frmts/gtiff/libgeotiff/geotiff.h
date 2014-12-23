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

#ifndef __geotiff_h_
#define __geotiff_h_

/**
 * \file geotiff.h
 *
 * Primary libgeotiff include file.
 *
 * This is the defacto registry for valid GEOTIFF GeoKeys
 * and their associated symbolic values. This is also the only file
 * of the GeoTIFF library which needs to be included in client source
 * code.
 */

/* This Version code should only change if a drastic
 * alteration is made to the GeoTIFF key structure. Readers
 * encountering a larger value should give up gracefully.
 */
#define GvCurrentVersion   1

#define LIBGEOTIFF_VERSION 1410

#include "geo_config.h"
#include "geokeys.h"

/**********************************************************************
 * Do we want to build as a DLL on windows?
 **********************************************************************/
#if !defined(CPL_DLL)
#  if defined(_WIN32) && defined(BUILD_AS_DLL)
#    define CPL_DLL     __declspec(dllexport)
#  else
#    define CPL_DLL
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


/**********************************************************************
 *
 *                 Public Function Declarations
 *
 **********************************************************************/

/* TIFF-level interface */
GTIF CPL_DLL *GTIFNew(void *tif);
GTIF CPL_DLL *GTIFNewSimpleTags(void *tif);
GTIF CPL_DLL *GTIFNewWithMethods(void *tif, TIFFMethod*);
void CPL_DLL  GTIFFree(GTIF *gtif);
int  CPL_DLL  GTIFWriteKeys(GTIF *gtif);
void CPL_DLL  GTIFDirectoryInfo(GTIF *gtif, int *versions, int *keycount);

/* GeoKey Access */
int  CPL_DLL  GTIFKeyInfo(GTIF *gtif, geokey_t key, int *size, tagtype_t* type);
int  CPL_DLL  GTIFKeyGet(GTIF *gtif, geokey_t key, void *val, int index,
                         int count);
int  CPL_DLL  GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type,
                         int count,...);

/* Metadata Import-Export utilities */
void  CPL_DLL  GTIFPrint(GTIF *gtif, GTIFPrintMethod print, void *aux);
int   CPL_DLL  GTIFImport(GTIF *gtif, GTIFReadMethod scan, void *aux);
char  CPL_DLL *GTIFKeyName(geokey_t key);
char  CPL_DLL *GTIFValueName(geokey_t key,int value);
char  CPL_DLL *GTIFTypeName(tagtype_t type);
char  CPL_DLL *GTIFTagName(int tag);
int   CPL_DLL  GTIFKeyCode(char * key);
int   CPL_DLL  GTIFValueCode(geokey_t key,char *value);
int   CPL_DLL  GTIFTypeCode(char *type);
int   CPL_DLL  GTIFTagCode(char *tag);

/* Translation between image/PCS space */

int CPL_DLL    GTIFImageToPCS( GTIF *gtif, double *x, double *y );
int CPL_DLL    GTIFPCSToImage( GTIF *gtif, double *x, double *y );

#if defined(__cplusplus)
}
#endif

#endif /* __geotiff_h_ */
