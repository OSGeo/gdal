/******************************************************************************
 * $Id$
 *
 * Name:     cpl.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 *****************************************************************************/

%include constraints.i

#ifdef SWIGCSHARP
typedef enum
{
    CE_None = 0,
    CE_Log = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
} CPLErr;
#endif

%inline %{
typedef char retStringAndCPLFree;
%}

%inline %{
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, "%s", message );
  }

  CPLErr PushErrorHandler( char const * pszCallbackName = NULL ) {
    CPLErrorHandler pfnHandler = NULL;
    if( pszCallbackName == NULL || EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
      pfnHandler = CPLQuietErrorHandler;
    else if( EQUAL(pszCallbackName,"CPLDefaultErrorHandler") )
      pfnHandler = CPLDefaultErrorHandler;
    else if( EQUAL(pszCallbackName,"CPLLoggingErrorHandler") )
      pfnHandler = CPLLoggingErrorHandler;

    if ( pfnHandler == NULL )
      return CE_Fatal;

    CPLPushErrorHandler( pfnHandler );

    return CE_None;
  }

%}

#ifdef SWIGJAVA
%inline%{
  void Error( CPLErr msg_class, int err_code, const char* msg ) {
    CPLError( msg_class, err_code, "%s", msg );
  }
%}
#else
%inline%{
  void Error( CPLErr msg_class = CE_Failure, int err_code = 0, const char* msg = "error" ) {
    CPLError( msg_class, err_code, "%s", msg );
  }
%}
#endif

#ifdef SWIGRUBY
%rename (push_error_handler) CPLPushErrorHandler;
%rename (pop_error_handler) CPLPopErrorHandler;
%rename (error_reset) CPLErrorReset;
%rename (get_last_error_no) CPLGetLastErrorNo;
%rename (get_last_error_type) CPLGetLastErrorType;
%rename (get_last_error_msg) CPLGetLastErrorMsg;
%rename (push_finder_location) CPLPushFinderLocation;
%rename (pop_finder_location) CPLPopFinderLocation;
%rename (finder_clean) CPLFinderClean;
%rename (find_file) CPLFindFile;
%rename (read_dir) VSIReadDir;
%rename (mkdir) VSIMkdir;
%rename (rmdir) VSIRmdir;
%rename (rename) VSIRename;
%rename (set_config_option) CPLSetConfigOption;
%rename (get_config_option) wrapper_CPLGetConfigOption;
%rename (binary_to_hex) CPLBinaryToHex;
%rename (hex_to_binary) CPLHexToBinary;
%rename (file_from_mem_buffer) wrapper_VSIFileFromMemBuffer;
%rename (unlink) VSIUnlink;
%rename (has_thread_support) wrapper_HasThreadSupport;
#else
%rename (PushErrorHandler) CPLPushErrorHandler;
%rename (PopErrorHandler) CPLPopErrorHandler;
%rename (ErrorReset) CPLErrorReset;
%rename (GetLastErrorNo) CPLGetLastErrorNo;
%rename (GetLastErrorType) CPLGetLastErrorType;
%rename (GetLastErrorMsg) CPLGetLastErrorMsg;
%rename (PushFinderLocation) CPLPushFinderLocation;
%rename (PopFinderLocation) CPLPopFinderLocation;
%rename (FinderClean) CPLFinderClean;
#ifdef SWIGPERL
%rename (_FindFile) CPLFindFile;
%rename (_ReadDir) VSIReadDir;
#else
%rename (FindFile) CPLFindFile;
%rename (ReadDir) VSIReadDir;
#endif
%rename (Mkdir) VSIMkdir;
%rename (Rmdir) VSIRmdir;
%rename (Rename) VSIRename;
%rename (SetConfigOption) CPLSetConfigOption;
%rename (GetConfigOption) wrapper_CPLGetConfigOption;
%rename (CPLBinaryToHex) CPLBinaryToHex;
%rename (CPLHexToBinary) CPLHexToBinary;
%rename (FileFromMemBuffer) wrapper_VSIFileFromMemBuffer;
%rename (Unlink) VSIUnlink;
%rename (HasThreadSupport) wrapper_HasThreadSupport;
#endif

#ifndef SWIGJAVA
void CPLPushErrorHandler( CPLErrorHandler );
#endif

void CPLPopErrorHandler();

void CPLErrorReset();

#ifndef SWIGJAVA
%feature( "kwargs" ) EscapeString;
#endif

#ifdef SWIGJAVA
%apply (int nLen, unsigned char *pBuf ) {( int len, unsigned char *bin_string )};
%inline %{
retStringAndCPLFree* EscapeString(int len, unsigned char *bin_string , int scheme) {
    return CPLEscapeString((const char*)bin_string, len, scheme);
} 

retStringAndCPLFree* EscapeString(const char* str, int scheme) {
    return CPLEscapeString(str, (str) ? strlen(str) : 0, scheme);
} 
%}
%clear (int len, unsigned char *bin_string);
#elif defined(SWIGCSHARP)
%inline %{
retStringAndCPLFree* EscapeString(int len, char *bin_string , int scheme) {
    return CPLEscapeString((const char*)bin_string, len, scheme);
} 
%}

retStringAndCPLFree* EscapeString(int len, char *bin_string , int scheme=CPLES_SQL) {
    return CPLEscapeString(bin_string, len, scheme);
}
#elif defined(SWIGPYTHON) || defined(SWIGPERL)
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
%inline %{
retStringAndCPLFree* EscapeString(int len, char *bin_string , int scheme=CPLES_SQL) {
    return CPLEscapeString(bin_string, len, scheme);
} 
%}
%clear (int len, char *bin_string);
#else
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
%inline %{
char* EscapeString(int len, char *bin_string , int scheme=CPLES_SQL) {
    return CPLEscapeString(bin_string, len, scheme);
} 
%}
%clear (int len, char *bin_string);
#endif

#if defined(SWIGPYTHON) || defined(SWIGCSHARP)
/* We don't want errors to be cleared or thrown by this */
/* call */
%exception CPLGetLastErrorNo
{
    result = CPLGetLastErrorNo();
}
#endif
int CPLGetLastErrorNo();

#if defined(SWIGPYTHON) || defined(SWIGCSHARP)
/* We don't want errors to be cleared or thrown by this */
/* call */
%exception CPLGetLastErrorType
{
    result = CPLGetLastErrorType();
}
int CPLGetLastErrorType();
#else
CPLErr CPLGetLastErrorType();
#endif

#if defined(SWIGPYTHON) || defined(SWIGCSHARP)
/* We don't want errors to be cleared or thrown by this */
/* call */
%exception CPLGetLastErrorMsg
{
    result = (char*)CPLGetLastErrorMsg();
}
#endif
const char *CPLGetLastErrorMsg();

void CPLPushFinderLocation( const char * utf8_path );

void CPLPopFinderLocation();

void CPLFinderClean();

const char * CPLFindFile( const char *pszClass, const char *utf8_path );

%apply (char **CSL) {char **};
char **VSIReadDir( const char * utf8_path );
%clear char **;

%apply Pointer NONNULL {const char * pszKey};
void CPLSetConfigOption( const char * pszKey, const char * pszValue );

%inline {
const char *wrapper_CPLGetConfigOption( const char * pszKey, const char * pszDefault = NULL )
{
    return CPLGetConfigOption( pszKey, pszDefault );
}
}
%clear const char * pszKey;

/* Provide hooks to hex encoding methods */
#if defined(SWIGJAVA) || defined(SWIGPERL)
%apply (int nLen, unsigned char *pBuf ) {( int nBytes, const GByte *pabyData )};
retStringAndCPLFree* CPLBinaryToHex( int nBytes, const GByte *pabyData );
%clear ( int nBytes, const GByte *pabyData );
#elif defined(SWIGCSHARP)
retStringAndCPLFree* CPLBinaryToHex( int nBytes, const GByte *pabyData );
#elif defined(SWIGPYTHON)
%apply (int nLen, char *pBuf) {( int nBytes, const GByte *pabyData )};
retStringAndCPLFree* CPLBinaryToHex( int nBytes, const GByte *pabyData );
%clear ( int nBytes, const GByte *pabyData );
#else
/* FIXME : wrong typemap. The string should be freed */
char * CPLBinaryToHex( int nBytes, const GByte *pabyData );
#endif

#ifdef SWIGJAVA
%apply (GByte* outBytes) {GByte*};
#endif
GByte *CPLHexToBinary( const char *pszHex, int *pnBytes );
#ifdef SWIGJAVA
%clear GByte*;
#endif

/* Inappropriate typemap for Ruby bindings */
#ifndef SWIGRUBY

%apply Pointer NONNULL {const char * pszFilename};
/* Added in GDAL 1.7.0 */
#ifdef SWIGJAVA
%apply (int nLen, unsigned char *pBuf ) {( int nBytes, const GByte *pabyData )};
#elif defined(SWIGPYTHON)
%apply (int nLen, char *pBuf) {( int nBytes, const GByte *pabyData )};
#endif
%inline {
void wrapper_VSIFileFromMemBuffer( const char* utf8_path, int nBytes, const GByte *pabyData)
{
    GByte* pabyDataDup = (GByte*)VSIMalloc(nBytes);
    if (pabyDataDup == NULL)
            return;
    memcpy(pabyDataDup, pabyData, nBytes);
    VSIFCloseL(VSIFileFromMemBuffer(utf8_path, (GByte*) pabyDataDup, nBytes, TRUE));
}

}
#if defined(SWIGJAVA) || defined(SWIGPYTHON)
%clear ( int nBytes, const GByte *pabyData );
#endif

/* Added in GDAL 1.7.0 */
int VSIUnlink(const char * utf8_path );

/* Added in GDAL 1.7.0 */
/* Thread support is necessary for binding languages with threaded GC */
/* even if the user doesn't explicitely use threads */
%inline {
int wrapper_HasThreadSupport()
{
    return strcmp(CPLGetThreadingModel(), "stub") != 0;
}
}

/* Added for GDAL 1.8 */
int VSIMkdir(const char *utf8_path, int mode );
int VSIRmdir(const char *utf8_path );
int VSIRename(const char * pszOld, const char *pszNew );

/* Added for GDAL 1.8 

   We do not bother renaming the VSI*L api as this wrapping is not
   considered "official", or available for use by application code. 
   It is just for some testing stuff. 
*/

#if !defined(SWIGJAVA)

typedef void VSILFILE;

#if defined(SWIGPERL)
%apply RETURN_NONE_TRUE_IS_ERROR {RETURN_NONE};
RETURN_NONE VSIStatL( const char * utf8_path, VSIStatBufL *psStatBuf );
%clear RETURN_NONE;

#elif defined(SWIGPYTHON)

%{
typedef struct
{
  int     mode;
  GIntBig size;
  GIntBig mtime;
} StatBuf;
%}

#define VSI_STAT_EXISTS_FLAG    0x1
#define VSI_STAT_NATURE_FLAG    0x2
#define VSI_STAT_SIZE_FLAG      0x4

struct StatBuf
{
%apply (GIntBig bigint) { GIntBig };
%immutable;
  int         mode;
  GIntBig     size;
  GIntBig     mtime;
%mutable;
%clear (GIntBig bigint);

%extend {
  StatBuf( StatBuf *psStatBuf ) {
    StatBuf *self = (StatBuf*) CPLMalloc( sizeof( StatBuf ) );
    self->mode = psStatBuf->mode;
    self->size = psStatBuf->size;
    self->mtime = psStatBuf->mtime;
    return self;
  }

  ~StatBuf() {
    CPLFree(self);
  }

  int IsDirectory()
  {
     return (self->mode & S_IFDIR) != 0;
  }

} /* extend */
} /* StatBuf */ ;

%rename (VSIStatL) wrapper_VSIStatL;
%inline {
int wrapper_VSIStatL( const char * utf8_path, StatBuf *psStatBufOut, int nFlags = 0 )
{
    VSIStatBufL sStat;
    memset(&sStat, 0, sizeof(sStat));
    memset(psStatBufOut, 0, sizeof(StatBuf));
    int nRet = VSIStatExL(utf8_path, &sStat, nFlags);
    psStatBufOut->mode = sStat.st_mode;
    psStatBufOut->size = (GIntBig)sStat.st_size;
    psStatBufOut->mtime = (GIntBig)sStat.st_mtime;
    return nRet;
}
}

#endif

VSILFILE   *VSIFOpenL( const char *utf8_path, const char *pszMode );
void    VSIFCloseL( VSILFILE * );

#if defined(SWIGPYTHON)
%apply (GIntBig bigint) { GIntBig };
int     VSIFSeekL( VSILFILE *, GIntBig, int );
GIntBig    VSIFTellL( VSILFILE * );
int     VSIFTruncateL( VSILFILE *, GIntBig );
%clear (GIntBig bigint);
#else
int     VSIFSeekL( VSILFILE *, long, int );
long    VSIFTellL( VSILFILE * );
int     VSIFTruncateL( VSILFILE *, long );
#endif

#if defined(SWIGPYTHON)
%rename (VSIFWriteL) wrapper_VSIFWriteL;
%inline {
int wrapper_VSIFWriteL( int nLen, char *pBuf, int size, int memb, VSILFILE * f)
{
    if (nLen < size * memb)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Inconsistant buffer size with 'size' and 'memb' values");
        return 0;
    }
    return VSIFWriteL(pBuf, size, memb, f);
}
}
#elif defined(SWIGPERL)
size_t VSIFWriteL(const void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);
#else
int     VSIFWriteL( const char *, int, int, VSILFILE * );
#endif

#if defined(SWIGPERL)
size_t VSIFReadL(void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);
#endif
/* VSIFReadL() handled specially in python/gdal_python.i */

#endif

#endif
