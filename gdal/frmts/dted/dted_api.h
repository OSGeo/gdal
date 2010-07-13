/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Public (C callable) interface for DTED/CDED reading.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ****************************************************************************/

#ifndef _DTED_API_H_INCLUDED
#define _DTED_API_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      To avoid dependence on CPL, just define AVOID_CPL when          */
/*      compiling.                                                      */
/* -------------------------------------------------------------------- */
#ifndef AVOID_CPL
#  include "cpl_conv.h"
#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CPL_C_START
#define CPL_C_END

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#ifndef EQUAL
#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS)
#  define EQUALN(a,b,n)           (strnicmp(a,b,n)==0)
#  define EQUAL(a,b)              (stricmp(a,b)==0)
#else
#  define EQUALN(a,b,n)           (strncasecmp(a,b,n)==0)
#  define EQUAL(a,b)              (strcasecmp(a,b)==0)
#endif
#endif

#ifndef ABS
#define ABS(x)  (((x)>=0) ? (x) : -(x))
#endif

#ifndef MIN
#define MIN(x,y) (((x)<(y)) ? (x) : (y))
#endif

#define VSIFTellL ftell
#define VSIFOpenL fopen
#define VSIFCloseL fclose
#define VSIFReadL fread
#define VSIFWriteL fwrite
#define CPLMalloc malloc
#define CPLCalloc calloc
#define CPLFree free
#define GInt16  short
#define GByte   unsigned char
#define VSIFSeekL fseek
#define CPLAssert assert
#define VSIStrdup strdup

#endif

/* -------------------------------------------------------------------- */
/*      DTED information structure.  All of this can be considered      */
/*      public information.                                             */
/* -------------------------------------------------------------------- */
CPL_C_START

#define DTED_UHL_SIZE 80
#define DTED_DSI_SIZE 648
#define DTED_ACC_SIZE 2700

#define DTED_NODATA_VALUE -32767

typedef struct {
  FILE          *fp;
  int           bUpdate;

  int           nXSize;
  int           nYSize;

  double        dfULCornerX;            /* in long/lat degrees */
  double        dfULCornerY;
  double        dfPixelSizeX;
  double        dfPixelSizeY;

  int           nUHLOffset;
  char          *pachUHLRecord;

  int           nDSIOffset;
  char          *pachDSIRecord;

  int           nACCOffset;
  char          *pachACCRecord;

  int           nDataOffset;

} DTEDInfo;

/* -------------------------------------------------------------------- */
/*      DTED access API.  Get info directly from the structure          */
/*      (DTEDInfo).                                                     */
/* -------------------------------------------------------------------- */
DTEDInfo *DTEDOpen( const char * pszFilename, const char * pszAccess,
                    int bTestOpen );

/**     Read one single sample. The coordinates are given from the
        top-left corner of the file (contrary to the internal
        organisation or a DTED file)
*/
int DTEDReadPoint( DTEDInfo * psDInfo, int nXOff, int nYOff, GInt16* panVal);

/**    Read one profile line.  These are organized in bottom to top
       order starting from the leftmost column (0).
*/
int DTEDReadProfile( DTEDInfo * psDInfo, int nColumnOffset,
                     GInt16 * panData );

/* Extented version of DTEDReadProfile that enable the user to specify */
/* whether he wants the checksums to be verified */
int DTEDReadProfileEx( DTEDInfo * psDInfo, int nColumnOffset,
                       GInt16 * panData, int bVerifyChecksum );

/**    Write one profile line.
       @warning Contrary to DTEDReadProfile,
                the profile should be organized from top to bottom
*/
int DTEDWriteProfile( DTEDInfo *psDInfo, int nColumnOffset, GInt16 *panData);

void DTEDClose( DTEDInfo * );

const char *DTEDCreate( const char *pszFilename, 
                        int nLevel, int nLLOriginLat, int nLLOriginLong );

/* -------------------------------------------------------------------- */
/*      Metadata support.                                               */
/* -------------------------------------------------------------------- */
typedef enum {
    DTEDMD_VERTACCURACY_UHL = 1,            /* UHL 29+4, ACC 8+4 */
    DTEDMD_VERTACCURACY_ACC = 2,           
    DTEDMD_SECURITYCODE_UHL = 3,            /* UHL 33+3, DSI 4+1 */
    DTEDMD_SECURITYCODE_DSI = 4,           
    DTEDMD_UNIQUEREF_UHL = 5,               /* UHL 36+12, DSI 65+15*/
    DTEDMD_UNIQUEREF_DSI = 6,              
    DTEDMD_DATA_EDITION = 7,            /* DSI 88+2 */
    DTEDMD_MATCHMERGE_VERSION = 8,      /* DSI 90+1 */
    DTEDMD_MAINT_DATE = 9,              /* DSI 91+4 */
    DTEDMD_MATCHMERGE_DATE = 10,        /* DSI 95+4 */
    DTEDMD_MAINT_DESCRIPTION = 11,      /* DSI 99+4 */
    DTEDMD_PRODUCER = 12,               /* DSI 103+8 */
    DTEDMD_VERTDATUM = 13,              /* DSI 142+3 */
    DTEDMD_DIGITIZING_SYS = 14,         /* DSI 150+10 */
    DTEDMD_COMPILATION_DATE = 15,       /* DSI 160+4 */
    DTEDMD_HORIZACCURACY = 16,          /* ACC 4+4 */
    DTEDMD_REL_HORIZACCURACY = 17,      /* ACC 12+4 */
    DTEDMD_REL_VERTACCURACY = 18,       /* ACC 16+4 */
    DTEDMD_HORIZDATUM = 19,             /* DSI 145+5 */ 
    DTEDMD_ORIGINLONG = 20,             /* UHL 5+7 */
    DTEDMD_ORIGINLAT = 21,              /* UHL 13+7 */
    DTEDMD_NIMA_DESIGNATOR = 22,        /* DSI 60 + 5 */
    DTEDMD_MAX = 22
} DTEDMetaDataCode;

    
char *DTEDGetMetadata( DTEDInfo *, DTEDMetaDataCode );
int   DTEDSetMetadata( DTEDInfo *, DTEDMetaDataCode, const char *);

/* -------------------------------------------------------------------- */
/*      Point stream writer API.                                        */
/* -------------------------------------------------------------------- */
void *DTEDCreatePtStream( const char *pszPath, int nLevel );
int   DTEDWritePt( void *hStream, double dfLong, double dfLat, double dfElev );
void  DTEDFillPtStream( void *hStream, int nPixelSearchDist );
void  DTEDPtStreamSetMetadata( void *hStream, DTEDMetaDataCode, const char *);
void  DTEDClosePtStream( void *hStream );
void  DTEDPtStreamTrimEdgeOnlyTiles( void *hStream );

CPL_C_END

#endif /* ndef _DTED_API_H_INCLUDED */


