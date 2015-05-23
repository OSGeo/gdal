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

%include typedefs.i

%inline %{
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, "%s", message );
  }

  CPLErr SetErrorHandler( char const * pszCallbackName = NULL )
  {
    CPLErrorHandler pfnHandler = NULL;
    if( pszCallbackName == NULL || EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
      pfnHandler = CPLQuietErrorHandler;
    else if( EQUAL(pszCallbackName,"CPLDefaultErrorHandler") )
      pfnHandler = CPLDefaultErrorHandler;
    else if( EQUAL(pszCallbackName,"CPLLoggingErrorHandler") )
      pfnHandler = CPLLoggingErrorHandler;

    if ( pfnHandler == NULL )
      return CE_Fatal;

    CPLSetErrorHandler( pfnHandler );

    return CE_None;
  }
%}

#ifdef SWIGPYTHON

%{
void CPL_STDCALL PyCPLErrorHandler(CPLErr eErrClass, int err_no, const char* pszErrorMsg)
{
    void* user_data = CPLGetErrorHandlerUserData();
    PyObject *psArgs;

    psArgs = Py_BuildValue("(iis)", eErrClass, err_no, pszErrorMsg );
    PyEval_CallObject( (PyObject*)user_data, psArgs);
    Py_XDECREF(psArgs);
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

  void PopErrorHandler()
  {
     void* user_data = CPLGetErrorHandlerUserData();
     if( user_data != NULL )
       Py_XDECREF((PyObject*)user_data);
     CPLPopErrorHandler();
  }
%}

#else /* #ifdef SWIGPYTHON */

void CPLPushErrorHandler( CPLErrorHandler ); /* is this really needed? */

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

%rename (PopErrorHandler) CPLPopErrorHandler;
void CPLPopErrorHandler();

#endif /* #ifdef SWIGPYTHON */

/* this was in Java bindings without default values for the arguments, why? */
%inline%{
  void Error( CPLErr msg_class = CE_Failure, int err_code = 0, const char* msg = "error" ) {
    CPLError( msg_class, err_code, "%s", msg );
  }
%}

retStringAndCPLFree*
GOA2GetAuthorizationURL( const char *pszScope );

retStringAndCPLFree*
GOA2GetRefreshToken( const char *pszAuthToken, const char *pszScope );

retStringAndCPLFree*
GOA2GetAccessToken( const char *pszRefreshToken, const char *pszScope );

%rename (ErrorReset) CPLErrorReset;
void CPLErrorReset();

%inline%{
retStringAndCPLFree* EscapeString(int nLen, char *pBuf, int scheme=CPLES_SQL) {
    return CPLEscapeString((const char*)pBuf, nLen, scheme);
} 

retStringAndCPLFree* EscapeString(const char* str, int scheme) {
    return CPLEscapeString(str, (str) ? strlen(str) : 0, scheme);
}
%}

%rename (GetLastErrorNo) CPLGetLastErrorNo;
/* We don't want errors to be cleared or thrown by this call */
%exception CPLGetLastErrorNo
{
    result = CPLGetLastErrorNo();
}
int CPLGetLastErrorNo();

%rename (GetLastErrorType) CPLGetLastErrorType;
/* We don't want errors to be cleared or thrown by this call */
%exception CPLGetLastErrorType
{
    result = CPLGetLastErrorType();
}
int CPLGetLastErrorType();

%rename (GetLastErrorMsg) CPLGetLastErrorMsg;
/* We don't want errors to be cleared or thrown by this call */
%exception CPLGetLastErrorMsg
{
    result = (char*)CPLGetLastErrorMsg();
}
const char *CPLGetLastErrorMsg();

%rename (PushFinderLocation) CPLPushFinderLocation;
void CPLPushFinderLocation( const char * utf8_path );

%rename (PopFinderLocation) CPLPopFinderLocation;
void CPLPopFinderLocation();

%rename (FinderClean) CPLFinderClean;
void CPLFinderClean();

%rename (FindFile) CPLFindFile;
const char * CPLFindFile( const char *pszClass, const char *utf8_path );

%rename (ReadDir) VSIReadDir;
%apply (char **CSL) {char **};
char **VSIReadDir( const char * utf8_path );
%clear char **;

%rename (ReadDirRecursive) VSIReadDirRecursive;
%apply (char **CSL) {char **};
char **VSIReadDirRecursive( const char * utf8_path );
%clear char **;

%rename (SetConfigOption) CPLSetConfigOption;
%apply Pointer NONNULL {const char * pszKey};
void CPLSetConfigOption( const char * pszKey, const char * pszValue );

%rename (GetConfigOption) wrapper_CPLGetConfigOption;
%inline {
const char *wrapper_CPLGetConfigOption( const char * pszKey, const char * pszDefault = NULL )
{
    return CPLGetConfigOption( pszKey, pszDefault );
}
}
%clear const char * pszKey;

%rename (BinaryToHex) wrapped_CPLBinaryToHex;
%inline {
retStringAndCPLFree* wrapped_CPLBinaryToHex(int nLen, char *pBuf)
{
    return CPLBinaryToHex(nLen, (const GByte *)pBuf);
}
}

%rename (HexToBinary) CPLHexToBinary;
GByte *CPLHexToBinary( const char *pszHex, int *pnBytes );

%apply Pointer NONNULL {const char * pszFilename};
/* Added in GDAL 1.7.0 */
%rename (FileFromMemBuffer) wrapper_VSIFileFromMemBuffer;
%inline {
void wrapper_VSIFileFromMemBuffer( const char* utf8_path, int nLen, char *pBuf)
{
    GByte* pabyDataDup = (GByte*)VSIMalloc(nLen);
    if (pabyDataDup == NULL)
            return;
    memcpy(pabyDataDup, pBuf, nLen);
    VSIFCloseL(VSIFileFromMemBuffer(utf8_path, (GByte*) pabyDataDup, nLen, TRUE));
}
}

/* Added in GDAL 1.7.0 */
%rename (Unlink) VSIUnlink;
VSI_RETVAL VSIUnlink(const char * utf8_path );

/* Added in GDAL 1.7.0 */
/* Thread support is necessary for binding languages with threaded GC */
/* even if the user doesn't explicitly use threads */
%rename (HasThreadSupport) wrapper_HasThreadSupport;
%inline {
int wrapper_HasThreadSupport()
{
    return strcmp(CPLGetThreadingModel(), "stub") != 0;
}
}

/* Added for GDAL 1.8 */
%rename (Mkdir) VSIMkdir;
VSI_RETVAL VSIMkdir(const char *utf8_path, int mode );

%rename (Rmdir) VSIRmdir;
VSI_RETVAL VSIRmdir(const char *utf8_path );

%rename (Rename) VSIRename;
VSI_RETVAL VSIRename(const char * pszOld, const char *pszNew );

/* Added for GDAL 1.8 

   We do not bother renaming the VSI*L api as this wrapping is not
   considered "official", or available for use by application code. 
   It is just for some testing stuff. 
*/

#if defined(SWIGPERL)
VSI_RETVAL VSIStatL( const char * utf8_path, VSIStatBufL *psStatBuf );
size_t VSIFWriteL(const void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);
size_t VSIFReadL(void *pBuffer, size_t nSize, size_t nCount, VSILFILE *fp);

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

%rename (VSIFWriteL) wrapper_VSIFWriteL;
%inline {
int wrapper_VSIFWriteL( int nLen, char *pBuf, int size, int memb, VSILFILE * f)
{
    if (nLen < size * memb)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Inconsistent buffer size with 'size' and 'memb' values");
        return 0;
    }
    return VSIFWriteL(pBuf, size, memb, f);
}
}

/* VSIFReadL() handled specially in python/gdal_python.i */

#elif !defined(SWIGJAVA)

int     VSIFWriteL( const char *, int, int, VSILFILE * );

#endif /* defined(SWIGPERL) */

#if !defined(SWIGJAVA)

%rename (VSIFOpenL) wrapper_VSIFOpenL;
%inline %{
VSILFILE   *wrapper_VSIFOpenL( const char *utf8_path, const char *pszMode )
{
    if (!pszMode) /* would lead to segfault */
        pszMode = "r";
    return VSIFOpenL( utf8_path, pszMode );
}
%}
VSI_RETVAL VSIFCloseL( VSILFILE * );
VSI_RETVAL VSIFSeekL( VSILFILE *, GIntBig, int );
GIntBig    VSIFTellL( VSILFILE * );
VSI_RETVAL VSIFTruncateL( VSILFILE *, GIntBig );

#endif /* !defined(SWIGJAVA) */
