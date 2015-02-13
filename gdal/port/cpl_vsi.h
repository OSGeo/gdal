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
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

FILE CPL_DLL *  VSIFOpen( const char *, const char * ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFClose( FILE * );
int CPL_DLL     VSIFSeek( FILE *, long, int );
long CPL_DLL    VSIFTell( FILE * );
void CPL_DLL    VSIRewind( FILE * );
void CPL_DLL    VSIFFlush( FILE * );

size_t CPL_DLL  VSIFRead( void *, size_t, size_t, FILE * );
size_t CPL_DLL  VSIFWrite( const void *, size_t, size_t, FILE * );
char CPL_DLL   *VSIFGets( char *, int, FILE * );
int CPL_DLL     VSIFPuts( const char *, FILE * );
int CPL_DLL     VSIFPrintf( FILE *, const char *, ... ) CPL_PRINT_FUNC_FORMAT(2, 3);

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

/* Make VSIL_STRICT_ENFORCE active in DEBUG builds */
#ifdef DEBUG
#define VSIL_STRICT_ENFORCE
#endif

#ifdef VSIL_STRICT_ENFORCE
typedef struct _VSILFILE VSILFILE;
#else
typedef FILE VSILFILE;
#endif

VSILFILE CPL_DLL *  VSIFOpenL( const char *, const char * ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFCloseL( VSILFILE * );
int CPL_DLL     VSIFSeekL( VSILFILE *, vsi_l_offset, int );
vsi_l_offset CPL_DLL VSIFTellL( VSILFILE * );
void CPL_DLL    VSIRewindL( VSILFILE * );
size_t CPL_DLL  VSIFReadL( void *, size_t, size_t, VSILFILE * );
int CPL_DLL     VSIFReadMultiRangeL( int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes, VSILFILE * );
size_t CPL_DLL  VSIFWriteL( const void *, size_t, size_t, VSILFILE * );
int CPL_DLL     VSIFEofL( VSILFILE * );
int CPL_DLL     VSIFTruncateL( VSILFILE *, vsi_l_offset );
int CPL_DLL     VSIFFlushL( VSILFILE * );
int CPL_DLL     VSIFPrintfL( VSILFILE *, const char *, ... ) CPL_PRINT_FUNC_FORMAT(2, 3);
int CPL_DLL     VSIFPutcL( int, VSILFILE * );

int CPL_DLL     VSIIngestFile( VSILFILE* fp,
                               const char* pszFilename,
                               GByte** ppabyRet,
                               vsi_l_offset* pnSize,
                               GIntBig nMaxSize );

#if defined(VSI_STAT64_T)
typedef struct VSI_STAT64_T VSIStatBufL;
#else
#define VSIStatBufL    VSIStatBuf
#endif

int CPL_DLL     VSIStatL( const char *, VSIStatBufL * );

#define VSI_STAT_EXISTS_FLAG    0x1
#define VSI_STAT_NATURE_FLAG    0x2
#define VSI_STAT_SIZE_FLAG      0x4

int CPL_DLL     VSIStatExL( const char * pszFilename, VSIStatBufL * psStatBuf, int nFlags );

int CPL_DLL     VSIIsCaseSensitiveFS( const char * pszFilename );

void CPL_DLL   *VSIFGetNativeFileDescriptorL( VSILFILE* );

/* ==================================================================== */
/*      Memory allocation                                               */
/* ==================================================================== */

void CPL_DLL   *VSICalloc( size_t, size_t ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL   *VSIMalloc( size_t ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL    VSIFree( void * );
void CPL_DLL   *VSIRealloc( void *, size_t ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL   *VSIStrdup( const char * ) CPL_WARN_UNUSED_RESULT;

/**
 VSIMalloc2 allocates (nSize1 * nSize2) bytes.
 In case of overflow of the multiplication, or if memory allocation fails, a
 NULL pointer is returned and a CE_Failure error is raised with CPLError().
 If nSize1 == 0 || nSize2 == 0, a NULL pointer will also be returned.
 CPLFree() or VSIFree() can be used to free memory allocated by this function.
*/
void CPL_DLL *VSIMalloc2( size_t nSize1, size_t nSize2 ) CPL_WARN_UNUSED_RESULT;

/**
 VSIMalloc3 allocates (nSize1 * nSize2 * nSize3) bytes.
 In case of overflow of the multiplication, or if memory allocation fails, a
 NULL pointer is returned and a CE_Failure error is raised with CPLError().
 If nSize1 == 0 || nSize2 == 0 || nSize3 == 0, a NULL pointer will also be returned.
 CPLFree() or VSIFree() can be used to free memory allocated by this function.
*/
void CPL_DLL *VSIMalloc3( size_t nSize1, size_t nSize2, size_t nSize3 ) CPL_WARN_UNUSED_RESULT;

GIntBig CPL_DLL CPLGetPhysicalRAM(void);
GIntBig CPL_DLL CPLGetUsablePhysicalRAM(void);

/* ==================================================================== */
/*      Other...                                                        */
/* ==================================================================== */

#define CPLReadDir VSIReadDir
char CPL_DLL **VSIReadDir( const char * );
char CPL_DLL **VSIReadDirRecursive( const char *pszPath );
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
void CPL_DLL VSIInstallSubFileHandler(void);
void VSIInstallCurlFileHandler(void);
void VSIInstallCurlStreamingFileHandler(void);
void VSIInstallGZipFileHandler(void); /* No reason to export that */
void VSIInstallZipFileHandler(void); /* No reason to export that */
void VSIInstallStdinHandler(void); /* No reason to export that */
void VSIInstallStdoutHandler(void); /* No reason to export that */
void CPL_DLL VSIInstallSparseFileHandler(void);
void VSIInstallTarFileHandler(void); /* No reason to export that */
void CPL_DLL VSICleanupFileManager(void);

VSILFILE CPL_DLL *VSIFileFromMemBuffer( const char *pszFilename,
                                    GByte *pabyData, 
                                    vsi_l_offset nDataLength,
                                    int bTakeOwnership );
GByte CPL_DLL *VSIGetMemFileBuffer( const char *pszFilename, 
                                    vsi_l_offset *pnDataLength, 
                                    int bUnlinkAndSeize );

typedef size_t (*VSIWriteFunction)(const void* ptr, size_t size, size_t nmemb, FILE* stream);
void CPL_DLL VSIStdoutSetRedirection( VSIWriteFunction pFct, FILE* stream );

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
