/**********************************************************************
 *
 *  geotiff.h - Public interface for Geotiff tag parsing.
 *
 *   This is the defacto registry for valid GEOTIFF GeoKeys
 *   and their associated symbolic values. This is also the only file
 *   of the GeoTIFF library which needs to be included in client source
 *   code.
 *
 *   Written By: Niles D. Ritter
 *
 **********************************************************************/

#ifndef __geotiff_h_
#define __geotiff_h_


/* This Version code should only change if a drastic
 * alteration is made to the GeoTIFF key structure. Readers
 * encountering a larger value should give up gracefully.
 */
#define GvCurrentVersion   1

#include "geokeys.h"

/**********************************************************************
 *
 *                 Public Structures & Definitions
 *
 **********************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct gtiff GTIF;   /* struct gtiff is private */
typedef unsigned short tifftag_t;
typedef unsigned short geocode_t;
typedef int (*GTIFPrintMethod)(char *string, void *aux);
typedef int (*GTIFReadMethod)(char *string, void *aux);

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
GTIF*     GTIFNew(void *tif);
void      GTIFFree(GTIF *gtif);
int       GTIFWriteKeys(GTIF *gtif);
void      GTIFDirectoryInfo(GTIF *gtif, int *versions, int *keycount);

/* GeoKey Access */
int       GTIFKeyInfo(GTIF *gtif, geokey_t key, int *size, tagtype_t* type);
int       GTIFKeyGet(GTIF *gtif, geokey_t key, void *val, int index, int count);
int       GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type, int count,...);

/* Metadata Import-Export utilities */
void      GTIFPrint(GTIF *gtif, GTIFPrintMethod print, void *aux);
int       GTIFImport(GTIF *gtif, GTIFReadMethod scan, void *aux);
char*     GTIFKeyName(geokey_t key);
char*     GTIFValueName(geokey_t key,int value);
char*     GTIFTypeName(tagtype_t type);
char*     GTIFTagName(int tag);
int       GTIFKeyCode(char * key);
int       GTIFValueCode(geokey_t key,char *value);
int       GTIFTypeCode(char *type);
int       GTIFTagCode(char *tag);

#if defined(__cplusplus)
}
#endif

#endif /* __geotiff_h_ */
