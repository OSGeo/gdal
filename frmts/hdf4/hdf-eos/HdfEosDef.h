/*****************************************************************************
 *
 * This module has a number of additions and improvements over the original
 * implementation to be suitable for usage in GDAL HDF driver.
 *
 * Andrey Kiselev <dron@ak4719.spb.edu> is responsible for all the changes.
 ****************************************************************************/

/*
Copyright (C) 1996 Hughes and Applied Research Corporation

Permission to use, modify, and distribute this software and its documentation
for any purpose without fee is hereby granted, provided that the above
copyright notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation.
*/

#ifndef HDFEOSDEF_H_
#define HDFEOSDEF_H_

#include <stdint.h>

/* include header file for EASE grid */
#include "ease.h"

/* Working Buffer Sizes */
#define HDFE_MAXMEMBUF  256*256*16
#define HDFE_NAMBUFSIZE 32000
#define HDFE_DIMBUFSIZE 64000


/* Field Merge */
#define HDFE_NOMERGE   0
#define HDFE_AUTOMERGE 1


/* XXentries Modes */
#define HDFE_NENTDIM   0
#define HDFE_NENTMAP   1
#define HDFE_NENTIMAP  2
#define HDFE_NENTGFLD  3
#define HDFE_NENTDFLD  4


/* GCTP projection codes */
#define GCTP_GEO       0
#define GCTP_UTM       1
#define GCTP_SPCS      2
#define GCTP_ALBERS    3
#define GCTP_LAMCC     4
#define GCTP_MERCAT    5
#define GCTP_PS        6
#define GCTP_POLYC     7
#define GCTP_EQUIDC    8
#define GCTP_TM        9
#define	GCTP_STEREO   10
#define GCTP_LAMAZ    11
#define	GCTP_AZMEQD   12
#define GCTP_GNOMON   13
#define	GCTP_ORTHO    14
#define GCTP_GVNSP    15
#define	GCTP_SNSOID   16
#define GCTP_EQRECT   17
#define	GCTP_MILLER   18
#define GCTP_VGRINT   19
#define	GCTP_HOM      20
#define GCTP_ROBIN    21
#define	GCTP_SOM      22
#define GCTP_ALASKA   23
#define	GCTP_GOOD     24
#define GCTP_MOLL     25
#define	GCTP_IMOLL    26
#define GCTP_HAMMER   27
#define	GCTP_WAGIV    28
#define GCTP_WAGVII   29
#define	GCTP_OBLEQA   30
#define	GCTP_ISINUS1  31
#define	GCTP_CEA      97
#define	GCTP_BCEA     98
#define	GCTP_ISINUS   99


/* Compression Modes */
#define HDFE_COMP_NONE     0
#define HDFE_COMP_RLE      1
#define HDFE_COMP_NBIT     2
#define HDFE_COMP_SKPHUFF  3
#define HDFE_COMP_DEFLATE  4


/* Tiling Codes */
#define HDFE_NOTILE        0
#define HDFE_TILE          1


/* Swath Subset Modes */
#define HDFE_MIDPOINT       0
#define HDFE_ENDPOINT       1
#define HDFE_ANYPOINT       2
#define HDFE_INTERNAL       0
#define HDFE_EXTERNAL       1
#define HDFE_NOPREVSUB     -1



/* Grid Origin */
#define HDFE_GD_UL         0
#define HDFE_GD_UR         1
#define HDFE_GD_LL         2
#define HDFE_GD_LR         3



/* Grid Pixel Registration */
#define HDFE_CENTER        0
#define HDFE_CORNER        1


/* Angle Conversion Codes */
#define HDFE_RAD_DEG      0
#define HDFE_DEG_RAD      1
#define HDFE_DMS_DEG      2
#define HDFE_DEG_DMS      3
#define HDFE_RAD_DMS      4
#define HDFE_DMS_RAD      5


#ifdef __cplusplus
extern "C" {
#endif

/* Swath Prototype */
int32_t SWopen(const char *, int);
int32_t SWattach(int32_t, const char *);
int32_t SWdiminfo(int32_t, const char *);
int SWfieldinfo(int32_t, const char *, int32_t *, int32_t [], int32_t *, char *, size_t);
int SWreadattr(int32_t, const char *, VOIDP);
int SWattrinfo(int32_t, const char *, int32_t *, int32_t *);
int32_t SWinqdims(int32_t, char *, int32_t []);
int32_t SWinqmaps(int32_t, char *, int32_t [], int32_t []);
int32_t SWinqgeofields(int32_t, char *, int32_t [], int32_t []);
int32_t SWinqdatafields(int32_t, char *, int32_t [],int32_t []);
int32_t SWinqattrs(int32_t, char *, int32_t *);
int32_t SWnentries(int32_t, int32_t, int32_t *);
int32_t SWinqswath(const char *, char *, int32_t *);
int SWreadfield(int32_t, const char *, int32_t [], int32_t [], int32_t [], VOIDP);
int SWgetfillvalue(int32_t, const char *, VOIDP);
int SWdetach(int32_t);
int SWclose(int32_t);
int SWsdid(int32_t, const char *, int32_t *);

/* Grid Prototypes */
int32_t GDopen(const char *, int);
int32_t GDattach(int32_t, const char *);
int32_t GDdiminfo(int32_t, const char *);
int GDgridinfo(int32_t, int32_t *, int32_t *, float64 [], float64 []);
int GDprojinfo(int32_t, int32_t *, int32_t *, int32_t *, float64 []);
int GDfieldinfo(int32_t, const char *, int32_t *, int32_t [], int32_t *, char dimlist[HDFE_DIMBUFSIZE+1]);
int GDtileinfo(int32_t, const char *, int32_t *, int32_t *, int32_t []);
int GDreadtile(int32_t, const char *, int32_t [],  VOIDP);
int GDreadfield(int32_t, const char *, int32_t [], int32_t [], int32_t [], VOIDP);
int GDreadattr(int32_t, const char *, VOIDP);
int GDattrinfo(int32_t, const char *, int32_t *, int32_t *);
int32_t GDinqfields(int32_t, char *, int32_t [], int32_t []);
int32_t GDinqattrs(int32_t, char *, int32_t *);
int32_t GDnentries(int32_t, int32_t, int32_t *);
int32_t GDinqgrid(const char *, char *, int32_t *);
int GDgetfillvalue(int32_t, const char *, VOIDP);
int GDdetach(int32_t);
int GDclose(int32_t);
int GDsdid(int32_t, const char *, int32_t *);


/* EH Utility Prototypes */
int32_t EHnumstr(const char *);
int32_t EHparsestr(const char *, const char, char *[], size_t, int32_t [], size_t);
int32_t EHstrwithin(const char *, const char *, const char);
int EHchkODL(const char *);
int32_t EHopen(const char *, int);
int EHchkfid(int32_t, const char *, int32_t *, int32_t *, uint8_t *);
int EHidinfo(int32_t, int32_t *, int32_t *);
int32_t EHgetid(int32_t, int32_t, const char *, int, const char *);
int EHgetmetavalue(char *[], const char *, char *);
char * EHmetagroup(int32_t, const char *, const char *, const char *, char *[]);
int EHattr(int32_t, int32_t, const char *, int32_t, int32_t, const char *, VOIDP);
int EHattrinfo(int32_t, int32_t, const char *, int32_t *, int32_t *);
int32_t EHattrcat(int32_t, int32_t, char *, int32_t *);
/* 9/3/97 Abe changed the first argument from
   float64 (float64 []) to float64 (*) (float64 [])  for SunOS
   float64 () (float64 []) to float64 (*) (float64 [])  for all other OSs */
int32_t EHinquire(const char *, const char *, char *, int32_t *);
int EHclose(int32_t);

int32_t VgetnameSafe(int32_t vkey, char *vgname, size_t vgnamesize);

#ifdef __cplusplus
}
#endif

#ifndef CPL_ARRAYSIZE
#define CPL_ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#endif  /* #ifndef HDFEOSDEF_H_ */










