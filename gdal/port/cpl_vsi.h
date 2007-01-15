/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Include file defining Virtual File System (VSI) functions, a
 *           layer over POSIX file and other system services. 
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * $Log$
 * Revision 1.31  2006/03/27 15:24:41  fwarmerdam
 * buffer in FWrite is const
 *
 * Revision 1.30  2006/03/21 20:11:54  fwarmerdam
 * fixup headers a bit
 *
 * Revision 1.29  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.28  2006/01/10 17:03:56  fwarmerdam
 * added VSI Rename support
 *
 * Revision 1.27  2005/10/07 00:26:27  fwarmerdam
 * add documentation
 *
 * Revision 1.26  2005/10/03 18:56:40  fwarmerdam
 * always define large file api - cygwin fix
 *
 * Revision 1.25  2005/09/15 18:32:35  fwarmerdam
 * added VSICleanupFileManager
 *
 * Revision 1.24  2005/09/12 16:53:33  fwarmerdam
 * fixed VSIGetMemFileBuffer declaration
 *
 * Revision 1.23  2005/09/12 16:50:37  fwarmerdam
 * added VSIMemFile buffer fetcher
 *
 * Revision 1.22  2005/09/11 18:31:41  fwarmerdam
 * ensure a distinct VSIStatL() exists on win32
 *
 * Revision 1.21  2005/09/11 18:01:28  fwarmerdam
 * preliminary implementatin of fully virtualized large file api
 *
 * Revision 1.20  2005/04/12 03:51:11  fwarmerdam
 * Fixed stat64 problem.
 *
 * Revision 1.19  2005/04/12 00:27:39  fwarmerdam
 * added macos large file support
 *
 * Revision 1.18  2003/09/10 19:44:36  warmerda
 * added VSIStrerrno()
 *
 * Revision 1.17  2003/09/08 08:11:40  dron
 * Added VSIGMTime() and VSILocalTime().
 *
 * Revision 1.16  2003/05/27 20:44:40  warmerda
 * added VSI io debugging macros
 */

#ifndef CPL_VSI_H_INCLUDED
#define CPL_VSI_H_INCLUDED

#include "cpl_port.h"
/**
 * \file cpl_vsi.h
 *
 * Standard C Covers
 *
 * The VSI functions are intended to be hookable aliases for Standard C
 * I/O, memory allocation and other system functions. They are intended
 * to allow virtualization of disk I/O so that non file data sources
 * can be made to appear as files, and so that additional error trapping
 * and reporting can be interested.  The memory access API is aliased
 * so that special application memory management services can be used.
 *
 * Is is intended that each of these functions retains exactly the same
 * calling pattern as the original Standard C functions they relate to.
 * This means we don't have to provide custom documentation, and also means
 * that the default implementation is very simple.
 */


/* -------------------------------------------------------------------- */
/*      We need access to ``struct stat''.                              */
/* -------------------------------------------------------------------- */

/* Unix */
#if !defined(_WIN32) && !defined(_WIN32_WCE)
#  include <unistd.h>
#endif

/* Windows */
#if !defined(macos_pre10) && !defined(_WIN32_WCE)
#  include <sys/stat.h>
#endif

/* Windows CE */
#if defined(_WIN32_WCE)
#  include <wce_stat.h>
#endif

CPL_C_START

/* ==================================================================== */
/*      stdio file access functions.  These may not support large       */
/*      files, and don't necessarily go through the virtualization      */
/*      API.                                                            */
/* ==================================================================== */

FILE CPL_DLL *  VSIFOpen( const char *, const char * );
int CPL_DLL     VSIFClose( FILE * );
int CPL_DLL     VSIFSeek( FILE *, long, int );
long CPL_DLL    VSIFTell( FILE * );
void CPL_DLL    VSIRewind( FILE * );
void CPL_DLL    VSIFFlush( FILE * );

size_t CPL_DLL  VSIFRead( void *, size_t, size_t, FILE * );
size_t CPL_DLL  VSIFWrite( const void *, size_t, size_t, FILE * );
char CPL_DLL   *VSIFGets( char *, int, FILE * );
int CPL_DLL     VSIFPuts( const char *, FILE * );
int CPL_DLL     VSIFPrintf( FILE *, const char *, ... );

int CPL_DLL     VSIFGetc( FILE * );
int CPL_DLL     VSIFPutc( int, FILE * );
int CPL_DLL     VSIUngetc( int, FILE * );
int CPL_DLL     VSIFEof( FILE * );

/* ==================================================================== */
/*      VSIStat() related.                                              */
/* ==================================================================== */

typedef struct stat VSIStatBuf;
int CPL_DLL VSIStat( const char *, VSIStatBuf * );

#ifdef _WIN32
#  define VSI_ISLNK(x)  ( 0 )            /* N/A on Windows */
#  define VSI_ISREG(x)  ((x) & S_IFREG)
#  define VSI_ISDIR(x)  ((x) & S_IFDIR)
#  define VSI_ISCHR(x)  ((x) & S_IFCHR)
#  define VSI_ISBLK(x)  ( 0 )            /* N/A on Windows */
#else
#  define VSI_ISLNK(x)  S_ISLNK(x)
#  define VSI_ISREG(x)  S_ISREG(x)
#  define VSI_ISDIR(x)  S_ISDIR(x)
#  define VSI_ISCHR(x)  S_ISCHR(x)
#  define VSI_ISBLK(x)  S_ISBLK(x)
#endif

/* ==================================================================== */
/*      64bit stdio file access functions.  If we have a big size       */
/*      defined, then provide protypes for the large file API,          */
/*      otherwise redefine to use the regular api.                      */
/* ==================================================================== */
typedef GUIntBig vsi_l_offset;

FILE CPL_DLL *  VSIFOpenL( const char *, const char * );
int CPL_DLL     VSIFCloseL( FILE * );
int CPL_DLL     VSIFSeekL( FILE *, vsi_l_offset, int );
vsi_l_offset CPL_DLL VSIFTellL( FILE * );
void CPL_DLL    VSIRewindL( FILE * );
size_t CPL_DLL  VSIFReadL( void *, size_t, size_t, FILE * );
size_t CPL_DLL  VSIFWriteL( const void *, size_t, size_t, FILE * );
int CPL_DLL     VSIFEofL( FILE * );
int CPL_DLL    VSIFFlushL( FILE * );

#if defined(VSI_STAT64_T)
typedef struct VSI_STAT64_T VSIStatBufL;
#else
#define VSIStatBufL    VSIStatBuf
#endif

int CPL_DLL     VSIStatL( const char *, VSIStatBufL * );

/* ==================================================================== */
/*      Memory allocation                                               */
/* ==================================================================== */

void CPL_DLL   *VSICalloc( size_t, size_t );
void CPL_DLL   *VSIMalloc( size_t );
void CPL_DLL    VSIFree( void * );
void CPL_DLL   *VSIRealloc( void *, size_t );
char CPL_DLL   *VSIStrdup( const char * );

/* ==================================================================== */
/*      Other...                                                        */
/* ==================================================================== */

int CPL_DLL VSIMkdir( const char * pathname, long mode );
int CPL_DLL VSIRmdir( const char * pathname );
int CPL_DLL VSIUnlink( const char * pathname );
int CPL_DLL VSIRename( const char * oldpath, const char * newpath );
char CPL_DLL *VSIStrerror( int );

/* ==================================================================== */
/*      Install special file access handlers.                           */
/* ==================================================================== */
void CPL_DLL VSIInstallMemFileHandler(void);
void CPL_DLL VSIInstallLargeFileHandler(void);
void CPL_DLL VSICleanupFileManager(void);

FILE CPL_DLL *VSIFileFromMemBuffer( const char *pszFilename, 
                                    GByte *pabyData, 
                                    vsi_l_offset nDataLength,
                                    int bTakeOwnership );
GByte CPL_DLL *VSIGetMemFileBuffer( const char *pszFilename, 
                                    vsi_l_offset *pnDataLength, 
                                    int bUnlinkAndSeize );

/* ==================================================================== */
/*      Time quering.                                                   */
/* ==================================================================== */

unsigned long CPL_DLL VSITime( unsigned long * );
const char CPL_DLL *VSICTime( unsigned long );
struct tm CPL_DLL *VSIGMTime( const time_t *pnTime,
                              struct tm *poBrokenTime );
struct tm CPL_DLL *VSILocalTime( const time_t *pnTime,
                                 struct tm *poBrokenTime );

/* -------------------------------------------------------------------- */
/*      the following can be turned on for detailed logging of          */
/*      almost all IO calls.                                            */
/* -------------------------------------------------------------------- */
#ifdef VSI_DEBUG

#ifndef DEBUG
#  define DEBUG
#endif

#include "cpl_error.h"

#define VSIDebug4(f,a1,a2,a3,a4)   CPLDebug( "VSI", f, a1, a2, a3, a4 );
#define VSIDebug3( f, a1, a2, a3 ) CPLDebug( "VSI", f, a1, a2, a3 );
#define VSIDebug2( f, a1, a2 )     CPLDebug( "VSI", f, a1, a2 );
#define VSIDebug1( f, a1 )         CPLDebug( "VSI", f, a1 );
#else
#define VSIDebug4( f, a1, a2, a3, a4 ) {}
#define VSIDebug3( f, a1, a2, a3 ) {}
#define VSIDebug2( f, a1, a2 )     {}
#define VSIDebug1( f, a1 )         {}
#endif

CPL_C_END

#endif /* ndef CPL_VSI_H_INCLUDED */
