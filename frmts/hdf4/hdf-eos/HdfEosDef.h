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
int32 SWopen(const char *, intn);
int32 SWattach(int32, const char *);
int32 SWdiminfo(int32, const char *);
intn SWmapinfo(int32, const char *, const char *, int32 *, int32 *);
int32 SWidxmapinfo(int32, const char *, const char *, int32 []);
intn SWfieldinfo(int32, const char *, int32 *, int32 [], int32 *, char *);
intn SWcompinfo(int32, const char *, int32 *, intn []);
intn SWreadattr(int32, const char *, VOIDP);
intn SWattrinfo(int32, const char *, int32 *, int32 *);
int32 SWinqdims(int32, char *, int32 []);
int32 SWinqmaps(int32, char *, int32 [], int32 []);
int32 SWinqidxmaps(int32, char *, int32 []);
int32 SWinqgeofields(int32, char *, int32 [], int32 []);
int32 SWinqdatafields(int32, char *, int32 [],int32 []);
int32 SWinqattrs(int32, char *, int32 *);
int32 SWnentries(int32, int32, int32 *);
int32 SWinqswath(const char *, char *, int32 *);
intn SWreadfield(int32, const char *, int32 [], int32 [], int32 [], VOIDP);
intn SWgetfillvalue(int32, const char *, VOIDP);
intn SWdetach(int32);
intn SWclose(int32);
intn SWgeomapinfo(int32, const char *);
intn SWsdid(int32, const char *, int32 *);

/* Grid Prototypes */
int32 GDopen(const char *, intn);
int32 GDattach(int32, const char *);
int32 GDdiminfo(int32, const char *);
intn GDgridinfo(int32, int32 *, int32 *, float64 [], float64 []);
intn GDprojinfo(int32, int32 *, int32 *, int32 *, float64 []);
intn GDorigininfo(int32, int32 *);
intn GDpixreginfo(int32, int32 *);
intn GDcompinfo(int32, const char *, int32 *, intn []);
intn GDfieldinfo(int32, const char *, int32 *, int32 [], int32 *, char *);
intn GDtileinfo(int32, const char *, int32 *, int32 *, int32 []);
intn GDreadtile(int32, const char *, int32 [],  VOIDP);
intn GDreadfield(int32, const char *, int32 [], int32 [], int32 [], VOIDP);
intn GDreadattr(int32, const char *, VOIDP);
intn GDattrinfo(int32, const char *, int32 *, int32 *);
int32 GDinqdims(int32, char *, int32 []);
int32 GDinqfields(int32, char *, int32 [], int32 []);
int32 GDinqattrs(int32, char *, int32 *);
int32 GDnentries(int32, int32, int32 *);
int32 GDinqgrid(const char *, char *, int32 *);
intn GDgetpixels(int32, int32, float64 [], float64 [], int32 [], int32 []);
int32 GDgetpixvalues(int32, int32, int32 [], int32 [], const char *, VOIDP);
intn GDgetfillvalue(int32, const char *, VOIDP);
intn GDdetach(int32);
intn GDclose(int32);
intn GDsdid(int32, const char *, int32 *);


/* EH Utility Prototypes */
int32 EHnumstr(const char *);
float64 EHconvAng(float64, intn);
int32 EHparsestr(const char *, const char, char *[], int32 []);
int32 EHstrwithin(const char *, const char *, const char);
intn EHchkODL(const char *);
intn EHloadliststr(char *[], int32, char *, char);
intn EHgetversion(int32, char *);
int32 EHopen(const char *, intn);
intn EHchkfid(int32, const char *, int32 *, int32 *, uint8 *);
intn EHidinfo(int32, int32 *, int32 *);
int32 EHgetid(int32, int32, const char *, intn, const char *);
intn EHrevflds(const char *, char *);
intn EHgetmetavalue(char *[], const char *, char *);
char * EHmetagroup(int32, const char *, const char *, const char *, char *[]);
intn EHattr(int32, int32, const char *, int32, int32, const char *, VOIDP);
intn EHattrinfo(int32, int32, const char *, int32 *, int32 *);
int32 EHattrcat(int32, int32, char *, int32 *);
/* 9/3/97 Abe changed the first argument from
   float64 (float64 []) to float64 (*) (float64 [])  for SunOS
   float64 () (float64 []) to float64 (*) (float64 [])  for all other OSs */
int32 EHinquire(const char *, const char *, char *, int32 *);
intn EHclose(int32);



#ifdef __cplusplus
}
#endif


#endif  /* #ifndef HDFEOSDEF_H_ */










