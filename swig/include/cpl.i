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

%rename (PushErrorHandler) CPLPushErrorHandler;
void CPLPushErrorHandler( CPLErrorHandler );

%rename (PopErrorHandler) CPLPopErrorHandler;
void CPLPopErrorHandler();

%rename (ErrorReset) CPLErrorReset;
void CPLErrorReset();

%rename (GetLastErrorNo) CPLGetLastErrorNo;
int CPLGetLastErrorNo();

%rename (GetLastErrorType) CPLGetLastErrorType;
CPLErr CPLGetLastErrorType();

%rename (GetLastErrorMsg) CPLGetLastErrorMsg;
char const *CPLGetLastErrorMsg();

%rename (PushFinderLocation) CPLPushFinderLocation;
void CPLPushFinderLocation( const char * );

%rename (PopFinderLocation) CPLPopFinderLocation;
void CPLPopFinderLocation();

%rename (FinderClean) CPLFinderClean;
void CPLFinderClean();

%rename (FindFile) CPLFindFile;
const char * CPLFindFile( const char *, const char * );

%rename (SetConfigOption) CPLSetConfigOption;
void CPLSetConfigOption( const char *, const char * );

%rename (GetConfigOption) CPLGetConfigOption;
const char * CPLGetConfigOption( const char *, const char * );
