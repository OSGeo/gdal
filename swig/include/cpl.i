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

%apply Pointer NONNULL {const char *message};
%inline %{
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, "%s", message );
  }
%}
%clear (const char *message);

%inline %{
  CPLErr SetErrorHandler( CPLErrorHandler pfnErrorHandler = NULL, void* user_data = NULL )
  {
    if( pfnErrorHandler == NULL )
    {
        pfnErrorHandler = CPLDefaultErrorHandler;
    }

    CPLSetErrorHandlerEx( pfnErrorHandler, user_data );

    return CE_None;
  }
%}

%rename (SetCurrentErrorHandlerCatchDebug) CPLSetCurrentErrorHandlerCatchDebug;
void CPLSetCurrentErrorHandlerCatchDebug( int bCatchDebug );

#ifdef SWIGPYTHON

%nothread;

%{
extern "C" int CPL_DLL GDALIsInGlobalDestructor();

void CPL_STDCALL PyCPLErrorHandler(CPLErr eErrClass, int err_no, const char* pszErrorMsg)
{
    if( GDALIsInGlobalDestructor() )
    {
        // this is typically during Python interpreter shutdown, and ends up in a crash
        // because error handling tries to do thread initialization.
        return;
    }

    void* user_data = CPLGetErrorHandlerUserData();
    PyObject *psArgs;

    SWIG_PYTHON_THREAD_BEGIN_BLOCK;

    psArgs = Py_BuildValue("(iis)", eErrClass, err_no, pszErrorMsg );
    PyObject_CallObject( (PyObject*)user_data, psArgs);
    Py_XDECREF(psArgs);

    SWIG_PYTHON_THREAD_END_BLOCK;
}
%}

%inline %{
  CPLErr PushErrorHandler( CPLErrorHandler pfnErrorHandler = NULL, void* user_data = NULL )
  {
    if( pfnErrorHandler == NULL )
        CPLPushErrorHandler(CPLQuietErrorHandler);
    else
        CPLPushErrorHandlerEx(pfnErrorHandler, user_data);
    return CE_None;
  }
%}

%inline %{
  void PopErrorHandler()
  {
     void* user_data = CPLGetErrorHandlerUserData();
     if( user_data != NULL )
     {
         Py_XDECREF((PyObject*)user_data);
     }
     CPLPopErrorHandler();
  }
%}

%thread;

#else
%inline %{
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
#endif

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

%rename (PushErrorHandler) CPLPushErrorHandler;
%rename (PopErrorHandler) CPLPopErrorHandler;
%rename (ErrorReset) CPLErrorReset;
%rename (GetLastErrorNo) CPLGetLastErrorNo;
%rename (GetLastErrorType) CPLGetLastErrorType;
%rename (GetLastErrorMsg) CPLGetLastErrorMsg;
%rename (GetErrorCounter) CPLGetErrorCounter;
%rename (PushFinderLocation) CPLPushFinderLocation;
%rename (PopFinderLocation) CPLPopFinderLocation;
%rename (FinderClean) CPLFinderClean;
%rename (FindFile) CPLFindFile;
%rename (ReadDir) wrapper_VSIReadDirEx;
%rename (ReadDirRecursive) VSIReadDirRecursive;
%rename (Mkdir) VSIMkdir;
%rename (MkdirRecursive) VSIMkdirRecursive;
%rename (Rmdir) VSIRmdir;
%rename (RmdirRecursive) VSIRmdirRecursive;
%rename (AbortPendingUploads) VSIAbortPendingUploads;
%rename (Rename) VSIRename;
%rename (GetActualURL) VSIGetActualURL;
%rename (GetSignedURL) wrapper_VSIGetSignedURL;
%rename (GetFileSystemsPrefixes) VSIGetFileSystemsPrefixes;
%rename (GetFileSystemOptions) VSIGetFileSystemOptions;
%rename (SetConfigOption) CPLSetConfigOption;
%rename (GetConfigOption) wrapper_CPLGetConfigOption;
%rename (SetThreadLocalConfigOption) CPLSetThreadLocalConfigOption;
%rename (GetThreadLocalConfigOption) wrapper_CPLGetThreadLocalConfigOption;
%rename (CPLBinaryToHex) CPLBinaryToHex;
%rename (CPLHexToBinary) CPLHexToBinary;
%rename (FileFromMemBuffer) wrapper_VSIFileFromMemBuffer;
%rename (Unlink) VSIUnlink;
%rename (HasThreadSupport) wrapper_HasThreadSupport;
%rename (NetworkStatsReset) VSINetworkStatsReset;
%rename (NetworkStatsGetAsSerializedJSON) VSINetworkStatsGetAsSerializedJSON;

%apply Pointer NONNULL {const char *pszScope};
retStringAndCPLFree*
GOA2GetAuthorizationURL( const char *pszScope );
%clear (const char *pszScope);

%apply Pointer NONNULL {const char *pszAuthToken};
retStringAndCPLFree*
GOA2GetRefreshToken( const char *pszAuthToken, const char *pszScope );
%clear (const char *pszAuthToken);

%apply Pointer NONNULL {const char *pszRefreshToken};
retStringAndCPLFree*
GOA2GetAccessToken( const char *pszRefreshToken, const char *pszScope );
%clear (const char *pszRefreshToken);

#if !defined(SWIGJAVA) && !defined(SWIGPYTHON)
void CPLPushErrorHandler( CPLErrorHandler );
#endif

#if !defined(SWIGPYTHON)
void CPLPopErrorHandler();
#endif

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
#elif defined(SWIGPYTHON)

%feature( "kwargs" ) wrapper_EscapeString;
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
%inline %{
retStringAndCPLFree* wrapper_EscapeString(int len, char *bin_string , int scheme=CPLES_SQL) {
    return CPLEscapeString(bin_string, len, scheme);
}
%}
%clear (int len, char *bin_string);

%feature( "kwargs" ) EscapeBinary;
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
%apply (size_t *nLen, char **pBuf) { (size_t *pnLenOut, char** pOut) };
%inline %{
void EscapeBinary(int len, char *bin_string, size_t *pnLenOut, char** pOut, int scheme=CPLES_SQL) {
    *pOut = CPLEscapeString(bin_string, len, scheme);
    *pnLenOut = *pOut ? strlen(*pOut) : 0;
}
%}
%clear (int len, char *bin_string);
%clean  (size_t *pnLenOut, char* pOut);

#elif defined(SWIGPERL)
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
#ifdef SWIGPYTHON
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
#endif
    result = CPLGetLastErrorNo();
}
#endif
int CPLGetLastErrorNo();

#if defined(SWIGPYTHON) || defined(SWIGCSHARP)
/* We don't want errors to be cleared or thrown by this */
/* call */
%exception CPLGetLastErrorType
{
#ifdef SWIGPYTHON
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
#endif
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
#ifdef SWIGPYTHON
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
#endif
    result = (char*)CPLGetLastErrorMsg();
}
#endif
const char *CPLGetLastErrorMsg();


#if defined(SWIGPYTHON) || defined(SWIGCSHARP)
/* We don't want errors to be cleared or thrown by this */
/* call */
%exception CPLGetErrorCounter
{
#ifdef SWIGPYTHON
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
#endif
    result = CPLGetErrorCounter();
}
#endif
unsigned int CPLGetErrorCounter();


int VSIGetLastErrorNo();
const char *VSIGetLastErrorMsg();
void VSIErrorReset();

void CPLPushFinderLocation( const char * utf8_path );

void CPLPopFinderLocation();

void CPLFinderClean();

const char * CPLFindFile( const char *pszClass, const char *utf8_path );

%apply (char **CSL) {char **};
%inline {
char **wrapper_VSIReadDirEx( const char * utf8_path, int nMaxFiles = 0 )
{
    return VSIReadDirEx(utf8_path, nMaxFiles);
}
}
%clear char **;

%apply (char **CSL) {char **};
char **VSIReadDirRecursive( const char * utf8_path );
%clear char **;

#ifdef SWIGPYTHON
%rename (OpenDir) wrapper_VSIOpenDir;
%inline {
VSIDIR* wrapper_VSIOpenDir( const char * utf8_path,
                            int nRecurseDepth = -1,
                            char** options = NULL )
{
    return VSIOpenDir(utf8_path, nRecurseDepth, options);
}
}

%{
typedef struct
{
    char*        name;
    int          mode;
    GIntBig      size;
    GIntBig      mtime;
    bool         modeKnown;
    bool         sizeKnown;
    bool         mtimeKnown;
    char**       extra;
} DirEntry;
%}

struct DirEntry
{
%immutable;
    char*        name;
    int          mode;
    GIntBig      size;
    GIntBig      mtime;
    bool         modeKnown;
    bool         sizeKnown;
    bool         mtimeKnown;

%apply (char **dict) {char **};
    char**       extra;
%clear char **;
%mutable;

%extend {
  DirEntry( const DirEntry *entryIn ) {
    DirEntry *self = (DirEntry*) CPLMalloc( sizeof( DirEntry ) );
    self->name = CPLStrdup(entryIn->name);
    self->mode = entryIn->mode;
    self->size = entryIn->size;
    self->mtime = entryIn->mtime;
    self->modeKnown = entryIn->modeKnown;
    self->sizeKnown = entryIn->sizeKnown;
    self->mtimeKnown = entryIn->mtimeKnown;
    self->extra = CSLDuplicate(entryIn->extra);
    return self;
  }

  ~DirEntry() {
    CPLFree(self->name);
    CSLDestroy(self->extra);
    CPLFree(self);
  }

  bool IsDirectory()
  {
     return (self->mode & S_IFDIR) != 0;
  }

} /* extend */
} /* DirEntry */ ;

%rename (GetNextDirEntry) wrapper_VSIGetNextDirEntry;
%newobject wrapper_VSIGetNextDirEntry;
%apply Pointer NONNULL {VSIDIR* dir};
%inline {
DirEntry* wrapper_VSIGetNextDirEntry(VSIDIR* dir)
{
    const VSIDIREntry* vsiEntry = VSIGetNextDirEntry(dir);
    if( vsiEntry == nullptr )
    {
        return nullptr;
    }
    DirEntry* entry = (DirEntry*) CPLMalloc( sizeof( DirEntry ) );
    entry->name = CPLStrdup(vsiEntry->pszName);
    entry->mode = vsiEntry->nMode;
    entry->size = vsiEntry->nSize;
    entry->mtime = vsiEntry->nMTime;
    entry->modeKnown = vsiEntry->bModeKnown == TRUE;
    entry->sizeKnown = vsiEntry->bSizeKnown == TRUE;
    entry->mtimeKnown = vsiEntry->bMTimeKnown == TRUE;
    entry->extra = CSLDuplicate(vsiEntry->papszExtra);
    return entry;
}
}

%rename (CloseDir) VSICloseDir;
void VSICloseDir(VSIDIR* dir);

#endif

%apply Pointer NONNULL {const char * pszKey};
void CPLSetConfigOption( const char * pszKey, const char * pszValue );
void CPLSetThreadLocalConfigOption( const char * pszKey, const char * pszValue );

%inline {
const char *wrapper_CPLGetConfigOption( const char * pszKey, const char * pszDefault = NULL )
{
    return CPLGetConfigOption( pszKey, pszDefault );
}
const char *wrapper_CPLGetThreadLocalConfigOption( const char * pszKey, const char * pszDefault = NULL )
{
    return CPLGetThreadLocalConfigOption( pszKey, pszDefault );
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

%apply Pointer NONNULL {const char * pszFilename};
/* Added in GDAL 1.7.0 */

#if defined(SWIGPYTHON)

%apply (GIntBig nLen, char *pBuf) {( GIntBig nBytes, const char *pabyData )};
%inline {
void wrapper_VSIFileFromMemBuffer( const char* utf8_path, GIntBig nBytes, const char *pabyData)
{
    const size_t nSize = static_cast<size_t>(nBytes);
    void* pabyDataDup = VSIMalloc(nSize);
    if (pabyDataDup == NULL)
            return;
    memcpy(pabyDataDup, pabyData, nSize);
    VSIFCloseL(VSIFileFromMemBuffer(utf8_path, (GByte*) pabyDataDup, nSize, TRUE));
}
}
%clear ( GIntBig nBytes, const GByte *pabyData );
#else
#if defined(SWIGJAVA)
%apply (int nLen, unsigned char *pBuf ) {( int nBytes, const GByte *pabyData )};
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
#if defined(SWIGJAVA)
%clear ( int nBytes, const GByte *pabyData );
#endif
#endif

/* Added in GDAL 1.7.0 */
VSI_RETVAL VSIUnlink(const char * utf8_path );

%rename (UnlinkBatch) wrapper_VSIUnlinkBatch;
%apply (char **options) {char ** files};
%inline {
bool wrapper_VSIUnlinkBatch(char** files)
{
    int* success = VSIUnlinkBatch(files);
    if( !success )
        return false;
    int bRet = true;
    for( int i = 0; files && files[i]; i++ )
    {
        if( !success[i] ) {
            bRet = false;
            break;
        }
    }
    VSIFree(success);
    return bRet;
}
}
%clear (char **files);

/* Added in GDAL 1.7.0 */
/* Thread support is necessary for binding languages with threaded GC */
/* even if the user doesn't explicitly use threads */
%inline {
int wrapper_HasThreadSupport()
{
    return strcmp(CPLGetThreadingModel(), "stub") != 0;
}
}

/* Added for GDAL 1.8 */
VSI_RETVAL VSIMkdir(const char *utf8_path, int mode );
VSI_RETVAL VSIRmdir(const char *utf8_path );

/* Added for GDAL 2.3 */
VSI_RETVAL VSIMkdirRecursive(const char *utf8_path, int mode );
VSI_RETVAL VSIRmdirRecursive(const char *utf8_path );

%apply (const char* utf8_path) {(const char* pszOld)};
%apply (const char* utf8_path) {(const char* pszNew)};
VSI_RETVAL VSIRename(const char * pszOld, const char *pszNew );
%clear (const char* pszOld);
%clear (const char* pszNew);

#if defined(SWIGPYTHON)
%rename (Sync) wrapper_VSISync;

%apply (const char* utf8_path) {(const char* pszSource)};
%apply (const char* utf8_path) {(const char* pszTarget)};
%feature( "kwargs" ) wrapper_VSISync;

%inline {
bool wrapper_VSISync(const char* pszSource,
                     const char* pszTarget,
                     char** options = NULL,
                     GDALProgressFunc callback=NULL,
                     void* callback_data=NULL)
{
    return VSISync( pszSource, pszTarget, options, callback, callback_data, nullptr );
}
}

%clear (const char* pszSource);
%clear (const char* pszTarget);

bool VSIAbortPendingUploads(const char *utf8_path );

#endif

const char* VSIGetActualURL(const char * utf8_path);

%inline {
retStringAndCPLFree* wrapper_VSIGetSignedURL(const char * utf8_path, char** options = NULL )
{
    return VSIGetSignedURL( utf8_path, options );
}
}

%apply (char **CSL) {char **};
char** VSIGetFileSystemsPrefixes();
%clear char **;

const char* VSIGetFileSystemOptions(const char * utf8_path);


/* Added for GDAL 1.8

   We do not bother renaming the VSI*L api as this wrapping is not
   considered "official", or available for use by application code.
   It is just for some testing stuff.
*/

#if !defined(SWIGJAVA)

#if !defined(SWIGCSHARP)
class VSILFILE
{
    private:
        VSILFILE();
        ~VSILFILE();
};
#endif

#if defined(SWIGPERL)
VSI_RETVAL VSIStatL( const char * utf8_path, VSIStatBufL *psStatBuf );

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
#define VSI_STAT_SET_ERROR_FLAG 0x8
#define VSI_STAT_CACHE_ONLY     0x10

struct StatBuf
{
%immutable;
  int         mode;
  GIntBig     size;
  GIntBig     mtime;
%mutable;

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

%rename (GetFileMetadata) VSIGetFileMetadata;
%apply (char **dict) { char ** };
char** VSIGetFileMetadata( const char *utf8_path, const char* domain,
                           char** options = NULL );
%clear char **;

%rename (SetFileMetadata) VSISetFileMetadata;
%apply (char **dict) { char ** metadata };
bool VSISetFileMetadata( const char * utf8_path,
                         char** metadata,
                         const char* domain,
                         char** options = NULL );
%clear char **;

%apply Pointer NONNULL {VSILFILE* fp};

%rename (VSIFOpenL) wrapper_VSIFOpenL;
%inline %{
VSILFILE   *wrapper_VSIFOpenL( const char *utf8_path, const char *pszMode )
{
    if (!pszMode) /* would lead to segfault */
        pszMode = "r";
    return VSIFOpenL( utf8_path, pszMode );
}
%}

%rename (VSIFOpenExL) wrapper_VSIFOpenExL;
%apply (char **dict) { char ** };
%inline %{
VSILFILE   *wrapper_VSIFOpenExL( const char *utf8_path, const char *pszMode, int bSetError = FALSE, char** options = NULL )
{
    if (!pszMode) /* would lead to segfault */
        pszMode = "r";
    return VSIFOpenEx2L( utf8_path, pszMode, bSetError, options );
}
%}
%clear char **;

int VSIFEofL( VSILFILE* fp );
int VSIFFlushL( VSILFILE* fp );

VSI_RETVAL VSIFCloseL( VSILFILE* fp );

#if defined(SWIGPYTHON)
int     VSIFSeekL( VSILFILE* fp, GIntBig offset, int whence);
GIntBig    VSIFTellL( VSILFILE* fp );
int     VSIFTruncateL( VSILFILE* fp, GIntBig length );

int     VSISupportsSparseFiles( const char* utf8_path );

#define VSI_RANGE_STATUS_UNKNOWN    0
#define VSI_RANGE_STATUS_DATA       1
#define VSI_RANGE_STATUS_HOLE       2

int     VSIFGetRangeStatusL( VSILFILE* fp, GIntBig offset, GIntBig length );
#else
VSI_RETVAL VSIFSeekL( VSILFILE* fp, long offset, int whence);
long    VSIFTellL( VSILFILE* fp );
VSI_RETVAL VSIFTruncateL( VSILFILE* fp, long length );
#endif

#if defined(SWIGPYTHON)
%rename (VSIFWriteL) wrapper_VSIFWriteL;
%inline {
int wrapper_VSIFWriteL( int nLen, char *pBuf, int size, int memb, VSILFILE* fp)
{
    if (nLen < static_cast<GIntBig>(size) * memb)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Inconsistent buffer size with 'size' and 'memb' values");
        return 0;
    }
    return static_cast<int>(VSIFWriteL(pBuf, size, memb, fp));
}
}
#elif defined(SWIGPERL)
size_t VSIFWriteL(const void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);
#else
int     VSIFWriteL( const char *, int, int, VSILFILE *fp );
#endif

#if defined(SWIGPERL)
size_t VSIFReadL(void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);
#endif
/* VSIFReadL() handled specially in python/gdal_python.i */

#if defined(SWIGPERL)
void VSIStdoutSetRedirection( VSIWriteFunction pFct, FILE* stream );
%inline {
void VSIStdoutUnsetRedirection()
{
    VSIStdoutSetRedirection( fwrite, stdout );
}
}
#endif

void VSICurlClearCache();
void VSICurlPartialClearCache( const char* utf8_path );

void VSINetworkStatsReset();
retStringAndCPLFree* VSINetworkStatsGetAsSerializedJSON( char** options = NULL );

#endif /* !defined(SWIGJAVA) */

%apply (char **CSL) {char **};
%rename (ParseCommandLine) CSLParseCommandLine;
char **CSLParseCommandLine( const char * utf8_path );
%clear char **;
