/**********************************************************************
 *
 *  geo_keyp.h - private interface for GeoTIFF geokey tag parsing
 *
 *     Written by: Niles D. Ritter
 *
 **********************************************************************/

#ifndef __geo_keyp_h_
#define __geo_keyp_h_

#include <stdlib.h> /* for size_t */

/*
 * This structure contains the internal program
 * representation of the key entry.
 */
struct GeoKey {
	int       gk_key;    /* GeoKey ID        */
	size_t    gk_size;   /* data byte size   */
	tagtype_t gk_type;   /* TIFF data type   */
	long      gk_count;  /* number of values */
	char*     gk_data;   /* pointer to data, or value */
};
typedef struct GeoKey GeoKey;

/*
 *  This structure represents the file-organization of
 *  the key entry. Note that it assumes that short entries
 *  are aligned along 2-byte boundaries.
 */
struct KeyEntry {
	pinfo_t ent_key;        /* GeoKey ID            */
	pinfo_t ent_location;   /* TIFF Tag ID or 0     */
	pinfo_t ent_count;      /* GeoKey value count   */
	pinfo_t ent_val_offset; /* value or tag offset  */
};
typedef struct KeyEntry KeyEntry;

/*
 * This is the header of the CoordSystemInfoTag. The 'Version'
 *  will only change if the CoorSystemInfoTag structure changes;
 *  The Major Revision will be incremented whenever a new set of
 *  Keys is added or changed, while the Minor revision will be
 *  incremented when only the set of Key-values is increased.
 */
struct KeyHeader{
	pinfo_t hdr_version;      /* GeoTIFF Version          */
	pinfo_t hdr_rev_major;    /* GeoKey Major Revision #  */
	pinfo_t hdr_rev_minor;    /* GeoKey Minor Revision #  */
	pinfo_t hdr_num_keys;     /* Number of GeoKeys        */
};
typedef struct KeyHeader KeyHeader;

/*
 * This structure holds temporary data while reading or writing
 *  the tags.
 */
struct TempKeyData {
    char   *tk_asciiParams;
    int     tk_asciiParamsLength;
    int     tk_asciiParamsOffset;
};
typedef struct TempKeyData TempKeyData;


struct gtiff {
   tiff_t*    gt_tif;      /* TIFF file descriptor  */
   TIFFMethod gt_methods;  /* TIFF i/o methods      */
   int        gt_flags;    /* file flags            */
   
   pinfo_t    gt_version;  /* GeoTIFF Version       */
   pinfo_t    gt_rev_major;/* GeoKey Key Revision   */
   pinfo_t    gt_rev_minor;/* GeoKey Code Revision  */
   
   int        gt_num_keys; /* number of keys        */
   GeoKey*    gt_keys;     /* array of keys         */
   int*       gt_keyindex; /* index of a key, if set*/
   int        gt_keymin;   /* smallest key set      */
   int        gt_keymax;   /* largest key set       */
   
   pinfo_t*   gt_short;    /* array of SHORT vals   */
   double*    gt_double;   /* array of DOUBLE vals  */
   int        gt_nshorts;  /* number of SHORT vals  */
   int        gt_ndoubles; /* number of DOUBLE vals */
};  

typedef enum {
	FLAG_FILE_OPEN=1,
	FLAG_FILE_MODIFIED=2
} gtiff_flags;

#define MAX_KEYINDEX 65535   /* largest possible key    */
#define MAX_KEYS 100         /* maximum keys in a file  */
#define MAX_VALUES 1000      /* maximum values in a tag */

#endif /* __geo_keyp_h_ */

