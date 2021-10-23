#ifndef SHAPEFILE_H_INCLUDED
#define SHAPEFILE_H_INCLUDED

/******************************************************************************
 * $Id$
 *
 * Project:  Shapelib
 * Purpose:  Primary include file for Shapelib.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2012-2016, Even Rouault <even dot rouault at spatialys.com>
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see COPYING).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 */

#include <stdio.h>

#ifdef USE_DBMALLOC
#include <dbmalloc.h>
#endif

#ifdef USE_CPL
#include "cpl_conv.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/*                        Configuration options.                        */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Should the DBFReadStringAttribute() strip leading and           */
/*      trailing white space?                                           */
/* -------------------------------------------------------------------- */
#define TRIM_DBF_WHITESPACE

/* -------------------------------------------------------------------- */
/*      Should we write measure values to the Multipatch object?        */
/*      Reportedly ArcView crashes if we do write it, so for now it     */
/*      is disabled.                                                    */
/* -------------------------------------------------------------------- */
#define DISABLE_MULTIPATCH_MEASURE

/* -------------------------------------------------------------------- */
/*      SHPAPI_CALL                                                     */
/*                                                                      */
/*      The following two macros are present to allow forcing           */
/*      various calling conventions on the Shapelib API.                */
/*                                                                      */
/*      To force __stdcall conventions (needed to call Shapelib         */
/*      from Visual Basic and/or Dephi I believe) the makefile could    */
/*      be modified to define:                                          */
/*                                                                      */
/*        /DSHPAPI_CALL=__stdcall                                       */
/*                                                                      */
/*      If it is desired to force export of the Shapelib API without    */
/*      using the shapelib.def file, use the following definition.      */
/*                                                                      */
/*        /DSHAPELIB_DLLEXPORT                                          */
/*                                                                      */
/*      To get both at once it will be necessary to hack this           */
/*      include file to define:                                         */
/*                                                                      */
/*        #define SHPAPI_CALL __declspec(dllexport) __stdcall           */
/*        #define SHPAPI_CALL1 __declspec(dllexport) * __stdcall        */
/*                                                                      */
/*      The complexity of the situation is partly caused by the        */
/*      peculiar requirement of Visual C++ that __stdcall appear        */
/*      after any "*"'s in the return value of a function while the     */
/*      __declspec(dllexport) must appear before them.                  */
/* -------------------------------------------------------------------- */

#ifdef SHAPELIB_DLLEXPORT
#  define SHPAPI_CALL __declspec(dllexport)
#  define SHPAPI_CALL1(x)  __declspec(dllexport) x
#endif

#ifndef SHPAPI_CALL
#  if defined(USE_GCC_VISIBILITY_FLAG)
#    define SHPAPI_CALL     __attribute__ ((visibility("default")))
#    define SHPAPI_CALL1(x) __attribute__ ((visibility("default")))     x
#  else
#    define SHPAPI_CALL
#  endif
#endif

#ifndef SHPAPI_CALL1
#  define SHPAPI_CALL1(x)      x SHPAPI_CALL
#endif

/* -------------------------------------------------------------------- */
/*      Macros for controlling CVSID and ensuring they don't appear     */
/*      as unreferenced variables resulting in lots of warnings.        */
/* -------------------------------------------------------------------- */
#ifndef DISABLE_CVSID
#  if defined(__GNUC__) && __GNUC__ >= 4
#    define SHP_CVSID(string)     static const char cpl_cvsid[] __attribute__((used)) = string;
#  else
#    define SHP_CVSID(string)     static const char cpl_cvsid[] = string; \
static const char *cvsid_aw() { return( cvsid_aw() ? NULL : cpl_cvsid ); }
#  endif
#else
#  define SHP_CVSID(string)
#endif

/* -------------------------------------------------------------------- */
/*      On some platforms, additional file IO hooks are defined that    */
/*      UTF-8 encoded filenames Unicode filenames                       */
/* -------------------------------------------------------------------- */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#  define SHPAPI_WINDOWS
#  define SHPAPI_UTF8_HOOKS
#endif

/* -------------------------------------------------------------------- */
/*      IO/Error hook functions.                                        */
/* -------------------------------------------------------------------- */
typedef int *SAFile;

#ifndef SAOffset
typedef unsigned long SAOffset;
#endif

typedef struct {
    SAFile     (*FOpen) ( const char *filename, const char *access);
    SAOffset   (*FRead) ( void *p, SAOffset size, SAOffset nmemb, SAFile file);
    SAOffset   (*FWrite)( void *p, SAOffset size, SAOffset nmemb, SAFile file);
    SAOffset   (*FSeek) ( SAFile file, SAOffset offset, int whence );
    SAOffset   (*FTell) ( SAFile file );
    int        (*FFlush)( SAFile file );
    int        (*FClose)( SAFile file );
    int        (*Remove) ( const char *filename );

    void       (*Error) ( const char *message );
    double     (*Atof)  ( const char *str );
} SAHooks;

void SHPAPI_CALL SASetupDefaultHooks( SAHooks *psHooks );
#ifdef SHPAPI_UTF8_HOOKS
void SHPAPI_CALL SASetupUtf8Hooks( SAHooks *psHooks );
#endif

/************************************************************************/
/*                             SHP Support.                             */
/************************************************************************/
typedef struct tagSHPObject SHPObject;

typedef struct
{
    SAHooks sHooks;

    SAFile      fpSHP;
    SAFile      fpSHX;

    int         nShapeType;  /* SHPT_* */

    unsigned int nFileSize;  /* SHP file */

    int         nRecords;
    int         nMaxRecords;
    unsigned int*panRecOffset;
    unsigned int *panRecSize;

    double      adBoundsMin[4];
    double      adBoundsMax[4];

    int         bUpdated;

    unsigned char *pabyRec;
    int         nBufSize;

    int            bFastModeReadObject;
    unsigned char *pabyObjectBuf;
    int            nObjectBufSize;
    SHPObject*     psCachedObject;
} SHPInfo;

typedef SHPInfo * SHPHandle;

/* -------------------------------------------------------------------- */
/*      Shape types (nSHPType)                                          */
/* -------------------------------------------------------------------- */
#define SHPT_NULL       0
#define SHPT_POINT      1
#define SHPT_ARC        3
#define SHPT_POLYGON    5
#define SHPT_MULTIPOINT 8
#define SHPT_POINTZ     11
#define SHPT_ARCZ       13
#define SHPT_POLYGONZ   15
#define SHPT_MULTIPOINTZ 18
#define SHPT_POINTM     21
#define SHPT_ARCM       23
#define SHPT_POLYGONM   25
#define SHPT_MULTIPOINTM 28
#define SHPT_MULTIPATCH 31

/* -------------------------------------------------------------------- */
/*      Part types - everything but SHPT_MULTIPATCH just uses           */
/*      SHPP_RING.                                                      */
/* -------------------------------------------------------------------- */

#define SHPP_TRISTRIP   0
#define SHPP_TRIFAN     1
#define SHPP_OUTERRING  2
#define SHPP_INNERRING  3
#define SHPP_FIRSTRING  4
#define SHPP_RING       5

/* -------------------------------------------------------------------- */
/*      SHPObject - represents on shape (without attributes) read       */
/*      from the .shp file.                                             */
/* -------------------------------------------------------------------- */
struct tagSHPObject
{
    int    nSHPType;

    int    nShapeId;  /* -1 is unknown/unassigned */

    int    nParts;
    int    *panPartStart;
    int    *panPartType;

    int    nVertices;
    double *padfX;
    double *padfY;
    double *padfZ;
    double *padfM;

    double dfXMin;
    double dfYMin;
    double dfZMin;
    double dfMMin;

    double dfXMax;
    double dfYMax;
    double dfZMax;
    double dfMMax;

    int    bMeasureIsUsed;
    int    bFastModeReadObject;
};

/* -------------------------------------------------------------------- */
/*      SHP API Prototypes                                              */
/* -------------------------------------------------------------------- */

/* If pszAccess is read-only, the fpSHX field of the returned structure */
/* will be NULL as it is not necessary to keep the SHX file open */
SHPHandle SHPAPI_CALL
      SHPOpen( const char * pszShapeFile, const char * pszAccess );
SHPHandle SHPAPI_CALL
      SHPOpenLL( const char *pszShapeFile, const char *pszAccess,
                 SAHooks *psHooks );
SHPHandle SHPAPI_CALL
      SHPOpenLLEx( const char *pszShapeFile, const char *pszAccess,
                  SAHooks *psHooks, int bRestoreSHX );

int SHPAPI_CALL
      SHPRestoreSHX( const char *pszShapeFile, const char *pszAccess,
                  SAHooks *psHooks );

/* If setting bFastMode = TRUE, the content of SHPReadObject() is owned by the SHPHandle. */
/* So you cannot have 2 valid instances of SHPReadObject() simultaneously. */
/* The SHPObject padfZ and padfM members may be NULL depending on the geometry */
/* type. It is illegal to free at hand any of the pointer members of the SHPObject structure */
void SHPAPI_CALL SHPSetFastModeReadObject( SHPHandle hSHP, int bFastMode );

SHPHandle SHPAPI_CALL
      SHPCreate( const char * pszShapeFile, int nShapeType );
SHPHandle SHPAPI_CALL
      SHPCreateLL( const char * pszShapeFile, int nShapeType,
                   SAHooks *psHooks );
void SHPAPI_CALL
      SHPGetInfo( SHPHandle hSHP, int * pnEntities, int * pnShapeType,
                  double * padfMinBound, double * padfMaxBound );

SHPObject SHPAPI_CALL1(*)
      SHPReadObject( SHPHandle hSHP, int iShape );
int SHPAPI_CALL
      SHPWriteObject( SHPHandle hSHP, int iShape, SHPObject * psObject );

void SHPAPI_CALL
      SHPDestroyObject( SHPObject * psObject );
void SHPAPI_CALL
      SHPComputeExtents( SHPObject * psObject );
SHPObject SHPAPI_CALL1(*)
      SHPCreateObject( int nSHPType, int nShapeId, int nParts,
                       const int * panPartStart, const int * panPartType,
                       int nVertices,
                       const double * padfX, const double * padfY,
                       const double * padfZ, const double * padfM );
SHPObject SHPAPI_CALL1(*)
      SHPCreateSimpleObject( int nSHPType, int nVertices,
                             const double * padfX,
                             const double * padfY,
                             const double * padfZ );

int SHPAPI_CALL
      SHPRewindObject( SHPHandle hSHP, SHPObject * psObject );

void SHPAPI_CALL SHPClose( SHPHandle hSHP );
void SHPAPI_CALL SHPWriteHeader( SHPHandle hSHP );

const char SHPAPI_CALL1(*)
      SHPTypeName( int nSHPType );
const char SHPAPI_CALL1(*)
      SHPPartTypeName( int nPartType );

/* -------------------------------------------------------------------- */
/*      Shape quadtree indexing API.                                    */
/* -------------------------------------------------------------------- */

/* this can be two or four for binary or quad tree */
#define MAX_SUBNODE 4

/* upper limit of tree levels for automatic estimation */
#define MAX_DEFAULT_TREE_DEPTH 12

typedef struct shape_tree_node
{
    /* region covered by this node */
    double      adfBoundsMin[4];
    double      adfBoundsMax[4];

    /* list of shapes stored at this node.  The papsShapeObj pointers
       or the whole list can be NULL */
    int         nShapeCount;
    int         *panShapeIds;
    SHPObject   **papsShapeObj;

    int         nSubNodes;
    struct shape_tree_node *apsSubNode[MAX_SUBNODE];

} SHPTreeNode;

typedef struct
{
    SHPHandle   hSHP;

    int         nMaxDepth;
    int         nDimension;
    int         nTotalCount;

    SHPTreeNode *psRoot;
} SHPTree;

SHPTree SHPAPI_CALL1(*)
      SHPCreateTree( SHPHandle hSHP, int nDimension, int nMaxDepth,
                     double *padfBoundsMin, double *padfBoundsMax );
void SHPAPI_CALL
      SHPDestroyTree( SHPTree * hTree );

int SHPAPI_CALL
      SHPWriteTree( SHPTree *hTree, const char * pszFilename );

int SHPAPI_CALL
      SHPTreeAddShapeId( SHPTree * hTree, SHPObject * psObject );
int SHPAPI_CALL
      SHPTreeRemoveShapeId( SHPTree * hTree, int nShapeId );

void SHPAPI_CALL
      SHPTreeTrimExtraNodes( SHPTree * hTree );

int SHPAPI_CALL1(*)
      SHPTreeFindLikelyShapes( SHPTree * hTree,
                               double * padfBoundsMin,
                               double * padfBoundsMax,
                               int * );
int SHPAPI_CALL
      SHPCheckBoundsOverlap( double *, double *, double *, double *, int );

int SHPAPI_CALL1(*)
SHPSearchDiskTree( FILE *fp,
                   double *padfBoundsMin, double *padfBoundsMax,
                   int *pnShapeCount );

typedef struct SHPDiskTreeInfo* SHPTreeDiskHandle;

SHPTreeDiskHandle SHPAPI_CALL
    SHPOpenDiskTree( const char* pszQIXFilename,
                     SAHooks *psHooks );

void SHPAPI_CALL
    SHPCloseDiskTree( SHPTreeDiskHandle hDiskTree );

int SHPAPI_CALL1(*)
SHPSearchDiskTreeEx( SHPTreeDiskHandle hDiskTree,
                     double *padfBoundsMin, double *padfBoundsMax,
                     int *pnShapeCount );

int SHPAPI_CALL
    SHPWriteTreeLL(SHPTree *hTree, const char *pszFilename, SAHooks *psHooks );

/* -------------------------------------------------------------------- */
/*      SBN Search API                                                  */
/* -------------------------------------------------------------------- */

typedef struct SBNSearchInfo* SBNSearchHandle;

SBNSearchHandle SHPAPI_CALL
    SBNOpenDiskTree( const char* pszSBNFilename,
                 SAHooks *psHooks );

void SHPAPI_CALL
    SBNCloseDiskTree( SBNSearchHandle hSBN );

int SHPAPI_CALL1(*)
SBNSearchDiskTree( SBNSearchHandle hSBN,
                   double *padfBoundsMin, double *padfBoundsMax,
                   int *pnShapeCount );

int SHPAPI_CALL1(*)
SBNSearchDiskTreeInteger( SBNSearchHandle hSBN,
                          int bMinX, int bMinY, int bMaxX, int bMaxY,
                          int *pnShapeCount );

void SHPAPI_CALL SBNSearchFreeIds( int* panShapeId );

/************************************************************************/
/*                             DBF Support.                             */
/************************************************************************/
typedef struct
{
    SAHooks sHooks;

    SAFile      fp;

    int         nRecords;

    int         nRecordLength; /* Must fit on uint16 */
    int         nHeaderLength; /* File header length (32) + field
                                  descriptor length + spare space.
                                  Must fit on uint16 */
    int         nFields;
    int         *panFieldOffset;
    int         *panFieldSize;
    int         *panFieldDecimals;
    char        *pachFieldType;

    char        *pszHeader; /* Field descriptors */

    int         nCurrentRecord;
    int         bCurrentRecordModified;
    char        *pszCurrentRecord;

    int         nWorkFieldLength;
    char        *pszWorkField;

    int         bNoHeader;
    int         bUpdated;

    union
    {
        double      dfDoubleField;
        int         nIntField;
    } fieldValue;

    int         iLanguageDriver;
    char        *pszCodePage;

    int         nUpdateYearSince1900; /* 0-255 */
    int         nUpdateMonth; /* 1-12 */
    int         nUpdateDay; /* 1-31 */

    int         bWriteEndOfFileChar; /* defaults to TRUE */

    int         bRequireNextWriteSeek;
} DBFInfo;

typedef DBFInfo * DBFHandle;

typedef enum {
  FTString,
  FTInteger,
  FTDouble,
  FTLogical,
  FTDate,
  FTInvalid
} DBFFieldType;

/* Field descriptor/header size */
#define XBASE_FLDHDR_SZ         32
/* Shapelib read up to 11 characters, even if only 10 should normally be used */
#define XBASE_FLDNAME_LEN_READ  11
/* On writing, we limit to 10 characters */
#define XBASE_FLDNAME_LEN_WRITE 10
/* Normally only 254 characters should be used. We tolerate 255 historically */
#define XBASE_FLD_MAX_WIDTH     255

DBFHandle SHPAPI_CALL
      DBFOpen( const char * pszDBFFile, const char * pszAccess );
DBFHandle SHPAPI_CALL
      DBFOpenLL( const char * pszDBFFile, const char * pszAccess,
                 SAHooks *psHooks );
DBFHandle SHPAPI_CALL
      DBFCreate( const char * pszDBFFile );
DBFHandle SHPAPI_CALL
      DBFCreateEx( const char * pszDBFFile, const char * pszCodePage );
DBFHandle SHPAPI_CALL
      DBFCreateLL( const char * pszDBFFile, const char * pszCodePage, SAHooks *psHooks );

int SHPAPI_CALL
      DBFGetFieldCount( DBFHandle psDBF );
int SHPAPI_CALL
      DBFGetRecordCount( DBFHandle psDBF );
int SHPAPI_CALL
      DBFAddField( DBFHandle hDBF, const char * pszFieldName,
                   DBFFieldType eType, int nWidth, int nDecimals );

int SHPAPI_CALL
      DBFAddNativeFieldType( DBFHandle hDBF, const char * pszFieldName,
                             char chType, int nWidth, int nDecimals );

int SHPAPI_CALL
      DBFDeleteField( DBFHandle hDBF, int iField );

int SHPAPI_CALL
      DBFReorderFields( DBFHandle psDBF, int* panMap );

int SHPAPI_CALL
      DBFAlterFieldDefn( DBFHandle psDBF, int iField, const char * pszFieldName,
                         char chType, int nWidth, int nDecimals );

DBFFieldType SHPAPI_CALL
      DBFGetFieldInfo( DBFHandle psDBF, int iField,
                       char * pszFieldName, int * pnWidth, int * pnDecimals );

int SHPAPI_CALL
      DBFGetFieldIndex(DBFHandle psDBF, const char *pszFieldName);

int SHPAPI_CALL
      DBFReadIntegerAttribute( DBFHandle hDBF, int iShape, int iField );
double SHPAPI_CALL
      DBFReadDoubleAttribute( DBFHandle hDBF, int iShape, int iField );
const char SHPAPI_CALL1(*)
      DBFReadStringAttribute( DBFHandle hDBF, int iShape, int iField );
const char SHPAPI_CALL1(*)
      DBFReadLogicalAttribute( DBFHandle hDBF, int iShape, int iField );
int SHPAPI_CALL
      DBFIsAttributeNULL( DBFHandle hDBF, int iShape, int iField );

int SHPAPI_CALL
      DBFWriteIntegerAttribute( DBFHandle hDBF, int iShape, int iField,
                                int nFieldValue );
int SHPAPI_CALL
      DBFWriteDoubleAttribute( DBFHandle hDBF, int iShape, int iField,
                               double dFieldValue );
int SHPAPI_CALL
      DBFWriteStringAttribute( DBFHandle hDBF, int iShape, int iField,
                               const char * pszFieldValue );
int SHPAPI_CALL
     DBFWriteNULLAttribute( DBFHandle hDBF, int iShape, int iField );

int SHPAPI_CALL
     DBFWriteLogicalAttribute( DBFHandle hDBF, int iShape, int iField,
                               const char lFieldValue);
int SHPAPI_CALL
     DBFWriteAttributeDirectly(DBFHandle psDBF, int hEntity, int iField,
                               void * pValue );
const char SHPAPI_CALL1(*)
      DBFReadTuple(DBFHandle psDBF, int hEntity );
int SHPAPI_CALL
      DBFWriteTuple(DBFHandle psDBF, int hEntity, void * pRawTuple );

int SHPAPI_CALL DBFIsRecordDeleted( DBFHandle psDBF, int iShape );
int SHPAPI_CALL DBFMarkRecordDeleted( DBFHandle psDBF, int iShape,
                                      int bIsDeleted );

DBFHandle SHPAPI_CALL
      DBFCloneEmpty(DBFHandle psDBF, const char * pszFilename );

void SHPAPI_CALL
      DBFClose( DBFHandle hDBF );
void    SHPAPI_CALL
      DBFUpdateHeader( DBFHandle hDBF );
char SHPAPI_CALL
      DBFGetNativeFieldType( DBFHandle hDBF, int iField );

const char SHPAPI_CALL1(*)
      DBFGetCodePage(DBFHandle psDBF );

void SHPAPI_CALL
    DBFSetLastModifiedDate( DBFHandle psDBF, int nYYSince1900, int nMM, int nDD );

void SHPAPI_CALL DBFSetWriteEndOfFileChar( DBFHandle psDBF, int bWriteFlag );

#ifdef __cplusplus
}
#endif

#endif /* ndef SHAPEFILE_H_INCLUDED */
