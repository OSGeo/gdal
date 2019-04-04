/******************************************************************************
 * $Id$
 *
 * Project:  Shapelib
 * Purpose:  Implementation of .dbf access API documented in dbf_api.html.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * $Log: dbfopen.c,v $
 * Revision 1.92  2016-12-05 18:44:08  erouault
 * * dbfopen.c, shapefil.h: write DBF end-of-file character 0x1A by default.
 * This behaviour can be controlled with the DBFSetWriteEndOfFileChar()
 * function.
 *
 * Revision 1.91  2016-12-05 12:44:05  erouault
 * * Major overhaul of Makefile build system to use autoconf/automake.
 *
 * * Warning fixes in contrib/
 *
 * Revision 1.90  2016-12-04 15:30:15  erouault
 * * shpopen.c, dbfopen.c, shptree.c, shapefil.h: resync with
 * GDAL Shapefile driver. Mostly cleanups. SHPObject and DBFInfo
 * structures extended with new members. New functions:
 * DBFSetLastModifiedDate, SHPOpenLLEx, SHPRestoreSHX,
 * SHPSetFastModeReadObject
 *
 * * sbnsearch.c: new file to implement original ESRI .sbn spatial
 * index reading. (no write support). New functions:
 * SBNOpenDiskTree, SBNCloseDiskTree, SBNSearchDiskTree,
 * SBNSearchDiskTreeInteger, SBNSearchFreeIds
 *
 * * Makefile, makefile.vc, CMakeLists.txt, shapelib.def: updates
 * with new file and symbols.
 *
 * * commit: helper script to cvs commit
 *
 * Revision 1.89  2011-07-24 05:59:25  fwarmerdam
 * minimize use of CPLError in favor of SAHooks.Error()
 *
 * Revision 1.88  2011-05-13 17:35:17  fwarmerdam
 * added DBFReorderFields() and DBFAlterFields() functions (from Even)
 *
 * Revision 1.87  2011-05-07 22:41:02  fwarmerdam
 * ensure pending record is flushed when adding a native field (GDAL #4073)
 *
 * Revision 1.86  2011-04-17 15:15:29  fwarmerdam
 * Removed unused variable.
 *
 * Revision 1.85  2010-12-06 16:09:34  fwarmerdam
 * fix buffer read overrun fetching code page (bug 2276)
 *
 * Revision 1.84  2009-10-29 19:59:48  fwarmerdam
 * avoid crash on truncated header (gdal #3093)
 *
 * Revision 1.83  2008/11/12 14:28:15  fwarmerdam
 * DBFCreateField() now works on files with records
 *
 * Revision 1.82  2008/11/11 17:47:09  fwarmerdam
 * added DBFDeleteField() function
 *
 * Revision 1.81  2008/01/03 17:48:13  bram
 * in DBFCreate, use default code page LDID/87 (= 0x57, ANSI)
 * instead of LDID/3.  This seems to be the same as what ESRI
 * would be doing by default.
 *
 * Revision 1.80  2007/12/30 14:36:39  fwarmerdam
 * avoid syntax issue with last comment.
 *
 * Revision 1.79  2007/12/30 14:35:48  fwarmerdam
 * Avoid char* / unsigned char* warnings.
 *
 * Revision 1.78  2007/12/18 18:28:07  bram
 * - create hook for client specific atof (bugzilla ticket 1615)
 * - check for NULL handle before closing cpCPG file, and close after reading.
 *
 * Revision 1.77  2007/12/15 20:25:21  bram
 * dbfopen.c now reads the Code Page information from the DBF file, and exports
 * this information as a string through the DBFGetCodePage function.  This is
 * either the number from the LDID header field ("LDID/<number>") or as the
 * content of an accompanying .CPG file.  When creating a DBF file, the code can
 * be set using DBFCreateEx.
 *
 * Revision 1.76  2007/12/12 22:21:32  bram
 * DBFClose: check for NULL psDBF handle before trying to close it.
 *
 * Revision 1.75  2007/12/06 13:58:19  fwarmerdam
 * make sure file offset calculations are done in as SAOffset
 *
 * Revision 1.74  2007/12/06 07:00:25  fwarmerdam
 * dbfopen now using SAHooks for fileio
 *
 * Revision 1.73  2007/09/03 19:48:11  fwarmerdam
 * move DBFReadAttribute() static dDoubleField into dbfinfo
 *
 * Revision 1.72  2007/09/03 19:34:06  fwarmerdam
 * Avoid use of static tuple buffer in DBFReadTuple()
 *
 * Revision 1.71  2006/06/22 14:37:18  fwarmerdam
 * avoid memory leak if dbfopen fread fails
 *
 * Revision 1.70  2006/06/17 17:47:05  fwarmerdam
 * use calloc() for dbfinfo in DBFCreate
 *
 * Revision 1.69  2006/06/17 15:34:32  fwarmerdam
 * disallow creating fields wider than 255
 *
 * Revision 1.68  2006/06/17 15:12:40  fwarmerdam
 * Fixed C++ style comments.
 *
 * Revision 1.67  2006/06/17 00:24:53  fwarmerdam
 * Don't treat non-zero decimals values as high order byte for length
 * for strings.  It causes serious corruption for some files.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1202
 *
 * Revision 1.66  2006/03/29 18:26:20  fwarmerdam
 * fixed bug with size of pachfieldtype in dbfcloneempty
 *
 * Revision 1.65  2006/02/15 01:14:30  fwarmerdam
 * added DBFAddNativeFieldType
 *
 * Revision 1.64  2006/02/09 00:29:04  fwarmerdam
 * Changed to put spaces into string fields that are NULL as
 * per http://bugzilla.maptools.org/show_bug.cgi?id=316.
 *
 * Revision 1.63  2006/01/25 15:35:43  fwarmerdam
 * check success on DBFFlushRecord
 *
 * Revision 1.62  2006/01/10 16:28:03  fwarmerdam
 * Fixed typo in CPLError.
 *
 * Revision 1.61  2006/01/10 16:26:29  fwarmerdam
 * Push loading record buffer into DBFLoadRecord.
 * Implement CPL error reporting if USE_CPL defined.
 *
 * Revision 1.60  2006/01/05 01:27:27  fwarmerdam
 * added dbf deletion mark/fetch
 *
 * Revision 1.59  2005/03/14 15:20:28  fwarmerdam
 * Fixed last change.
 *
 * Revision 1.58  2005/03/14 15:18:54  fwarmerdam
 * Treat very wide fields with no decimals as double.  This is
 * more than 32bit integer fields.
 *
 * Revision 1.57  2005/02/10 20:16:54  fwarmerdam
 * Make the pszStringField buffer for DBFReadAttribute() static char [256]
 * as per bug 306.
 *
 * Revision 1.56  2005/02/10 20:07:56  fwarmerdam
 * Fixed bug 305 in DBFCloneEmpty() - header length problem.
 *
 * Revision 1.55  2004/09/26 20:23:46  fwarmerdam
 * avoid warnings with rcsid and signed/unsigned stuff
 *
 * Revision 1.54  2004/09/15 16:26:10  fwarmerdam
 * Treat all blank numeric fields as null too.
 */

#include "shapefil.h"

#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef USE_CPL
#include "cpl_string.h"
#else

#if defined(WIN32) || defined(_WIN32)
#    define STRCASECMP(a,b)         (stricmp(a,b))
#  else
#include <strings.h>
#    define STRCASECMP(a,b)         (strcasecmp(a,b))
#endif

#if defined(_MSC_VER)
# if _MSC_VER < 1900
#     define snprintf _snprintf
# endif
#elif defined(WIN32) || defined(_WIN32)
#  ifndef snprintf
#     define snprintf _snprintf
#  endif
#endif

#define CPLsprintf sprintf
#define CPLsnprintf snprintf
#endif

SHP_CVSID("$Id$")

#ifndef FALSE
#  define FALSE		0
#  define TRUE		1
#endif

/* File header size */
#define XBASE_FILEHDR_SZ         32

#define HEADER_RECORD_TERMINATOR 0x0D

/* See http://www.manmrk.net/tutorials/database/xbase/dbf.html */
#define END_OF_FILE_CHARACTER    0x1A

#ifdef USE_CPL
CPL_INLINE static void CPL_IGNORE_RET_VAL_INT(CPL_UNUSED int unused) {}
#else
#define CPL_IGNORE_RET_VAL_INT(x) x
#endif

#ifdef __cplusplus
#define STATIC_CAST(type,x) static_cast<type>(x)
#define REINTERPRET_CAST(type,x) reinterpret_cast<type>(x)
#define CONST_CAST(type,x) const_cast<type>(x)
#define SHPLIB_NULLPTR nullptr
#else
#define STATIC_CAST(type,x) ((type)(x))
#define REINTERPRET_CAST(type,x) ((type)(x))
#define CONST_CAST(type,x) ((type)(x))
#define SHPLIB_NULLPTR NULL
#endif

/************************************************************************/
/*                             SfRealloc()                              */
/*                                                                      */
/*      A realloc cover function that will access a NULL pointer as     */
/*      a valid input.                                                  */
/************************************************************************/

static void * SfRealloc( void * pMem, int nNewSize )

{
    if( pMem == SHPLIB_NULLPTR )
        return malloc(nNewSize);
    else
        return realloc(pMem,nNewSize);
}

/************************************************************************/
/*                           DBFWriteHeader()                           */
/*                                                                      */
/*      This is called to write out the file header, and field          */
/*      descriptions before writing any actual data records.  This      */
/*      also computes all the DBFDataSet field offset/size/decimals     */
/*      and so forth values.                                            */
/************************************************************************/

static void DBFWriteHeader(DBFHandle psDBF)

{
    unsigned char	abyHeader[XBASE_FILEHDR_SZ] = { 0 };

    if( !psDBF->bNoHeader )
        return;

    psDBF->bNoHeader = FALSE;

/* -------------------------------------------------------------------- */
/*	Initialize the file header information.				*/
/* -------------------------------------------------------------------- */
    abyHeader[0] = 0x03;		/* memo field? - just copying 	*/

    /* write out update date */
    abyHeader[1] = STATIC_CAST(unsigned char, psDBF->nUpdateYearSince1900);
    abyHeader[2] = STATIC_CAST(unsigned char, psDBF->nUpdateMonth);
    abyHeader[3] = STATIC_CAST(unsigned char, psDBF->nUpdateDay);

    /* record count preset at zero */

    abyHeader[8] = STATIC_CAST(unsigned char, psDBF->nHeaderLength % 256);
    abyHeader[9] = STATIC_CAST(unsigned char, psDBF->nHeaderLength / 256);

    abyHeader[10] = STATIC_CAST(unsigned char, psDBF->nRecordLength % 256);
    abyHeader[11] = STATIC_CAST(unsigned char, psDBF->nRecordLength / 256);

    abyHeader[29] = STATIC_CAST(unsigned char, psDBF->iLanguageDriver);

/* -------------------------------------------------------------------- */
/*      Write the initial 32 byte file header, and all the field        */
/*      descriptions.                                     		*/
/* -------------------------------------------------------------------- */
    psDBF->sHooks.FSeek( psDBF->fp, 0, 0 );
    psDBF->sHooks.FWrite( abyHeader, XBASE_FILEHDR_SZ, 1, psDBF->fp );
    psDBF->sHooks.FWrite( psDBF->pszHeader, XBASE_FLDHDR_SZ, psDBF->nFields,
                          psDBF->fp );

/* -------------------------------------------------------------------- */
/*      Write out the newline character if there is room for it.        */
/* -------------------------------------------------------------------- */
    if( psDBF->nHeaderLength > XBASE_FLDHDR_SZ*psDBF->nFields +
                               XBASE_FLDHDR_SZ )
    {
        char	cNewline;

        cNewline = HEADER_RECORD_TERMINATOR;
        psDBF->sHooks.FWrite( &cNewline, 1, 1, psDBF->fp );
    }

/* -------------------------------------------------------------------- */
/*      If the file is new, add a EOF character.                        */
/* -------------------------------------------------------------------- */
    if( psDBF->nRecords == 0 && psDBF->bWriteEndOfFileChar )
    {
        char ch = END_OF_FILE_CHARACTER;

        psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
    }
}

/************************************************************************/
/*                           DBFFlushRecord()                           */
/*                                                                      */
/*      Write out the current record if there is one.                   */
/************************************************************************/

static int DBFFlushRecord( DBFHandle psDBF )

{
    SAOffset	nRecordOffset;

    if( psDBF->bCurrentRecordModified && psDBF->nCurrentRecord > -1 )
    {
	psDBF->bCurrentRecordModified = FALSE;

	nRecordOffset =
            psDBF->nRecordLength * STATIC_CAST(SAOffset, psDBF->nCurrentRecord)
            + psDBF->nHeaderLength;

	if( psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 ) != 0
            || psDBF->sHooks.FWrite( psDBF->pszCurrentRecord,
                                     psDBF->nRecordLength,
                                     1, psDBF->fp ) != 1 )
        {
            char szMessage[128];
            snprintf( szMessage, sizeof(szMessage), "Failure writing DBF record %d.",
                     psDBF->nCurrentRecord );
            psDBF->sHooks.Error( szMessage );
            return FALSE;
        }

        if( psDBF->nCurrentRecord == psDBF->nRecords - 1 )
        {
            if( psDBF->bWriteEndOfFileChar )
            {
                char ch = END_OF_FILE_CHARACTER;
                psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           DBFLoadRecord()                            */
/************************************************************************/

static int DBFLoadRecord( DBFHandle psDBF, int iRecord )

{
    if( psDBF->nCurrentRecord != iRecord )
    {
        SAOffset nRecordOffset;

	if( !DBFFlushRecord( psDBF ) )
            return FALSE;

	nRecordOffset =
            psDBF->nRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

	if( psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, SEEK_SET ) != 0 )
        {
            char szMessage[128];
            snprintf( szMessage, sizeof(szMessage), "fseek(%ld) failed on DBF file.",
                      STATIC_CAST(long, nRecordOffset) );
            psDBF->sHooks.Error( szMessage );
            return FALSE;
        }

	if( psDBF->sHooks.FRead( psDBF->pszCurrentRecord,
                                 psDBF->nRecordLength, 1, psDBF->fp ) != 1 )
        {
            char szMessage[128];
            snprintf( szMessage, sizeof(szMessage), "fread(%d) failed on DBF file.",
                     psDBF->nRecordLength );
            psDBF->sHooks.Error( szMessage );
            return FALSE;
        }

	psDBF->nCurrentRecord = iRecord;
    }

    return TRUE;
}

/************************************************************************/
/*                          DBFUpdateHeader()                           */
/************************************************************************/

void SHPAPI_CALL
DBFUpdateHeader( DBFHandle psDBF )

{
    unsigned char		abyFileHeader[XBASE_FILEHDR_SZ];

    if( psDBF->bNoHeader )
        DBFWriteHeader( psDBF );

    if( !DBFFlushRecord( psDBF ) )
        return;

    psDBF->sHooks.FSeek( psDBF->fp, 0, 0 );
    psDBF->sHooks.FRead( abyFileHeader, sizeof(abyFileHeader), 1, psDBF->fp );

    abyFileHeader[1] = STATIC_CAST(unsigned char, psDBF->nUpdateYearSince1900);
    abyFileHeader[2] = STATIC_CAST(unsigned char, psDBF->nUpdateMonth);
    abyFileHeader[3] = STATIC_CAST(unsigned char, psDBF->nUpdateDay);
    abyFileHeader[4] = STATIC_CAST(unsigned char, psDBF->nRecords & 0xFF);
    abyFileHeader[5] = STATIC_CAST(unsigned char, (psDBF->nRecords>>8) & 0xFF);
    abyFileHeader[6] = STATIC_CAST(unsigned char, (psDBF->nRecords>>16) & 0xFF);
    abyFileHeader[7] = STATIC_CAST(unsigned char, (psDBF->nRecords>>24) & 0xFF);

    psDBF->sHooks.FSeek( psDBF->fp, 0, 0 );
    psDBF->sHooks.FWrite( abyFileHeader, sizeof(abyFileHeader), 1, psDBF->fp );

    psDBF->sHooks.FFlush( psDBF->fp );
}

/************************************************************************/
/*                       DBFSetLastModifiedDate()                       */
/************************************************************************/

void SHPAPI_CALL
DBFSetLastModifiedDate( DBFHandle psDBF, int nYYSince1900, int nMM, int nDD )
{
    psDBF->nUpdateYearSince1900 = nYYSince1900;
    psDBF->nUpdateMonth = nMM;
    psDBF->nUpdateDay = nDD;
}

/************************************************************************/
/*                              DBFOpen()                               */
/*                                                                      */
/*      Open a .dbf file.                                               */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFOpen( const char * pszFilename, const char * pszAccess )

{
    SAHooks sHooks;

    SASetupDefaultHooks( &sHooks );

    return DBFOpenLL( pszFilename, pszAccess, &sHooks );
}

/************************************************************************/
/*                      DBFGetLenWithoutExtension()                     */
/************************************************************************/

static int DBFGetLenWithoutExtension(const char* pszBasename)
{
    int i;
    int nLen = STATIC_CAST(int, strlen(pszBasename));
    for( i = nLen-1;
         i > 0 && pszBasename[i] != '/' && pszBasename[i] != '\\';
         i-- )
    {
        if( pszBasename[i] == '.' )
        {
            return i;
        }
    }
    return nLen;
}

/************************************************************************/
/*                              DBFOpen()                               */
/*                                                                      */
/*      Open a .dbf file.                                               */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFOpenLL( const char * pszFilename, const char * pszAccess, SAHooks *psHooks )

{
    DBFHandle		psDBF;
    SAFile		pfCPG;
    unsigned char	*pabyBuf;
    int			nFields, nHeadLen, iField;
    char		*pszFullname;
    int                 nBufSize = 500;
    int                 nLenWithoutExtension;

/* -------------------------------------------------------------------- */
/*      We only allow the access strings "rb" and "r+".                  */
/* -------------------------------------------------------------------- */
    if( strcmp(pszAccess,"r") != 0 && strcmp(pszAccess,"r+") != 0
        && strcmp(pszAccess,"rb") != 0 && strcmp(pszAccess,"rb+") != 0
        && strcmp(pszAccess,"r+b") != 0 )
        return SHPLIB_NULLPTR;

    if( strcmp(pszAccess,"r") == 0 )
        pszAccess = "rb";

    if( strcmp(pszAccess,"r+") == 0 )
        pszAccess = "rb+";

/* -------------------------------------------------------------------- */
/*	Compute the base (layer) name.  If there is any extension	*/
/*	on the passed in filename we will strip it off.			*/
/* -------------------------------------------------------------------- */
    nLenWithoutExtension = DBFGetLenWithoutExtension(pszFilename);
    pszFullname = STATIC_CAST(char *, malloc(nLenWithoutExtension + 5));
    memcpy(pszFullname, pszFilename, nLenWithoutExtension);
    memcpy(pszFullname + nLenWithoutExtension, ".dbf", 5);

    psDBF = STATIC_CAST(DBFHandle, calloc( 1, sizeof(DBFInfo) ));
    psDBF->fp = psHooks->FOpen( pszFullname, pszAccess );
    memcpy( &(psDBF->sHooks), psHooks, sizeof(SAHooks) );

    if( psDBF->fp == SHPLIB_NULLPTR )
    {
        memcpy(pszFullname + nLenWithoutExtension, ".DBF", 5);
        psDBF->fp = psDBF->sHooks.FOpen(pszFullname, pszAccess );
    }

    memcpy(pszFullname + nLenWithoutExtension, ".cpg", 5);
    pfCPG = psHooks->FOpen( pszFullname, "r" );
    if( pfCPG == SHPLIB_NULLPTR )
    {
        memcpy(pszFullname + nLenWithoutExtension, ".CPG", 5);
        pfCPG = psHooks->FOpen( pszFullname, "r" );
    }

    free( pszFullname );

    if( psDBF->fp == SHPLIB_NULLPTR )
    {
        free( psDBF );
        if( pfCPG ) psHooks->FClose( pfCPG );
        return SHPLIB_NULLPTR;
    }

    psDBF->bNoHeader = FALSE;
    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;

/* -------------------------------------------------------------------- */
/*  Read Table Header info                                              */
/* -------------------------------------------------------------------- */
    pabyBuf = STATIC_CAST(unsigned char *, malloc(nBufSize));
    if( psDBF->sHooks.FRead( pabyBuf, XBASE_FILEHDR_SZ, 1, psDBF->fp ) != 1 )
    {
        psDBF->sHooks.FClose( psDBF->fp );
        if( pfCPG ) psDBF->sHooks.FClose( pfCPG );
        free( pabyBuf );
        free( psDBF );
        return SHPLIB_NULLPTR;
    }

    DBFSetLastModifiedDate(psDBF, pabyBuf[1], pabyBuf[2], pabyBuf[3]);

    psDBF->nRecords =
     pabyBuf[4]|(pabyBuf[5]<<8)|(pabyBuf[6]<<16)|((pabyBuf[7]&0x7f)<<24);

    psDBF->nHeaderLength = nHeadLen = pabyBuf[8]|(pabyBuf[9]<<8);
    psDBF->nRecordLength = pabyBuf[10]|(pabyBuf[11]<<8);
    psDBF->iLanguageDriver = pabyBuf[29];

    if (psDBF->nRecordLength == 0 || nHeadLen < XBASE_FILEHDR_SZ)
    {
        psDBF->sHooks.FClose( psDBF->fp );
        if( pfCPG ) psDBF->sHooks.FClose( pfCPG );
        free( pabyBuf );
        free( psDBF );
        return SHPLIB_NULLPTR;
    }

    psDBF->nFields = nFields = (nHeadLen - XBASE_FILEHDR_SZ) / XBASE_FLDHDR_SZ;

    /* coverity[tainted_data] */
    psDBF->pszCurrentRecord = STATIC_CAST(char *, malloc(psDBF->nRecordLength));

/* -------------------------------------------------------------------- */
/*  Figure out the code page from the LDID and CPG                      */
/* -------------------------------------------------------------------- */

    psDBF->pszCodePage = SHPLIB_NULLPTR;
    if( pfCPG )
    {
        size_t n;
        memset( pabyBuf, 0, nBufSize);
        psDBF->sHooks.FRead( pabyBuf, nBufSize - 1, 1, pfCPG );
        n = strcspn( REINTERPRET_CAST(char *, pabyBuf), "\n\r" );
        if( n > 0 )
        {
            pabyBuf[n] = '\0';
            psDBF->pszCodePage = STATIC_CAST(char *, malloc(n + 1));
            memcpy( psDBF->pszCodePage, pabyBuf, n + 1 );
        }
		psDBF->sHooks.FClose( pfCPG );
    }
    if( psDBF->pszCodePage == SHPLIB_NULLPTR && pabyBuf[29] != 0 )
    {
        snprintf( REINTERPRET_CAST(char *, pabyBuf), nBufSize, "LDID/%d", psDBF->iLanguageDriver );
        psDBF->pszCodePage = STATIC_CAST(char *, malloc(strlen(REINTERPRET_CAST(char*, pabyBuf)) + 1));
        strcpy( psDBF->pszCodePage, REINTERPRET_CAST(char *, pabyBuf) );
    }

/* -------------------------------------------------------------------- */
/*  Read in Field Definitions                                           */
/* -------------------------------------------------------------------- */

    pabyBuf = STATIC_CAST(unsigned char *, SfRealloc(pabyBuf,nHeadLen));
    psDBF->pszHeader = REINTERPRET_CAST(char *, pabyBuf);

    psDBF->sHooks.FSeek( psDBF->fp, XBASE_FILEHDR_SZ, 0 );
    if( psDBF->sHooks.FRead( pabyBuf, nHeadLen-XBASE_FILEHDR_SZ, 1,
                             psDBF->fp ) != 1 )
    {
        psDBF->sHooks.FClose( psDBF->fp );
        free( pabyBuf );
        free( psDBF->pszCurrentRecord );
        free( psDBF->pszCodePage );
        free( psDBF );
        return SHPLIB_NULLPTR;
    }

    psDBF->panFieldOffset = STATIC_CAST(int *, malloc(sizeof(int) * nFields));
    psDBF->panFieldSize = STATIC_CAST(int *, malloc(sizeof(int) * nFields));
    psDBF->panFieldDecimals = STATIC_CAST(int *, malloc(sizeof(int) * nFields));
    psDBF->pachFieldType = STATIC_CAST(char *, malloc(sizeof(char) * nFields));

    for( iField = 0; iField < nFields; iField++ )
    {
	unsigned char		*pabyFInfo;

	pabyFInfo = pabyBuf+iField*XBASE_FLDHDR_SZ;
        if( pabyFInfo[0] == HEADER_RECORD_TERMINATOR )
        {
            psDBF->nFields = iField;
            break;
        }

	if( pabyFInfo[11] == 'N' || pabyFInfo[11] == 'F' )
	{
	    psDBF->panFieldSize[iField] = pabyFInfo[16];
	    psDBF->panFieldDecimals[iField] = pabyFInfo[17];
	}
	else
	{
	    psDBF->panFieldSize[iField] = pabyFInfo[16];
	    psDBF->panFieldDecimals[iField] = 0;

/*
** The following seemed to be used sometimes to handle files with long
** string fields, but in other cases (such as bug 1202) the decimals field
** just seems to indicate some sort of preferred formatting, not very
** wide fields.  So I have disabled this code.  FrankW.
	    psDBF->panFieldSize[iField] = pabyFInfo[16] + pabyFInfo[17]*256;
	    psDBF->panFieldDecimals[iField] = 0;
*/
	}

	psDBF->pachFieldType[iField] = STATIC_CAST(char, pabyFInfo[11]);
	if( iField == 0 )
	    psDBF->panFieldOffset[iField] = 1;
	else
	    psDBF->panFieldOffset[iField] =
	      psDBF->panFieldOffset[iField-1] + psDBF->panFieldSize[iField-1];
    }

    /* Check that the total width of fields does not exceed the record width */
    if( psDBF->nFields > 0 &&
        psDBF->panFieldOffset[psDBF->nFields-1] +
            psDBF->panFieldSize[psDBF->nFields-1] > psDBF->nRecordLength )
    {
        DBFClose( psDBF );
        return SHPLIB_NULLPTR;
    }

    DBFSetWriteEndOfFileChar( psDBF, TRUE );

    return( psDBF );
}

/************************************************************************/
/*                              DBFClose()                              */
/************************************************************************/

void SHPAPI_CALL
DBFClose(DBFHandle psDBF)
{
    if( psDBF == SHPLIB_NULLPTR )
        return;

/* -------------------------------------------------------------------- */
/*      Write out header if not already written.                        */
/* -------------------------------------------------------------------- */
    if( psDBF->bNoHeader )
        DBFWriteHeader( psDBF );

    CPL_IGNORE_RET_VAL_INT(DBFFlushRecord( psDBF ));

/* -------------------------------------------------------------------- */
/*      Update last access date, and number of records if we have	*/
/*	write access.                					*/
/* -------------------------------------------------------------------- */
    if( psDBF->bUpdated )
        DBFUpdateHeader( psDBF );

/* -------------------------------------------------------------------- */
/*      Close, and free resources.                                      */
/* -------------------------------------------------------------------- */
    psDBF->sHooks.FClose( psDBF->fp );

    if( psDBF->panFieldOffset != SHPLIB_NULLPTR )
    {
        free( psDBF->panFieldOffset );
        free( psDBF->panFieldSize );
        free( psDBF->panFieldDecimals );
        free( psDBF->pachFieldType );
    }

    if( psDBF->pszWorkField != SHPLIB_NULLPTR )
        free( psDBF->pszWorkField );

    free( psDBF->pszHeader );
    free( psDBF->pszCurrentRecord );
    free( psDBF->pszCodePage );

    free( psDBF );
}

/************************************************************************/
/*                             DBFCreate()                              */
/*                                                                      */
/* Create a new .dbf file with default code page LDID/87 (0x57)         */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFCreate( const char * pszFilename )

{
    return DBFCreateEx( pszFilename, "LDID/87" ); // 0x57
}

/************************************************************************/
/*                            DBFCreateEx()                             */
/*                                                                      */
/*      Create a new .dbf file.                                         */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFCreateEx( const char * pszFilename, const char* pszCodePage )

{
    SAHooks sHooks;

    SASetupDefaultHooks( &sHooks );

    return DBFCreateLL( pszFilename, pszCodePage , &sHooks );
}

/************************************************************************/
/*                             DBFCreate()                              */
/*                                                                      */
/*      Create a new .dbf file.                                         */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFCreateLL( const char * pszFilename, const char * pszCodePage, SAHooks *psHooks )

{
    DBFHandle	psDBF;
    SAFile	fp;
    char	*pszFullname;
    int		ldid = -1;
    char chZero = '\0';
    int         nLenWithoutExtension;

/* -------------------------------------------------------------------- */
/*	Compute the base (layer) name.  If there is any extension	*/
/*	on the passed in filename we will strip it off.			*/
/* -------------------------------------------------------------------- */
    nLenWithoutExtension = DBFGetLenWithoutExtension(pszFilename);
    pszFullname = STATIC_CAST(char *, malloc(nLenWithoutExtension + 5));
    memcpy(pszFullname, pszFilename, nLenWithoutExtension);
    memcpy(pszFullname + nLenWithoutExtension, ".dbf", 5);

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    fp = psHooks->FOpen( pszFullname, "wb" );
    if( fp == SHPLIB_NULLPTR )
    {
        free( pszFullname );
        return SHPLIB_NULLPTR;
    }

    psHooks->FWrite( &chZero, 1, 1, fp );
    psHooks->FClose( fp );

    fp = psHooks->FOpen( pszFullname, "rb+" );
    if( fp == SHPLIB_NULLPTR )
    {
        free( pszFullname );
        return SHPLIB_NULLPTR;
    }

    memcpy(pszFullname + nLenWithoutExtension, ".cpg", 5);
    if( pszCodePage != SHPLIB_NULLPTR )
    {
        if( strncmp( pszCodePage, "LDID/", 5 ) == 0 )
        {
            ldid = atoi( pszCodePage + 5 );
            if( ldid > 255 )
                ldid = -1; // don't use 0 to indicate out of range as LDID/0 is a valid one
        }
        if( ldid < 0 )
        {
            SAFile fpCPG = psHooks->FOpen( pszFullname, "w" );
            psHooks->FWrite( CONST_CAST(void*, STATIC_CAST(const void*, pszCodePage)), strlen(pszCodePage), 1, fpCPG );
            psHooks->FClose( fpCPG );
        }
    }
    if( pszCodePage == SHPLIB_NULLPTR || ldid >= 0 )
    {
        psHooks->Remove( pszFullname );
    }

    free( pszFullname );

/* -------------------------------------------------------------------- */
/*	Create the info structure.					*/
/* -------------------------------------------------------------------- */
    psDBF = STATIC_CAST(DBFHandle, calloc(1,sizeof(DBFInfo)));

    memcpy( &(psDBF->sHooks), psHooks, sizeof(SAHooks) );
    psDBF->fp = fp;
    psDBF->nRecords = 0;
    psDBF->nFields = 0;
    psDBF->nRecordLength = 1;
    psDBF->nHeaderLength = XBASE_FILEHDR_SZ + 1; /* + 1 for HEADER_RECORD_TERMINATOR */

    psDBF->panFieldOffset = SHPLIB_NULLPTR;
    psDBF->panFieldSize = SHPLIB_NULLPTR;
    psDBF->panFieldDecimals = SHPLIB_NULLPTR;
    psDBF->pachFieldType = SHPLIB_NULLPTR;
    psDBF->pszHeader = SHPLIB_NULLPTR;

    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;
    psDBF->pszCurrentRecord = SHPLIB_NULLPTR;

    psDBF->bNoHeader = TRUE;

    psDBF->iLanguageDriver = ldid > 0 ? ldid : 0;
    psDBF->pszCodePage = SHPLIB_NULLPTR;
    if( pszCodePage )
    {
        psDBF->pszCodePage = STATIC_CAST(char *, malloc( strlen(pszCodePage) + 1 ));
        strcpy( psDBF->pszCodePage, pszCodePage );
    }
    DBFSetLastModifiedDate(psDBF, 95, 7, 26); /* dummy date */

    DBFSetWriteEndOfFileChar(psDBF, TRUE);

    return( psDBF );
}

/************************************************************************/
/*                            DBFAddField()                             */
/*                                                                      */
/*      Add a field to a newly created .dbf or to an existing one       */
/************************************************************************/

int SHPAPI_CALL
DBFAddField(DBFHandle psDBF, const char * pszFieldName,
            DBFFieldType eType, int nWidth, int nDecimals )

{
    char chNativeType = 'C';

    if( eType == FTLogical )
        chNativeType = 'L';
    else if( eType == FTDate )
	chNativeType = 'D';
    else if( eType == FTString )
        chNativeType = 'C';
    else
        chNativeType = 'N';

    return DBFAddNativeFieldType( psDBF, pszFieldName, chNativeType,
                                  nWidth, nDecimals );
}

/************************************************************************/
/*                        DBFGetNullCharacter()                         */
/************************************************************************/

static char DBFGetNullCharacter(char chType)
{
    switch (chType)
    {
      case 'N':
      case 'F':
        return '*';
      case 'D':
        return '0';
      case 'L':
       return '?';
      default:
       return ' ';
    }
}

/************************************************************************/
/*                            DBFAddField()                             */
/*                                                                      */
/*      Add a field to a newly created .dbf file before any records     */
/*      are written.                                                    */
/************************************************************************/

int SHPAPI_CALL
DBFAddNativeFieldType(DBFHandle psDBF, const char * pszFieldName,
                      char chType, int nWidth, int nDecimals )

{
    char	*pszFInfo;
    int		i;
    int         nOldRecordLength, nOldHeaderLength;
    char        *pszRecord;
    char        chFieldFill;
    SAOffset    nRecordOffset;

    /* make sure that everything is written in .dbf */
    if( !DBFFlushRecord( psDBF ) )
        return -1;

    if( psDBF->nHeaderLength + XBASE_FLDHDR_SZ > 65535 )
    {
        char szMessage[128];
        snprintf( szMessage, sizeof(szMessage),
                  "Cannot add field %s. Header length limit reached "
                  "(max 65535 bytes, 2046 fields).",
                  pszFieldName );
        psDBF->sHooks.Error( szMessage );
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Do some checking to ensure we can add records to this file.     */
/* -------------------------------------------------------------------- */
    if( nWidth < 1 )
        return -1;

    if( nWidth > XBASE_FLD_MAX_WIDTH )
        nWidth = XBASE_FLD_MAX_WIDTH;

    if( psDBF->nRecordLength + nWidth > 65535 )
    {
        char szMessage[128];
        snprintf( szMessage, sizeof(szMessage),
                  "Cannot add field %s. Record length limit reached "
                  "(max 65535 bytes).",
                  pszFieldName );
        psDBF->sHooks.Error( szMessage );
        return -1;
    }

    nOldRecordLength = psDBF->nRecordLength;
    nOldHeaderLength = psDBF->nHeaderLength;

/* -------------------------------------------------------------------- */
/*      SfRealloc all the arrays larger to hold the additional field      */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    psDBF->nFields++;

    psDBF->panFieldOffset = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldOffset, sizeof(int) * psDBF->nFields ));

    psDBF->panFieldSize = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldSize, sizeof(int) * psDBF->nFields ));

    psDBF->panFieldDecimals = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldDecimals, sizeof(int) * psDBF->nFields ));

    psDBF->pachFieldType = STATIC_CAST(char *,
        SfRealloc( psDBF->pachFieldType, sizeof(char) * psDBF->nFields ));

/* -------------------------------------------------------------------- */
/*      Assign the new field information fields.                        */
/* -------------------------------------------------------------------- */
    psDBF->panFieldOffset[psDBF->nFields-1] = psDBF->nRecordLength;
    psDBF->nRecordLength += nWidth;
    psDBF->panFieldSize[psDBF->nFields-1] = nWidth;
    psDBF->panFieldDecimals[psDBF->nFields-1] = nDecimals;
    psDBF->pachFieldType[psDBF->nFields-1] = chType;

/* -------------------------------------------------------------------- */
/*      Extend the required header information.                         */
/* -------------------------------------------------------------------- */
    psDBF->nHeaderLength += XBASE_FLDHDR_SZ;
    psDBF->bUpdated = FALSE;

    psDBF->pszHeader = STATIC_CAST(char *, SfRealloc(psDBF->pszHeader,
                                          psDBF->nFields*XBASE_FLDHDR_SZ));

    pszFInfo = psDBF->pszHeader + XBASE_FLDHDR_SZ * (psDBF->nFields-1);

    for( i = 0; i < XBASE_FLDHDR_SZ; i++ )
        pszFInfo[i] = '\0';

    strncpy( pszFInfo, pszFieldName, XBASE_FLDNAME_LEN_WRITE );

    pszFInfo[11] = psDBF->pachFieldType[psDBF->nFields-1];

    if( chType == 'C' )
    {
        pszFInfo[16] = STATIC_CAST(unsigned char, nWidth % 256);
        pszFInfo[17] = STATIC_CAST(unsigned char, nWidth / 256);
    }
    else
    {
        pszFInfo[16] = STATIC_CAST(unsigned char, nWidth);
        pszFInfo[17] = STATIC_CAST(unsigned char, nDecimals);
    }

/* -------------------------------------------------------------------- */
/*      Make the current record buffer appropriately larger.            */
/* -------------------------------------------------------------------- */
    psDBF->pszCurrentRecord = STATIC_CAST(char *, SfRealloc(psDBF->pszCurrentRecord,
                                                 psDBF->nRecordLength));

    /* we're done if dealing with new .dbf */
    if( psDBF->bNoHeader )
        return( psDBF->nFields - 1 );

/* -------------------------------------------------------------------- */
/*      For existing .dbf file, shift records                           */
/* -------------------------------------------------------------------- */

    /* alloc record */
    pszRecord = STATIC_CAST(char *, malloc(sizeof(char) * psDBF->nRecordLength));

    chFieldFill = DBFGetNullCharacter(chType);

    for (i = psDBF->nRecords-1; i >= 0; --i)
    {
        nRecordOffset = nOldRecordLength * STATIC_CAST(SAOffset, i) + nOldHeaderLength;

        /* load record */
        psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
        psDBF->sHooks.FRead( pszRecord, nOldRecordLength, 1, psDBF->fp );

        /* set new field's value to NULL */
        memset(pszRecord + nOldRecordLength, chFieldFill, nWidth);

        nRecordOffset = psDBF->nRecordLength * STATIC_CAST(SAOffset, i) + psDBF->nHeaderLength;

        /* move record to the new place*/
        psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
        psDBF->sHooks.FWrite( pszRecord, psDBF->nRecordLength, 1, psDBF->fp );
    }

    if( psDBF->bWriteEndOfFileChar )
    {
        char ch = END_OF_FILE_CHARACTER;

        nRecordOffset =
            psDBF->nRecordLength * STATIC_CAST(SAOffset,psDBF->nRecords) + psDBF->nHeaderLength;

        psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
        psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
    }

    /* free record */
    free(pszRecord);

    /* force update of header with new header, record length and new field */
    psDBF->bNoHeader = TRUE;
    DBFUpdateHeader( psDBF );

    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;
    psDBF->bUpdated = TRUE;

    return( psDBF->nFields-1 );
}

/************************************************************************/
/*                          DBFReadAttribute()                          */
/*                                                                      */
/*      Read one of the attribute fields of a record.                   */
/************************************************************************/

static void *DBFReadAttribute(DBFHandle psDBF, int hEntity, int iField,
                              char chReqType )

{
    unsigned char	*pabyRec;
    void	*pReturnField = SHPLIB_NULLPTR;

/* -------------------------------------------------------------------- */
/*      Verify selection.                                               */
/* -------------------------------------------------------------------- */
    if( hEntity < 0 || hEntity >= psDBF->nRecords )
        return SHPLIB_NULLPTR;

    if( iField < 0 || iField >= psDBF->nFields )
        return SHPLIB_NULLPTR;

/* -------------------------------------------------------------------- */
/*	Have we read the record?					*/
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, hEntity ) )
        return SHPLIB_NULLPTR;

    pabyRec = REINTERPRET_CAST(unsigned char *, psDBF->pszCurrentRecord);

/* -------------------------------------------------------------------- */
/*      Ensure we have room to extract the target field.                */
/* -------------------------------------------------------------------- */
    if( psDBF->panFieldSize[iField] >= psDBF->nWorkFieldLength )
    {
        psDBF->nWorkFieldLength = psDBF->panFieldSize[iField] + 100;
        if( psDBF->pszWorkField == SHPLIB_NULLPTR )
            psDBF->pszWorkField = STATIC_CAST(char *, malloc(psDBF->nWorkFieldLength));
        else
            psDBF->pszWorkField = STATIC_CAST(char *, realloc(psDBF->pszWorkField,
                                                   psDBF->nWorkFieldLength));
    }

/* -------------------------------------------------------------------- */
/*	Extract the requested field.					*/
/* -------------------------------------------------------------------- */
    memcpy( psDBF->pszWorkField,
	     REINTERPRET_CAST(const char *, pabyRec) + psDBF->panFieldOffset[iField],
	     psDBF->panFieldSize[iField] );
    psDBF->pszWorkField[psDBF->panFieldSize[iField]] = '\0';

    pReturnField = psDBF->pszWorkField;

/* -------------------------------------------------------------------- */
/*      Decode the field.                                               */
/* -------------------------------------------------------------------- */
    if( chReqType == 'I' )
    {
        psDBF->fieldValue.nIntField = atoi(psDBF->pszWorkField);

        pReturnField = &(psDBF->fieldValue.nIntField);
    }
    else if( chReqType == 'N' )
    {
        psDBF->fieldValue.dfDoubleField = psDBF->sHooks.Atof(psDBF->pszWorkField);

        pReturnField = &(psDBF->fieldValue.dfDoubleField);
    }

/* -------------------------------------------------------------------- */
/*      Should we trim white space off the string attribute value?      */
/* -------------------------------------------------------------------- */
#ifdef TRIM_DBF_WHITESPACE
    else
    {
        char	*pchSrc, *pchDst;

        pchDst = pchSrc = psDBF->pszWorkField;
        while( *pchSrc == ' ' )
            pchSrc++;

        while( *pchSrc != '\0' )
            *(pchDst++) = *(pchSrc++);
        *pchDst = '\0';

        while( pchDst != psDBF->pszWorkField && *(--pchDst) == ' ' )
            *pchDst = '\0';
    }
#endif

    return pReturnField;
}

/************************************************************************/
/*                        DBFReadIntAttribute()                         */
/*                                                                      */
/*      Read an integer attribute.                                      */
/************************************************************************/

int SHPAPI_CALL
DBFReadIntegerAttribute( DBFHandle psDBF, int iRecord, int iField )

{
    int	*pnValue;

    pnValue = STATIC_CAST(int *, DBFReadAttribute( psDBF, iRecord, iField, 'I' ));

    if( pnValue == SHPLIB_NULLPTR )
        return 0;
    else
        return *pnValue;
}

/************************************************************************/
/*                        DBFReadDoubleAttribute()                      */
/*                                                                      */
/*      Read a double attribute.                                        */
/************************************************************************/

double SHPAPI_CALL
DBFReadDoubleAttribute( DBFHandle psDBF, int iRecord, int iField )

{
    double	*pdValue;

    pdValue = STATIC_CAST(double *, DBFReadAttribute( psDBF, iRecord, iField, 'N' ));

    if( pdValue == SHPLIB_NULLPTR )
        return 0.0;
    else
        return *pdValue ;
}

/************************************************************************/
/*                        DBFReadStringAttribute()                      */
/*                                                                      */
/*      Read a string attribute.                                        */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFReadStringAttribute( DBFHandle psDBF, int iRecord, int iField )

{
    return STATIC_CAST(const char *, DBFReadAttribute( psDBF, iRecord, iField, 'C' ) );
}

/************************************************************************/
/*                        DBFReadLogicalAttribute()                     */
/*                                                                      */
/*      Read a logical attribute.                                       */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFReadLogicalAttribute( DBFHandle psDBF, int iRecord, int iField )

{
    return STATIC_CAST(const char *, DBFReadAttribute( psDBF, iRecord, iField, 'L' ) );
}


/************************************************************************/
/*                         DBFIsValueNULL()                             */
/*                                                                      */
/*      Return TRUE if the passed string is NULL.                       */
/************************************************************************/

static int DBFIsValueNULL( char chType, const char* pszValue )
{
    int i;

    if( pszValue == SHPLIB_NULLPTR )
        return TRUE;

    switch(chType)
    {
      case 'N':
      case 'F':
        /*
        ** We accept all asterisks or all blanks as NULL
        ** though according to the spec I think it should be all
        ** asterisks.
        */
        if( pszValue[0] == '*' )
            return TRUE;

        for( i = 0; pszValue[i] != '\0'; i++ )
        {
            if( pszValue[i] != ' ' )
                return FALSE;
        }
        return TRUE;

      case 'D':
        /* NULL date fields have value "00000000" */
        return strncmp(pszValue,"00000000",8) == 0;

      case 'L':
        /* NULL boolean fields have value "?" */
        return pszValue[0] == '?';

      default:
        /* empty string fields are considered NULL */
        return strlen(pszValue) == 0;
    }
}

/************************************************************************/
/*                         DBFIsAttributeNULL()                         */
/*                                                                      */
/*      Return TRUE if value for field is NULL.                         */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/************************************************************************/

int SHPAPI_CALL
DBFIsAttributeNULL( DBFHandle psDBF, int iRecord, int iField )

{
    const char	*pszValue;

    pszValue = DBFReadStringAttribute( psDBF, iRecord, iField );

    if( pszValue == SHPLIB_NULLPTR )
        return TRUE;

    return DBFIsValueNULL( psDBF->pachFieldType[iField], pszValue );
}

/************************************************************************/
/*                          DBFGetFieldCount()                          */
/*                                                                      */
/*      Return the number of fields in this table.                      */
/************************************************************************/

int SHPAPI_CALL
DBFGetFieldCount( DBFHandle psDBF )

{
    return( psDBF->nFields );
}

/************************************************************************/
/*                         DBFGetRecordCount()                          */
/*                                                                      */
/*      Return the number of records in this table.                     */
/************************************************************************/

int SHPAPI_CALL
DBFGetRecordCount( DBFHandle psDBF )

{
    return( psDBF->nRecords );
}

/************************************************************************/
/*                          DBFGetFieldInfo()                           */
/*                                                                      */
/*      Return any requested information about the field.               */
/*      pszFieldName must be at least XBASE_FLDNAME_LEN_READ+1 (=12)    */
/*      bytes long.                                                     */
/************************************************************************/

DBFFieldType SHPAPI_CALL
DBFGetFieldInfo( DBFHandle psDBF, int iField, char * pszFieldName,
                 int * pnWidth, int * pnDecimals )

{
    if( iField < 0 || iField >= psDBF->nFields )
        return( FTInvalid );

    if( pnWidth != SHPLIB_NULLPTR )
        *pnWidth = psDBF->panFieldSize[iField];

    if( pnDecimals != SHPLIB_NULLPTR )
        *pnDecimals = psDBF->panFieldDecimals[iField];

    if( pszFieldName != SHPLIB_NULLPTR )
    {
	int	i;

	strncpy( pszFieldName, STATIC_CAST(char *,psDBF->pszHeader)+iField*XBASE_FLDHDR_SZ,
                 XBASE_FLDNAME_LEN_READ );
	pszFieldName[XBASE_FLDNAME_LEN_READ] = '\0';
	for( i = XBASE_FLDNAME_LEN_READ - 1; i > 0 && pszFieldName[i] == ' '; i-- )
	    pszFieldName[i] = '\0';
    }

    if ( psDBF->pachFieldType[iField] == 'L' )
	return( FTLogical );

    else if( psDBF->pachFieldType[iField] == 'D' )
	return( FTDate );

    else if( psDBF->pachFieldType[iField] == 'N'
             || psDBF->pachFieldType[iField] == 'F' )
    {
	if( psDBF->panFieldDecimals[iField] > 0
            || psDBF->panFieldSize[iField] >= 10 )
	    return( FTDouble );
	else
	    return( FTInteger );
    }
    else
    {
	return( FTString );
    }
}

/************************************************************************/
/*                         DBFWriteAttribute()                          */
/*									*/
/*	Write an attribute record to the file.				*/
/************************************************************************/

static int DBFWriteAttribute(DBFHandle psDBF, int hEntity, int iField,
			     void * pValue )

{
    int	       	i, j, nRetResult = TRUE;
    unsigned char	*pabyRec;
    char	szSField[XBASE_FLD_MAX_WIDTH+1], szFormat[20];

/* -------------------------------------------------------------------- */
/*	Is this a valid record?						*/
/* -------------------------------------------------------------------- */
    if( hEntity < 0 || hEntity > psDBF->nRecords )
        return( FALSE );

    if( psDBF->bNoHeader )
        DBFWriteHeader(psDBF);

/* -------------------------------------------------------------------- */
/*      Is this a brand new record?                                     */
/* -------------------------------------------------------------------- */
    if( hEntity == psDBF->nRecords )
    {
	if( !DBFFlushRecord( psDBF ) )
            return FALSE;

	psDBF->nRecords++;
	for( i = 0; i < psDBF->nRecordLength; i++ )
	    psDBF->pszCurrentRecord[i] = ' ';

	psDBF->nCurrentRecord = hEntity;
    }

/* -------------------------------------------------------------------- */
/*      Is this an existing record, but different than the last one     */
/*      we accessed?                                                    */
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, hEntity ) )
        return FALSE;

    pabyRec = REINTERPRET_CAST(unsigned char *,psDBF->pszCurrentRecord);

    psDBF->bCurrentRecordModified = TRUE;
    psDBF->bUpdated = TRUE;

/* -------------------------------------------------------------------- */
/*      Translate NULL value to valid DBF file representation.          */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/* -------------------------------------------------------------------- */
    if( pValue == SHPLIB_NULLPTR )
    {
        memset( pabyRec+psDBF->panFieldOffset[iField],
                DBFGetNullCharacter(psDBF->pachFieldType[iField]),
                psDBF->panFieldSize[iField] );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Assign all the record fields.                                   */
/* -------------------------------------------------------------------- */
    switch( psDBF->pachFieldType[iField] )
    {
      case 'D':
      case 'N':
      case 'F':
      {
        int		nWidth = psDBF->panFieldSize[iField];

        if( STATIC_CAST(int,sizeof(szSField))-2 < nWidth )
            nWidth = sizeof(szSField)-2;

        snprintf( szFormat, sizeof(szFormat), "%%%d.%df",
                    nWidth, psDBF->panFieldDecimals[iField] );
        CPLsnprintf(szSField, sizeof(szSField), szFormat, *STATIC_CAST(double *, pValue) );
        szSField[sizeof(szSField)-1] = '\0';
        if( STATIC_CAST(int,strlen(szSField)) > psDBF->panFieldSize[iField] )
        {
            szSField[psDBF->panFieldSize[iField]] = '\0';
            nRetResult = FALSE;
        }
        memcpy(REINTERPRET_CAST(char *, pabyRec+psDBF->panFieldOffset[iField]),
            szSField, strlen(szSField) );
        break;
      }

      case 'L':
        if (psDBF->panFieldSize[iField] >= 1  &&
            (*STATIC_CAST(char*,pValue) == 'F' || *STATIC_CAST(char*,pValue) == 'T'))
            *(pabyRec+psDBF->panFieldOffset[iField]) = *STATIC_CAST(char*,pValue);
        break;

      default:
	if( STATIC_CAST(int, strlen(STATIC_CAST(char *,pValue))) > psDBF->panFieldSize[iField] )
        {
	    j = psDBF->panFieldSize[iField];
            nRetResult = FALSE;
        }
	else
        {
            memset( pabyRec+psDBF->panFieldOffset[iField], ' ',
                    psDBF->panFieldSize[iField] );
	    j = STATIC_CAST(int, strlen(STATIC_CAST(char *,pValue)));
        }

	strncpy(REINTERPRET_CAST(char *, pabyRec+psDBF->panFieldOffset[iField]),
		STATIC_CAST(const char *, pValue), j );
	break;
    }

    return( nRetResult );
}

/************************************************************************/
/*                     DBFWriteAttributeDirectly()                      */
/*                                                                      */
/*      Write an attribute record to the file, but without any          */
/*      reformatting based on type.  The provided buffer is written     */
/*      as is to the field position in the record.                      */
/************************************************************************/

int SHPAPI_CALL
DBFWriteAttributeDirectly(DBFHandle psDBF, int hEntity, int iField,
                              void * pValue )

{
    int	       		i, j;
    unsigned char	*pabyRec;

/* -------------------------------------------------------------------- */
/*	Is this a valid record?						*/
/* -------------------------------------------------------------------- */
    if( hEntity < 0 || hEntity > psDBF->nRecords )
        return( FALSE );

    if( psDBF->bNoHeader )
        DBFWriteHeader(psDBF);

/* -------------------------------------------------------------------- */
/*      Is this a brand new record?                                     */
/* -------------------------------------------------------------------- */
    if( hEntity == psDBF->nRecords )
    {
	if( !DBFFlushRecord( psDBF ) )
            return FALSE;

	psDBF->nRecords++;
	for( i = 0; i < psDBF->nRecordLength; i++ )
	    psDBF->pszCurrentRecord[i] = ' ';

	psDBF->nCurrentRecord = hEntity;
    }

/* -------------------------------------------------------------------- */
/*      Is this an existing record, but different than the last one     */
/*      we accessed?                                                    */
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, hEntity ) )
        return FALSE;

    pabyRec = REINTERPRET_CAST(unsigned char *, psDBF->pszCurrentRecord);

/* -------------------------------------------------------------------- */
/*      Assign all the record fields.                                   */
/* -------------------------------------------------------------------- */
    if( STATIC_CAST(int, strlen(STATIC_CAST(char *, pValue))) > psDBF->panFieldSize[iField] )
        j = psDBF->panFieldSize[iField];
    else
    {
        memset( pabyRec+psDBF->panFieldOffset[iField], ' ',
                psDBF->panFieldSize[iField] );
        j = STATIC_CAST(int, strlen(STATIC_CAST(char *, pValue)));
    }

    strncpy(REINTERPRET_CAST(char *, pabyRec+psDBF->panFieldOffset[iField]),
            STATIC_CAST(const char *, pValue), j );

    psDBF->bCurrentRecordModified = TRUE;
    psDBF->bUpdated = TRUE;

    return( TRUE );
}

/************************************************************************/
/*                      DBFWriteDoubleAttribute()                       */
/*                                                                      */
/*      Write a double attribute.                                       */
/************************************************************************/

int SHPAPI_CALL
DBFWriteDoubleAttribute( DBFHandle psDBF, int iRecord, int iField,
                         double dValue )

{
    return( DBFWriteAttribute( psDBF, iRecord, iField, STATIC_CAST(void *, &dValue) ) );
}

/************************************************************************/
/*                      DBFWriteIntegerAttribute()                      */
/*                                                                      */
/*      Write a integer attribute.                                      */
/************************************************************************/

int SHPAPI_CALL
DBFWriteIntegerAttribute( DBFHandle psDBF, int iRecord, int iField,
                          int nValue )

{
    double	dValue = nValue;

    return( DBFWriteAttribute( psDBF, iRecord, iField, STATIC_CAST(void *, &dValue) ) );
}

/************************************************************************/
/*                      DBFWriteStringAttribute()                       */
/*                                                                      */
/*      Write a string attribute.                                       */
/************************************************************************/

int SHPAPI_CALL
DBFWriteStringAttribute( DBFHandle psDBF, int iRecord, int iField,
                         const char * pszValue )

{
    return( DBFWriteAttribute( psDBF, iRecord, iField, STATIC_CAST(void *, CONST_CAST(char*, pszValue))) );
}

/************************************************************************/
/*                      DBFWriteNULLAttribute()                         */
/*                                                                      */
/*      Write a string attribute.                                       */
/************************************************************************/

int SHPAPI_CALL
DBFWriteNULLAttribute( DBFHandle psDBF, int iRecord, int iField )

{
    return( DBFWriteAttribute( psDBF, iRecord, iField, SHPLIB_NULLPTR ) );
}

/************************************************************************/
/*                      DBFWriteLogicalAttribute()                      */
/*                                                                      */
/*      Write a logical attribute.                                      */
/************************************************************************/

int SHPAPI_CALL
DBFWriteLogicalAttribute( DBFHandle psDBF, int iRecord, int iField,
		       const char lValue)

{
    return( DBFWriteAttribute( psDBF, iRecord, iField, STATIC_CAST(void *, CONST_CAST(char*, &lValue)) ) );
}

/************************************************************************/
/*                         DBFWriteTuple()                              */
/*									*/
/*	Write an attribute record to the file.				*/
/************************************************************************/

int SHPAPI_CALL
DBFWriteTuple(DBFHandle psDBF, int hEntity, void * pRawTuple )

{
    int	       		i;
    unsigned char	*pabyRec;

/* -------------------------------------------------------------------- */
/*	Is this a valid record?						*/
/* -------------------------------------------------------------------- */
    if( hEntity < 0 || hEntity > psDBF->nRecords )
        return( FALSE );

    if( psDBF->bNoHeader )
        DBFWriteHeader(psDBF);

/* -------------------------------------------------------------------- */
/*      Is this a brand new record?                                     */
/* -------------------------------------------------------------------- */
    if( hEntity == psDBF->nRecords )
    {
	if( !DBFFlushRecord( psDBF ) )
            return FALSE;

	psDBF->nRecords++;
	for( i = 0; i < psDBF->nRecordLength; i++ )
	    psDBF->pszCurrentRecord[i] = ' ';

	psDBF->nCurrentRecord = hEntity;
    }

/* -------------------------------------------------------------------- */
/*      Is this an existing record, but different than the last one     */
/*      we accessed?                                                    */
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, hEntity ) )
        return FALSE;

    pabyRec = REINTERPRET_CAST(unsigned char *, psDBF->pszCurrentRecord);

    memcpy ( pabyRec, pRawTuple,  psDBF->nRecordLength );

    psDBF->bCurrentRecordModified = TRUE;
    psDBF->bUpdated = TRUE;

    return( TRUE );
}

/************************************************************************/
/*                            DBFReadTuple()                            */
/*                                                                      */
/*      Read a complete record.  Note that the result is only valid     */
/*      till the next record read for any reason.                       */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFReadTuple(DBFHandle psDBF, int hEntity )

{
    if( hEntity < 0 || hEntity >= psDBF->nRecords )
        return SHPLIB_NULLPTR;

    if( !DBFLoadRecord( psDBF, hEntity ) )
        return SHPLIB_NULLPTR;

    return STATIC_CAST(const char *, psDBF->pszCurrentRecord);
}

/************************************************************************/
/*                          DBFCloneEmpty()                              */
/*                                                                      */
/*      Read one of the attribute fields of a record.                   */
/************************************************************************/

DBFHandle SHPAPI_CALL
DBFCloneEmpty(DBFHandle psDBF, const char * pszFilename )
{
    DBFHandle	newDBF;

   newDBF = DBFCreateEx ( pszFilename, psDBF->pszCodePage );
   if ( newDBF == SHPLIB_NULLPTR ) return SHPLIB_NULLPTR;

   newDBF->nFields = psDBF->nFields;
   newDBF->nRecordLength = psDBF->nRecordLength;
   newDBF->nHeaderLength = psDBF->nHeaderLength;

   if( psDBF->pszHeader )
   {
        newDBF->pszHeader = STATIC_CAST(char *, malloc ( XBASE_FLDHDR_SZ * psDBF->nFields ));
        memcpy ( newDBF->pszHeader, psDBF->pszHeader, XBASE_FLDHDR_SZ * psDBF->nFields );
   }

   newDBF->panFieldOffset = STATIC_CAST(int *, malloc ( sizeof(int) * psDBF->nFields ));
   memcpy ( newDBF->panFieldOffset, psDBF->panFieldOffset, sizeof(int) * psDBF->nFields );
   newDBF->panFieldSize = STATIC_CAST(int *, malloc ( sizeof(int) * psDBF->nFields ));
   memcpy ( newDBF->panFieldSize, psDBF->panFieldSize, sizeof(int) * psDBF->nFields );
   newDBF->panFieldDecimals = STATIC_CAST(int *, malloc ( sizeof(int) * psDBF->nFields ));
   memcpy ( newDBF->panFieldDecimals, psDBF->panFieldDecimals, sizeof(int) * psDBF->nFields );
   newDBF->pachFieldType = STATIC_CAST(char *, malloc ( sizeof(char) * psDBF->nFields ));
   memcpy ( newDBF->pachFieldType, psDBF->pachFieldType, sizeof(char)*psDBF->nFields );

   newDBF->bNoHeader = TRUE;
   newDBF->bUpdated = TRUE;
   newDBF->bWriteEndOfFileChar = psDBF->bWriteEndOfFileChar;

   DBFWriteHeader ( newDBF );
   DBFClose ( newDBF );

   newDBF = DBFOpen ( pszFilename, "rb+" );
   newDBF->bWriteEndOfFileChar = psDBF->bWriteEndOfFileChar;

   return ( newDBF );
}

/************************************************************************/
/*                       DBFGetNativeFieldType()                        */
/*                                                                      */
/*      Return the DBase field type for the specified field.            */
/*                                                                      */
/*      Value can be one of: 'C' (String), 'D' (Date), 'F' (Float),     */
/*                           'N' (Numeric, with or without decimal),    */
/*                           'L' (Logical),                             */
/*                           'M' (Memo: 10 digits .DBT block ptr)       */
/************************************************************************/

char SHPAPI_CALL
DBFGetNativeFieldType( DBFHandle psDBF, int iField )

{
    if( iField >=0 && iField < psDBF->nFields )
        return psDBF->pachFieldType[iField];

    return  ' ';
}

/************************************************************************/
/*                          DBFGetFieldIndex()                          */
/*                                                                      */
/*      Get the index number for a field in a .dbf file.                */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/************************************************************************/

int SHPAPI_CALL
DBFGetFieldIndex(DBFHandle psDBF, const char *pszFieldName)

{
    char          name[XBASE_FLDNAME_LEN_READ+1];
    int           i;

    for( i = 0; i < DBFGetFieldCount(psDBF); i++ )
    {
        DBFGetFieldInfo( psDBF, i, name, SHPLIB_NULLPTR, SHPLIB_NULLPTR );
        if(!STRCASECMP(pszFieldName,name))
            return(i);
    }
    return(-1);
}

/************************************************************************/
/*                         DBFIsRecordDeleted()                         */
/*                                                                      */
/*      Returns TRUE if the indicated record is deleted, otherwise      */
/*      it returns FALSE.                                               */
/************************************************************************/

int SHPAPI_CALL DBFIsRecordDeleted( DBFHandle psDBF, int iShape )

{
/* -------------------------------------------------------------------- */
/*      Verify selection.                                               */
/* -------------------------------------------------------------------- */
    if( iShape < 0 || iShape >= psDBF->nRecords )
        return TRUE;

/* -------------------------------------------------------------------- */
/*	Have we read the record?					*/
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, iShape ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      '*' means deleted.                                              */
/* -------------------------------------------------------------------- */
    return psDBF->pszCurrentRecord[0] == '*';
}

/************************************************************************/
/*                        DBFMarkRecordDeleted()                        */
/************************************************************************/

int SHPAPI_CALL DBFMarkRecordDeleted( DBFHandle psDBF, int iShape,
                                      int bIsDeleted )

{
    char chNewFlag;

/* -------------------------------------------------------------------- */
/*      Verify selection.                                               */
/* -------------------------------------------------------------------- */
    if( iShape < 0 || iShape >= psDBF->nRecords )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Is this an existing record, but different than the last one     */
/*      we accessed?                                                    */
/* -------------------------------------------------------------------- */
    if( !DBFLoadRecord( psDBF, iShape ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Assign value, marking record as dirty if it changes.            */
/* -------------------------------------------------------------------- */
    if( bIsDeleted )
        chNewFlag = '*';
    else
        chNewFlag = ' ';

    if( psDBF->pszCurrentRecord[0] != chNewFlag )
    {
        psDBF->bCurrentRecordModified = TRUE;
        psDBF->bUpdated = TRUE;
        psDBF->pszCurrentRecord[0] = chNewFlag;
    }

    return TRUE;
}

/************************************************************************/
/*                            DBFGetCodePage                            */
/************************************************************************/

const char SHPAPI_CALL1(*)
DBFGetCodePage(DBFHandle psDBF )
{
    if( psDBF == SHPLIB_NULLPTR )
        return SHPLIB_NULLPTR;
    return psDBF->pszCodePage;
}

/************************************************************************/
/*                          DBFDeleteField()                            */
/*                                                                      */
/*      Remove a field from a .dbf file                                 */
/************************************************************************/

int SHPAPI_CALL
DBFDeleteField(DBFHandle psDBF, int iField)
{
    int nOldRecordLength, nOldHeaderLength;
    int nDeletedFieldOffset, nDeletedFieldSize;
    SAOffset nRecordOffset;
    char* pszRecord;
    int i, iRecord;

    if (iField < 0 || iField >= psDBF->nFields)
        return FALSE;

    /* make sure that everything is written in .dbf */
    if( !DBFFlushRecord( psDBF ) )
        return FALSE;

    /* get information about field to be deleted */
    nOldRecordLength = psDBF->nRecordLength;
    nOldHeaderLength = psDBF->nHeaderLength;
    nDeletedFieldOffset = psDBF->panFieldOffset[iField];
    nDeletedFieldSize = psDBF->panFieldSize[iField];

    /* update fields info */
    for (i = iField + 1; i < psDBF->nFields; i++)
    {
        psDBF->panFieldOffset[i-1] = psDBF->panFieldOffset[i] - nDeletedFieldSize;
        psDBF->panFieldSize[i-1] = psDBF->panFieldSize[i];
        psDBF->panFieldDecimals[i-1] = psDBF->panFieldDecimals[i];
        psDBF->pachFieldType[i-1] = psDBF->pachFieldType[i];
    }

    /* resize fields arrays */
    psDBF->nFields--;

    psDBF->panFieldOffset = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldOffset, sizeof(int) * psDBF->nFields ));

    psDBF->panFieldSize = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldSize, sizeof(int) * psDBF->nFields ));

    psDBF->panFieldDecimals = STATIC_CAST(int *,
        SfRealloc( psDBF->panFieldDecimals, sizeof(int) * psDBF->nFields ));

    psDBF->pachFieldType = STATIC_CAST(char *,
        SfRealloc( psDBF->pachFieldType, sizeof(char) * psDBF->nFields ));

    /* update header information */
    psDBF->nHeaderLength -= XBASE_FLDHDR_SZ;
    psDBF->nRecordLength -= nDeletedFieldSize;

    /* overwrite field information in header */
    memmove(psDBF->pszHeader + iField*XBASE_FLDHDR_SZ,
           psDBF->pszHeader + (iField+1)*XBASE_FLDHDR_SZ,
           sizeof(char) * (psDBF->nFields - iField)*XBASE_FLDHDR_SZ);

    psDBF->pszHeader = STATIC_CAST(char *, SfRealloc(psDBF->pszHeader,
                                          psDBF->nFields*XBASE_FLDHDR_SZ));

    /* update size of current record appropriately */
    psDBF->pszCurrentRecord = STATIC_CAST(char *, SfRealloc(psDBF->pszCurrentRecord,
                                                 psDBF->nRecordLength));

    /* we're done if we're dealing with not yet created .dbf */
    if ( psDBF->bNoHeader && psDBF->nRecords == 0 )
        return TRUE;

    /* force update of header with new header and record length */
    psDBF->bNoHeader = TRUE;
    DBFUpdateHeader( psDBF );

    /* alloc record */
    pszRecord = STATIC_CAST(char *, malloc(sizeof(char) * nOldRecordLength));

    /* shift records to their new positions */
    for (iRecord = 0; iRecord < psDBF->nRecords; iRecord++)
    {
        nRecordOffset =
            nOldRecordLength * STATIC_CAST(SAOffset,iRecord) + nOldHeaderLength;

        /* load record */
        psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
        psDBF->sHooks.FRead( pszRecord, nOldRecordLength, 1, psDBF->fp );

        nRecordOffset =
            psDBF->nRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

        /* move record in two steps */
        psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
        psDBF->sHooks.FWrite( pszRecord, nDeletedFieldOffset, 1, psDBF->fp );
        psDBF->sHooks.FWrite( pszRecord + nDeletedFieldOffset + nDeletedFieldSize,
                              nOldRecordLength - nDeletedFieldOffset - nDeletedFieldSize,
                              1, psDBF->fp );

    }

    if( psDBF->bWriteEndOfFileChar )
    {
        char ch = END_OF_FILE_CHARACTER;
        SAOffset nEOFOffset =
            psDBF->nRecordLength * STATIC_CAST(SAOffset,psDBF->nRecords) + psDBF->nHeaderLength;

        psDBF->sHooks.FSeek( psDBF->fp, nEOFOffset, 0 );
        psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
    }

    /* TODO: truncate file */

    /* free record */
    free(pszRecord);

    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;
    psDBF->bUpdated = TRUE;

    return TRUE;
}

/************************************************************************/
/*                          DBFReorderFields()                          */
/*                                                                      */
/*      Reorder the fields of a .dbf file                               */
/*                                                                      */
/* panMap must be exactly psDBF->nFields long and be a permutation      */
/* of [0, psDBF->nFields-1]. This assumption will not be asserted in the*/
/* code of DBFReorderFields.                                            */
/************************************************************************/

int SHPAPI_CALL
DBFReorderFields( DBFHandle psDBF, int* panMap )
{
    SAOffset nRecordOffset;
    int      i, iRecord;
    int     *panFieldOffsetNew;
    int     *panFieldSizeNew;
    int     *panFieldDecimalsNew;
    char    *pachFieldTypeNew;
    char    *pszHeaderNew;
    char    *pszRecord;
    char    *pszRecordNew;

    if ( psDBF->nFields == 0 )
        return TRUE;

    /* make sure that everything is written in .dbf */
    if( !DBFFlushRecord( psDBF ) )
        return FALSE;

    /* a simple malloc() would be enough, but calloc() helps clang static analyzer */
    panFieldOffsetNew = STATIC_CAST(int *, calloc(sizeof(int), psDBF->nFields));
    panFieldSizeNew = STATIC_CAST(int *, calloc(sizeof(int),  psDBF->nFields));
    panFieldDecimalsNew = STATIC_CAST(int *, calloc(sizeof(int), psDBF->nFields));
    pachFieldTypeNew = STATIC_CAST(char *, calloc(sizeof(char), psDBF->nFields));
    pszHeaderNew = STATIC_CAST(char*, malloc(sizeof(char) * XBASE_FLDHDR_SZ * 
                                  psDBF->nFields));

    /* shuffle fields definitions */
    for(i=0; i < psDBF->nFields; i++)
    {
        panFieldSizeNew[i] = psDBF->panFieldSize[panMap[i]];
        panFieldDecimalsNew[i] = psDBF->panFieldDecimals[panMap[i]];
        pachFieldTypeNew[i] = psDBF->pachFieldType[panMap[i]];
        memcpy(pszHeaderNew + i * XBASE_FLDHDR_SZ,
               psDBF->pszHeader + panMap[i] * XBASE_FLDHDR_SZ, XBASE_FLDHDR_SZ);
    }
    panFieldOffsetNew[0] = 1;
    for(i=1; i < psDBF->nFields; i++)
    {
        panFieldOffsetNew[i] = panFieldOffsetNew[i - 1] + panFieldSizeNew[i - 1];
    }

    free(psDBF->pszHeader);
    psDBF->pszHeader = pszHeaderNew;

    /* we're done if we're dealing with not yet created .dbf */
    if ( !(psDBF->bNoHeader && psDBF->nRecords == 0) )
    {
        /* force update of header with new header and record length */
        psDBF->bNoHeader = TRUE;
        DBFUpdateHeader( psDBF );

        /* alloc record */
        pszRecord = STATIC_CAST(char *, malloc(sizeof(char) * psDBF->nRecordLength));
        pszRecordNew = STATIC_CAST(char *, malloc(sizeof(char) * psDBF->nRecordLength));

        /* shuffle fields in records */
        for (iRecord = 0; iRecord < psDBF->nRecords; iRecord++)
        {
            nRecordOffset =
                psDBF->nRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

            /* load record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FRead( pszRecord, psDBF->nRecordLength, 1, psDBF->fp );

            pszRecordNew[0] = pszRecord[0];

            for(i=0; i < psDBF->nFields; i++)
            {
                memcpy(pszRecordNew + panFieldOffsetNew[i],
                       pszRecord + psDBF->panFieldOffset[panMap[i]],
                       psDBF->panFieldSize[panMap[i]]);
            }

            /* write record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FWrite( pszRecordNew, psDBF->nRecordLength, 1, psDBF->fp );
        }

        /* free record */
        free(pszRecord);
        free(pszRecordNew);
    }

    free(psDBF->panFieldOffset);
    free(psDBF->panFieldSize);
    free(psDBF->panFieldDecimals);
    free(psDBF->pachFieldType);

    psDBF->panFieldOffset = panFieldOffsetNew;
    psDBF->panFieldSize = panFieldSizeNew;
    psDBF->panFieldDecimals =panFieldDecimalsNew;
    psDBF->pachFieldType = pachFieldTypeNew;

    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;
    psDBF->bUpdated = TRUE;

    return TRUE;
}


/************************************************************************/
/*                          DBFAlterFieldDefn()                         */
/*                                                                      */
/*      Alter a field definition in a .dbf file                         */
/************************************************************************/

int SHPAPI_CALL
DBFAlterFieldDefn( DBFHandle psDBF, int iField, const char * pszFieldName,
                    char chType, int nWidth, int nDecimals )
{
    int   i;
    int   iRecord;
    int   nOffset;
    int   nOldWidth;
    int   nOldRecordLength;
    SAOffset  nRecordOffset;
    char* pszFInfo;
    char  chOldType;
    int   bIsNULL;
    char chFieldFill;

    if (iField < 0 || iField >= psDBF->nFields)
        return FALSE;

    /* make sure that everything is written in .dbf */
    if( !DBFFlushRecord( psDBF ) )
        return FALSE;

    chFieldFill = DBFGetNullCharacter(chType);

    chOldType = psDBF->pachFieldType[iField];
    nOffset = psDBF->panFieldOffset[iField];
    nOldWidth = psDBF->panFieldSize[iField];
    nOldRecordLength = psDBF->nRecordLength;

/* -------------------------------------------------------------------- */
/*      Do some checking to ensure we can add records to this file.     */
/* -------------------------------------------------------------------- */
    if( nWidth < 1 )
        return -1;

    if( nWidth > XBASE_FLD_MAX_WIDTH )
        nWidth = XBASE_FLD_MAX_WIDTH;

/* -------------------------------------------------------------------- */
/*      Assign the new field information fields.                        */
/* -------------------------------------------------------------------- */
    psDBF->panFieldSize[iField] = nWidth;
    psDBF->panFieldDecimals[iField] = nDecimals;
    psDBF->pachFieldType[iField] = chType;

/* -------------------------------------------------------------------- */
/*      Update the header information.                                  */
/* -------------------------------------------------------------------- */
    pszFInfo = psDBF->pszHeader + XBASE_FLDHDR_SZ * iField;

    for( i = 0; i < XBASE_FLDHDR_SZ; i++ )
        pszFInfo[i] = '\0';

    strncpy( pszFInfo, pszFieldName, XBASE_FLDNAME_LEN_WRITE );

    pszFInfo[11] = psDBF->pachFieldType[iField];

    if( chType == 'C' )
    {
        pszFInfo[16] = STATIC_CAST(unsigned char, nWidth % 256);
        pszFInfo[17] = STATIC_CAST(unsigned char, nWidth / 256);
    }
    else
    {
        pszFInfo[16] = STATIC_CAST(unsigned char, nWidth);
        pszFInfo[17] = STATIC_CAST(unsigned char, nDecimals);
    }

/* -------------------------------------------------------------------- */
/*      Update offsets                                                  */
/* -------------------------------------------------------------------- */
    if (nWidth != nOldWidth)
    {
        for (i = iField + 1; i < psDBF->nFields; i++)
             psDBF->panFieldOffset[i] += nWidth - nOldWidth;
        psDBF->nRecordLength += nWidth - nOldWidth;

        psDBF->pszCurrentRecord = STATIC_CAST(char *, SfRealloc(psDBF->pszCurrentRecord,
                                                     psDBF->nRecordLength));
    }

    /* we're done if we're dealing with not yet created .dbf */
    if ( psDBF->bNoHeader && psDBF->nRecords == 0 )
        return TRUE;

    /* force update of header with new header and record length */
    psDBF->bNoHeader = TRUE;
    DBFUpdateHeader( psDBF );

    if (nWidth < nOldWidth || (nWidth == nOldWidth && chType != chOldType))
    {
        char* pszRecord = STATIC_CAST(char *, malloc(sizeof(char) * nOldRecordLength));
        char* pszOldField = STATIC_CAST(char *, malloc(sizeof(char) * (nOldWidth + 1)));

        /* cppcheck-suppress uninitdata */
        pszOldField[nOldWidth] = 0;

        /* move records to their new positions */
        for (iRecord = 0; iRecord < psDBF->nRecords; iRecord++)
        {
            nRecordOffset =
                nOldRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

            /* load record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FRead( pszRecord, nOldRecordLength, 1, psDBF->fp );

            memcpy(pszOldField, pszRecord + nOffset, nOldWidth);
            bIsNULL = DBFIsValueNULL( chOldType, pszOldField );

            if (nWidth != nOldWidth)
            {
                if ((chOldType == 'N' || chOldType == 'F' || chOldType == 'D') && pszOldField[0] == ' ')
                {
                    /* Strip leading spaces when truncating a numeric field */
                    memmove( pszRecord + nOffset,
                            pszRecord + nOffset + nOldWidth - nWidth,
                            nWidth );
                }
                if (nOffset + nOldWidth < nOldRecordLength)
                {
                    memmove( pszRecord + nOffset + nWidth,
                            pszRecord + nOffset + nOldWidth,
                            nOldRecordLength - (nOffset + nOldWidth));
                }
            }

            /* Convert null value to the appropriate value of the new type */
            if (bIsNULL)
            {
                memset( pszRecord + nOffset, chFieldFill, nWidth);
            }

            nRecordOffset =
                psDBF->nRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

            /* write record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FWrite( pszRecord, psDBF->nRecordLength, 1, psDBF->fp );
        }

        if( psDBF->bWriteEndOfFileChar )
        {
            char ch = END_OF_FILE_CHARACTER;

            nRecordOffset =
                psDBF->nRecordLength * STATIC_CAST(SAOffset,psDBF->nRecords) + psDBF->nHeaderLength;

            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
        }
        /* TODO: truncate file */

        free(pszRecord);
        free(pszOldField);
    }
    else if (nWidth > nOldWidth)
    {
        char* pszRecord = STATIC_CAST(char *, malloc(sizeof(char) * psDBF->nRecordLength));
        char* pszOldField = STATIC_CAST(char *, malloc(sizeof(char) * (nOldWidth + 1)));

        /* cppcheck-suppress uninitdata */
        pszOldField[nOldWidth] = 0;

        /* move records to their new positions */
        for (iRecord = psDBF->nRecords - 1; iRecord >= 0; iRecord--)
        {
            nRecordOffset =
                nOldRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

            /* load record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FRead( pszRecord, nOldRecordLength, 1, psDBF->fp );

            memcpy(pszOldField, pszRecord + nOffset, nOldWidth);
            bIsNULL = DBFIsValueNULL( chOldType, pszOldField );

            if (nOffset + nOldWidth < nOldRecordLength)
            {
                memmove( pszRecord + nOffset + nWidth,
                         pszRecord + nOffset + nOldWidth,
                         nOldRecordLength - (nOffset + nOldWidth));
            }

            /* Convert null value to the appropriate value of the new type */
            if (bIsNULL)
            {
                memset( pszRecord + nOffset, chFieldFill, nWidth);
            }
            else
            {
                if ((chOldType == 'N' || chOldType == 'F'))
                {
                    /* Add leading spaces when expanding a numeric field */
                    memmove( pszRecord + nOffset + nWidth - nOldWidth,
                             pszRecord + nOffset, nOldWidth );
                    memset( pszRecord + nOffset, ' ', nWidth - nOldWidth );
                }
                else
                {
                    /* Add trailing spaces */
                    memset(pszRecord + nOffset + nOldWidth, ' ', nWidth - nOldWidth);
                }
            }

            nRecordOffset =
                psDBF->nRecordLength * STATIC_CAST(SAOffset,iRecord) + psDBF->nHeaderLength;

            /* write record */
            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FWrite( pszRecord, psDBF->nRecordLength, 1, psDBF->fp );
        }

        if( psDBF->bWriteEndOfFileChar )
        {
            char ch = END_OF_FILE_CHARACTER;

            nRecordOffset =
                psDBF->nRecordLength * STATIC_CAST(SAOffset,psDBF->nRecords) + psDBF->nHeaderLength;

            psDBF->sHooks.FSeek( psDBF->fp, nRecordOffset, 0 );
            psDBF->sHooks.FWrite( &ch, 1, 1, psDBF->fp );
        }

        free(pszRecord);
        free(pszOldField);
    }

    psDBF->nCurrentRecord = -1;
    psDBF->bCurrentRecordModified = FALSE;
    psDBF->bUpdated = TRUE;

    return TRUE;
}

/************************************************************************/
/*                    DBFSetWriteEndOfFileChar()                        */
/************************************************************************/

void SHPAPI_CALL DBFSetWriteEndOfFileChar( DBFHandle psDBF, int bWriteFlag )
{
    psDBF->bWriteEndOfFileChar = bWriteFlag;
}
