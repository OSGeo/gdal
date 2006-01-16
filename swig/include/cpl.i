/******************************************************************************
 * $Id$
 *
 * Name:     cpl.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.4  2006/01/16 08:07:04  cfis
 * Exposed CPLHexToBinary and CPLHexToBinary to scripting languages.
 *
 * Revision 1.3  2005/10/02 23:31:27  cfis
 * Updated the renames to include support for Ruby.
 *
 * Revision 1.2  2005/02/16 16:54:48  kruland
 * Removed the python code from the wrapper for now.  Wrapped a simple version
 * of PushErrorHandler(char const*) which allows assignment of the CPL defined
 * error handlers.
 *
 *
*/

%inline %{
  void Debug( const char *msg_class, const char *message ) {
    CPLDebug( msg_class, message );
  }
  void Error( CPLErr msg_class = CE_Failure, int err_code = 0, const char* msg = "error" ) {
    CPLError( msg_class, err_code, msg );
  }

  CPLErr PushErrorHandler( char const * pszCallbackName = "CPLQuietErrorHandler" ) {
    CPLErrorHandler pfnHandler = NULL;
    if( EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
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

#ifndef SWIGRUBY
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
%rename (SetConfigOption) CPLSetConfigOption;
%rename (GetConfigOption) CPLGetConfigOption;
%rename (CPLBinaryToHex) CPLBinaryToHex;
%rename (CPLHexToBinary) CPLHexToBinary;
#else
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
%rename (set_config_option) CPLSetConfigOption;
%rename (get_config_option) CPLGetConfigOption;
%rename (binary_to_hex) CPLBinaryToHex;
%rename (hex_to_binary) CPLHexToBinary;
#endif

void CPLPushErrorHandler( CPLErrorHandler );

void CPLPopErrorHandler();

void CPLErrorReset();

int CPLGetLastErrorNo();

CPLErr CPLGetLastErrorType();

char const *CPLGetLastErrorMsg();

void CPLPushFinderLocation( const char * );

void CPLPopFinderLocation();

void CPLFinderClean();

const char * CPLFindFile( const char *, const char * );

void CPLSetConfigOption( const char *, const char * );

const char * CPLGetConfigOption( const char *, const char * );

/* Provide hooks to hex encoding methods */
char *CPLBinaryToHex( int nBytes, const GByte *pabyData );
GByte *CPLHexToBinary( const char *pszHex, int *pnBytes );

