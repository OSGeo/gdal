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
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, message );
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
    CPLError( msg_class, err_code, msg );
  }
%}
#else
%inline%{
  void Error( CPLErr msg_class = CE_Failure, int err_code = 0, const char* msg = "error" ) {
    CPLError( msg_class, err_code, msg );
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
%rename (set_config_option) CPLSetConfigOption;
%rename (get_config_option) wrapper_CPLGetConfigOption;
%rename (binary_to_hex) CPLBinaryToHex;
%rename (hex_to_binary) CPLHexToBinary;
%rename (file_from_mem_buffer) wrapper_VSIFileFromMemBuffer;
%rename (unlink) VSIUnlink;
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
%rename (FindFile) CPLFindFile;
%rename (ReadDir) VSIReadDir;
%rename (SetConfigOption) CPLSetConfigOption;
%rename (GetConfigOption) wrapper_CPLGetConfigOption;
%rename (CPLBinaryToHex) CPLBinaryToHex;
%rename (CPLHexToBinary) CPLHexToBinary;
%rename (FileFromMemBuffer) wrapper_VSIFileFromMemBuffer;
%rename (Unlink) VSIUnlink;
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
%{
typedef char retStringAndCPLFree;
%}
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
#else
#ifndef SWIGCSHARP
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
#endif
%inline %{
char* EscapeString(int len, char *bin_string , int scheme=CPLES_SQL) {
    return CPLEscapeString(bin_string, len, scheme);
} 
%}
#ifndef SWIGCSHARP
%clear (int len, char *bin_string);
#endif
#endif

int CPLGetLastErrorNo();

CPLErr CPLGetLastErrorType();

char const *CPLGetLastErrorMsg();

void CPLPushFinderLocation( const char * pszLocation );

void CPLPopFinderLocation();

void CPLFinderClean();

const char * CPLFindFile( const char *pszClass, const char *pszBasename );

#if defined(SWIGPYTHON) || defined (SWIGJAVA)
%apply (char **out_ppsz_and_free) {char **};
#else
/* FIXME: wrong typemap. VSIReadDir() return should be CSLDestroy'ed */
%apply (char **options) {char **};
#endif
char **VSIReadDir( const char * pszDirName );
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
#ifdef SWIGJAVA
%apply (int nLen, unsigned char *pBuf ) {( int nBytes, const GByte *pabyData )};
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
#ifdef SWIGJAVA
%apply (int nLen, unsigned char *pBuf ) {( int nBytes, const GByte *pabyData )};
#elif defined(SWIGPYTHON)
%apply (int nLen, char *pBuf) {( int nBytes, const GByte *pabyData )};
#endif
%inline {
void wrapper_VSIFileFromMemBuffer( const char* pszFilename, int nBytes, const GByte *pabyData)
{
    GByte* pabyDataDup = (GByte*)VSIMalloc(nBytes);
    if (pabyDataDup == NULL)
            return;
    memcpy(pabyDataDup, pabyData, nBytes);
    VSIFCloseL(VSIFileFromMemBuffer(pszFilename, (GByte*) pabyDataDup, nBytes, TRUE));
}

}
#if defined(SWIGJAVA) || defined(SWIGPYTHON)
%clear ( int nBytes, const GByte *pabyData );
#endif

/* Added in GDAL 1.7.0 */
int VSIUnlink(const char * pszFilename );
