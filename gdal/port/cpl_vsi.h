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
 * It is intended that each of these functions retains exactly the same
 * calling pattern as the original Standard C functions they relate to.
 * This means we don't have to provide custom documentation, and also means
 * that the default implementation is very simple.
 */

/* -------------------------------------------------------------------- */
/*      We need access to ``struct stat''.                              */
/* -------------------------------------------------------------------- */

/* Unix */
#if !defined(_WIN32)
#  include <unistd.h>
#endif

/* Windows */
#include <sys/stat.h>

CPL_C_START

/*! @cond Doxygen_Suppress */
#ifdef ENABLE_EXPERIMENTAL_CPL_WARN_UNUSED_RESULT
#define EXPERIMENTAL_CPL_WARN_UNUSED_RESULT CPL_WARN_UNUSED_RESULT
#else
#define EXPERIMENTAL_CPL_WARN_UNUSED_RESULT
#endif
/*! @endcond */

/* ==================================================================== */
/*      stdio file access functions.  These do not support large       */
/*      files, and do not go through the virtualization API.           */
/* ==================================================================== */

/*! @cond Doxygen_Suppress */

FILE CPL_DLL *  VSIFOpen( const char *, const char * ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFClose( FILE * );
int CPL_DLL     VSIFSeek( FILE *, long, int ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
long CPL_DLL    VSIFTell( FILE * ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL    VSIRewind( FILE * );
void CPL_DLL    VSIFFlush( FILE * );

size_t CPL_DLL  VSIFRead( void *, size_t, size_t, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
size_t CPL_DLL  VSIFWrite( const void *, size_t, size_t, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
char CPL_DLL   *VSIFGets( char *, int, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFPuts( const char *, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFPrintf( FILE *, CPL_FORMAT_STRING(const char *), ... ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT CPL_PRINT_FUNC_FORMAT(2, 3);

int CPL_DLL     VSIFGetc( FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFPutc( int, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIUngetc( int, FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFEof( FILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;

/*! @endcond */

/* ==================================================================== */
/*      VSIStat() related.                                              */
/* ==================================================================== */

/*! @cond Doxygen_Suppress */
typedef struct stat VSIStatBuf;
int CPL_DLL VSIStat( const char *, VSIStatBuf * ) CPL_WARN_UNUSED_RESULT;
/*! @endcond */

#ifdef _WIN32
#  define VSI_ISLNK(x)  ( 0 )            /* N/A on Windows */
#  define VSI_ISREG(x)  ((x) & S_IFREG)
#  define VSI_ISDIR(x)  ((x) & S_IFDIR)
#  define VSI_ISCHR(x)  ((x) & S_IFCHR)
#  define VSI_ISBLK(x)  ( 0 )            /* N/A on Windows */
#else
/** Test if the file is a symbolic link */
#  define VSI_ISLNK(x)  S_ISLNK(x)
/** Test if the file is a regular file */
#  define VSI_ISREG(x)  S_ISREG(x)
/** Test if the file is a directory */
#  define VSI_ISDIR(x)  S_ISDIR(x)
/*! @cond Doxygen_Suppress */
#  define VSI_ISCHR(x)  S_ISCHR(x)
#  define VSI_ISBLK(x)  S_ISBLK(x)
/*! @endcond */
#endif

/* ==================================================================== */
/*      64bit stdio file access functions.  If we have a big size       */
/*      defined, then provide prototypes for the large file API,        */
/*      otherwise redefine to use the regular api.                      */
/* ==================================================================== */

/** Type for a file offset */
typedef GUIntBig vsi_l_offset;
/** Maximum value for a file offset */
#define VSI_L_OFFSET_MAX GUINTBIG_MAX

/*! @cond Doxygen_Suppress */
/* Make VSIL_STRICT_ENFORCE active in DEBUG builds */
#ifdef DEBUG
#define VSIL_STRICT_ENFORCE
#endif
/*! @endcond */

#ifdef VSIL_STRICT_ENFORCE
/** Opaque type for a FILE that implements the VSIVirtualHandle API */
typedef struct _VSILFILE VSILFILE;
#else
/** Opaque type for a FILE that implements the VSIVirtualHandle API */
typedef FILE VSILFILE;
#endif

VSILFILE CPL_DLL *  VSIFOpenL( const char *, const char * ) CPL_WARN_UNUSED_RESULT;
VSILFILE CPL_DLL *  VSIFOpenExL( const char *, const char *, int ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFCloseL( VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFSeekL( VSILFILE *, vsi_l_offset, int ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
vsi_l_offset CPL_DLL VSIFTellL( VSILFILE * ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL    VSIRewindL( VSILFILE * );
size_t CPL_DLL  VSIFReadL( void *, size_t, size_t, VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFReadMultiRangeL( int nRanges, void ** ppData, const vsi_l_offset* panOffsets, const size_t* panSizes, VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
size_t CPL_DLL  VSIFWriteL( const void *, size_t, size_t, VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFEofL( VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFTruncateL( VSILFILE *, vsi_l_offset ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFFlushL( VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;
int CPL_DLL     VSIFPrintfL( VSILFILE *, CPL_FORMAT_STRING(const char *), ... ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT CPL_PRINT_FUNC_FORMAT(2, 3);
int CPL_DLL     VSIFPutcL( int, VSILFILE * ) EXPERIMENTAL_CPL_WARN_UNUSED_RESULT;

/** Range status */
typedef enum
{
    VSI_RANGE_STATUS_UNKNOWN, /**< Unknown */
    VSI_RANGE_STATUS_DATA,    /**< Data present */
    VSI_RANGE_STATUS_HOLE     /**< Hole */
} VSIRangeStatus;

VSIRangeStatus CPL_DLL VSIFGetRangeStatusL( VSILFILE * fp, vsi_l_offset nStart, vsi_l_offset nLength );

int CPL_DLL     VSIIngestFile( VSILFILE* fp,
                               const char* pszFilename,
                               GByte** ppabyRet,
                               vsi_l_offset* pnSize,
                               GIntBig nMaxSize ) CPL_WARN_UNUSED_RESULT;

#if defined(VSI_STAT64_T)
/** Type for VSIStatL() */
typedef struct VSI_STAT64_T VSIStatBufL;
#else
/** Type for VSIStatL() */
#define VSIStatBufL    VSIStatBuf
#endif

int CPL_DLL     VSIStatL( const char *, VSIStatBufL * ) CPL_WARN_UNUSED_RESULT;

/** Flag provided to VSIStatExL() to test if the file exists */
#define VSI_STAT_EXISTS_FLAG         0x1
/** Flag provided to VSIStatExL() to query the nature (file/dir) of the file */
#define VSI_STAT_NATURE_FLAG         0x2
/** Flag provided to VSIStatExL() to query the file size */
#define VSI_STAT_SIZE_FLAG           0x4
/** Flag provided to VSIStatExL() to issue a VSIError in case of failure */
#define VSI_STAT_SET_ERROR_FLAG      0x8

int CPL_DLL     VSIStatExL( const char * pszFilename, VSIStatBufL * psStatBuf, int nFlags ) CPL_WARN_UNUSED_RESULT;

int CPL_DLL     VSIIsCaseSensitiveFS( const char * pszFilename );

int CPL_DLL     VSISupportsSparseFiles( const char* pszPath );

void CPL_DLL   *VSIFGetNativeFileDescriptorL( VSILFILE* );

/* ==================================================================== */
/*      Memory allocation                                               */
/* ==================================================================== */

void CPL_DLL   *VSICalloc( size_t, size_t ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL   *VSIMalloc( size_t ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL    VSIFree( void * );
void CPL_DLL   *VSIRealloc( void *, size_t ) CPL_WARN_UNUSED_RESULT;
char CPL_DLL   *VSIStrdup( const char * ) CPL_WARN_UNUSED_RESULT;

void CPL_DLL   *VSIMallocAligned( size_t nAlignment, size_t nSize ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL   *VSIMallocAlignedAuto( size_t nSize ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL    VSIFreeAligned( void* ptr );

void CPL_DLL   *VSIMallocAlignedAutoVerbose( size_t nSize, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSIMallocAlignedAutoVerbose() with FILE and LINE reporting */
#define VSI_MALLOC_ALIGNED_AUTO_VERBOSE( size ) VSIMallocAlignedAutoVerbose(size,__FILE__,__LINE__)

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

/** VSIMallocVerbose */
void CPL_DLL   *VSIMallocVerbose( size_t nSize, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_MALLOC_VERBOSE */
#define VSI_MALLOC_VERBOSE( size ) VSIMallocVerbose(size,__FILE__,__LINE__)

/** VSIMalloc2Verbose */
void CPL_DLL   *VSIMalloc2Verbose( size_t nSize1, size_t nSize2, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_MALLOC2_VERBOSE */
#define VSI_MALLOC2_VERBOSE( nSize1, nSize2 ) VSIMalloc2Verbose(nSize1,nSize2,__FILE__,__LINE__)

/** VSIMalloc3Verbose */
void CPL_DLL   *VSIMalloc3Verbose( size_t nSize1, size_t nSize2, size_t nSize3, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_MALLOC3_VERBOSE */
#define VSI_MALLOC3_VERBOSE( nSize1, nSize2, nSize3 ) VSIMalloc3Verbose(nSize1,nSize2,nSize3,__FILE__,__LINE__)

/** VSICallocVerbose */
void CPL_DLL   *VSICallocVerbose(  size_t nCount, size_t nSize, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_CALLOC_VERBOSE */
#define VSI_CALLOC_VERBOSE( nCount, nSize ) VSICallocVerbose(nCount,nSize,__FILE__,__LINE__)

/** VSIReallocVerbose */
void CPL_DLL   *VSIReallocVerbose(  void* pOldPtr, size_t nNewSize, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_REALLOC_VERBOSE */
#define VSI_REALLOC_VERBOSE( pOldPtr, nNewSize ) VSIReallocVerbose(pOldPtr,nNewSize,__FILE__,__LINE__)

/** VSIStrdupVerbose */
char CPL_DLL   *VSIStrdupVerbose(  const char* pszStr, const char* pszFile, int nLine ) CPL_WARN_UNUSED_RESULT;
/** VSI_STRDUP_VERBOSE */
#define VSI_STRDUP_VERBOSE( pszStr ) VSIStrdupVerbose(pszStr,__FILE__,__LINE__)

GIntBig CPL_DLL CPLGetPhysicalRAM(void);
GIntBig CPL_DLL CPLGetUsablePhysicalRAM(void);

/* ==================================================================== */
/*      Other...                                                        */
/* ==================================================================== */

/** Alias of VSIReadDir() */
#define CPLReadDir VSIReadDir
char CPL_DLL **VSIReadDir( const char * );
char CPL_DLL **VSIReadDirRecursive( const char *pszPath );
char CPL_DLL **VSIReadDirEx( const char *pszPath, int nMaxFiles );
int CPL_DLL VSIMkdir( const char * pathname, long mode );
int CPL_DLL VSIRmdir( const char * pathname );
int CPL_DLL VSIUnlink( const char * pathname );
int CPL_DLL VSIRename( const char * oldpath, const char * newpath );
char CPL_DLL *VSIStrerror( int );
GIntBig CPL_DLL VSIGetDiskFreeSpace(const char *pszDirname);

/* ==================================================================== */
/*      Install special file access handlers.                           */
/* ==================================================================== */
void CPL_DLL VSIInstallMemFileHandler(void);
/*! @cond Doxygen_Suppress */
void CPL_DLL VSIInstallLargeFileHandler(void);
/*! @endcond */
void CPL_DLL VSIInstallSubFileHandler(void);
void VSIInstallCurlFileHandler(void);
void CPL_DLL VSICurlClearCache(void);
void VSIInstallCurlStreamingFileHandler(void);
void VSIInstallS3FileHandler(void);
void VSIInstallS3StreamingFileHandler(void);
void VSIInstallGSFileHandler(void);
void VSIInstallGSStreamingFileHandler(void);
void VSIInstallGZipFileHandler(void); /* No reason to export that */
void VSIInstallZipFileHandler(void); /* No reason to export that */
void VSIInstallStdinHandler(void); /* No reason to export that */
void VSIInstallStdoutHandler(void); /* No reason to export that */
void CPL_DLL VSIInstallSparseFileHandler(void);
void VSIInstallTarFileHandler(void); /* No reason to export that */
void CPL_DLL VSIInstallCryptFileHandler(void);
void CPL_DLL VSISetCryptKey(const GByte* pabyKey, int nKeySize);
/*! @cond Doxygen_Suppress */
void CPL_DLL VSICleanupFileManager(void);
/*! @endcond */

VSILFILE CPL_DLL *VSIFileFromMemBuffer( const char *pszFilename,
                                    GByte *pabyData,
                                    vsi_l_offset nDataLength,
                                    int bTakeOwnership ) CPL_WARN_UNUSED_RESULT;
GByte CPL_DLL *VSIGetMemFileBuffer( const char *pszFilename,
                                    vsi_l_offset *pnDataLength,
                                    int bUnlinkAndSeize );

/** Callback used by VSIStdoutSetRedirection() */
typedef size_t (*VSIWriteFunction)(const void* ptr, size_t size, size_t nmemb, FILE* stream);
void CPL_DLL VSIStdoutSetRedirection( VSIWriteFunction pFct, FILE* stream );

/* ==================================================================== */
/*      Time querying.                                                  */
/* ==================================================================== */

/*! @cond Doxygen_Suppress */
unsigned long CPL_DLL VSITime( unsigned long * );
const char CPL_DLL *VSICTime( unsigned long );
struct tm CPL_DLL *VSIGMTime( const time_t *pnTime,
                              struct tm *poBrokenTime );
struct tm CPL_DLL *VSILocalTime( const time_t *pnTime,
                                 struct tm *poBrokenTime );
/*! @endcond */

/*! @cond Doxygen_Suppress */
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
/*! @endcond */

CPL_C_END

#endif /* ndef CPL_VSI_H_INCLUDED */
